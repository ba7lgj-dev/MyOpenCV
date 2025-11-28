#include "applicationcore.h"
#include <QDateTime>
#include <QStringList>
#include <opencv2/videoio.hpp>

ApplicationCore::ApplicationCore(QObject *parent)
    : QObject(parent)
{
    estimator = new MultiLineCannyWidthEstimator();
    series0 = new QLineSeries();
    series1 = new QLineSeries();
    chartView = new QChartView();
    chartView->chart()->legend()->setVisible(true);
    series0->setName("Camera0");
    series1->setName("Camera1");
    chartView->chart()->addSeries(series0);
    chartView->chart()->addSeries(series1);
    chartView->chart()->createDefaultAxes();

    connect(&pump, &Cp2102PumpController::safetyTriggered, this, &ApplicationCore::onPumpSafety);
}

ApplicationCore::~ApplicationCore()
{
    delete estimator;
    delete chartView;
}

void ApplicationCore::initialize()
{
    if (!cfg.load(configPath)) {
        cfg.restoreDefaults();
        cfg.save(configPath);
    }

    availableCameras = scanCameras();
    emit availableCamerasChanged(availableCameras);
    emit message(tr("检测到摄像头: %1").arg([&](){
        QStringList items;
        for (int idx : availableCameras) items << QString::number(idx);
        return items.join(", ");
    }()));

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
    if (cfg.config().dualCameraMode) {
        cam1.start();
    } else {
        cam1.stop();
    }
    camerasRunning = true;
    if (!cfg.config().pumpPort.isEmpty()) {
        pump.open(cfg.config().pumpPort);
    }
}

void ApplicationCore::stopCameras()
{
    cam0.stop();
    cam1.stop();
    camerasRunning = false;
    pump.close();
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
    emit message(enabled ? tr("Auto pump enabled") : tr("Auto pump disabled"));
}

void ApplicationCore::setCameraIndex(int id, int cameraIndex)
{
    cfg.setCameraIndex(id, cameraIndex);
    applyCameraConfig(id);
}

void ApplicationCore::setCameraName(int id, const QString &name)
{
    cfg.setCameraName(id, name);
    emit cameraConfigApplied(id, cfg.camera(id));
}

void ApplicationCore::setLineRatio(int id, double ratio)
{
    cfg.setLineRatio(id, ratio);
    emit cameraConfigApplied(id, cfg.camera(id));
}

void ApplicationCore::setLineColor(int id, const QColor &color)
{
    cfg.setLineColor(id, color);
    emit cameraConfigApplied(id, cfg.camera(id));
}

void ApplicationCore::setRotation(int id, int rotation)
{
    cfg.setRotation(id, rotation);
    applyCameraConfig(id);
}

void ApplicationCore::swapCameras()
{
    cfg.swapCameras();
    applyCameraConfig(0);
    applyCameraConfig(1);
    emit cameraConfigApplied(0, cfg.camera(0));
    emit cameraConfigApplied(1, cfg.camera(1));
}

void ApplicationCore::setDualCameraMode(bool dual)
{
    cfg.setDualCameraMode(dual);
    if (!dual) {
        cam1.stop();
    } else if (camerasRunning) {
        cam1.start();
    }
}

void ApplicationCore::rescanCameras()
{
    availableCameras = scanCameras();
    emit availableCamerasChanged(availableCameras);
    emit message(tr("重新扫描摄像头: %1").arg([&](){
        QStringList items;
        for (int idx : availableCameras) items << QString::number(idx);
        return items.join(", ");
    }()));
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
    if (result.widthMM >= cfgCam.thresholdMM) return;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastPulseMs < cfgCam.cooldownMs) return;
    int pulse = PumpPolicy::calcPulseMs(cfgCam.thresholdMM, result.widthMM);
    pump.pulseLow(pulse);
    lastPulseMs = now;
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

void ApplicationCore::applyCameraConfig(int id)
{
    CameraConfig cfgCam = cfg.camera(id);
    UsbCamera *cam = id == 0 ? &cam0 : &cam1;
    bool shouldRun = camerasRunning && (id == 0 || cfg.config().dualCameraMode);
    cam->stop();
    cam->close();
    cam->open(cfgCam.index);
    cam->setConfig(cfgCam);
    if (shouldRun) {
        cam->start();
    }
    emit cameraConfigApplied(id, cfgCam);
}

QVector<int> ApplicationCore::scanCameras(int maxIndex) const
{
    QVector<int> indexes;
    for (int i = 0; i <= maxIndex; ++i) {
        cv::VideoCapture cap;
        if (cap.open(i)) {
            indexes.append(i);
            cap.release();
        }
    }
    return indexes;
}

