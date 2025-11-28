#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QPainter>
#include <QPen>
#include <algorithm>

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    ui->chartView->setChart(core.trendChart()->chart());
    setupConnections();
    core.initialize();
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
    connect(ui->actionRestore, &QAction::triggered, &core, &ApplicationCore::restoreDefaults);

    connect(&core, &ApplicationCore::cameraFrame, this, &xingaodaApp::onCameraFrame);
    connect(&core, &ApplicationCore::widthUpdated, this, &xingaodaApp::onWidthUpdated);
    connect(&core, &ApplicationCore::message, this, &xingaodaApp::onMessage);
    connect(&core, &ApplicationCore::safetyModeEnabled, this, &xingaodaApp::onSafety);
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
    QImage painted = img.copy();
    paintOverlay(id, painted);
    QPixmap pix = QPixmap::fromImage(painted).scaled(400, 250, Qt::KeepAspectRatio);
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
    lastTrend[id] = lastResult[id].widthMM;
    lastResult[id] = result;
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

void xingaodaApp::paintOverlay(int id, QImage &img)
{
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);

    CameraConfig cfgCam = core.config()->camera(id);
    int h = img.height();
    int w = img.width();
    int lineY = cfgCam.lineHeightPx > 0 ? cfgCam.lineHeightPx : static_cast<int>(cfgCam.lineRatio * h);
    int regionH = cfgCam.widthRegionHeight > 0 ? cfgCam.widthRegionHeight : h / 4;
    int regionTop = std::max(0, lineY - regionH / 2);

    painter.setPen(QPen(cfgCam.lineColor, 2));
    painter.drawLine(0, lineY, w, lineY);
    painter.drawRect(0, regionTop, w, std::min(regionH, h - regionTop));

    const WidthResult &r = lastResult[id];
    if (r.valid) {
        painter.setPen(QPen(Qt::green, 2, Qt::DashLine));
        painter.drawLine(r.leftX, 0, r.leftX, h);
        painter.drawLine(r.rightX, 0, r.rightX, h);

        QString arrow = "→";
        if (lastTrend[id] > 0) {
            if (r.widthMM > lastTrend[id]) arrow = "↑"; else if (r.widthMM < lastTrend[id]) arrow = "↓";
        }
        painter.setPen(QPen(Qt::yellow, 2));
        painter.drawText(10, 20, tr("宽度: %1 mm (%2 px) %3").arg(r.widthMM, 0, 'f', 1).arg(r.widthPixels, 0, 'f', 1).arg(arrow));
    }
}

