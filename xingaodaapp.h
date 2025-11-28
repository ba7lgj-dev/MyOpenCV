#ifndef XINGAODAAPP_H
#define XINGAODAAPP_H

#include <QMainWindow>
#include <QTimer>
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
    void onCalib0();
    void onCalib1();
    void onAutoExp0();
    void onAutoExp1();
    void onCameraFrame(int id, const QImage &img);
    void onWidthUpdated(int id, const WidthResult &result);
    void onMessage(const QString &msg);
    void onSafety();
    void onManageCameras();

private:
    void setupConnections();
    void updateWidthLabel(int id, const WidthResult &result);
    QImage renderCameraFrame(int id, const QImage &img);
    void updateCameraTitles();

    Ui::xingaodaApp *ui;
    ApplicationCore core;
    WidthResult latestResults[2];
};
#endif // XINGAODAAPP_H
