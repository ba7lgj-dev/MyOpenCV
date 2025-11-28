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
};

struct AppConfig {
    QString pumpPort;
    int pumpDurationMs {600};
    double pumpThresholdMM {1000};
    int pumpCooldownMs {2000};
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
    void setDualCameraMode(bool enabled);
    void restoreDefaults();
    CameraConfig camera(int idx) const;
    void setCameraConfig(int idx, const CameraConfig &cfg);
    PushConfig pushConfig() const;

    static QColor colorFromJson(const QJsonValue &v, const QColor &fallback = Qt::red);
    static QJsonValue colorToJson(const QColor &c);

signals:
    void configReloaded();

private:
    void fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;
    bool validateCameraConfig(CameraConfig &cfg) const;

    AppConfig appConfig;
    QString lastPath;
    mutable QMutex mutex;
};

#endif // CONFIGMANAGER_H
