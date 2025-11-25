#include "ConfigManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QJsonObject toJson(const CameraConfig &cfg) {
    QJsonObject obj;
    obj.insert("index", cfg.index);
    obj.insert("name", cfg.name);
    obj.insert("lineRatio", cfg.settings.lineRatio);
    obj.insert("threshold", cfg.settings.thresholdValue);
    obj.insert("brightness", cfg.settings.brightnessOffset);
    obj.insert("mmPerPixel", cfg.settings.mmPerPixel);
    obj.insert("alarmThresholdMm", cfg.alarmThresholdMm);
    obj.insert("alarmEnabled", cfg.alarmEnabled);
    obj.insert("autoInflate", cfg.autoInflate);
    obj.insert("inflateMs", cfg.inflateMs);
    obj.insert("cp2102Port", cfg.cp2102Port);
    return obj;
}

CameraConfig fromJson(const QJsonObject &obj) {
    CameraConfig cfg;
    cfg.index = obj.value("index").toInt(cfg.index);
    cfg.name = obj.value("name").toString();
    cfg.settings.lineRatio = obj.value("lineRatio").toDouble(cfg.settings.lineRatio);
    cfg.settings.thresholdValue = obj.value("threshold").toInt(cfg.settings.thresholdValue);
    cfg.settings.brightnessOffset = obj.value("brightness").toInt(cfg.settings.brightnessOffset);
    cfg.settings.mmPerPixel = obj.value("mmPerPixel").toDouble(cfg.settings.mmPerPixel);
    cfg.alarmThresholdMm = obj.value("alarmThresholdMm").toDouble(cfg.alarmThresholdMm);
    cfg.alarmEnabled = obj.value("alarmEnabled").toBool(cfg.alarmEnabled);
    cfg.autoInflate = obj.value("autoInflate").toBool(cfg.autoInflate);
    cfg.inflateMs = obj.value("inflateMs").toDouble(cfg.inflateMs);
    cfg.cp2102Port = obj.value("cp2102Port").toString();
    return cfg;
}
} // namespace

ConfigManager::ConfigManager(QString path) : m_path(std::move(path)) {}

AppConfig ConfigManager::load() {
    AppConfig config;
    QFile file(m_path);
    if (!file.exists()) {
        return config;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return config;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();
    config.webhookUrl = root.value("webhookUrl").toString();

    QJsonArray cameraArray = root.value("cameras").toArray();
    for (const auto &item : cameraArray) {
        config.cameras.push_back(fromJson(item.toObject()));
    }
    return config;
}

void ConfigManager::save(const AppConfig &config) const {
    QJsonObject root;
    root.insert("webhookUrl", config.webhookUrl);
    QJsonArray cameraArray;
    for (const auto &cam : config.cameras) {
        cameraArray.append(toJson(cam));
    }
    root.insert("cameras", cameraArray);

    QJsonDocument doc(root);
    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(doc.toJson());
}

