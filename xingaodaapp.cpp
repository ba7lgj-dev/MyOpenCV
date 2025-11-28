#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QColorDialog>
#include <QPainter>

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    ui->chartView->setChart(core.trendChart()->chart());
    setupConnections();
    core.initialize();
    loadConfigToUi();
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
    connect(ui->chkDualMode, &QCheckBox::toggled, this, &xingaodaApp::onDualModeToggled);
    connect(ui->btnSwapCamera, &QPushButton::clicked, this, &xingaodaApp::onSwapCameras);
    connect(ui->comboCamera0, &QComboBox::currentIndexChanged, this, &xingaodaApp::onCameraSelectionChanged);
    connect(ui->comboCamera1, &QComboBox::currentIndexChanged, this, &xingaodaApp::onCameraSelectionChanged);
    connect(ui->editCamera0, &QLineEdit::editingFinished, this, &xingaodaApp::onCameraNameEdited);
    connect(ui->editCamera1, &QLineEdit::editingFinished, this, &xingaodaApp::onCameraNameEdited);
    connect(ui->comboRotation0, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &xingaodaApp::onRotationChanged);
    connect(ui->comboRotation1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &xingaodaApp::onRotationChanged);
    connect(ui->sliderLine0, &QSlider::valueChanged, this, &xingaodaApp::onLineRatioChanged);
    connect(ui->sliderLine1, &QSlider::valueChanged, this, &xingaodaApp::onLineRatioChanged);
    connect(ui->sliderHeight0, &QSlider::valueChanged, this, &xingaodaApp::onLineHeightChanged);
    connect(ui->sliderHeight1, &QSlider::valueChanged, this, &xingaodaApp::onLineHeightChanged);
    connect(ui->sliderRegion0, &QSlider::valueChanged, this, &xingaodaApp::onWidthRegionChanged);
    connect(ui->sliderRegion1, &QSlider::valueChanged, this, &xingaodaApp::onWidthRegionChanged);
    connect(ui->btnColor0, &QPushButton::clicked, this, &xingaodaApp::onLineColorClicked);
    connect(ui->btnColor1, &QPushButton::clicked, this, &xingaodaApp::onLineColorClicked);

    connect(&core, &ApplicationCore::cameraFrame, this, &xingaodaApp::onCameraFrame);
    connect(&core, &ApplicationCore::widthUpdated, this, &xingaodaApp::onWidthUpdated);
    connect(&core, &ApplicationCore::message, this, &xingaodaApp::onMessage);
    connect(&core, &ApplicationCore::safetyModeEnabled, this, &xingaodaApp::onSafety);
    connect(&core, &ApplicationCore::availableCamerasChanged, this, &xingaodaApp::onAvailableCameras);
    connect(&core, &ApplicationCore::cameraError, this, &xingaodaApp::onCameraError);
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
    QImage painted = drawOverlay(id, img);
    QPixmap pix = QPixmap::fromImage(painted).scaled(400, 250, Qt::KeepAspectRatio);
    if (id == 0) {
        ui->labelCam0->setPixmap(pix);
        ui->labelStatus0->setStyleSheet("");
    } else {
        ui->labelCam1->setPixmap(pix);
        ui->labelStatus1->setStyleSheet("");
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

void xingaodaApp::onCameraError(int id, const QString &msg)
{
    QLabel *label = id == 0 ? ui->labelStatus0 : ui->labelStatus1;
    label->setText(msg);
    label->setStyleSheet("color:red");
}

void xingaodaApp::onAvailableCameras(const QVector<int> &indexes)
{
    auto cfg = core.config();
    const int current0 = cfg->camera(0).index;
    const int current1 = cfg->camera(1).index;
    auto updateCombo = [&](QComboBox *combo, int current){
        combo->blockSignals(true);
        combo->clear();
        for (int idx : indexes) {
            combo->addItem(QString::number(idx), idx);
        }
        int pos = combo->findData(current);
        if (pos >= 0) combo->setCurrentIndex(pos);
        combo->blockSignals(false);
    };
    updateCombo(ui->comboCamera0, current0);
    updateCombo(ui->comboCamera1, current1);
}

void xingaodaApp::onCameraNameEdited()
{
    int camId = senderCameraId(sender());
    if (camId < 0) return;
    QLineEdit *edit = camId == 0 ? ui->editCamera0 : ui->editCamera1;
    core.setCameraName(camId, edit->text());
}

void xingaodaApp::onCameraSelectionChanged()
{
    int camId = senderCameraId(sender());
    if (camId < 0) return;
    QComboBox *combo = camId == 0 ? ui->comboCamera0 : ui->comboCamera1;
    int index = combo->currentData().toInt();
    core.setCameraIndex(camId, index);
}

void xingaodaApp::onRotationChanged(int index)
{
    int camId = senderCameraId(sender());
    if (camId < 0) return;
    QComboBox *combo = camId == 0 ? ui->comboRotation0 : ui->comboRotation1;
    int deg = combo->itemData(index).toInt();
    core.setCameraRotation(camId, deg);
}

void xingaodaApp::onLineRatioChanged(int value)
{
    int camId = senderCameraId(sender());
    if (camId < 0) return;
    QLabel *lbl = camId == 0 ? ui->labelLinePos0 : ui->labelLinePos1;
    double ratio = value / 100.0;
    lbl->setText(tr("检测线: %1%").arg(value));
    core.setLineRatio(camId, ratio);
}

void xingaodaApp::onLineHeightChanged(int value)
{
    int camId = senderCameraId(sender());
    if (camId < 0) return;
    QLabel *lbl = camId == 0 ? ui->labelLineHeight0 : ui->labelLineHeight1;
    lbl->setText(tr("像素高度: %1").arg(value));
    core.setLineHeightPx(camId, value);
}

void xingaodaApp::onWidthRegionChanged(int value)
{
    int camId = senderCameraId(sender());
    if (camId < 0) return;
    QLabel *lbl = camId == 0 ? ui->labelRegion0 : ui->labelRegion1;
    lbl->setText(tr("检测区: %1px").arg(value));
    core.setWidthRegionHeight(camId, value);
}

void xingaodaApp::onLineColorClicked()
{
    int camId = senderCameraId(sender());
    if (camId < 0) return;
    QColor current = core.config()->camera(camId).lineColor;
    QColor chosen = QColorDialog::getColor(current, this, tr("选择检测线颜色"));
    if (!chosen.isValid()) return;
    core.setLineColor(camId, chosen);
    QPushButton *btn = camId == 0 ? ui->btnColor0 : ui->btnColor1;
    btn->setStyleSheet(QString("background:%1").arg(chosen.name()));
}

void xingaodaApp::onDualModeToggled(bool enabled)
{
    core.setDualCameraMode(enabled);
    ui->groupCam1->setVisible(enabled);
}

void xingaodaApp::onSwapCameras()
{
    core.swapCameras();
    loadConfigToUi();
}

void xingaodaApp::loadConfigToUi()
{
    auto cfg = core.config();
    auto appCfg = cfg->config();
    ui->chkDualMode->setChecked(appCfg.dualCameraMode);
    ui->groupCam1->setVisible(appCfg.dualCameraMode);
    ui->editCamera0->setText(appCfg.cameras[0].name);
    ui->editCamera1->setText(appCfg.cameras[1].name);
    ui->chkAutoPump->setChecked(appCfg.autoPumpEnabled);

    auto setRotation = [&](QComboBox *combo, int rot){
        int pos = combo->findData(rot);
        if (pos >= 0) combo->setCurrentIndex(pos);
    };
    setRotation(ui->comboRotation0, appCfg.cameras[0].rotation);
    setRotation(ui->comboRotation1, appCfg.cameras[1].rotation);

    ui->sliderLine0->setValue(static_cast<int>(appCfg.cameras[0].lineRatio * 100));
    ui->sliderLine1->setValue(static_cast<int>(appCfg.cameras[1].lineRatio * 100));
    ui->sliderHeight0->setValue(appCfg.cameras[0].lineHeightPx);
    ui->sliderHeight1->setValue(appCfg.cameras[1].lineHeightPx);
    ui->sliderRegion0->setValue(appCfg.cameras[0].widthRegionHeight);
    ui->sliderRegion1->setValue(appCfg.cameras[1].widthRegionHeight);
    ui->labelLinePos0->setText(tr("检测线: %1%").arg(ui->sliderLine0->value()));
    ui->labelLinePos1->setText(tr("检测线: %1%").arg(ui->sliderLine1->value()));
    ui->labelLineHeight0->setText(tr("像素高度: %1").arg(ui->sliderHeight0->value()));
    ui->labelLineHeight1->setText(tr("像素高度: %1").arg(ui->sliderHeight1->value()));
    ui->labelRegion0->setText(tr("检测区: %1px").arg(ui->sliderRegion0->value()));
    ui->labelRegion1->setText(tr("检测区: %1px").arg(ui->sliderRegion1->value()));
    ui->btnColor0->setStyleSheet(QString("background:%1").arg(appCfg.cameras[0].lineColor.name()));
    ui->btnColor1->setStyleSheet(QString("background:%1").arg(appCfg.cameras[1].lineColor.name()));
    onAvailableCameras(core.availableCameraIndexes());
}

QImage xingaodaApp::drawOverlay(int id, const QImage &img) const
{
    QImage out = img.copy();
    QPainter p(&out);
    CameraConfig cfg = core.config()->camera(id);
    int lineY = cfg.lineHeightPx > 0 ? cfg.lineHeightPx : static_cast<int>(cfg.lineRatio * out.height());
    lineY = std::min(out.height() - 1, std::max(0, lineY));
    int regionH = cfg.widthRegionHeight > 0 ? cfg.widthRegionHeight : out.height() / 2;
    int regionY = std::max(0, lineY - regionH / 2);
    regionH = std::min(regionH, out.height() - regionY - 1);
    p.setPen(QPen(cfg.lineColor, 2));
    p.drawLine(0, lineY, out.width(), lineY);
    p.drawRect(0, regionY, out.width() - 1, regionH);
    const WidthResult &wr = lastResults[id];
    if (wr.valid) {
        p.setPen(QPen(Qt::green, 2));
        int left = std::max(0, std::min(out.width()-1, wr.leftX));
        int right = std::max(0, std::min(out.width()-1, wr.rightX));
        p.drawLine(left, regionY, left, regionY + regionH);
        p.drawLine(right, regionY, right, regionY + regionH);
        p.drawText(left, regionY + 15, tr("左边界"));
        p.drawText(right - 40, regionY + 15, tr("右边界"));
    }
    return out;
}

int xingaodaApp::senderCameraId(QObject *senderObj) const
{
    if (!senderObj) return -1;
    if (senderObj == ui->comboCamera0 || senderObj == ui->editCamera0 || senderObj == ui->comboRotation0
        || senderObj == ui->sliderLine0 || senderObj == ui->btnColor0 || senderObj == ui->sliderHeight0
        || senderObj == ui->sliderRegion0) {
        return 0;
    }
    if (senderObj == ui->comboCamera1 || senderObj == ui->editCamera1 || senderObj == ui->comboRotation1
        || senderObj == ui->sliderLine1 || senderObj == ui->btnColor1 || senderObj == ui->sliderHeight1
        || senderObj == ui->sliderRegion1) {
        return 1;
    }
    return -1;
}

