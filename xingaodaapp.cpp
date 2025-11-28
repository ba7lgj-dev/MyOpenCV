#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include "cameramanagerdialog.h"
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QColorDialog>
#include <QPainter>
#include <QPen>
#include <QtGlobal>

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    ui->chartView->setChart(core.trendChart()->chart());
    setupConnections();
    core.initialize();
    syncCameraUi(0);
    syncCameraUi(1);
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

    connect(ui->sliderLine0, &QSlider::valueChanged, this, &xingaodaApp::onLineChanged0);
    connect(ui->sliderLine1, &QSlider::valueChanged, this, &xingaodaApp::onLineChanged1);
    connect(ui->spinBand0, qOverload<int>(&QSpinBox::valueChanged), this, &xingaodaApp::onBandChanged0);
    connect(ui->spinBand1, qOverload<int>(&QSpinBox::valueChanged), this, &xingaodaApp::onBandChanged1);
    connect(ui->btnColor0, &QPushButton::clicked, this, &xingaodaApp::onColor0);
    connect(ui->btnColor1, &QPushButton::clicked, this, &xingaodaApp::onColor1);
    connect(ui->comboRotation0, qOverload<int>(&QComboBox::currentIndexChanged), this, &xingaodaApp::onRotation0);
    connect(ui->comboRotation1, qOverload<int>(&QComboBox::currentIndexChanged), this, &xingaodaApp::onRotation1);
    connect(ui->actionCameraManager, &QAction::triggered, this, &xingaodaApp::onCameraManager);

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

void xingaodaApp::onLineChanged0(int value)
{
    CameraConfig cfgCam = core.config()->camera(0);
    cfgCam.lineRatio = value / 100.0;
    core.config()->setCameraConfig(0, cfgCam);
}

void xingaodaApp::onLineChanged1(int value)
{
    CameraConfig cfgCam = core.config()->camera(1);
    cfgCam.lineRatio = value / 100.0;
    core.config()->setCameraConfig(1, cfgCam);
}

void xingaodaApp::onBandChanged0(int value)
{
    CameraConfig cfgCam = core.config()->camera(0);
    cfgCam.widthRegionHeight = value;
    core.config()->setCameraConfig(0, cfgCam);
}

void xingaodaApp::onBandChanged1(int value)
{
    CameraConfig cfgCam = core.config()->camera(1);
    cfgCam.widthRegionHeight = value;
    core.config()->setCameraConfig(1, cfgCam);
}

void xingaodaApp::onColor0()
{
    CameraConfig cfgCam = core.config()->camera(0);
    QColor c = QColorDialog::getColor(cfgCam.lineColor, this, tr("选择摄像头0检测线颜色"));
    if (c.isValid()) {
        cfgCam.lineColor = c;
        core.config()->setCameraConfig(0, cfgCam);
    }
}

void xingaodaApp::onColor1()
{
    CameraConfig cfgCam = core.config()->camera(1);
    QColor c = QColorDialog::getColor(cfgCam.lineColor, this, tr("选择摄像头1检测线颜色"));
    if (c.isValid()) {
        cfgCam.lineColor = c;
        core.config()->setCameraConfig(1, cfgCam);
    }
}

void xingaodaApp::onRotation0(int idx)
{
    CameraConfig cfgCam = core.config()->camera(0);
    cfgCam.rotation = ui->comboRotation0->itemData(idx).toInt();
    core.config()->setCameraConfig(0, cfgCam);
    core.reloadCamerasFromConfig();
}

void xingaodaApp::onRotation1(int idx)
{
    CameraConfig cfgCam = core.config()->camera(1);
    cfgCam.rotation = ui->comboRotation1->itemData(idx).toInt();
    core.config()->setCameraConfig(1, cfgCam);
    core.reloadCamerasFromConfig();
}

void xingaodaApp::onCameraManager()
{
    CameraManagerDialog dlg(&core, this);
    dlg.exec();
    syncCameraUi(0);
    syncCameraUi(1);
}

void xingaodaApp::onCameraFrame(int id, const QImage &img)
{
    QImage overlay = drawOverlay(id, img);
    QPixmap pix = QPixmap::fromImage(overlay).scaled(400, 250, Qt::KeepAspectRatio);
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
    lastWidth[id] = result;
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

void xingaodaApp::syncCameraUi(int id)
{
    CameraConfig cfgCam = core.config()->camera(id);
    if (id == 0) {
        ui->sliderLine0->setValue(static_cast<int>(cfgCam.lineRatio * 100));
        ui->spinBand0->setValue(cfgCam.widthRegionHeight);
        ui->comboRotation0->setCurrentIndex(ui->comboRotation0->findData(cfgCam.rotation));
        ui->groupCam0->setTitle(cfgCam.name.isEmpty() ? tr("Camera 0") : cfgCam.name);
    } else {
        ui->sliderLine1->setValue(static_cast<int>(cfgCam.lineRatio * 100));
        ui->spinBand1->setValue(cfgCam.widthRegionHeight);
        ui->comboRotation1->setCurrentIndex(ui->comboRotation1->findData(cfgCam.rotation));
        ui->groupCam1->setTitle(cfgCam.name.isEmpty() ? tr("Camera 1") : cfgCam.name);
    }
}

QImage xingaodaApp::drawOverlay(int id, const QImage &src)
{
    QImage image = src.copy();
    CameraConfig cfgCam = core.config()->camera(id);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(cfgCam.lineColor, 2));

    double lineY = cfgCam.lineRatio * image.height();
    painter.drawLine(0, lineY, image.width(), lineY);
    int band = cfgCam.widthRegionHeight > 0 ? cfgCam.widthRegionHeight : image.height() / 10;
    QRect bandRect(0, static_cast<int>(lineY - band / 2), image.width(), band);
    painter.drawRect(bandRect);

    if (lastWidth[id].valid) {
        painter.setPen(QPen(Qt::green, 2));
        painter.drawLine(lastWidth[id].leftX, 0, lastWidth[id].leftX, image.height());
        painter.drawLine(lastWidth[id].rightX, 0, lastWidth[id].rightX, image.height());
        painter.drawText(QPointF(lastWidth[id].leftX + 2, 20), tr("左边界"));
        painter.drawText(QPointF(lastWidth[id].rightX - 50, 20), tr("右边界"));
    }
    return image;
}

