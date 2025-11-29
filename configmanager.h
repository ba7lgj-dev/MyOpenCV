#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QMutex>
#include <QString>
#include <QColor>

struct CameraConfig {
    int index {0};
    bool enabled {true};
    QString name;
    double lineRatio {0.5};
    int lineHeightPx {0};
    int widthRegionHeight {0};
    QColor lineColor {Qt::red};
    int rotation {0};
    double mmPerPixel {0.5};
    bool flipHorizontal {false};
    bool flipVertical {false};
    double cannyLow {50};
    double cannyHigh {150};
    int pulseMs {600};
    double thresholdMM {1000};
    int cooldownMs {2000};
    double exposure {-1};
    double brightness {-1};
    double targetGrayMin {80};
    double targetGrayMax {180};
    double autoExposureStep {1};
    double autoExposureMin {-10};
    double autoExposureMax {10};
};

struct PushConfig {
    QString url;
    QString token;
    QString templateText;
    bool enabled {false};
    int maxFailures {3};
    int throttleWindowMs {10000};
};

struct AppConfig {
    QString fusionStrategy {"average"};
    QString pumpPort;
    int pumpDurationMs {600};
    double autoStartThresholdMM {1000};
    double autoStopThresholdMM {1200};
    int autoPrecheckMs {5000};
    int autoMonitorMs {3000};
    int autoCooldownMs {2000};
    double autoMinInflationMM {5.0};
    bool autoPumpEnabled {false};
    int safetyMaxEvents {20};
    int safetyWindowMs {5 * 60 * 1000};
    bool dualCameraMode {true};
    CameraConfig cameras[2];
    PushConfig push;
};

class ConfigManager : public QObject {
    Q_OBJECT
public:
    explicit ConfigManager(QObject *parent = nullptr);
    bool load(const QString &path);
    bool save(const QString &path) const;
    const AppConfig &config() const { return appConfig; }
    QString configPath() const { return lastPath; }
    void updateMmPerPixel(int cameraId, double value);
    void setAutoPumpEnabled(bool enabled);
    void setPumpPort(const QString &port);
    void setPumpDurationMs(int ms);
    void setAutoStartThresholdMM(double mm);
    void setAutoStopThresholdMM(double mm);
    void setAutoPrecheckMs(int ms);
    void setAutoMonitorMs(int ms);
    void setAutoCooldownMs(int ms);
    void setMinInflationMM(double mm);
    void setDualCameraMode(bool enabled);
    void setFusionStrategy(const QString &strategy);
    void restoreDefaults();
    CameraConfig camera(int idx) const;
    void setCameraConfig(int idx, const CameraConfig &cfg);
    PushConfig pushConfig() const;
    void setPushConfig(const PushConfig &cfg);

    static QColor colorFromJson(const QJsonValue &v, const QColor &fallback = Qt::red);
    static QJsonValue colorToJson(const QColor &c);

signals:
    void configReloaded();

private:
    void fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;
    bool validateCameraConfig(CameraConfig &cfg) const;

    AppConfig appConfig;
    mutable QString lastPath;
    mutable QMutex mutex;
};

#endif // CONFIGMANAGER_H
