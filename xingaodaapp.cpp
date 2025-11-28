#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QPainter>
#include <QPen>

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    ui->chartView->setChart(core.trendChart()->chart());
    ui->sliderLine->setValue(static_cast<int>(core.config()->camera(0).lineRatio * 100));
    setupConnections();
    core.initialize();
    ui->chkAutoPump->setChecked(core.config()->config().autoPumpEnabled);
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
    connect(ui->sliderLine, &QSlider::valueChanged, this, &xingaodaApp::onLineChanged);
    connect(ui->actionResetConfig, &QAction::triggered, this, &xingaodaApp::onResetDefaults);

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
    QPixmap pix = drawOverlay(id, img).scaled(400, 250, Qt::KeepAspectRatio);
    if (id == 0) {
        ui->labelCam0->setPixmap(pix);
    } else {
        ui->labelCam1->setPixmap(pix);
    }
}

void xingaodaApp::updateWidthLabel(int id, const WidthResult &result)
{
    lastResult[id] = result;
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

void xingaodaApp::onLineChanged(int value)
{
    double ratio = value / 100.0;
    AppConfig appCfg = core.config()->config();
    appCfg.cameras[0].lineRatio = ratio;
    appCfg.cameras[1].lineRatio = ratio;
    core.config()->setConfig(appCfg);
    onMessage(tr("检测线高度已调整"));
}

void xingaodaApp::onResetDefaults()
{
    core.config()->resetDefaults();
    core.config()->save("config.json");
    onMessage(tr("已恢复出厂配置"));
}

QPixmap xingaodaApp::drawOverlay(int id, const QImage &img) const
{
    QPixmap pix = QPixmap::fromImage(img);
    QPainter p(&pix);
    CameraConfig camCfg = core.config()->camera(id);
    int y = static_cast<int>(camCfg.lineRatio * img.height());
    p.setPen(QPen(camCfg.detectLineColor, 2));
    p.drawLine(0, y, img.width(), y);
    const WidthResult &r = lastResult[id];
    if (r.valid) {
        QRect rect(r.leftX, y - 20, r.rightX - r.leftX, 40);
        p.setPen(QPen(Qt::green, 2));
        p.drawRect(rect);
        QString info = tr("宽度:%1mm (%2px)").arg(r.widthMM, 0, 'f', 1).arg(r.widthPixels, 0, 'f', 1);
        p.drawText(rect.adjusted(0, -25, 0, -5), info);
    }
    return pix;
}

