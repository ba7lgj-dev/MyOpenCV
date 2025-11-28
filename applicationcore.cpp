#include "applicationcore.h"
#include <QDateTime>

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
    rescanCameras();
    openCamerasFromConfig();
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
    }
    if (!cfg.config().pumpPort.isEmpty()) {
        pump.open(cfg.config().pumpPort);
    }
    running = true;
}

void ApplicationCore::stopCameras()
{
    cam0.stop();
    cam1.stop();
    pump.close();
    running = false;
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

void ApplicationCore::onFrame0(const cv::Mat &frame)
{
    handleWidth(0, frame);
}

void ApplicationCore::onFrame1(const cv::Mat &frame)
{
    if (!cfg.config().dualCameraMode) return;
    handleWidth(1, frame);
}

void ApplicationCore::handleWidth(int id, const cv::Mat &frame)
{
    CameraConfig cfgCam = cfg.camera(id);
    cv::Mat rotated = applyRotation(frame, cfgCam.rotation);
    WidthResult r = estimator->estimate(rotated, cfgCam);
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

cv::Mat ApplicationCore::applyRotation(const cv::Mat &frame, int rotation) const
{
    if (frame.empty()) return frame;
    cv::Mat rotated = frame;
    switch (rotation) {
    case 90:
        cv::rotate(frame, rotated, cv::ROTATE_90_CLOCKWISE);
        break;
    case 180:
        cv::rotate(frame, rotated, cv::ROTATE_180);
        break;
    case 270:
        cv::rotate(frame, rotated, cv::ROTATE_90_COUNTERCLOCKWISE);
        break;
    default:
        break;
    }
    return rotated;
}

void ApplicationCore::rescanCameras()
{
    availableIndices.clear();
    for (int i = 0; i < 10; ++i) {
        cv::VideoCapture cap;
        if (cap.open(i)) {
            availableIndices.append(i);
            cap.release();
        }
    }
}

void ApplicationCore::openCamerasFromConfig()
{
    cam0.close();
    cam1.close();
    CameraConfig c0 = cfg.camera(0);
    CameraConfig c1 = cfg.camera(1);
    cam0.open(c0.index);
    cam0.setConfig(c0);
    if (cfg.config().dualCameraMode) {
        cam1.open(c1.index);
        cam1.setConfig(c1);
    }
}

void ApplicationCore::reloadCameraConfig()
{
    bool wasRunning = running;
    stopCameras();
    openCamerasFromConfig();
    if (wasRunning) {
        startCameras();
    }
}

