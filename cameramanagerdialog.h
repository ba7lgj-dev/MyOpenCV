#ifndef CAMERAMANAGERDIALOG_H
#define CAMERAMANAGERDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include "applicationcore.h"

class CameraManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit CameraManagerDialog(ApplicationCore *core, QWidget *parent = nullptr);

private slots:
    void onSwap();
    void onRescan();
    void applyAndClose();

private:
    struct CameraWidgets {
        QComboBox *indexCombo {nullptr};
        QLineEdit *nameEdit {nullptr};
        QComboBox *rotationCombo {nullptr};
        QSlider *lineSlider {nullptr};
        QLabel *lineValue {nullptr};
        QPushButton *colorBtn {nullptr};
        QSpinBox *regionHeight {nullptr};
    };

    void buildUi();
    void populateCamera(int id, CameraWidgets &widgets);
    QColor selectColor(const QColor &current);
    void syncConfigFromWidgets(int id, const CameraWidgets &widgets);
    void refreshAvailableIndices();

    ApplicationCore *core {nullptr};
    ConfigManager *cfg {nullptr};
    QVector<int> currentIndices;
    CameraWidgets camWidgets[2];
    QCheckBox *dualModeCheck {nullptr};
    QPushButton *rescanBtn {nullptr};
};

#endif // CAMERAMANAGERDIALOG_H
