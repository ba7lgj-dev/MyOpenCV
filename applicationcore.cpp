#include "applicationcore.h"
#include <QDateTime>
#include <opencv2/opencv.hpp>

static const QString kDefaultConfigPath = QStringLiteral("config.json");

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
    retryTimer0.setSingleShot(true);
    retryTimer1.setSingleShot(true);
    connect(&retryTimer0, &QTimer::timeout, this, &ApplicationCore::retryOpenCamera0);
    connect(&retryTimer1, &QTimer::timeout, this, &ApplicationCore::retryOpenCamera1);
}

ApplicationCore::~ApplicationCore()
{
    delete estimator;
    delete chartView;
}

void ApplicationCore::initialize()
{
    if (!cfg.load(kDefaultConfigPath)) {
        cfg.save(kDefaultConfigPath);
    }

    scanCameras();

    autoPump = cfg.config().autoPumpEnabled;

    connect(&cam0, &UsbCamera::rawFrameReady, this, &ApplicationCore::onFrame0);
    connect(&cam1, &UsbCamera::rawFrameReady, this, &ApplicationCore::onFrame1);
    connect(&cam0, &UsbCamera::frameReady, [this](const QImage &img){ emit cameraFrame(0, img);});
    connect(&cam1, &UsbCamera::frameReady, [this](const QImage &img){ emit cameraFrame(1, img);});
    connect(&cam0, &UsbCamera::cameraError, this, [this](const QString &msg){ emit cameraError(0, msg); emit message(msg); LogManager::instance().logWarn(msg);});
    connect(&cam1, &UsbCamera::cameraError, this, [this](const QString &msg){ emit cameraError(1, msg); emit message(msg); LogManager::instance().logWarn(msg);});

    tryOpenCamera(0);
    tryOpenCamera(1);
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
    if (!cam0.isOpened()) tryOpenCamera(0);
    if (cfg.config().dualCameraMode && !cam1.isOpened()) tryOpenCamera(1);
    cam0.start();
    if (cfg.config().dualCameraMode) {
        cam1.start();
    }
    if (!cfg.config().pumpPort.isEmpty()) {
        pump.open(cfg.config().pumpPort);
    }
}

void ApplicationCore::stopCameras()
{
    cam0.stop();
    cam1.stop();
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

void ApplicationCore::setDualCameraMode(bool enabled)
{
    cfg.setDualCameraMode(enabled);
    emit message(enabled ? tr("Dual camera mode") : tr("Single camera mode"));
    if (!enabled) {
        cam1.stop();
    }
}

void ApplicationCore::setCameraIndex(int cameraId, int index)
{
    cfg.setCameraIndex(cameraId, index);
    tryOpenCamera(cameraId);
}

void ApplicationCore::setCameraName(int cameraId, const QString &name)
{
    cfg.setCameraName(cameraId, name);
}

void ApplicationCore::setCameraRotation(int cameraId, int rotation)
{
    cfg.setCameraRotation(cameraId, rotation);
    if (cameraId == 0) cam0.setConfig(cfg.camera(0)); else cam1.setConfig(cfg.camera(1));
}

void ApplicationCore::setLineRatio(int cameraId, double ratio)
{
    cfg.setLineRatio(cameraId, ratio);
}

void ApplicationCore::setLineColor(int cameraId, const QColor &color)
{
    cfg.setLineColor(cameraId, color);
}

void ApplicationCore::setLineHeightPx(int cameraId, int height)
{
    cfg.setLineHeightPx(cameraId, height);
}

void ApplicationCore::setWidthRegionHeight(int cameraId, int height)
{
    cfg.setWidthRegionHeight(cameraId, height);
}

void ApplicationCore::swapCameras()
{
    auto c0 = cfg.camera(0);
    auto c1 = cfg.camera(1);
    cfg.setCameraIndex(0, c1.index);
    cfg.setCameraIndex(1, c0.index);
    cfg.setCameraName(0, c1.name);
    cfg.setCameraName(1, c0.name);
    cfg.setCameraRotation(0, c1.rotation);
    cfg.setCameraRotation(1, c0.rotation);
    tryOpenCamera(0);
    tryOpenCamera(1);
}

QVector<int> ApplicationCore::availableCameraIndexes() const
{
    return availableIndexes;
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

bool ApplicationCore::tryOpenCamera(int id)
{
    UsbCamera *cam = id == 0 ? &cam0 : &cam1;
    CameraConfig c = cfg.camera(id);
    bool ok = cam->open(c.index);
    if (ok) {
        cam->setConfig(c);
        if (id == 0) retryTimer0.stop(); else retryTimer1.stop();
    } else {
        LogManager::instance().logWarn(tr("Camera %1 open failed, will retry").arg(id));
        emit cameraError(id, tr("Camera %1 open failed, will retry").arg(id));
        scheduleRetry(id);
    }
    return ok;
}

void ApplicationCore::scheduleRetry(int id)
{
    const int intervalMs = 3000;
    if (id == 0) {
        retryTimer0.start(intervalMs);
    } else {
        retryTimer1.start(intervalMs);
    }
}

void ApplicationCore::retryOpenCamera0()
{
    tryOpenCamera(0);
}

void ApplicationCore::retryOpenCamera1()
{
    tryOpenCamera(1);
}

void ApplicationCore::scanCameras()
{
    QVector<int> found;
    const int maxIndex = 5;
    for (int i = 0; i <= maxIndex; ++i) {
        cv::VideoCapture cap;
        if (cap.open(i)) {
            found.append(i);
        }
    }
    availableIndexes = found;
    emit availableCamerasChanged(found);
}

