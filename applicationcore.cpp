#include "applicationcore.h"
#include <QDateTime>
#include <QtGlobal>
#include <algorithm>
#include <opencv2/opencv.hpp>

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

    push = new PushManager(&cfg, this);
    autoPumpController = new AutoPumpController(&pump, push, &cfg);
    autoPumpController->moveToThread(&autoPumpThread);
    connect(&autoPumpThread, &QThread::finished, autoPumpController, &QObject::deleteLater);
    autoPumpThread.start();
    pump.moveToThread(&autoPumpThread);
    QMetaObject::invokeMethod(autoPumpController, [this]() { autoPumpController->updateSettings(cfg.config()); }, Qt::QueuedConnection);
    connect(&pump, &Cp2102PumpController::safetyTriggered, this, &ApplicationCore::onPumpSafety);
    connect(autoPumpController, &AutoPumpController::statusMessage, this, &ApplicationCore::message);
}

ApplicationCore::~ApplicationCore()
{
    delete estimator;
    delete chartView;
    autoPumpThread.quit();
    autoPumpThread.wait();
}

void ApplicationCore::initialize()
{
    if (!cfg.load(defaultConfigPath)) {
        cfg.restoreDefaults();
        cfg.save(defaultConfigPath);
    }
    if (push) {
        push->reloadConfig();
    }
    if (cfg.camera(0).enabled && cfg.camera(0).index >= 0) {
        cam0.open(cfg.camera(0).index);
        cam0.setConfig(cfg.camera(0));
    }
    if (cfg.camera(1).enabled && cfg.camera(1).index >= 0) {
        cam1.open(cfg.camera(1).index);
        cam1.setConfig(cfg.camera(1));
    }
    autoPump = cfg.config().autoPumpEnabled;
    applyPumpSettings();
    if (autoPumpController) {
        QMetaObject::invokeMethod(autoPumpController, [this]() { autoPumpController->updateSettings(cfg.config()); }, Qt::QueuedConnection);
    }

    connect(&cam0, &UsbCamera::rawFrameReady, this, &ApplicationCore::onFrame0);
    connect(&cam1, &UsbCamera::rawFrameReady, this, &ApplicationCore::onFrame1);
    connect(&cam0, &UsbCamera::frameReady, [this](const QImage &img){ cameraReady[0] = true; emit cameraFrame(0, img);});
    connect(&cam1, &UsbCamera::frameReady, [this](const QImage &img){ cameraReady[1] = true; emit cameraFrame(1, img);});
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

PushManager *ApplicationCore::pushManager()
{
    return push;
}

void ApplicationCore::startCameras()
{
    running = true;
    cameraReady[0] = false;
    cameraReady[1] = false;
    if (cfg.camera(0).enabled && cfg.camera(0).index >= 0) {
        cam0.start();
    }
    if (cfg.config().dualCameraMode && cfg.camera(1).enabled && cfg.camera(1).index >= 0) {
        cam1.start();
    }
    if (!cfg.config().pumpPort.isEmpty()) {
        const QString port = cfg.config().pumpPort;
        QMetaObject::invokeMethod(&pump, [port, this]() { pump.open(port); }, Qt::BlockingQueuedConnection);
    }
}

void ApplicationCore::stopCameras()
{
    running = false;
    cam0.stop();
    cam1.stop();
    QMetaObject::invokeMethod(&pump, [this]() { pump.close(); }, Qt::BlockingQueuedConnection);
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
    if (autoPumpController) {
        QMetaObject::invokeMethod(autoPumpController, [enabled, this]() { autoPumpController->setEnabled(enabled); }, Qt::QueuedConnection);
    }
    emit message(enabled ? tr("Auto pump enabled") : tr("Auto pump disabled"));
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
    updateFusion(id);
    LogManager::instance().logInfo(QString("Camera%1 width=%2px, %3mm").arg(id).arg(r.widthPixels).arg(r.widthMM));
}

QList<int> ApplicationCore::availableCameraIndices(int maxIndex) const
{
    QList<int> result;
    for (int i = 0; i <= maxIndex; ++i) {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) {
            result.append(i);
            cap.release();
        }
    }
    return result;
}

void ApplicationCore::reloadCamerasFromConfig()
{
    bool wasRunning = running;
    stopCameras();
    cam0.close();
    cam1.close();
    if (cfg.camera(0).enabled && cfg.camera(0).index >= 0) {
        cam0.open(cfg.camera(0).index);
        cam0.setConfig(cfg.camera(0));
    }
    if (cfg.camera(1).enabled && cfg.camera(1).index >= 0) {
        cam1.open(cfg.camera(1).index);
        cam1.setConfig(cfg.camera(1));
    }
    if (wasRunning) {
        startCameras();
    }
}

