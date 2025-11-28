#ifndef XINGAODAAPP_H
#define XINGAODAAPP_H

#include <QMainWindow>
#include <QTimer>
#include <QList>
#include "applicationcore.h"

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
    void onCalib0();
    void onCalib1();
    void onAutoExp0();
    void onAutoExp1();
    void onCameraFrame(int id, const QImage &img);
    void onWidthUpdated(int id, const WidthResult &result);
    void onMessage(const QString &msg);
    void onSafety();
    void onRescanCameras();
    void onCameraIndexChanged0(int idx);
    void onCameraIndexChanged1(int idx);
    void onSwapCameras();
    void onDualModeToggled(bool enabled);
    void onLineRatio0(int value);
    void onLineRatio1(int value);
    void onRegionHeight0(int value);
    void onRegionHeight1(int value);
    void onRotation0(int index);
    void onRotation1(int index);
    void onLineColor0();
    void onLineColor1();
    void onAvailableCameras(const QList<int> &indexes);
    void onCameraStatus(int id, const QString &msg, bool error);

private:
    void setupConnections();
    void updateWidthLabel(int id, const WidthResult &result);
    void syncCameraUi();
    void updateCameraStatusLabel(int id, const QString &msg, bool error);

    Ui::xingaodaApp *ui;
    ApplicationCore core;
};
#endif // XINGAODAAPP_H
