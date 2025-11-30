#include "applicationcore.h"
#include <QDateTime>
#include <QMetaType>
#include <QtGlobal>
#include <algorithm>
#include <numeric>
#include <limits>
#include <cmath>
#include <opencv2/opencv.hpp>

class WidthProcessingWorker : public QObject {
    Q_OBJECT
public:
    explicit WidthProcessingWorker(QObject *parent = nullptr)
        : QObject(parent)
        , estimator(new MultiLineCannyWidthEstimator())
    {}

    ~WidthProcessingWorker() override { delete estimator; }

public slots:
    void processFrame(int id, const cv::Mat &frame, const CameraConfig &cfg)
    {
        if (!estimator || frame.empty()) return;
        WidthResult r = estimator->estimate(frame, cfg);
        if (r.valid) {
            r.widthMM = r.widthPixels * cfg.mmPerPixel;
        }
        emit widthReady(id, r);
    }

signals:
    void widthReady(int id, const WidthResult &result);

private:
    IWidthEstimator *estimator {nullptr};
};

ApplicationCore::ApplicationCore(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<CameraConfig>("CameraConfig");
    qRegisterMetaType<WidthResult>("WidthResult");
    qRegisterMetaType<cv::Mat>("cv::Mat");

    widthWorker = new WidthProcessingWorker();
    widthWorker->moveToThread(&widthThread);
    connect(&widthThread, &QThread::finished, widthWorker, &QObject::deleteLater);
    connect(this, &ApplicationCore::processFrameRequest, widthWorker, &WidthProcessingWorker::processFrame, Qt::QueuedConnection);
    connect(widthWorker, &WidthProcessingWorker::widthReady, this, &ApplicationCore::onWidthResult, Qt::QueuedConnection);
    widthThread.start();
    series0 = new QLineSeries();
    series1 = new QLineSeries();
    chartView = new QChartView();
    chartView->chart()->legend()->setVisible(true);
    series0->setName("Camera0");
    series1->setName("Camera1");
    chartView->chart()->addSeries(series0);
    chartView->chart()->addSeries(series1);

    axisX = new QValueAxis(chartView->chart());
    axisY = new QValueAxis(chartView->chart());
    axisX->setTitleText(tr("时间 (s)"));
    axisX->setRange(0, trendWindowSeconds);
    axisX->setTickCount(static_cast<int>(trendWindowSeconds / trendTickSeconds) + 1);
    axisX->setLabelFormat("%.0f");
    axisY->setTitleText(tr("宽度 (cm)"));
    axisY->setRange(0.0, 100.0);
    axisY->setTickCount(6);
    axisY->setLabelFormat("%.1f");

    chartView->chart()->addAxis(axisX, Qt::AlignBottom);
    chartView->chart()->addAxis(axisY, Qt::AlignLeft);
    series0->attachAxis(axisX);
    series0->attachAxis(axisY);
    series1->attachAxis(axisX);
    series1->attachAxis(axisY);

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
    delete chartView;
    autoPumpThread.quit();
    autoPumpThread.wait();
    widthThread.quit();
    widthThread.wait();
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
        QTimer::singleShot(350, this, [this]() {
            cam1.start();
        });
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
    dispatchFrame(0, frame);
}

void ApplicationCore::onFrame1(const cv::Mat &frame)
{
    dispatchFrame(1, frame);
}

void ApplicationCore::dispatchFrame(int id, const cv::Mat &frame)
{
    if (!running || !widthWorker) return;
    emit processFrameRequest(id, frame, cfg.camera(id));
}

