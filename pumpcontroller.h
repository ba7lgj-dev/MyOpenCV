#ifndef PUMPCONTROLLER_H
#define PUMPCONTROLLER_H

#include <QObject>
#include <QSerialPort>
#include <deque>

class IPumpController : public QObject {
    Q_OBJECT
public:
    explicit IPumpController(QObject *parent = nullptr) : QObject(parent) {}
    ~IPumpController() override = default;
    virtual bool open(const QString &portName) = 0;
    virtual void close() = 0;

public slots:
    virtual void pulseLow(int ms) = 0;
    virtual void forceHigh() = 0;

signals:
    void safetyTriggered(const QString &msg);
};

struct PumpEvent {
    qint64 timestamp;
    int ms;
};

class Cp2102PumpController : public IPumpController {
    Q_OBJECT
public:
    explicit Cp2102PumpController(QObject *parent = nullptr);
    ~Cp2102PumpController() override;
    bool open(const QString &portName) override;
    void close() override;

public slots:
    void pulseLow(int ms) override;
    void forceHigh() override;

private:
    void pushEvent(int ms);
    void checkSafety();

    QSerialPort serial;
    std::deque<PumpEvent> recentEvents;
    int safetyWindowMs {5 * 60 * 1000};
    int safetyMaxEvents {20};
};

class PumpPolicy {
public:
    static int calcPulseMs(double targetMM, double currentMM) {
        double diffRatio = (targetMM - currentMM) / targetMM;
        if (diffRatio <= 0.05) return 300;
        if (diffRatio <= 0.10) return 600;
        return 1000;
    }
};

#endif // PUMPCONTROLLER_H
