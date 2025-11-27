#include "pumpcontroller.h"
#include "logmanager.h"
#include <QDateTime>
#include <QTimer>

Cp2102PumpController::Cp2102PumpController(QObject *parent)
    : IPumpController(parent)
{
}

Cp2102PumpController::~Cp2102PumpController()
{
    close();
}

bool Cp2102PumpController::open(const QString &portName)
{
    serial.setPortName(portName);
    serial.setBaudRate(QSerialPort::Baud9600);
    if (!serial.open(QIODevice::ReadWrite)) {
        LogManager::instance().logError(tr("Open pump serial %1 failed").arg(portName));
        return false;
    }
    serial.setDataTerminalReady(true);
    serial.setRequestToSend(true);
    return true;
}

void Cp2102PumpController::close()
{
    if (serial.isOpen()) {
        serial.close();
    }
}

void Cp2102PumpController::pulseLow(int ms)
{
    if (!serial.isOpen()) return;
    serial.setDataTerminalReady(false);
    serial.setRequestToSend(false);
    LogManager::instance().logInfo(tr("Auto pump triggered: pulse=%1ms").arg(ms));
    QTimer::singleShot(ms, this, [this, ms]() {
        if (!serial.isOpen()) return;
        serial.setDataTerminalReady(true);
        serial.setRequestToSend(true);
        pushEvent(ms);
        checkSafety();
    });
}

void Cp2102PumpController::forceHigh()
{
    if (!serial.isOpen()) return;
    serial.setDataTerminalReady(true);
    serial.setRequestToSend(true);
}

void Cp2102PumpController::pushEvent(int ms)
{
    PumpEvent ev{QDateTime::currentMSecsSinceEpoch(), ms};
    recentEvents.push_back(ev);
}

void Cp2102PumpController::checkSafety()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    while (!recentEvents.empty() && now - recentEvents.front().timestamp > safetyWindowMs) {
        recentEvents.pop_front();
    }
    if (static_cast<int>(recentEvents.size()) > safetyMaxEvents) {
        emit safetyTriggered(tr("Auto pump safety mode enabled: too many pulses"));
        LogManager::instance().logWarn("Auto pump safety mode enabled: too many pulses in window");
    }
}

