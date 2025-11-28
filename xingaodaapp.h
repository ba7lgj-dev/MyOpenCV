#ifndef XINGAODAAPP_H
#define XINGAODAAPP_H

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
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
    void onRescan();
    void onCamera0Changed(int index);
    void onCamera1Changed(int index);
    void onCamera0NameEdited(const QString &name);
    void onCamera1NameEdited(const QString &name);
    void onLineSlider0(int value);
    void onLineSlider1(int value);
    void onLineHeight0(int value);
    void onLineHeight1(int value);
    void onRegionHeight0(int value);
    void onRegionHeight1(int value);
    void onColor0();
    void onColor1();
    void onRotation0(int index);
    void onRotation1(int index);
    void onSwapCameras();
    void onDualModeToggled(bool checked);

private:
    void setupConnections();
    void updateWidthLabel(int id, const WidthResult &result);
    void refreshCameraSelectors();
    void paintOverlay(int id, QLabel *label, QImage frame);

    Ui::xingaodaApp *ui;
    ApplicationCore core;
    WidthResult lastResults[2];
};
#endif // XINGAODAAPP_H
