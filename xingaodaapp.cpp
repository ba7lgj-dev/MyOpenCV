#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include "cameramanagerdialog.h"
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QPainter>
#include <QTransform>
#include <algorithm>

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    ui->chartView->setChart(core.trendChart()->chart());
    setupConnections();
    core.initialize();
    updateCameraTitles();
}

xingaodaApp::~xingaodaApp()
{
    core.stopCameras();
    delete ui;
}

void xingaodaApp::setupConnections()
{
    connect(ui->btnStart, &QPushButton::clicked, this, &xingaodaApp::onStart);
    connect(ui->btnStop, &QPushButton::clicked, this, &xingaodaApp::onStop);
    connect(ui->btnCalib0, &QPushButton::clicked, this, &xingaodaApp::onCalib0);
    connect(ui->btnCalib1, &QPushButton::clicked, this, &xingaodaApp::onCalib1);
    connect(ui->btnAutoExp0, &QPushButton::clicked, this, &xingaodaApp::onAutoExp0);
    connect(ui->btnAutoExp1, &QPushButton::clicked, this, &xingaodaApp::onAutoExp1);
    connect(ui->chkAutoPump, &QCheckBox::toggled, &core, &ApplicationCore::setAutoPumpEnabled);

    connect(&core, &ApplicationCore::cameraFrame, this, &xingaodaApp::onCameraFrame);
    connect(&core, &ApplicationCore::widthUpdated, this, &xingaodaApp::onWidthUpdated);
    connect(&core, &ApplicationCore::message, this, &xingaodaApp::onMessage);
    connect(&core, &ApplicationCore::safetyModeEnabled, this, &xingaodaApp::onSafety);
    connect(ui->actionCameraManager, &QAction::triggered, this, &xingaodaApp::onManageCameras);
}

void xingaodaApp::onStart()
{
    core.startCameras();
    onMessage(tr("Cameras started"));
}

void xingaodaApp::onStop()
{
    core.stopCameras();
    onMessage(tr("Cameras stopped"));
}

void xingaodaApp::onCalib0()
{
    core.calibrateWidth(0, ui->spinReal0->value());
}

void xingaodaApp::onCalib1()
{
    core.calibrateWidth(1, ui->spinReal1->value());
}

void xingaodaApp::onAutoExp0()
{
    core.toggleAutoExposure(0);
}

void xingaodaApp::onAutoExp1()
{
    core.toggleAutoExposure(1);
}

void xingaodaApp::onCameraFrame(int id, const QImage &img)
{
    QImage decorated = renderCameraFrame(id, img);
    QPixmap pix = QPixmap::fromImage(decorated).scaled(400, 250, Qt::KeepAspectRatio);
    if (id == 0) {
        ui->labelCam0->setPixmap(pix);
    } else {
        ui->labelCam1->setPixmap(pix);
    }
}

void xingaodaApp::updateWidthLabel(int id, const WidthResult &result)
{
    QString text = QString("Width: %1 px / %2 mm").arg(result.widthPixels, 0, 'f', 1).arg(result.widthMM, 0, 'f', 1);
    if (id == 0) {
        ui->labelWidth0->setText(text);
        ui->labelStatus0->setText(tr("Row %1, L=%2, R=%3").arg(result.usedRow).arg(result.leftX).arg(result.rightX));
    } else {
        ui->labelWidth1->setText(text);
        ui->labelStatus1->setText(tr("Row %1, L=%2, R=%3").arg(result.usedRow).arg(result.leftX).arg(result.rightX));
    }
}

void xingaodaApp::onWidthUpdated(int id, const WidthResult &result)
{
    latestResults[id] = result;
    updateWidthLabel(id, result);
}

void xingaodaApp::onMessage(const QString &msg)
{
    ui->plainTextLog->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss ") + msg);
    statusBar()->showMessage(msg, 3000);
}

void xingaodaApp::onSafety()
{
    ui->chkAutoPump->setChecked(false);
    onMessage(tr("自动加气安全模式，已关闭自动加气"));
}

void xingaodaApp::onManageCameras()
{
    CameraManagerDialog dlg(&core, this);
    dlg.exec();
    updateCameraTitles();
}

QImage xingaodaApp::renderCameraFrame(int id, const QImage &img)
{
    CameraConfig cfgCam = core.config()->camera(id);
    QImage rotated = img;
    if (cfgCam.rotation != 0) {
        QTransform t;
        t.rotate(cfgCam.rotation);
        rotated = img.transformed(t);
    }

    QImage canvas = rotated.copy();
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(cfgCam.lineColor, 2));

    int lineY = static_cast<int>(cfgCam.lineRatio * canvas.height());
    painter.drawLine(0, lineY, canvas.width(), lineY);

    int regionHeight = cfgCam.widthRegionHeight > 0 ? cfgCam.widthRegionHeight : canvas.height() / 2;
    regionHeight = std::max(10, std::min(regionHeight, canvas.height()));
    int top = lineY - regionHeight / 2;
    if (top < 0) top = 0;
    if (top + regionHeight > canvas.height()) top = canvas.height() - regionHeight;
    painter.drawRect(0, top, canvas.width() - 1, regionHeight);

    if (latestResults[id].valid) {
        painter.setPen(QPen(Qt::green, 2));
        painter.drawLine(latestResults[id].leftX, top, latestResults[id].leftX, top + regionHeight);
        painter.drawLine(latestResults[id].rightX, top, latestResults[id].rightX, top + regionHeight);
        painter.drawText(latestResults[id].leftX + 4, top + 20, tr("左边界"));
        painter.drawText(latestResults[id].rightX - 60, top + 20, tr("右边界"));
    }
    painter.end();
    return canvas;
}

void xingaodaApp::updateCameraTitles()
{
    ui->groupCam0->setTitle(core.config()->camera(0).name.isEmpty() ? tr("Camera 0") : core.config()->camera(0).name);
    ui->groupCam1->setTitle(core.config()->camera(1).name.isEmpty() ? tr("Camera 1") : core.config()->camera(1).name);
}

