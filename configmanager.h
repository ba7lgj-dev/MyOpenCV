#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QMutex>

struct CameraConfig {
    int index {0};
    double lineRatio {0.5};
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

struct AppConfig {
    QString pumpPort;
    int safetyMaxEvents {20};
    int safetyWindowMs {5 * 60 * 1000};
    CameraConfig cameras[2];
};

class ConfigManager : public QObject {
    Q_OBJECT
public:
    explicit ConfigManager(QObject *parent = nullptr);
    bool load(const QString &path);
    bool save(const QString &path) const;
    const AppConfig &config() const { return appConfig; }
    void updateMmPerPixel(int cameraId, double value);
    CameraConfig camera(int idx) const;

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
