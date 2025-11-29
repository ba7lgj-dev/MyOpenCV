#ifndef XINGAODAAPP_H
#define XINGAODAAPP_H

#include <QMainWindow>
#include <QTimer>
#include <QColor>
#include <QLabel>
#include "applicationcore.h"
#include "widthestimator.h"

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
    void onAutoExp0();
    void onAutoExp1();
    void onLineChanged0(int value);
    void onLineChanged1(int value);
    void onLineSpinChanged0(int value);
    void onLineSpinChanged1(int value);
    void onBandChanged0(int value);
    void onBandChanged1(int value);
    void onColor0();
    void onColor1();
    void onRotation0(int idx);
    void onRotation1(int idx);
    void onCameraIndex0(int idx);
    void onCameraIndex1(int idx);
    void onFlipH0(bool checked);
    void onFlipV0(bool checked);
    void onFlipH1(bool checked);
    void onFlipV1(bool checked);
    void onFusionStrategyChanged(int idx);
    void onCameraFrame(int id, const QImage &img);
    void onWidthUpdated(int id, const WidthResult &result);
    void onFusionUpdated(double fusedMm, double cam0Mm, double cam1Mm);
    void onMessage(const QString &msg);
    void onSafety();
    void onPumpThresholdChanged(double value);
    void onPumpStopThresholdChanged(double value);
    void onPumpDurationChanged(double value);
    void onPumpPrecheckChanged(double value);
    void onPumpMonitorChanged(double value);
    void onPumpCooldownChanged(double value);
    void onPumpMinInflationChanged(double value);
    void onPumpPortChanged(const QString &port);
    void onPushStatusChanged(int failures);
    void onPushFailureAlarm(int failures);
    void onPushEnabledChanged(bool enabled);
    void onPushUrlEdited(const QString &text);
    void onTestPush();

private:
    void setupRotationCombos();
    void setupConnections();
    void syncCameraUi(int id);
    QImage drawOverlay(int id, const QImage &src);
    void populateCameraIndexes();
    void syncPumpUi();
    void syncPushUi();
    void populatePumpPorts();
    void updateFusionLabels(double fusedCm);
    void applyLineRatio(int id, int value);

    Ui::xingaodaApp *ui;
    ApplicationCore core;
    WidthResult lastWidth[2];
    bool cameraOnline[2] {false, false};
    QLabel *pushStatusLabel {nullptr};
};
#endif // XINGAODAAPP_H
