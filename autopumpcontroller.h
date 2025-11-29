#ifndef AUTOPUMPCONTROLLER_H
#define AUTOPUMPCONTROLLER_H

#include <QObject>
#include <QThread>
#include <QElapsedTimer>
#include "pumpcontroller.h"
#include "pushmanager.h"
#include "configmanager.h"

class AutoPumpController : public QObject
{
    Q_OBJECT
public:
    explicit AutoPumpController(IPumpController *pump, PushManager *pushMgr, ConfigManager *cfg, QObject *parent = nullptr);

public slots:
    void updateSettings(const AppConfig &cfg);
    void handleWidthSample(int cameraId, double widthMm, bool frameStable = true);
    void setEnabled(bool enabled);
    void forceReset();

signals:
    void statusMessage(const QString &msg);

private:
    enum class State {
        Standby,
        Prejudge,
        Pumping,
        Monitoring,
        Cooling,
        Error
    };

    void changeState(State next, const QString &reason = QString());
    bool prerequisitesReady() const;
    void startPump(int cameraId, double currentWidth);
    void finishSuccess(double width);
    void handleAnomaly(const QString &key, const QString &reason);
    void logTransition(const QString &action, double width, int cameraId = -1) const;

    IPumpController *pump {nullptr};
    PushManager *push {nullptr};
    ConfigManager *cfg {nullptr};

    State state {State::Standby};
    QElapsedTimer timer;
    qint64 phaseStart {0};
    qint64 lastPumpTime {0};
    double pumpStartWidth {0.0};
    int pumpCameraId {0};
    bool enabled {false};
    bool frameStable {true};

    double startThreshold {1000.0};
    double stopThreshold {1200.0};
    int precheckMs {5000};
    int pulseMs {600};
    int monitorMs {3000};
    int cooldownMs {2000};
    double minInflation {5.0};
};

#endif // AUTOPUMPCONTROLLER_H
