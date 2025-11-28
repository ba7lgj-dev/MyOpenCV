#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QColorDialog>
#include <QPainter>
#include <QComboBox>
#include <algorithm>

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    ui->chartView->setChart(core.trendChart()->chart());
    refreshCameraControls();
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

    connect(&core, &ApplicationCore::cameraFrame, this, &xingaodaApp::onCameraFrame);
    connect(&core, &ApplicationCore::widthUpdated, this, &xingaodaApp::onWidthUpdated);
    connect(&core, &ApplicationCore::message, this, &xingaodaApp::onMessage);
    connect(&core, &ApplicationCore::safetyModeEnabled, this, &xingaodaApp::onSafety);
    connect(&core, &ApplicationCore::availableCamerasChanged, this, &xingaodaApp::onAvailableCameras);
    connect(&core, &ApplicationCore::cameraConfigApplied, this, [this](int id, const CameraConfig &cfg){
        Q_UNUSED(cfg);
        refreshCameraControls();
        updateWidthLabel(id, lastResults[id]);
    });

    connect(ui->comboCamera0, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){ onCameraIndexChanged(0, idx); });
    connect(ui->comboCamera1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){ onCameraIndexChanged(1, idx); });
    connect(ui->editName0, &QLineEdit::editingFinished, this, &xingaodaApp::onCameraNameChanged);
    connect(ui->editName1, &QLineEdit::editingFinished, this, &xingaodaApp::onCameraNameChanged);
    connect(ui->sliderLine0, &QSlider::valueChanged, this, [this](int v){ onLineRatioChanged(0, v); });
    connect(ui->sliderLine1, &QSlider::valueChanged, this, [this](int v){ onLineRatioChanged(1, v); });
    connect(ui->btnColor0, &QPushButton::clicked, this, [this](){ onColorClicked(0); });
    connect(ui->btnColor1, &QPushButton::clicked, this, [this](){ onColorClicked(1); });
    connect(ui->comboRotation0, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){ onRotationChanged(0, idx); });
    connect(ui->comboRotation1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){ onRotationChanged(1, idx); });
    connect(ui->btnSwap, &QPushButton::clicked, this, &xingaodaApp::onSwapCameras);
    connect(ui->chkDualMode, &QCheckBox::toggled, this, &xingaodaApp::onDualModeToggled);
    connect(ui->btnRescan, &QPushButton::clicked, this, &xingaodaApp::onRescan);
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
    QImage annotated = drawOverlays(id, img);
    QPixmap pix = QPixmap::fromImage(annotated).scaled(400, 250, Qt::KeepAspectRatio);
    if (id == 0) {
        ui->labelCam0->setPixmap(pix);
    } else {
        ui->labelCam1->setPixmap(pix);
    }
}

