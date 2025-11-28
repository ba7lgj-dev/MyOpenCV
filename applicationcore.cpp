#include "applicationcore.h"
#include <QDateTime>
#include <QFile>
#include <opencv2/opencv.hpp>

ApplicationCore::ApplicationCore(QObject *parent)
    : QObject(parent)
{
    estimator = new MultiLineCannyWidthEstimator();
    series0 = new QLineSeries();
    series1 = new QLineSeries();
    chartView = new QChartView();
    chartView->chart()->legend()->setVisible(true);
    series0->setName(tr("摄像头0"));
    series1->setName(tr("摄像头1"));
    chartView->chart()->addSeries(series0);
    chartView->chart()->addSeries(series1);
    chartView->chart()->createDefaultAxes();

    connect(&pump, &Cp2102PumpController::safetyTriggered, this, &ApplicationCore::onPumpSafety);
    connect(&push, &PushManager::message, this, &ApplicationCore::message);
    connect(&push, &PushManager::alarm, this, &ApplicationCore::onPushAlarm);
}

ApplicationCore::~ApplicationCore()
{
    delete estimator;
    delete chartView;
}

void ApplicationCore::initialize()
{
    QString cfgPath = "config.json";
    if (QFile::exists(cfgPath)) {
        cfg.load(cfgPath);
    } else {
        cfg.save(cfgPath);
    }
    scanCameras();
    cam0.open(cfg.camera(0).index);
    cam1.open(cfg.camera(1).index);
    cam0.setConfig(cfg.camera(0));
    cam1.setConfig(cfg.camera(1));
    autoPump = cfg.config().autoPumpEnabled;

    connect(&cam0, &UsbCamera::rawFrameReady, this, &ApplicationCore::onFrame0);
    connect(&cam1, &UsbCamera::rawFrameReady, this, &ApplicationCore::onFrame1);
    connect(&cam0, &UsbCamera::frameReady, [this](const QImage &img){ emit cameraFrame(0, img);});
    connect(&cam1, &UsbCamera::frameReady, [this](const QImage &img){ emit cameraFrame(1, img);});
    connect(&cam0, &UsbCamera::cameraError, this, [this](const QString &msg){ emit message(msg); LogManager::instance().logWarn(msg);});
    connect(&cam1, &UsbCamera::cameraError, this, [this](const QString &msg){ emit message(msg); LogManager::instance().logWarn(msg);});

    notifyPush(tr("程序启动"));
}

ConfigManager *ApplicationCore::config()
{
    return &cfg;
}

CalibrationManager *ApplicationCore::calibration()
{
    return &calib;
}

QChartView *ApplicationCore::trendChart()
{
    return chartView;
}

void ApplicationCore::startCameras()
{
    cam0.start();
    cam1.start();
    if (!cfg.config().pumpPort.isEmpty()) {
        pump.open(cfg.config().pumpPort);
    }
}

void ApplicationCore::stopCameras()
{
    cam0.stop();
    cam1.stop();
    pump.close();
    notifyPush(tr("正常关闭"));
}

void ApplicationCore::calibrateWidth(int cameraId, double realMM)
{
    calib.addCalibrationSample(cameraId, realMM, lastResult[cameraId].widthPixels);
}

void ApplicationCore::toggleAutoExposure(int cameraId)
{
    if (cameraId == 0) cam0.triggerAutoExposure();
    else cam1.triggerAutoExposure();
}

void ApplicationCore::setAutoPumpEnabled(bool enabled)
{
    autoPump = enabled;
    cfg.setAutoPumpEnabled(enabled);
    emit message(enabled ? tr("自动加气已开启") : tr("自动加气已关闭"));
}

void ApplicationCore::sendPush(const QString &title, const QString &detail)
{
    notifyPush(title, detail);
}

void ApplicationCore::onFrame0(const cv::Mat &frame)
{
    handleWidth(0, frame);
}

void ApplicationCore::onFrame1(const cv::Mat &frame)
{
    handleWidth(1, frame);
}

void ApplicationCore::handleWidth(int id, const cv::Mat &frame)
{
    CameraConfig cfgCam = cfg.camera(id);
    WidthResult r = estimator->estimate(frame, cfgCam);
    if (!r.valid) return;
    r.widthMM = r.widthPixels * cfgCam.mmPerPixel;
    lastResult[id] = r;
    calib.addCalibrationSample(id, r.widthMM, r.widthPixels); // keep smoothing
    appendTrend(id, r.widthMM);
    emit widthUpdated(id, r);
    processPumpLogic(id, r, cfgCam);
    LogManager::instance().logInfo(QString("Camera%1 width=%2px, %3mm").arg(id).arg(r.widthPixels).arg(r.widthMM));
}

void ApplicationCore::processPumpLogic(int id, const WidthResult &result, const CameraConfig &cfgCam)
{
    Q_UNUSED(id)
    if (!autoPump) return;
    double threshold = cfg.config().pumpThresholdMM > 0 ? cfg.config().pumpThresholdMM : cfgCam.thresholdMM;
    if (result.widthMM >= threshold) return;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int cooldown = cfg.config().pumpCooldownMs > 0 ? cfg.config().pumpCooldownMs : cfgCam.cooldownMs;
    if (now - lastPulseMs < cooldown) return;
    int pulse = PumpPolicy::calcPulseMs(threshold, result.widthMM);
    pump.pulseLow(pulse);
    lastPulseMs = now;
    LogManager::instance().logInfo(tr("自动加气已执行，脉冲=%1ms").arg(pulse));
}

void ApplicationCore::appendTrend(int id, double widthMM)
{
    qint64 x = QDateTime::currentMSecsSinceEpoch();
    QLineSeries *series = id == 0 ? series0 : series1;
    series->append(x, widthMM);
    while (series->count() > 200) {
        series->removePoints(0, 1);
    }
    chartView->chart()->axes(Qt::Horizontal).first()->setRange(x - 60000, x);
}

void ApplicationCore::onPumpSafety(const QString &msg)
{
    autoPump = false;
    emit safetyModeEnabled();
    emit message(msg);
}

void ApplicationCore::onPushAlarm(const QString &msg)
{
    emit message(msg);
}

void ApplicationCore::scanCameras()
{
    availableCameras.clear();
    for (int i = 0; i < 10; ++i) {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) {
            availableCameras.append(i);
            cap.release();
        }
    }
    if (availableCameras.isEmpty()) {
        emit message(tr("未检测到可用摄像头"));
        LogManager::instance().logWarn("No cameras detected");
    }
}

void ApplicationCore::applyCameraConfig(int id)
{
    if (id == 0) {
        cam0.close();
        cam0.open(cfg.camera(0).index);
        cam0.setConfig(cfg.camera(0));
    } else {
        cam1.close();
        cam1.open(cfg.camera(1).index);
        cam1.setConfig(cfg.camera(1));
    }
}

void ApplicationCore::notifyPush(const QString &title, const QString &detail)
{
    push.sendEvent(title, detail);
}

