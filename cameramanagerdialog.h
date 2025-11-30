#ifndef CAMERAMANAGERDIALOG_H
#define CAMERAMANAGERDIALOG_H

#include <QDialog>
#include <QList>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include "configmanager.h"

class ApplicationCore;

class CameraManagerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CameraManagerDialog(ApplicationCore *core, QWidget *parent = nullptr);

private slots:
    void onRescan();
    void onSwap();
    void onAccept();
    void onAbsoluteCalibrate();

private:
    void loadFromConfig();
    void populateIndices();

    ApplicationCore *core {nullptr};
    ConfigManager *cfg {nullptr};
    QList<int> indices;

    QComboBox *comboIndex0 {nullptr};
    QComboBox *comboIndex1 {nullptr};
    QLineEdit *editName0 {nullptr};
    QLineEdit *editName1 {nullptr};
    QComboBox *comboRotation0 {nullptr};
    QComboBox *comboRotation1 {nullptr};
    QCheckBox *chkDualMode {nullptr};
    QDoubleSpinBox *spinAbsoluteWidth {nullptr};
}; 

#endif // CAMERAMANAGERDIALOG_H
