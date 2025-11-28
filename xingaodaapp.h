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
    void onCameraError(int id, const QString &msg);
    void onAvailableCameras(const QVector<int> &indexes);
    void onCameraNameEdited();
    void onCameraSelectionChanged();
    void onRotationChanged(int index);
    void onLineRatioChanged(int value);
    void onLineHeightChanged(int value);
    void onWidthRegionChanged(int value);
    void onLineColorClicked();
    void onDualModeToggled(bool enabled);
    void onSwapCameras();

private:
    void setupConnections();
    void updateWidthLabel(int id, const WidthResult &result);
    void loadConfigToUi();
    QImage drawOverlay(int id, const QImage &img) const;
    int senderCameraId(QObject *sender) const;

    Ui::xingaodaApp *ui;
    ApplicationCore core;
    WidthResult lastResults[2];
};
#endif // XINGAODAAPP_H
