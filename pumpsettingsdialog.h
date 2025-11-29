#ifndef PUMPSETTINGSDIALOG_H
#define PUMPSETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
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
    QSpinBox *spinDuration {nullptr};
    QSpinBox *spinCooldown {nullptr};
    QSpinBox *spinPushFailures {nullptr};
    QLineEdit *editPushUrl {nullptr};
    QCheckBox *chkPushEnabled {nullptr};
    QLabel *labelStatus {nullptr};
};

#endif // PUMPSETTINGSDIALOG_H
