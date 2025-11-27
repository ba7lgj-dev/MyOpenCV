#include "calibrationmanager.h"
#include "logmanager.h"

CalibrationManager::CalibrationManager(ConfigManager *cfg)
    : configManager(cfg)
{
    if (cfg) {
        mmPerPixelMap[0] = cfg->camera(0).mmPerPixel;
        mmPerPixelMap[1] = cfg->camera(1).mmPerPixel;
    }
}

void CalibrationManager::addCalibrationSample(int cameraId, double realMM, double widthPixels)
{
    if (widthPixels <= 0 || realMM <= 0) return;
    double sample = realMM / widthPixels;
    double alpha = 0.8;
    double oldVal = mmPerPixelMap[cameraId];
    if (oldVal <= 0) {
        mmPerPixelMap[cameraId] = sample;
    } else {
        mmPerPixelMap[cameraId] = alpha * oldVal + (1.0 - alpha) * sample;
    }
    if (configManager) {
        configManager->updateMmPerPixel(cameraId, mmPerPixelMap[cameraId]);
    }
    LogManager::instance().logInfo(QString("Calibration updated cam%1 -> %2 mm/px").arg(cameraId).arg(mmPerPixelMap[cameraId]));
}

double CalibrationManager::getMmPerPixel(int cameraId) const
{
    auto it = mmPerPixelMap.find(cameraId);
    if (it != mmPerPixelMap.end()) return it->second;
    return 0.0;
}

