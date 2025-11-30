#ifndef CALIBRATIONMANAGER_H
#define CALIBRATIONMANAGER_H

#include <map>
#include "configmanager.h"

class CalibrationManager {
public:
    explicit CalibrationManager(ConfigManager *cfg);

    void addCalibrationSample(int cameraId, double realMM, double widthPixels);
    double getMmPerPixel(int cameraId) const;
    void setGlobalMmPerPixel(double value);
    void setCameraMmPerPixel(int cameraId, double value);

private:
    ConfigManager *configManager {nullptr};
    std::map<int, double> mmPerPixelMap;
};

#endif // CALIBRATIONMANAGER_H
