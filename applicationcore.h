#ifndef APPLICATIONCORE_H
#define APPLICATIONCORE_H

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QList>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include "camera.h"
#include "widthestimator.h"
#include "configmanager.h"
#include "calibrationmanager.h"
#include "pumpcontroller.h"
#include "logmanager.h"
#include "pushmanager.h"
#include "autopumpcontroller.h"

QT_CHARTS_USE_NAMESPACE

class WidthProcessingWorker;

class ApplicationCore : public QObject {
    Q_OBJECT
public:
    explicit ApplicationCore(QObject *parent = nullptr);
    ~ApplicationCore() override;

    void initialize();
    ConfigManager *config();
    CalibrationManager *calibration();
    QChartView *trendChart();
    PushManager *pushManager();
    QList<int> availableCameraIndices(int maxIndex = 8) const;
    void reloadCamerasFromConfig();
    void reloadPumpConfig();
    QString configPath() const { return defaultConfigPath; }
    bool testPumpPulse(const QString &portName, int pulseMs);
    bool applyCameraSelection(int id, int index, bool enabled);
    bool calibrateAllCameras(double realWidthMm, QString &errorMessage);
    double fusedWidthMM() const { return fusedWidth; }
    WidthResult lastWidthResult(int id) const { return lastResult[id]; }

signals:
    void cameraFrame(int id, const QImage &img);
    void widthUpdated(int id, const WidthResult &result);
    void fusedWidthUpdated(double fusedMm, double cam0Mm, double cam1Mm);
    void message(const QString &msg);
    void safetyModeEnabled();
    void processFrameRequest(int id, const cv::Mat &frame, const CameraConfig &cfg);

public slots:
    void startCameras();
    void stopCameras();
    void calibrateWidth(int cameraId, double realMM);
    void toggleAutoExposure(int cameraId);
    void setAutoPumpEnabled(bool enabled);

private slots:
    void onFrame0(const cv::Mat &frame);
    void onFrame1(const cv::Mat &frame);
    void onWidthResult(int id, const WidthResult &result);
    void onPumpSafety(const QString &msg);
    void updateFusion(int latestId);
    double calculateFusion(const QList<double> &values) const;
    int chooseFusionCameraId() const;

private:
    void dispatchFrame(int id, const cv::Mat &frame);
    void processPumpLogic(int id, const WidthResult &result, const CameraConfig &cfg);
    void appendTrend(int id, double widthMM);
    void applyPumpSettings();

    ConfigManager cfg;
    CalibrationManager calib{&cfg};
    UsbCamera cam0;
    UsbCamera cam1;
    Cp2102PumpController pump;
    PushManager *push {nullptr};
    AutoPumpController *autoPumpController {nullptr};
    QThread autoPumpThread;
    QThread widthThread;
    class WidthProcessingWorker *widthWorker {nullptr};
    bool autoPump {false};
    QChartView *chartView {nullptr};
    QLineSeries *series0 {nullptr};
    QLineSeries *series1 {nullptr};
    QValueAxis *axisX {nullptr};
    QValueAxis *axisY {nullptr};
    WidthResult lastResult[2];
    bool cameraReady[2] {false, false};
    double fusedWidth {0.0};
    bool fusedValid {false};
    QString defaultConfigPath {"config.json"};
    bool running {false};
    int pumpTriggerRequirement {3};
    double trendWindowSeconds {120.0};
    double trendTickSeconds {10.0};
};

#endif // APPLICATIONCORE_H
