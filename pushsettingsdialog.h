#ifndef PUSHSETTINGSDIALOG_H
#define PUSHSETTINGSDIALOG_H

#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include "configmanager.h"

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

    QLineEdit *editUrl {nullptr};
    QCheckBox *chkEnabled {nullptr};
    QSpinBox *spinMaxFailures {nullptr};
    QLabel *labelStatus {nullptr};
};

#endif // PUSHSETTINGSDIALOG_H
