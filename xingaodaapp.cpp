#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QColorDialog>
#include <QPainter>
#include <QPen>

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    ui->chartView->setChart(core.trendChart()->chart());
    setupConnections();
    refreshCameraSelectors();
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
    connect(ui->btnRescan, &QPushButton::clicked, this, &xingaodaApp::onRescan);
    connect(ui->comboCamera0, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &xingaodaApp::onCamera0Changed);
    connect(ui->comboCamera1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &xingaodaApp::onCamera1Changed);
    connect(ui->editName0, &QLineEdit::textEdited, this, &xingaodaApp::onCamera0NameEdited);
    connect(ui->editName1, &QLineEdit::textEdited, this, &xingaodaApp::onCamera1NameEdited);
    connect(ui->sliderLine0, &QSlider::valueChanged, this, &xingaodaApp::onLineSlider0);
    connect(ui->sliderLine1, &QSlider::valueChanged, this, &xingaodaApp::onLineSlider1);
    connect(ui->sliderLineHeight0, &QSlider::valueChanged, this, &xingaodaApp::onLineHeight0);
    connect(ui->sliderLineHeight1, &QSlider::valueChanged, this, &xingaodaApp::onLineHeight1);
    connect(ui->sliderRegion0, &QSlider::valueChanged, this, &xingaodaApp::onRegionHeight0);
    connect(ui->sliderRegion1, &QSlider::valueChanged, this, &xingaodaApp::onRegionHeight1);
    connect(ui->btnColor0, &QPushButton::clicked, this, &xingaodaApp::onColor0);
    connect(ui->btnColor1, &QPushButton::clicked, this, &xingaodaApp::onColor1);
    connect(ui->comboRotation0, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &xingaodaApp::onRotation0);
    connect(ui->comboRotation1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &xingaodaApp::onRotation1);
    connect(ui->btnSwap, &QPushButton::clicked, this, &xingaodaApp::onSwapCameras);
    connect(ui->chkDualMode, &QCheckBox::toggled, this, &xingaodaApp::onDualModeToggled);

    connect(&core, &ApplicationCore::cameraFrame, this, &xingaodaApp::onCameraFrame);
    connect(&core, &ApplicationCore::widthUpdated, this, &xingaodaApp::onWidthUpdated);
    connect(&core, &ApplicationCore::message, this, &xingaodaApp::onMessage);
    connect(&core, &ApplicationCore::safetyModeEnabled, this, &xingaodaApp::onSafety);
    connect(&core, &ApplicationCore::camerasScanned, this, [this](const QVector<int> &idx){
        ui->comboCamera0->blockSignals(true);
        ui->comboCamera1->blockSignals(true);
        ui->comboCamera0->clear();
        ui->comboCamera1->clear();
        for (int i : idx) {
            ui->comboCamera0->addItem(QString::number(i), i);
            ui->comboCamera1->addItem(QString::number(i), i);
        }
        ui->comboCamera0->setCurrentText(QString::number(core.config()->camera(0).index));
        ui->comboCamera1->setCurrentText(QString::number(core.config()->camera(1).index));
        ui->comboCamera0->blockSignals(false);
        ui->comboCamera1->blockSignals(false);
    });
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
    if (id == 0) {
        paintOverlay(id, ui->labelCam0, img);
    } else {
        paintOverlay(id, ui->labelCam1, img);
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
    lastResults[id] = result;
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

void xingaodaApp::onRescan()
{
    core.rescanCameras();
    refreshCameraSelectors();
}

void xingaodaApp::refreshCameraSelectors()
{
    auto cam0 = core.config()->camera(0);
    auto cam1 = core.config()->camera(1);
    ui->editName0->setText(cam0.name);
    ui->editName1->setText(cam1.name);
    ui->comboCamera0->setCurrentText(QString::number(cam0.index));
    ui->comboCamera1->setCurrentText(QString::number(cam1.index));
    ui->sliderLine0->setValue(static_cast<int>(cam0.lineRatio * 100));
    ui->sliderLine1->setValue(static_cast<int>(cam1.lineRatio * 100));
    ui->sliderLineHeight0->setValue(cam0.lineHeightPx);
    ui->sliderLineHeight1->setValue(cam1.lineHeightPx);
    ui->sliderRegion0->setValue(cam0.widthRegionHeight);
    ui->sliderRegion1->setValue(cam1.widthRegionHeight);
    ui->comboRotation0->setCurrentText(QString::number(cam0.rotation));
    ui->comboRotation1->setCurrentText(QString::number(cam1.rotation));
    ui->chkDualMode->setChecked(core.config()->config().dualCameraMode);
}

void xingaodaApp::onCamera0Changed(int index)
{
    int camIdx = ui->comboCamera0->itemData(index).isValid() ? ui->comboCamera0->itemData(index).toInt() : ui->comboCamera0->itemText(index).toInt();
    core.updateCameraSelection(0, camIdx, ui->editName0->text());
}

void xingaodaApp::onCamera1Changed(int index)
{
    int camIdx = ui->comboCamera1->itemData(index).isValid() ? ui->comboCamera1->itemData(index).toInt() : ui->comboCamera1->itemText(index).toInt();
    core.updateCameraSelection(1, camIdx, ui->editName1->text());
}

void xingaodaApp::onCamera0NameEdited(const QString &name)
{
    core.updateCameraSelection(0, ui->comboCamera0->currentData().toInt(), name);
}

void xingaodaApp::onCamera1NameEdited(const QString &name)
{
    core.updateCameraSelection(1, ui->comboCamera1->currentData().toInt(), name);
}

void xingaodaApp::onLineSlider0(int value)
{
    core.setDetectionLineRatio(0, value / 100.0);
}

void xingaodaApp::onLineSlider1(int value)
{
    core.setDetectionLineRatio(1, value / 100.0);
}

void xingaodaApp::onLineHeight0(int value)
{
    core.setDetectionLineHeight(0, value);
}

void xingaodaApp::onLineHeight1(int value)
{
    core.setDetectionLineHeight(1, value);
}

void xingaodaApp::onRegionHeight0(int value)
{
    core.setDetectionWidthRegion(0, value);
}

void xingaodaApp::onRegionHeight1(int value)
{
    core.setDetectionWidthRegion(1, value);
}

void xingaodaApp::onColor0()
{
    QColor c = QColorDialog::getColor(Qt::red, this, tr("选择摄像头0线条颜色"));
    if (c.isValid()) core.setDetectionLineColor(0, c);
}

void xingaodaApp::onColor1()
{
    QColor c = QColorDialog::getColor(Qt::red, this, tr("选择摄像头1线条颜色"));
    if (c.isValid()) core.setDetectionLineColor(1, c);
}

void xingaodaApp::onRotation0(int index)
{
    int angle = ui->comboRotation0->itemData(index).isValid() ? ui->comboRotation0->itemData(index).toInt() : ui->comboRotation0->itemText(index).toInt();
    core.setRotation(0, angle);
}

void xingaodaApp::onRotation1(int index)
{
    int angle = ui->comboRotation1->itemData(index).isValid() ? ui->comboRotation1->itemData(index).toInt() : ui->comboRotation1->itemText(index).toInt();
    core.setRotation(1, angle);
}

void xingaodaApp::onSwapCameras()
{
    core.swapCameraOrder();
    refreshCameraSelectors();
}

void xingaodaApp::onDualModeToggled(bool checked)
{
    core.setDualCameraMode(checked);
}

void xingaodaApp::paintOverlay(int id, QLabel *label, QImage frame)
{
    auto cfgCam = core.config()->camera(id);
    QPainter p(&frame);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(cfgCam.lineColor, 2);
    p.setPen(pen);
    int y = cfgCam.lineHeightPx > 0 ? cfgCam.lineHeightPx : static_cast<int>(frame.height() * cfgCam.lineRatio);
    y = qBound(0, y, frame.height() - 1);
    p.drawLine(0, y, frame.width(), y);
    int region = cfgCam.widthRegionHeight > 0 ? cfgCam.widthRegionHeight : frame.height() / 4;
    int top = qMax(0, y - region / 2);
    p.drawRect(0, top, frame.width() - 1, region);
    WidthResult r = lastResults[id];
    if (r.valid) {
        p.setPen(QPen(Qt::yellow, 2));
        p.drawText(10, y - 10, tr("L=%1").arg(r.leftX));
        p.drawText(frame.width() - 80, y - 10, tr("R=%1").arg(r.rightX));
        p.drawLine(r.leftX, top, r.leftX, top + region);
        p.drawLine(r.rightX, top, r.rightX, top + region);
    }
    QPixmap pix = QPixmap::fromImage(frame.scaled(400, 250, Qt::KeepAspectRatio));
    label->setPixmap(pix);
}

