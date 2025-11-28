#include "configmanager.h"
#include "logmanager.h"
#include <QMutexLocker>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    appConfig.cameras[0].index = 0;
    appConfig.cameras[1].index = 1;
}

void ConfigManager::resetDefaults()
{
    appConfig = AppConfig();
    appConfig.cameras[0].index = 0;
    appConfig.cameras[0].name = QStringLiteral("左摄像头");
    appConfig.cameras[1].index = 1;
    appConfig.cameras[1].name = QStringLiteral("右摄像头");
    appConfig.push.templateContent = QStringLiteral("系统通知: ${message}");
    if (!lastPath.isEmpty()) {
        save(lastPath);
    }
    emit configReloaded();
}

bool ConfigManager::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return false;
    fromJson(doc.object());
    lastPath = path;
    emit configReloaded();
    return true;
}

bool ConfigManager::save(const QString &path) const
{
    QMutexLocker locker(&mutex);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

void ConfigManager::setConfig(const AppConfig &cfg)
{
    QString pathCopy;
    {
        QMutexLocker locker(&mutex);
        appConfig = cfg;
        pathCopy = lastPath;
    }
    if (!pathCopy.isEmpty()) {
        save(pathCopy);
    }
    emit configReloaded();
}

void ConfigManager::updateMmPerPixel(int cameraId, double value)
{
    QMutexLocker locker(&mutex);
    if (cameraId < 0 || cameraId > 1) return;
    appConfig.cameras[cameraId].mmPerPixel = value;
    if (!lastPath.isEmpty()) {
        save(lastPath);
    }
}

CameraConfig ConfigManager::camera(int idx) const
{
    QMutexLocker locker(&mutex);
    if (idx < 0 || idx > 1) return CameraConfig();
    return appConfig.cameras[idx];
}

void ConfigManager::updateCameraConfig(int idx, const CameraConfig &cfg)
{
    QMutexLocker locker(&mutex);
    if (idx < 0 || idx > 1) return;
    appConfig.cameras[idx] = cfg;
    if (!lastPath.isEmpty()) {
        save(lastPath);
    }
    emit configReloaded();
}

void ConfigManager::updatePushConfig(const PushConfig &push)
{
    QMutexLocker locker(&mutex);
    appConfig.push = push;
    if (!lastPath.isEmpty()) {
        save(lastPath);
    }
    emit configReloaded();
}

void ConfigManager::fromJson(const QJsonObject &obj)
{
    if (obj.contains("pumpPort")) {
        appConfig.pumpPort = obj.value("pumpPort").toString();
    }
    appConfig.autoPumpEnabled = obj.value("autoPumpEnabled").toBool(appConfig.autoPumpEnabled);
    appConfig.safetyMaxEvents = obj.value("safetyMaxEvents").toInt(appConfig.safetyMaxEvents);
    appConfig.safetyWindowMs = obj.value("safetyWindowMs").toInt(appConfig.safetyWindowMs);
    appConfig.dualCameraMode = obj.value("dualCameraMode").toBool(appConfig.dualCameraMode);

    if (obj.contains("cameras") && obj.value("cameras").isArray()) {
        QJsonArray arr = obj.value("cameras").toArray();
        for (int i = 0; i < arr.size() && i < 2; ++i) {
            QJsonObject c = arr.at(i).toObject();
            CameraConfig cfg;
            cfg.index = c.value("index").toInt(i);
            cfg.name = c.value("name").toString(i == 0 ? tr("左摄像头") : tr("右摄像头"));
            cfg.lineRatio = c.value("lineRatio").toDouble(0.5);
            cfg.mmPerPixel = c.value("mmPerPixel").toDouble(0.5);
            cfg.flipHorizontal = c.value("flipHorizontal").toBool(false);
            cfg.flipVertical = c.value("flipVertical").toBool(false);
            cfg.rotation = c.value("rotation").toInt(0);
            cfg.cannyLow = c.value("cannyLow").toDouble(50);
            cfg.cannyHigh = c.value("cannyHigh").toDouble(150);
            cfg.pulseMs = c.value("pulseMs").toInt(600);
            cfg.thresholdMM = c.value("thresholdMM").toDouble(1000);
            cfg.cooldownMs = c.value("cooldownMs").toInt(2000);
            cfg.exposure = c.value("exposure").toDouble(-1);
            cfg.brightness = c.value("brightness").toDouble(-1);
            cfg.targetGrayMin = c.value("targetGrayMin").toDouble(80);
            cfg.targetGrayMax = c.value("targetGrayMax").toDouble(180);
            cfg.autoExposureStep = c.value("autoExposureStep").toDouble(1);
            cfg.autoExposureMin = c.value("autoExposureMin").toDouble(-10);
            cfg.autoExposureMax = c.value("autoExposureMax").toDouble(10);
            QColor col(c.value("detectLineColor").toString());
            if (col.isValid()) {
                cfg.detectLineColor = col;
            }
            if (!validateCameraConfig(cfg)) {
                LogManager::instance().logWarn(tr("Invalid config for camera %1, fallback to defaults").arg(i));
            }
            appConfig.cameras[i] = cfg;
        }
    }

    if (obj.contains("push")) {
        QJsonObject p = obj.value("push").toObject();
        appConfig.push.url = p.value("url").toString();
        appConfig.push.token = p.value("token").toString();
        appConfig.push.templateContent = p.value("templateContent").toString(appConfig.push.templateContent);
        appConfig.push.enabled = p.value("enabled").toBool(false);
        appConfig.push.maxRetries = p.value("maxRetries").toInt(appConfig.push.maxRetries);
        appConfig.push.failureThreshold = p.value("failureThreshold").toInt(appConfig.push.failureThreshold);
    }
}

QJsonObject ConfigManager::toJson() const
{
    QJsonObject obj;
    obj.insert("pumpPort", appConfig.pumpPort);
    obj.insert("autoPumpEnabled", appConfig.autoPumpEnabled);
    obj.insert("safetyMaxEvents", appConfig.safetyMaxEvents);
    obj.insert("safetyWindowMs", appConfig.safetyWindowMs);
    obj.insert("dualCameraMode", appConfig.dualCameraMode);

    QJsonArray arr;
    for (int i = 0; i < 2; ++i) {
        const auto &cfg = appConfig.cameras[i];
        QJsonObject c;
        c.insert("index", cfg.index);
        c.insert("name", cfg.name);
        c.insert("lineRatio", cfg.lineRatio);
        c.insert("mmPerPixel", cfg.mmPerPixel);
        c.insert("flipHorizontal", cfg.flipHorizontal);
        c.insert("flipVertical", cfg.flipVertical);
        c.insert("rotation", cfg.rotation);
        c.insert("cannyLow", cfg.cannyLow);
        c.insert("cannyHigh", cfg.cannyHigh);
        c.insert("pulseMs", cfg.pulseMs);
        c.insert("thresholdMM", cfg.thresholdMM);
        c.insert("cooldownMs", cfg.cooldownMs);
        c.insert("exposure", cfg.exposure);
        c.insert("brightness", cfg.brightness);
        c.insert("targetGrayMin", cfg.targetGrayMin);
        c.insert("targetGrayMax", cfg.targetGrayMax);
        c.insert("autoExposureStep", cfg.autoExposureStep);
        c.insert("autoExposureMin", cfg.autoExposureMin);
        c.insert("autoExposureMax", cfg.autoExposureMax);
        c.insert("detectLineColor", cfg.detectLineColor.name(QColor::HexRgb));
        arr.append(c);
    }
    obj.insert("cameras", arr);

    QJsonObject p;
    p.insert("url", appConfig.push.url);
    p.insert("token", appConfig.push.token);
    p.insert("templateContent", appConfig.push.templateContent);
    p.insert("enabled", appConfig.push.enabled);
    p.insert("maxRetries", appConfig.push.maxRetries);
    p.insert("failureThreshold", appConfig.push.failureThreshold);
    obj.insert("push", p);
    return obj;
}

bool ConfigManager::validateCameraConfig(CameraConfig &cfg) const
{
    bool ok = true;
    if (cfg.lineRatio < 0.0 || cfg.lineRatio > 1.0) {
        cfg.lineRatio = 0.5;
        ok = false;
    }
    if (cfg.pulseMs < 100 || cfg.pulseMs > 5000) {
        cfg.pulseMs = 600;
        ok = false;
    }
    if (cfg.thresholdMM < 10 || cfg.thresholdMM > 5000) {
        cfg.thresholdMM = 1000;
        ok = false;
    }
    if (cfg.mmPerPixel <= 0 || cfg.mmPerPixel > 10) {
        cfg.mmPerPixel = 0.5;
        ok = false;
    }
    return ok;
}

