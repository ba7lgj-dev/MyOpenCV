#ifndef XINGAODAAPP_H
#define XINGAODAAPP_H

#include <QMainWindow>
#include <QColor>
#include <QLabel>
#include <QAction>
#include <QPointer>
#include "applicationcore.h"
#include "widthestimator.h"
#include "cameramanagerdialog.h"
#include "pumpsettingsdialog.h"
#include "pushsettingsdialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class xingaodaApp; }
QT_END_NAMESPACE

class xingaodaApp : public QMainWindow
{
    Q_OBJECT

public:
    xingaodaApp(QWidget *parent = nullptr);
    ~xingaodaApp();

private slots:
    void onStart();
    void onStop();
    void onCalibrateAll();
    void openCameraManager();
    void openPumpSettings();
    void openPushSettings();
    void openDebugTools();
    void toggleAutoPump();
    void reloadConfig();
    void saveConfig();
    void onCameraFrame(int id, const QImage &img);
    void onWidthUpdated(int id, const WidthResult &result);
    void onFusionUpdated(double fusedMm, double cam0Mm, double cam1Mm);
    void onMessage(const QString &msg);
    void onSafety();
    void onPushStatusChanged(int failures);
    void onPushFailureAlarm(int failures);
    void onAutoThresholdEdited();

private:
    void setupConnections();
    void createMenus();
    void updateAutoPumpAction();
    QImage drawOverlay(int id, const QImage &src);
    void updateFusionLabels(double fusedCm);

    Ui::xingaodaApp *ui;
    ApplicationCore core;
    WidthResult lastWidth[2];
    bool cameraOnline[2] {false, false};
    QLabel *pushStatusLabel {nullptr};
    QAction *actionStart {nullptr};
    QAction *actionStop {nullptr};
    QAction *actionAutoPump {nullptr};
    QPointer<CameraManagerDialog> cameraDialog;
    QPointer<PumpSettingsDialog> pumpDialog;
    QPointer<PushSettingsDialog> pushDialog;
    double lastCalibrationWidthCm {80.0};
};
#endif // XINGAODAAPP_H