void ApplicationCore::onWidthResult(int id, const WidthResult &result)
{
    if (!result.valid) return;
    lastResult[id] = result;
    calib.addCalibrationSample(id, result.widthMM, result.widthPixels); // keep smoothing
    appendTrend(id, result.widthMM);
    emit widthUpdated(id, result);
    updateFusion(id);
    LogManager::instance().logInfo(QString("Camera%1 width=%2px, %3mm").arg(id).arg(result.widthPixels).arg(result.widthMM));
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

bool ApplicationCore::absoluteCalibrateAllCameras(double realWidthMm, QString &errorMessage)
{
    if (realWidthMm <= 0) {
        errorMessage = tr("请输入有效的真实宽度");
        return false;
    }

    double newMmPerPixel[2] {-1.0, -1.0};
    bool anyActive = false;
    const bool dualMode = cfg.config().dualCameraMode;

    for (int i = 0; i < 2; ++i) {
        const CameraConfig camCfg = cfg.camera(i);
        const bool active = camCfg.enabled && (i == 0 || dualMode);
        if (!active) {
            continue;
        }
        anyActive = true;
        if (!cameraReady[i]) {
            errorMessage = tr("摄像头%1未就绪或断流，无法校准").arg(i);
            return false;
        }
        const WidthResult &res = lastResult[i];
        if (!res.valid) {
            errorMessage = tr("摄像头%1未检测到有效宽度，请检查画面").arg(i);
            return false;
        }
        if (res.widthPixels <= 1.0) {
            errorMessage = tr("摄像头%1检测的像素宽度过小，无法校准").arg(i);
            return false;
        }
        const double mmPerPixel = realWidthMm / res.widthPixels;
        if (!std::isfinite(mmPerPixel) || mmPerPixel <= 0.0 || mmPerPixel > 10.0) {
            errorMessage = tr("摄像头%1换算得到的比例无效").arg(i);
            return false;
        }
        newMmPerPixel[i] = mmPerPixel;
    }

    if (!anyActive) {
        errorMessage = tr("没有可用的摄像头可供校准");
        return false;
    }

    for (int i = 0; i < 2; ++i) {
        if (newMmPerPixel[i] > 0) {
            calib.setCameraMmPerPixel(i, newMmPerPixel[i]);
            if (lastResult[i].valid) {
                lastResult[i].widthMM = lastResult[i].widthPixels * newMmPerPixel[i];
                emit widthUpdated(i, lastResult[i]);
            }
        }
    }

    updateFusion(-1);
    return true;
}

bool ApplicationCore::calibrateAllCameras(double realWidthMm, QString &errorMessage)
{
    if (realWidthMm <= 0) {
        errorMessage = tr("请输入有效的真实宽度");
        return false;
    }

    QList<double> measuredSamples;
    if (fusedValid && fusedWidth > 0) {
        measuredSamples.append(fusedWidth);
    }
    for (int i = 0; i < 2; ++i) {
        if (lastResult[i].valid && lastResult[i].widthMM > 0) {
            measuredSamples.append(lastResult[i].widthMM);
        }
    }

    if (measuredSamples.isEmpty()) {
        errorMessage = tr("摄像头未就绪，无法校准宽度");
        return false;
    }

    const double measuredMm = std::accumulate(measuredSamples.begin(), measuredSamples.end(), 0.0) / measuredSamples.size();
    if (measuredMm <= 0) {
        errorMessage = tr("当前宽度无效，无法校准");
        return false;
    }

    const double ratio = realWidthMm / measuredMm;
    double baseMmPerPixel = cfg.camera(0).mmPerPixel;
    if (baseMmPerPixel <= 0) {
        baseMmPerPixel = cfg.camera(1).mmPerPixel;
    }
    if (baseMmPerPixel <= 0) {
        baseMmPerPixel = 0.5;
    }
    double newMmPerPixel = baseMmPerPixel * ratio;
    newMmPerPixel = qBound(0.0001, newMmPerPixel, 10.0);

    cfg.updateMmPerPixel(0, newMmPerPixel);
    cfg.updateMmPerPixel(1, newMmPerPixel);
    calib.setGlobalMmPerPixel(newMmPerPixel);

    for (int i = 0; i < 2; ++i) {
        if (lastResult[i].valid) {
            lastResult[i].widthMM *= ratio;
        }
    }
    if (fusedValid) {
        fusedWidth *= ratio;
    }

    emit fusedWidthUpdated(fusedValid ? fusedWidth : 0.0,
                          lastResult[0].valid ? lastResult[0].widthMM : 0.0,
                          lastResult[1].valid ? lastResult[1].widthMM : 0.0);
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
    const double xSeconds = x / 1000.0;
    const double widthCm = widthMM / 10.0;
    QLineSeries *series = id == 0 ? series0 : series1;
    series->append(xSeconds, widthCm);
    while (series->count() > 0 && series->points().first().x() < xSeconds - trendWindowSeconds) {
        series->removePoints(0, 1);
    }

    if (axisX) {
        axisX->setRange(xSeconds - trendWindowSeconds, xSeconds);
    }

    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    auto updateRange = [&](QLineSeries *s) {
        for (const QPointF &p : s->points()) {
            if (p.x() < xSeconds - trendWindowSeconds) continue;
            minY = std::min(minY, p.y());
            maxY = std::max(maxY, p.y());
        }
    };
    updateRange(series0);
    updateRange(series1);

    if (minY == std::numeric_limits<double>::max() || maxY == std::numeric_limits<double>::lowest()) {
        minY = 0.0;
        maxY = 10.0;
    }
    if (qFuzzyCompare(minY, maxY)) {
        minY = std::max(0.0, minY - 5.0);
        maxY += 5.0;
    }
    const double padding = std::max(1.0, (maxY - minY) * 0.15);
    minY = std::max(0.0, minY - padding);
    maxY += padding;
    if (axisY) {
        axisY->setRange(minY, maxY);
        axisY->setTickCount(6);
        if (axisX) {
            const int tickCount = static_cast<int>((trendWindowSeconds / trendTickSeconds) + 1);
            axisX->setTickCount(std::max(3, tickCount));
        }
    }
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

#include "applicationcore.moc"

