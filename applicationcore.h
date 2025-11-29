#ifndef APPLICATIONCORE_H
#define APPLICATIONCORE_H

#include <QObject>
#include <QTimer>
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
    PushManager *pushNotifier() { return &push; }
    QList<int> availableCameraIndices(int maxIndex = 8) const;
    void reloadCamerasFromConfig();
    void reloadPumpConfig();
    void reloadPushConfig();
    QString configPath() const { return defaultConfigPath; }
    bool testPumpPulse(const QString &portName, int pulseMs);
    bool sendTestPush(const QString &message);

signals:
    void cameraFrame(int id, const QImage &img);
    void widthUpdated(int id, const WidthResult &result);
    void message(const QString &msg);
    void safetyModeEnabled();

public slots:
    void startCameras();
    void stopCameras();
    void calibrateWidth(int cameraId, double realMM);
    void toggleAutoExposure(int cameraId);
    void setAutoPumpEnabled(bool enabled);

private slots:
    void onFrame0(const cv::Mat &frame);
    void onFrame1(const cv::Mat &frame);
    void handleWidth(int id, const cv::Mat &frame);
    void onPumpSafety(const QString &msg);

private:
    void processPumpLogic(int id, const WidthResult &result, const CameraConfig &cfg);
    void appendTrend(int id, double widthMM);
    void applyPumpSettings();
    void notifyStartup();
    void notifyShutdown();
    void notifyPumpTrigger(int id, double widthMM);

    ConfigManager cfg;
    CalibrationManager calib{&cfg};
    IWidthEstimator *estimator {nullptr};
    UsbCamera cam0;
    UsbCamera cam1;
    Cp2102PumpController pump;
    bool autoPump {false};
    qint64 lastPulseMs {0};
    QChartView *chartView {nullptr};
    QLineSeries *series0 {nullptr};
    QLineSeries *series1 {nullptr};
    QValueAxis *axisX {nullptr};
    QValueAxis *axisY {nullptr};
    qint64 startTimeMs {0};
    WidthResult lastResult[2];
    int pumpTriggerCount[2] {0, 0};
    QString defaultConfigPath {"config.json"};
    bool running {false};
    int pumpTriggerRequirement {3};
    PushManager push;
};

#endif // APPLICATIONCORE_H
