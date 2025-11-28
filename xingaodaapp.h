#ifndef XINGAODAAPP_H
#define XINGAODAAPP_H

#include <QMainWindow>
#include <QTimer>
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
    void onCameraNameEdited(int id, const QString &name);
    void onRotationChanged(int id, int value);
    void onLineRatioChanged(int id, int sliderValue);
    void onLineColorChanged(int id);
    void onLineHeightChanged(int id, int value);
    void onWidthRegionChanged(int id, int value);
    void onPumpConfigUpdated();
    void onPushConfigUpdated();
    void onRestoreDefaults();
    void onTestPush();

private:
    void setupConnections();
    void updateWidthLabel(int id, const WidthResult &result);
    QPixmap drawOverlay(int id, const QImage &img) const;
    QColor askColor(const QColor &current) const;
    void refreshUiFromConfig();

    Ui::xingaodaApp *ui;
    ApplicationCore core;
    WidthResult lastResult[2];
};
#endif // XINGAODAAPP_H
