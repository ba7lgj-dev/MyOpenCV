#ifndef XINGAODAAPP_H
#define XINGAODAAPP_H

#include <QMainWindow>
#include <QTimer>
#include <QColor>
#include "applicationcore.h"
#include "widthestimator.h"

class QCloseEvent;

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
    void onLineChanged0(int value);
    void onLineChanged1(int value);
    void onBandChanged0(int value);
    void onBandChanged1(int value);
    void onColor0();
    void onColor1();
    void onRotation0(int idx);
    void onRotation1(int idx);
    void onCameraManager();
    void onPumpSettings();
    void onCameraFrame(int id, const QImage &img);
    void onWidthUpdated(int id, const WidthResult &result);
    void onMessage(const QString &msg);
    void onSafety();
    void onPumpThresholdChanged(double value);
    void onPushEnabled(bool enabled);
    void onPushUrlEdited();
    void onTestPush();
    void onPushFailed(const QString &msg, int failures);
    void onPushRecovered();

private:
    void setupConnections();
    void updateWidthLabel(int id, const WidthResult &result);
    void updateWidthSummary();
    double estimatedWidth() const;
    void syncCameraUi(int id);
    QImage drawOverlay(int id, const QImage &src);
    void closeEvent(QCloseEvent *event) override;
    void initRotationCombos();

    Ui::xingaodaApp *ui;
    ApplicationCore core;
    WidthResult lastWidth[2];
    bool shutdownNotified {false};
};
#endif // XINGAODAAPP_H