void ApplicationCore::reloadPumpConfig()
{
    applyPumpSettings();
    if (running) {
        const QString port = cfg.config().pumpPort;
        QMetaObject::invokeMethod(&pump, [port, this]() {
            pump.close();
            if (!port.isEmpty()) {
                pump.open(port);
            }
        }, Qt::BlockingQueuedConnection);
    }
    if (autoPumpController) {
        QMetaObject::invokeMethod(autoPumpController, [this]() { autoPumpController->updateSettings(cfg.config()); }, Qt::QueuedConnection);
    }
}

bool ApplicationCore::testPumpPulse(const QString &portName, int pulseMs)
{
    const int duration = qBound(50, pulseMs, 20000);
    if (portName.isEmpty()) {
        return false;
    }

    Cp2102PumpController tester;
    tester.setSafetyLimits(cfg.config().safetyMaxEvents, cfg.config().safetyWindowMs);
    if (!tester.open(portName)) {
        return false;
    }

    tester.forceHigh();
    tester.pulseLow(duration);
    tester.forceHigh();
    tester.close();
    LogManager::instance().logInfo(tr("Pump test pulse sent on %1 for %2 ms").arg(portName).arg(duration));
    return true;
}

bool ApplicationCore::applyCameraSelection(int id, int index, bool enabled)
{
    CameraConfig camCfg = cfg.camera(id);
    camCfg.index = index;
    camCfg.enabled = enabled;
    cfg.setCameraConfig(id, camCfg);

    bool ok = true;
    if (enabled && index >= 0) {
        cv::VideoCapture test(index);
        ok = test.isOpened();
    }
    reloadCamerasFromConfig();
    return ok;
}

bool ApplicationCore::calibrateAllCameras(double realWidthMm, QString &errorMessage)
{
    QList<int> calibrated;
    for (int i = 0; i < 2; ++i) {
        if (lastResult[i].valid && lastResult[i].widthPixels > 0.0) {
            CameraConfig cfgCam = cfg.camera(i);
            cfgCam.mmPerPixel = realWidthMm / lastResult[i].widthPixels;
            cfg.setCameraConfig(i, cfgCam);
            calibrated.append(i);
        }
    }
    if (calibrated.isEmpty()) {
        errorMessage = tr("摄像头未就绪，无法校准宽度");
        return false;
    }
    reloadCamerasFromConfig();
    return true;
}

void ApplicationCore::processPumpLogic(int id, const WidthResult &result, const CameraConfig &cfgCam)
{
    Q_UNUSED(id)
    Q_UNUSED(result)
    Q_UNUSED(cfgCam)
}

void ApplicationCore::updateFusion(int latestId)
{
    Q_UNUSED(latestId)
    QList<double> values;
    if (lastResult[0].valid) {
        values.append(lastResult[0].widthMM);
    }
    if (lastResult[1].valid) {
        values.append(lastResult[1].widthMM);
    }
    fusedValid = !values.isEmpty();
    if (fusedValid) {
        fusedWidth = calculateFusion(values);
    } else {
        fusedWidth = 0.0;
    }
    emit fusedWidthUpdated(fusedValid ? fusedWidth : 0.0,
                          lastResult[0].valid ? lastResult[0].widthMM : 0.0,
                          lastResult[1].valid ? lastResult[1].widthMM : 0.0);

    if (autoPumpController && fusedValid) {
        const int camId = chooseFusionCameraId();
        QMetaObject::invokeMethod(autoPumpController,
                                  [this, camId]() { autoPumpController->handleWidthSample(camId, fusedWidth, true); },
                                  Qt::QueuedConnection);
    }
}

double ApplicationCore::calculateFusion(const QList<double> &values) const
{
    if (values.isEmpty()) return 0.0;
    const QString strategy = cfg.config().fusionStrategy.toLower();
    if (strategy == QLatin1String("min")) {
        return *std::min_element(values.begin(), values.end());
    }
    if (strategy == QLatin1String("max")) {
        return *std::max_element(values.begin(), values.end());
    }
    double sum = 0.0;
    for (double v : values) sum += v;
    return sum / values.size();
}

int ApplicationCore::chooseFusionCameraId() const
{
    if (lastResult[0].valid && lastResult[1].valid) {
        return lastResult[0].widthMM <= lastResult[1].widthMM ? 0 : 1;
    }
    if (lastResult[0].valid) return 0;
    if (lastResult[1].valid) return 1;
    return 0;
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

void ApplicationCore::applyPumpSettings()
{
    pump.setSafetyLimits(cfg.config().safetyMaxEvents, cfg.config().safetyWindowMs);
}

