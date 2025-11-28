#include "applicationcore.h"
#include <QDateTime>
#include <QPainter>
#include <algorithm>

ApplicationCore::ApplicationCore(QObject *parent)
    : QObject(parent)
{
    estimator = new MultiLineCannyWidthEstimator();
    series0 = new QLineSeries();
    series1 = new QLineSeries();
    chartView = new QChartView();
    chartView->chart()->legend()->setVisible(true);
    series0->setName(cfg.camera(0).name.isEmpty() ? "Camera0" : cfg.camera(0).name);
    series1->setName(cfg.camera(1).name.isEmpty() ? "Camera1" : cfg.camera(1).name);
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
    autoPump = cfg.config().autoPumpEnabled;

    connect(&cam0, &UsbCamera::rawFrameReady, this, &ApplicationCore::onFrame0);
    connect(&cam1, &UsbCamera::rawFrameReady, this, &ApplicationCore::onFrame1);
    connect(&cam0, &UsbCamera::frameReady, [this](const QImage &img){ emit cameraFrame(0, decorateFrame(0, img, cfg.camera(0)));});
    connect(&cam1, &UsbCamera::frameReady, [this](const QImage &img){ emit cameraFrame(1, decorateFrame(1, img, cfg.camera(1)));});
    connect(&cam0, &UsbCamera::cameraError, this, [this](const QString &msg){ emit message(msg); emit cameraStatus(0, msg, true); LogManager::instance().logWarn(msg);});
    connect(&cam1, &UsbCamera::cameraError, this, [this](const QString &msg){ emit message(msg); emit cameraStatus(1, msg, true); LogManager::instance().logWarn(msg);});

    rescanCameras();
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
    stopCameras();
    applyCameraSettings();
    cam0.start();
    camerasRunning = true;
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
    cam0.close();
    cam1.close();
    pump.close();
    camerasRunning = false;
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

void ApplicationCore::rescanCameras()
{
    QList<int> indexes = detectCameras();
    emit availableCamerasChanged(indexes);
}

void ApplicationCore::setCameraIndex(int cameraId, int index)
{
    bool wasRunning = camerasRunning;
    if (wasRunning) stopCameras();
    cfg.updateCamera(cameraId, [index](CameraConfig &c){ c.index = index; });
    if (wasRunning) startCameras();
}

void ApplicationCore::setCameraName(int cameraId, const QString &name)
{
    cfg.updateCamera(cameraId, [name](CameraConfig &c){ c.name = name; });
    if (cameraId == 0) series0->setName(name);
    else series1->setName(name);
}

void ApplicationCore::setCameraRotation(int cameraId, int rotation)
{
    cfg.updateCamera(cameraId, [rotation](CameraConfig &c){ c.rotation = rotation; });
    if (camerasRunning) {
        if (cameraId == 0) cam0.setConfig(cfg.camera(0)); else cam1.setConfig(cfg.camera(1));
    }
}

void ApplicationCore::setLineRatio(int cameraId, double ratio)
{
    cfg.updateCamera(cameraId, [ratio](CameraConfig &c){ c.lineRatio = ratio; });
}

void ApplicationCore::setLineHeight(int cameraId, int px)
{
    cfg.updateCamera(cameraId, [px](CameraConfig &c){ c.lineHeightPx = px; });
}

void ApplicationCore::setRegionHeight(int cameraId, int px)
{
    cfg.updateCamera(cameraId, [px](CameraConfig &c){ c.widthRegionHeight = px; });
}

void ApplicationCore::setLineColor(int cameraId, const QColor &color)
{
    cfg.updateCamera(cameraId, [color](CameraConfig &c){ c.lineColor = color; });
}

void ApplicationCore::swapCameras()
{
    CameraConfig c0 = cfg.camera(0);
    CameraConfig c1 = cfg.camera(1);
    bool wasRunning = camerasRunning;
    if (wasRunning) stopCameras();
    cfg.updateCamera(0, [c1](CameraConfig &c){ c = c1; });
    cfg.updateCamera(1, [c0](CameraConfig &c){ c = c0; });
    if (wasRunning) startCameras();
}

void ApplicationCore::setDualCameraMode(bool enabled)
{
    cfg.setDualCameraMode(enabled);
    if (camerasRunning) {
        startCameras();
    }
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

void ApplicationCore::applyCameraSettings()
{
    CameraConfig c0 = cfg.camera(0);
    CameraConfig c1 = cfg.camera(1);
    cam0.open(c0.index);
    cam0.setConfig(c0);
    if (cfg.config().dualCameraMode) {
        cam1.open(c1.index);
        cam1.setConfig(c1);
    }
}

QList<int> ApplicationCore::detectCameras(int maxIndex) const
{
    QList<int> available;
    for (int i = 0; i <= maxIndex; ++i) {
        cv::VideoCapture cap;
        if (cap.open(i)) {
            available.append(i);
            cap.release();
        }
    }
    return available;
}

QImage ApplicationCore::decorateFrame(int id, const QImage &img, const CameraConfig &cfgCam)
{
    QImage copy = img.copy();
    QPainter painter(&copy);
    painter.setRenderHint(QPainter::Antialiasing);
    int h = copy.height();
    int w = copy.width();
    int targetRow = cfgCam.lineHeightPx > 0 ? std::min(cfgCam.lineHeightPx, h - 1)
                                           : static_cast<int>(std::clamp(cfgCam.lineRatio, 0.0, 1.0) * h);
    int regionHeight = cfgCam.widthRegionHeight > 0 ? std::min(cfgCam.widthRegionHeight, h) : h / 2;
    int startRow = std::max(0, std::min(targetRow - regionHeight / 2, h - regionHeight));
    painter.setPen(QPen(cfgCam.lineColor.isValid() ? cfgCam.lineColor : Qt::red, 2));
    painter.drawLine(0, targetRow, w, targetRow);
    painter.drawRect(0, startRow, w - 1, regionHeight);

    if (lastResult[id].valid) {
        painter.setPen(QPen(Qt::yellow, 2));
        painter.drawLine(lastResult[id].leftX, startRow, lastResult[id].leftX, startRow + regionHeight);
        painter.drawLine(lastResult[id].rightX, startRow, lastResult[id].rightX, startRow + regionHeight);
        painter.drawText(lastResult[id].leftX + 2, startRow + 15, tr("左边界"));
        painter.drawText(std::max(0, lastResult[id].rightX - 60), startRow + 15, tr("右边界"));
    }

    painter.end();
    return copy;
}

