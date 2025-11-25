#include "SerialPumpController.h"

#include <QThread>

SerialPumpController::SerialPumpController() = default;

void SerialPumpController::pulseLow(const QString &portName, int durationMs) {
    if (portName.isEmpty()) {
        return;
    }

    if (m_port.isOpen() && m_port.portName() != portName) {
        m_port.close();
    }

    if (!m_port.isOpen()) {
        m_port.setPortName(portName);
        m_port.setBaudRate(QSerialPort::Baud115200);
        if (!m_port.open(QIODevice::ReadWrite)) {
            return;
        }
    }

    m_port.setDataTerminalReady(true);
    m_port.setRequestToSend(true);
    m_port.setDataTerminalReady(false);
    m_port.setRequestToSend(false);
    QThread::msleep(static_cast<unsigned long>(durationMs));
    m_port.setDataTerminalReady(true);
    m_port.setRequestToSend(true);
}

