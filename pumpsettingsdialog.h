#ifndef PUMPSETTINGSDIALOG_H
#define PUMPSETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include "configmanager.h"

class ApplicationCore;

class PumpSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PumpSettingsDialog(ApplicationCore *core, QWidget *parent = nullptr);

private slots:
    void onRefreshPorts();
    void onTestPort();
    void onAccept();

private:
    void loadConfig();
    void populatePorts();

    ApplicationCore *core {nullptr};
    ConfigManager *cfg {nullptr};

    QComboBox *comboPort {nullptr};
    QDoubleSpinBox *spinDuration {nullptr};
    QDoubleSpinBox *spinPrecheck {nullptr};
    QDoubleSpinBox *spinCooldown {nullptr};
    QDoubleSpinBox *spinStartThreshold {nullptr};
    QDoubleSpinBox *spinNoChangeTimeout {nullptr};
    QDoubleSpinBox *spinMinInflation {nullptr};
    QLabel *labelStatus {nullptr};
};

#endif // PUMPSETTINGSDIALOG_H
