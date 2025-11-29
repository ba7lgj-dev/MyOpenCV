#ifndef PUSHSETTINGSDIALOG_H
#define PUSHSETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include "configmanager.h"
#include "pushmanager.h"

class ApplicationCore;

class PushSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PushSettingsDialog(ApplicationCore *core, QWidget *parent = nullptr);

private slots:
    void onAccept();
    void onTest();

private:
    void loadConfig();

    ApplicationCore *core {nullptr};
    ConfigManager *cfg {nullptr};
    PushManager *push {nullptr};

    QLineEdit *editUrl {nullptr};
    QCheckBox *chkEnabled {nullptr};
    QSpinBox *spinMaxFailures {nullptr};
    QSpinBox *spinThrottleSeconds {nullptr};
    QLabel *labelStatus {nullptr};
};

#endif // PUSHSETTINGSDIALOG_H

