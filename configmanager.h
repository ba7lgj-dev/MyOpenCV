#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QMutex>
#include <QColor>
#include <QSettings>

struct CameraConfig {
    int index {0};
    QString name {QStringLiteral("摄像头")};
    double lineRatio {0.5};
    double mmPerPixel {0.5};
    bool flipHorizontal {false};
    bool flipVertical {false};
    int rotation {0};
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
    QColor detectLineColor {Qt::red};
};

struct PushConfig {
    QString url;
    QString token;
    QString templateContent {QStringLiteral("系统通知: ${message}")};
    bool enabled {false};
    int maxRetries {3};
    int failureThreshold {3};
};

struct AppConfig {
    QString pumpPort;
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
    void setConfig(const AppConfig &cfg);
    void updateMmPerPixel(int cameraId, double value);
    CameraConfig camera(int idx) const;
    void resetDefaults();
    QString lastLoadedPath() const { return lastPath; }
    void updateCameraConfig(int idx, const CameraConfig &cfg);
    void updatePushConfig(const PushConfig &push);

signals:
    void configReloaded();

private:
    void fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;
    void fromSettings(QSettings &settings);
    void toSettings(QSettings &settings) const;
    bool validateCameraConfig(CameraConfig &cfg) const;

    AppConfig appConfig;
    QString lastPath;
    mutable QMutex mutex;
};

#endif // CONFIGMANAGER_H
