#ifndef XINGAODAAPP_H
#define XINGAODAAPP_H

#include <QMainWindow>
#include <QTimer>
#include <QMap>
#include <functional>
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
    void onLineChanged(int value);
    void onResetDefaults();
    void onCameraSettings();
    void onDetectSettings();
    void onPumpSettings();
    void onPushSettings();
    void onConfigReloaded();

private:
    void setupConnections();
    void updateWidthLabel(int id, const WidthResult &result);
    QPixmap drawOverlay(int id, const QImage &img) const;
    void showCameraDialog();
    void showDetectDialog();
    void showPumpDialog();
    void showPushDialog();
    QWidget *buildCameraGroup(int idx, const CameraConfig &cfg, QMap<QString, QWidget *> &widgets);
    QWidget *buildDetectGroup(int idx, const CameraConfig &cfg, QMap<QString, QWidget *> &widgets);

    Ui::xingaodaApp *ui;
    ApplicationCore core;
    WidthResult lastResult[2];
};
#endif // XINGAODAAPP_H