void xingaodaApp::updateWidthLabel(int id, const WidthResult &result)
{
    QString text;
    QString status;
    if (!result.valid) {
        text = tr("Width: --");
        status = tr("Status: Idle");
    } else {
        text = QString("Width: %1 px / %2 mm").arg(result.widthPixels, 0, 'f', 1).arg(result.widthMM, 0, 'f', 1);
        status = tr("Row %1, L=%2, R=%3").arg(result.usedRow).arg(result.leftX).arg(result.rightX);
    }
    if (id == 0) {
        ui->labelWidth0->setText(text);
        ui->labelStatus0->setText(status);
    } else {
        ui->labelWidth1->setText(text);
        ui->labelStatus1->setText(status);
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
    bool isError = msg.contains("失败") || msg.contains("错误") || msg.contains("busy", Qt::CaseInsensitive);
    statusBar()->setStyleSheet(isError ? "color:red" : "");
    statusBar()->showMessage(msg, 3000);
}

void xingaodaApp::onSafety()
{
    ui->chkAutoPump->setChecked(false);
    onMessage(tr("自动加气安全模式，已关闭自动加气"));
}

void xingaodaApp::onCameraIndexChanged(int id, int index)
{
    if (index < 0) return;
    if (id == 0) {
        core.setCameraIndex(id, ui->comboCamera0->itemData(index).toInt());
    } else {
        core.setCameraIndex(id, ui->comboCamera1->itemData(index).toInt());
    }
}

void xingaodaApp::onCameraNameChanged()
{
    core.setCameraName(0, ui->editName0->text());
    core.setCameraName(1, ui->editName1->text());
}

void xingaodaApp::onLineRatioChanged(int id, int value)
{
    double ratio = value / 100.0;
    core.setLineRatio(id, ratio);
}

void xingaodaApp::onColorClicked(int id)
{
    QColor color = QColorDialog::getColor(Qt::red, this, tr("选择检测线颜色"));
    if (!color.isValid()) return;
    core.setLineColor(id, color);
    refreshCameraControls();
}

void xingaodaApp::onRotationChanged(int id, int idx)
{
    QComboBox *combo = id == 0 ? ui->comboRotation0 : ui->comboRotation1;
    int angle = combo->itemText(idx).toInt();
    core.setRotation(id, angle);
}

void xingaodaApp::onSwapCameras()
{
    core.swapCameras();
    refreshCameraControls();
}

void xingaodaApp::onDualModeToggled(bool enabled)
{
    core.setDualCameraMode(enabled);
}

void xingaodaApp::onAvailableCameras(const QVector<int> &indexes)
{
    ui->comboCamera0->blockSignals(true);
    ui->comboCamera1->blockSignals(true);
    ui->comboCamera0->clear();
    ui->comboCamera1->clear();
    for (int idx : indexes) {
        ui->comboCamera0->addItem(QString::number(idx), idx);
        ui->comboCamera1->addItem(QString::number(idx), idx);
    }
    refreshCameraControls();
    ui->comboCamera0->blockSignals(false);
    ui->comboCamera1->blockSignals(false);
}

void xingaodaApp::onRescan()
{
    core.rescanCameras();
}

void xingaodaApp::refreshCameraControls()
{
    CameraConfig c0 = core.config()->camera(0);
    CameraConfig c1 = core.config()->camera(1);
    ui->groupCam0->setTitle(c0.name.isEmpty() ? tr("Camera 0") : c0.name);
    ui->groupCam1->setTitle(c1.name.isEmpty() ? tr("Camera 1") : c1.name);
    ui->editName0->setText(c0.name);
    ui->editName1->setText(c1.name);
    ui->chkDualMode->setChecked(core.config()->config().dualCameraMode);

    auto setComboToIndex = [](QComboBox *combo, int value){
        int idx = combo->findData(value);
        if (idx >= 0) combo->setCurrentIndex(idx);
    };
    setComboToIndex(ui->comboCamera0, c0.index);
    setComboToIndex(ui->comboCamera1, c1.index);
    ui->sliderLine0->setValue(static_cast<int>(c0.lineRatio * 100));
    ui->sliderLine1->setValue(static_cast<int>(c1.lineRatio * 100));

    auto selectRotation = [](QComboBox *combo, int value){
        int idx = combo->findText(QString::number(value));
        if (idx >= 0) combo->setCurrentIndex(idx);
    };
    selectRotation(ui->comboRotation0, c0.rotation);
    selectRotation(ui->comboRotation1, c1.rotation);

    QString style0 = QString("background:%1").arg(c0.lineColor.name());
    QString style1 = QString("background:%1").arg(c1.lineColor.name());
    ui->btnColor0->setStyleSheet(style0);
    ui->btnColor1->setStyleSheet(style1);
}

QImage xingaodaApp::drawOverlays(int id, const QImage &img)
{
    QImage annotated = img.copy();
    CameraConfig cfgCam = core.config()->camera(id);
    QPainter painter(&annotated);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(cfgCam.lineColor);
    pen.setWidth(2);
    painter.setPen(pen);
    int lineY = cfgCam.lineHeightPx > 0 ? cfgCam.lineHeightPx : static_cast<int>(cfgCam.lineRatio * annotated.height());
    lineY = std::clamp(lineY, 0, annotated.height() - 1);
    painter.drawLine(0, lineY, annotated.width(), lineY);

    int regionHeight = cfgCam.widthRegionHeight > 0 ? cfgCam.widthRegionHeight : annotated.height() / 4;
    int top = std::clamp(lineY - regionHeight / 2, 0, annotated.height() - 1);
    QRect region(0, top, annotated.width(), std::min(regionHeight, annotated.height() - top));
    painter.drawRect(region);

    const WidthResult &res = lastResults[id];
    if (res.valid) {
        QPen boundaryPen(Qt::yellow);
        boundaryPen.setWidth(2);
        painter.setPen(boundaryPen);
        painter.drawLine(res.leftX, 0, res.leftX, annotated.height());
        painter.drawLine(res.rightX, 0, res.rightX, annotated.height());
        painter.drawText(QPoint(res.leftX + 5, 20), tr("左边界"));
        painter.drawText(QPoint(res.rightX - 60, 20), tr("右边界"));
    }
    return annotated;
}

