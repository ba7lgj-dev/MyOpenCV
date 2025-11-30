#include "autopumpcontroller.h"
#include "logmanager.h"
#include "alertratelimiter.h"
#include <QDateTime>
#include <QtGlobal>

AutoPumpController::AutoPumpController(IPumpController *pump, PushManager *pushMgr, ConfigManager *cfg, QObject *parent)
    : QObject(parent), pump(pump), push(pushMgr), cfg(cfg)
{
    timer.start();
}

void AutoPumpController::updateSettings(const AppConfig &cfgValue)
{
    startThreshold = qBound(10.0, cfgValue.autoStartThresholdMM, 10000.0);
    precheckMs = qBound(1000, cfgValue.autoPrecheckMs, 60000);
    pulseMs = qBound(50, cfgValue.pumpDurationMs, 20000);
    cooldownMs = qBound(0, cfgValue.autoCooldownMs, 600000);
    noChangeTimeoutMs = qBound(1000, cfgValue.autoNoChangeTimeoutMs, 60000);
    minInflation = qBound(0.0, cfgValue.autoMinInflationMM, 5000.0);
    enabled = cfgValue.autoPumpEnabled;
}

void AutoPumpController::setEnabled(bool enabledValue)
{
    enabled = enabledValue;
    if (!enabled) {
        forceReset();
    }
}

void AutoPumpController::forceReset()
{
    changeState(State::Standby, tr("reset"));
    frameStable = true;
    pumpStartWidth = 0.0;
    pumpCameraId = 0;
    phaseStart = timer.elapsed();
    inflationObserved = false;
    anomalyRaised = false;
}

void AutoPumpController::handleWidthSample(int cameraId, double widthMm, bool frameOk)
{
    frameStable = frameOk;
    const qint64 now = timer.elapsed();

    if (!enabled) {
        state = State::Standby;
        return;
    }

    if (state == State::Error) {
        if (now - phaseStart >= cooldownMs) {
            changeState(State::Standby, tr("cooldown finished"));
        }
        return;
    }

    if (!frameStable) {
        handleAnomaly(QStringLiteral("camera_stream_unstable"), tr("camera stream unstable"));
        return;
    }

    switch (state) {
    case State::Standby:
        if (widthMm < startThreshold) {
            pumpCameraId = cameraId;
            phaseStart = now;
            changeState(State::Prejudge, tr("width below start threshold"));
        }
        break;
    case State::Prejudge:
        if (widthMm >= startThreshold) {
            changeState(State::Standby, tr("width recovered during precheck"));
            break;
        }
        if (now - phaseStart >= precheckMs) {
            if (prerequisitesReady()) {
                startPump(cameraId, widthMm);
            } else {
                handleAnomaly(QStringLiteral("pump_prerequisites_not_met"), tr("prerequisites not met"));
            }
        }
        break;
    case State::Pumping:
        break;
    case State::Cooling: {
        const double delta = widthMm - pumpStartWidth;
        if (!inflationObserved && delta >= minInflation) {
            inflationObserved = true;
        }
        if (!inflationObserved && !anomalyRaised && (now - lastPumpTime >= noChangeTimeoutMs)) {
            anomalyRaised = true;
            handleAnomaly(QStringLiteral("no_width_change"), tr("no width change observed after pump"));
            break;
        }
        if (now - phaseStart >= cooldownMs) {
            changeState(State::Standby, tr("cooldown finished"));
        }
        break;
    }
    default:
        break;
    }
}

bool AutoPumpController::prerequisitesReady() const
{
    if (!pump) return false;
    if (!pump->isOpen()) return false;
    if (!frameStable) return false;
    return true;
}

void AutoPumpController::startPump(int cameraId, double currentWidth)
{
    pumpCameraId = cameraId;
    pumpStartWidth = currentWidth;
    inflationObserved = false;
    anomalyRaised = false;
    changeState(State::Pumping, tr("start pump"));

    if (!pump->pulseLow(pulseMs)) {
        handleAnomaly(QStringLiteral("pump_pulse_failed"), tr("pump pulse failed: %1").arg(pump->lastError()));
        return;
    }
    lastPumpTime = timer.elapsed();
    if (push) {
        push->sendPumpTriggered(cameraId, currentWidth);
    }
    changeState(State::Cooling, tr("pump pulse finished"));
    phaseStart = timer.elapsed();
}

void AutoPumpController::finishSuccess(double width)
{
    const QString msg = tr("Auto pump success: width=%1cm")
                            .arg(width / 10.0, 0, 'f', 2);
    LogManager::instance().logInfo(msg);
    emit statusMessage(msg);
    changeState(State::Cooling, tr("enter cooldown"));
    phaseStart = timer.elapsed();
}

void AutoPumpController::handleAnomaly(const QString &key, const QString &reason)
{
    const QString msg = tr("Auto pump anomaly: %1").arg(reason);
    LogManager::instance().logError(msg);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const int windowMs = cfg ? cfg->pushConfig().throttleWindowMs : 10000;
    const bool allowed = AlertRateLimiter::instance().shouldAllow(key, windowMs, nowMs);
    if (allowed) {
        emit statusMessage(msg);
        if (push) {
            push->sendException(key, msg);
        }
    } else {
        const QString skipped = tr("异常重复，已限流：%1").arg(key);
        LogManager::instance().logInfo(skipped);
    }
    changeState(State::Error, reason);
    phaseStart = timer.elapsed();
}

void AutoPumpController::changeState(AutoPumpController::State next, const QString &reason)
{
    state = next;
    logTransition(reason, pumpStartWidth, pumpCameraId);
}

void AutoPumpController::logTransition(const QString &action, double width, int cameraId) const
{
    const QString msg = tr("[AutoPump] state=%1 action=%2 cam=%3 width=%4 start=%5 pulse=%6 cooldown=%7 timeout=%8")
                            .arg(static_cast<int>(state))
                            .arg(action)
                            .arg(cameraId)
                            .arg(width)
                            .arg(startThreshold)
                            .arg(pulseMs)
                            .arg(cooldownMs)
                            .arg(noChangeTimeoutMs);
    LogManager::instance().logInfo(msg);
}

