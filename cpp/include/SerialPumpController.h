#pragma once

#include <QString>
#include <QSerialPort>

class SerialPumpController {
public:
    SerialPumpController();

    void pulseLow(const QString &portName, int durationMs);

private:
    QSerialPort m_port;
};

