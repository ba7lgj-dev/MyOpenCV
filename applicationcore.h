#ifndef APPLICATIONCORE_H
#define APPLICATIONCORE_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include "camera.h"
#include "widthestimator.h"
#include "configmanager.h"
#include "calibrationmanager.h"
#include "pumpcontroller.h"
#include "logmanager.h"

QT_CHARTS_USE_NAMESPACE

class ApplicationCore : public QObject {
    Q_OBJECT
public:
    explicit ApplicationCore(QObject *parent = nullptr);
    ~ApplicationCore() override;

    void initialize();
    ConfigManager *config();
    CalibrationManager *calibration();
    QChartView *trendChart();

signals:
    void cameraFrame(int id, const QImage &img);
    void widthUpdated(int id, const WidthResult &result);
    void message(const QString &msg);
    void safetyModeEnabled();
    void availableCamerasChanged(const QVector<int> &indexes);
    void cameraConfigApplied(int id, const CameraConfig &cfg);

public slots:
    void startCameras();
    void stopCameras();
    void calibrateWidth(int cameraId, double realMM);
    void toggleAutoExposure(int cameraId);
    void setAutoPumpEnabled(bool enabled);
    void setCameraIndex(int id, int cameraIndex);
    void setCameraName(int id, const QString &name);
    void setLineRatio(int id, double ratio);
    void setLineColor(int id, const QColor &color);
    void setRotation(int id, int rotation);
    void swapCameras();
    void setDualCameraMode(bool dual);
    void rescanCameras();

private slots:
    void onFrame0(const cv::Mat &frame);
    void onFrame1(const cv::Mat &frame);
    void handleWidth(int id, const cv::Mat &frame);
    void onPumpSafety(const QString &msg);

private:
    void processPumpLogic(int id, const WidthResult &result, const CameraConfig &cfg);
    void appendTrend(int id, double widthMM);
    void applyCameraConfig(int id);
    QVector<int> scanCameras(int maxIndex = 10) const;

    ConfigManager cfg;
    CalibrationManager calib{&cfg};
    IWidthEstimator *estimator {nullptr};
    UsbCamera cam0;
    UsbCamera cam1;
    Cp2102PumpController pump;
    bool autoPump {false};
    bool camerasRunning {false};
    QString configPath {"config.json"};
    qint64 lastPulseMs {0};
    QChartView *chartView {nullptr};
    QLineSeries *series0 {nullptr};
    QLineSeries *series1 {nullptr};
    WidthResult lastResult[2];
    QVector<int> availableCameras;
};

#endif // APPLICATIONCORE_H
