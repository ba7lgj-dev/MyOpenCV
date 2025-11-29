#include "configmanager.h"
#include "logmanager.h"
#include <QMutexLocker>
#include <QtGlobal>
#include <algorithm>
#include <QFileInfo>
#include <QDir>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    restoreDefaults();
}

bool ConfigManager::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMutexLocker locker(&mutex);
        lastPath = path;
        return false;
    }
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return false;
    {
        QMutexLocker locker(&mutex);
        fromJson(doc.object());
        lastPath = path;
    }
    loadPushOverride();
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
    lastPath = path;
    savePushOverride();
    return true;
}

void ConfigManager::updateMmPerPixel(int cameraId, double value)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        if (cameraId < 0 || cameraId > 1) return;
        appConfig.cameras[cameraId].mmPerPixel = value;
        savedPath = lastPath;
    }
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

void ConfigManager::setAutoPumpEnabled(bool enabled)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        appConfig.autoPumpEnabled = enabled;
        savedPath = lastPath;
    }
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

void ConfigManager::setPumpPort(const QString &port)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        appConfig.pumpPort = port;
        savedPath = lastPath;
    }
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

void ConfigManager::setPumpDurationMs(int ms)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        appConfig.pumpDurationMs = qBound(50, ms, 20000);
        savedPath = lastPath;
    }
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

void ConfigManager::setPumpThresholdMM(double mm)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        appConfig.pumpThresholdMM = qBound(10.0, mm, 10000.0);
        savedPath = lastPath;
    }
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

void ConfigManager::setPumpCooldownMs(int ms)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        appConfig.pumpCooldownMs = qBound(0, ms, 600000);
        savedPath = lastPath;
    }
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

void ConfigManager::setDualCameraMode(bool enabled)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        appConfig.dualCameraMode = enabled;
        savedPath = lastPath;
    }
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

void ConfigManager::setPushEnabled(bool enabled)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        appConfig.push.enabled = enabled;
        savedPath = lastPath;
    }
    savePushOverride();
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

void ConfigManager::setPushUrl(const QString &url)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        appConfig.push.url = url;
        savedPath = lastPath;
    }
    savePushOverride();
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

void ConfigManager::setPushMaxFailures(int maxFailures)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        appConfig.push.maxFailures = std::max(1, maxFailures);
        savedPath = lastPath;
    }
    savePushOverride();
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

void ConfigManager::setPushTemplate(const QString &text)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        appConfig.push.templateText = text;
        savedPath = lastPath;
    }
    savePushOverride();
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

CameraConfig ConfigManager::camera(int idx) const
{
    QMutexLocker locker(&mutex);
    if (idx < 0 || idx > 1) return CameraConfig();
    return appConfig.cameras[idx];
}

void ConfigManager::setCameraConfig(int idx, const CameraConfig &cfg)
{
    QString savedPath;
    {
        QMutexLocker locker(&mutex);
        if (idx < 0 || idx > 1) return;
        appConfig.cameras[idx] = cfg;
        savedPath = lastPath;
    }
    if (!savedPath.isEmpty()) {
        save(savedPath);
    }
}

PushConfig ConfigManager::pushConfig() const
{
    QMutexLocker locker(&mutex);
    return appConfig.push;
}

void ConfigManager::restoreDefaults()
{
    QMutexLocker locker(&mutex);
    appConfig = AppConfig();
    appConfig.cameras[0].index = 0;
    appConfig.cameras[1].index = 1;
    appConfig.cameras[0].name = tr("Left Camera");
    appConfig.cameras[1].name = tr("Right Camera");
    appConfig.push.url = QStringLiteral("https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=b3b26998-1042-472e-af7d-2b0649233be");
    appConfig.push.enabled = true;
    appConfig.push.maxFailures = 3;
    appConfig.push.templateText = tr("系统通知：%1");
}

