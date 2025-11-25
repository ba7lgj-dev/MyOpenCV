#pragma once

#include "CameraProcessor.h"

#include <QString>
#include <vector>

struct CameraConfig {
    int index{0};
    QString name;
    CameraSettings settings;
    double alarmThresholdMm{0.0};
    bool alarmEnabled{false};
    bool autoInflate{false};
    double inflateMs{600.0};
    QString cp2102Port;
};

struct AppConfig {
    QString webhookUrl;
    std::vector<CameraConfig> cameras;
};

class ConfigManager {
public:
    explicit ConfigManager(QString path = "config.json");

    AppConfig load();
    void save(const AppConfig &config) const;

private:
    QString m_path;
};

