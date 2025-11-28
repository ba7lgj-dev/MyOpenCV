#ifndef XINGAODAAPP_H
#define XINGAODAAPP_H

#include <QMainWindow>
#include <QTimer>
#include <QVector>
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
    void onCameraIndexChanged(int id, int index);
    void onCameraNameChanged();
    void onLineRatioChanged(int id, int value);
    void onColorClicked(int id);
    void onRotationChanged(int id, int idx);
    void onSwapCameras();
    void onDualModeToggled(bool enabled);
    void onAvailableCameras(const QVector<int> &indexes);
    void onRescan();

private:
    void setupConnections();
    void updateWidthLabel(int id, const WidthResult &result);
    void refreshCameraControls();
    QImage drawOverlays(int id, const QImage &img);

    Ui::xingaodaApp *ui;
    ApplicationCore core;
    WidthResult lastResults[2];
};
#endif // XINGAODAAPP_H