void ConfigManager::fromJson(const QJsonObject &obj)
{
    if (obj.contains("pumpPort")) {
        appConfig.pumpPort = obj.value("pumpPort").toString();
    }
    appConfig.pumpDurationMs = obj.value("pumpDurationMs").toInt(appConfig.pumpDurationMs);
    appConfig.pumpThresholdMM = obj.value("pumpThresholdMM").toDouble(appConfig.pumpThresholdMM);
    appConfig.pumpCooldownMs = obj.value("pumpCooldownMs").toInt(appConfig.pumpCooldownMs);
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
            cfg.name = c.value("name").toString();
            cfg.lineRatio = c.value("lineRatio").toDouble(0.5);
            cfg.lineHeightPx = c.value("lineHeightPx").toInt(0);
            cfg.widthRegionHeight = c.value("widthRegionHeight").toInt(0);
            cfg.lineColor = colorFromJson(c.value("lineColor"), Qt::red);
            cfg.rotation = c.value("rotation").toInt(0);
            cfg.mmPerPixel = c.value("mmPerPixel").toDouble(0.5);
            cfg.flipHorizontal = c.value("flipHorizontal").toBool(false);
            cfg.flipVertical = c.value("flipVertical").toBool(false);
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
            if (!validateCameraConfig(cfg)) {
                LogManager::instance().logWarn(tr("Invalid config for camera %1, fallback to defaults").arg(i));
            }
            appConfig.cameras[i] = cfg;
        }
    }

    if (obj.contains("push") && obj.value("push").isObject()) {
        QJsonObject p = obj.value("push").toObject();
        appConfig.push.url = p.value("url").toString();
        appConfig.push.token = p.value("token").toString();
        appConfig.push.templateText = p.value("templateText").toString();
        appConfig.push.enabled = p.value("enabled").toBool(false);
        appConfig.push.maxFailures = p.value("maxFailures").toInt(appConfig.push.maxFailures);
    }
}

QJsonObject ConfigManager::toJson() const
{
    QJsonObject obj;
    obj.insert("pumpPort", appConfig.pumpPort);
    obj.insert("pumpDurationMs", appConfig.pumpDurationMs);
    obj.insert("pumpThresholdMM", appConfig.pumpThresholdMM);
    obj.insert("pumpCooldownMs", appConfig.pumpCooldownMs);
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
        c.insert("lineHeightPx", cfg.lineHeightPx);
        c.insert("widthRegionHeight", cfg.widthRegionHeight);
        c.insert("lineColor", colorToJson(cfg.lineColor));
        c.insert("rotation", cfg.rotation);
        c.insert("mmPerPixel", cfg.mmPerPixel);
        c.insert("flipHorizontal", cfg.flipHorizontal);
        c.insert("flipVertical", cfg.flipVertical);
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
        arr.append(c);
    }
    obj.insert("cameras", arr);

    QJsonObject push;
    push.insert("url", appConfig.push.url);
    push.insert("token", appConfig.push.token);
    push.insert("templateText", appConfig.push.templateText);
    push.insert("enabled", appConfig.push.enabled);
    push.insert("maxFailures", appConfig.push.maxFailures);
    obj.insert("push", push);
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

QColor ConfigManager::colorFromJson(const QJsonValue &v, const QColor &fallback)
{
    if (v.isArray()) {
        QJsonArray arr = v.toArray();
        if (arr.size() >= 3) {
            return QColor(arr.at(0).toInt(), arr.at(1).toInt(), arr.at(2).toInt());
        }
    }
    if (v.isString()) {
        QColor c(v.toString());
        if (c.isValid()) return c;
    }
    return fallback;
}

QJsonValue ConfigManager::colorToJson(const QColor &c)
{
    QJsonArray arr;
    arr.append(c.red());
    arr.append(c.green());
    arr.append(c.blue());
    return arr;
}

bool ConfigManager::loadPushOverride(const QString &path)
{
    QString filePath = path;
    if (filePath.isEmpty()) {
        filePath = pushConfigPath();
    }
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject obj = doc.object();
    QMutexLocker locker(&mutex);
    appConfig.push.url = obj.value("url").toString(appConfig.push.url);
    appConfig.push.enabled = obj.value("enabled").toBool(appConfig.push.enabled);
    appConfig.push.maxFailures = obj.value("maxFailures").toInt(appConfig.push.maxFailures);
    appConfig.push.templateText = obj.value("templateText").toString(appConfig.push.templateText);
    return true;
}

bool ConfigManager::savePushOverride(const QString &path) const
{
    QString filePath = path;
    if (filePath.isEmpty()) {
        filePath = pushConfigPath();
    }
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    QMutexLocker locker(&mutex);
    QJsonObject obj;
    obj.insert("url", appConfig.push.url);
    obj.insert("enabled", appConfig.push.enabled);
    obj.insert("maxFailures", appConfig.push.maxFailures);
    obj.insert("templateText", appConfig.push.templateText);
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

QString ConfigManager::pushConfigPath() const
{
    if (!lastPath.isEmpty()) {
        QFileInfo fi(lastPath);
        if (fi.isDir()) {
            return fi.filePath() + "/push.json";
        }
        return fi.absoluteDir().filePath("push.json");
    }
    return QStringLiteral("push.json");
}

