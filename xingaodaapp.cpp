#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include <QComboBox>
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QColorDialog>
#include <QPainter>
#include <QPen>
#include <QtGlobal>
#include <QList>
#include <QDoubleSpinBox>
#include <QSerialPortInfo>
#include <QGroupBox>
#include <QPoint>
#include <cmath>

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    setupRotationCombos();
    ui->chartView->setChart(core.trendChart()->chart());
    setupConnections();
    core.initialize();
    populateCameraIndexes();
    syncCameraUi(0);
    syncCameraUi(1);
    syncPumpUi();
    syncPushUi();
    const QString fusion = core.config()->config().fusionStrategy;
    int fusionIdx = 0;
    if (fusion == QLatin1String("min")) fusionIdx = 1;
    else if (fusion == QLatin1String("max")) fusionIdx = 2;
    ui->comboFusionStrategy->setCurrentIndex(fusionIdx);
    updateFusionLabels(core.fusedWidthMM() / 10.0);

    pushStatusLabel = new QLabel(tr("推送未开启"), this);
    statusBar()->addPermanentWidget(pushStatusLabel);
    if (core.pushManager()) {
        connect(core.pushManager(), &PushManager::statusUpdated, this, &xingaodaApp::onPushStatusChanged);
        connect(core.pushManager(), &PushManager::consecutiveFailuresExceeded, this, &xingaodaApp::onPushFailureAlarm);
        core.pushManager()->sendStartup();
    }
    onPushStatusChanged(0);
}

xingaodaApp::~xingaodaApp()
{
    if (core.pushManager()) {
        core.pushManager()->sendShutdown();
    }
    core.stopCameras();
    delete ui;
}

void xingaodaApp::setupConnections()
{
    connect(ui->btnStart, &QPushButton::clicked, this, &xingaodaApp::onStart);
    connect(ui->btnStop, &QPushButton::clicked, this, &xingaodaApp::onStop);
    connect(ui->btnCalibrateAll, &QPushButton::clicked, this, &xingaodaApp::onCalibrateAll);
    connect(ui->btnAutoExp0, &QPushButton::clicked, this, &xingaodaApp::onAutoExp0);
    connect(ui->btnAutoExp1, &QPushButton::clicked, this, &xingaodaApp::onAutoExp1);
    connect(ui->chkAutoPump, &QCheckBox::toggled, &core, &ApplicationCore::setAutoPumpEnabled);

    connect(ui->sliderLine0, &QSlider::valueChanged, this, &xingaodaApp::onLineChanged0);
    connect(ui->sliderLine1, &QSlider::valueChanged, this, &xingaodaApp::onLineChanged1);
    connect(ui->spinLine0, qOverload<int>(&QSpinBox::valueChanged), this, &xingaodaApp::onLineSpinChanged0);
    connect(ui->spinLine1, qOverload<int>(&QSpinBox::valueChanged), this, &xingaodaApp::onLineSpinChanged1);
    connect(ui->spinBand0, qOverload<int>(&QSpinBox::valueChanged), this, &xingaodaApp::onBandChanged0);
    connect(ui->spinBand1, qOverload<int>(&QSpinBox::valueChanged), this, &xingaodaApp::onBandChanged1);
    connect(ui->btnColor0, &QPushButton::clicked, this, &xingaodaApp::onColor0);
    connect(ui->btnColor1, &QPushButton::clicked, this, &xingaodaApp::onColor1);
    connect(ui->comboRotation0, qOverload<int>(&QComboBox::currentIndexChanged), this, &xingaodaApp::onRotation0);
    connect(ui->comboRotation1, qOverload<int>(&QComboBox::currentIndexChanged), this, &xingaodaApp::onRotation1);
    connect(ui->comboCamera0, qOverload<int>(&QComboBox::currentIndexChanged), this, &xingaodaApp::onCameraIndex0);
    connect(ui->comboCamera1, qOverload<int>(&QComboBox::currentIndexChanged), this, &xingaodaApp::onCameraIndex1);
    connect(ui->chkFlipH0, &QCheckBox::toggled, this, &xingaodaApp::onFlipH0);
    connect(ui->chkFlipV0, &QCheckBox::toggled, this, &xingaodaApp::onFlipV0);
    connect(ui->chkFlipH1, &QCheckBox::toggled, this, &xingaodaApp::onFlipH1);
    connect(ui->chkFlipV1, &QCheckBox::toggled, this, &xingaodaApp::onFlipV1);
    connect(ui->comboFusionStrategy, qOverload<int>(&QComboBox::currentIndexChanged), this, &xingaodaApp::onFusionStrategyChanged);

    connect(ui->spinPumpThreshold, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &xingaodaApp::onPumpThresholdChanged);
    connect(ui->spinPumpStopThreshold, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &xingaodaApp::onPumpStopThresholdChanged);
    connect(ui->spinPumpDuration, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &xingaodaApp::onPumpDurationChanged);
    connect(ui->spinPrecheck, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &xingaodaApp::onPumpPrecheckChanged);
    connect(ui->spinMonitor, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &xingaodaApp::onPumpMonitorChanged);
    connect(ui->spinCooldown, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &xingaodaApp::onPumpCooldownChanged);
    connect(ui->spinMinInflation, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &xingaodaApp::onPumpMinInflationChanged);
    connect(ui->comboPumpPort, &QComboBox::currentTextChanged, this, &xingaodaApp::onPumpPortChanged);

    connect(ui->chkPushEnabled, &QCheckBox::toggled, this, &xingaodaApp::onPushEnabledChanged);
    connect(ui->editWebhook, &QLineEdit::textChanged, this, &xingaodaApp::onPushUrlEdited);
    connect(ui->btnTestPush, &QPushButton::clicked, this, &xingaodaApp::onTestPush);

    connect(&core, &ApplicationCore::cameraFrame, this, &xingaodaApp::onCameraFrame);
    connect(&core, &ApplicationCore::widthUpdated, this, &xingaodaApp::onWidthUpdated);
    connect(&core, &ApplicationCore::fusedWidthUpdated, this, &xingaodaApp::onFusionUpdated);
    connect(&core, &ApplicationCore::message, this, &xingaodaApp::onMessage);
    connect(&core, &ApplicationCore::safetyModeEnabled, this, &xingaodaApp::onSafety);
}

void xingaodaApp::setupRotationCombos()
{
    auto initCombo = [](QComboBox *combo) {
        const QList<int> rotations {0, 90, 180, 270};
        combo->clear();
        for (int deg : rotations) {
            combo->addItem(QString::number(deg) + QLatin1String("°"), deg);
        }
    };

    initCombo(ui->comboRotation0);
    initCombo(ui->comboRotation1);
}

void xingaodaApp::onStart()
{
    core.startCameras();
    onMessage(tr("Cameras started"));
}

void xingaodaApp::onStop()
{
    core.stopCameras();
    cameraOnline[0] = false;
    cameraOnline[1] = false;
    onMessage(tr("Cameras stopped"));
}

void xingaodaApp::onCalibrateAll()
{
    QString error;
    const double realWidthMm = ui->spinRealWidth->value() * 10.0;
    if (!core.calibrateAllCameras(realWidthMm, error)) {
        onMessage(error);
    } else {
        onMessage(tr("宽度校准完成"));
    }
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
    ui->spinLine0->blockSignals(true);
    ui->spinLine0->setValue(value);
    ui->spinLine0->blockSignals(false);
    applyLineRatio(0, value);
}

void xingaodaApp::onLineChanged1(int value)
{
    ui->spinLine1->blockSignals(true);
    ui->spinLine1->setValue(value);
    ui->spinLine1->blockSignals(false);
    applyLineRatio(1, value);
}

void xingaodaApp::onLineSpinChanged0(int value)
{
    ui->sliderLine0->blockSignals(true);
    ui->sliderLine0->setValue(value);
    ui->sliderLine0->blockSignals(false);
    applyLineRatio(0, value);
}

void xingaodaApp::onLineSpinChanged1(int value)
{
    ui->sliderLine1->blockSignals(true);
    ui->sliderLine1->setValue(value);
    ui->sliderLine1->blockSignals(false);
    applyLineRatio(1, value);
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

void xingaodaApp::onCameraIndex0(int idx)
{
    const int index = ui->comboCamera0->itemData(idx).toInt();
    const bool enabled = index >= 0;
    if (!core.applyCameraSelection(0, index, enabled)) {
        onMessage(tr("摄像头打开失败，请选择其他接口"));
    }
    cameraOnline[0] = false;
    syncCameraUi(0);
}

void xingaodaApp::onCameraIndex1(int idx)
{
    const int index = ui->comboCamera1->itemData(idx).toInt();
    const bool enabled = index >= 0;
    if (!core.applyCameraSelection(1, index, enabled)) {
        onMessage(tr("摄像头打开失败，请选择其他接口"));
    }
    cameraOnline[1] = false;
    syncCameraUi(1);
}

void xingaodaApp::onFlipH0(bool checked)
{
    CameraConfig cfgCam = core.config()->camera(0);
    cfgCam.flipHorizontal = checked;
    core.config()->setCameraConfig(0, cfgCam);
    core.reloadCamerasFromConfig();
}

void xingaodaApp::onFlipV0(bool checked)
{
    CameraConfig cfgCam = core.config()->camera(0);
    cfgCam.flipVertical = checked;
    core.config()->setCameraConfig(0, cfgCam);
    core.reloadCamerasFromConfig();
}

void xingaodaApp::onFlipH1(bool checked)
{
    CameraConfig cfgCam = core.config()->camera(1);
    cfgCam.flipHorizontal = checked;
    core.config()->setCameraConfig(1, cfgCam);
    core.reloadCamerasFromConfig();
}

void xingaodaApp::onFlipV1(bool checked)
{
    CameraConfig cfgCam = core.config()->camera(1);
    cfgCam.flipVertical = checked;
    core.config()->setCameraConfig(1, cfgCam);
    core.reloadCamerasFromConfig();
}

void xingaodaApp::onFusionStrategyChanged(int idx)
{
    const QString text = ui->comboFusionStrategy->itemText(idx);
    QString strategy = QLatin1String("average");
    if (text.contains(tr("小"))) {
        strategy = QLatin1String("min");
    } else if (text.contains(tr("大"))) {
        strategy = QLatin1String("max");
    }
    core.config()->setFusionStrategy(strategy);
    updateFusionLabels(core.fusedWidthMM() / 10.0);
}

void xingaodaApp::onCameraFrame(int id, const QImage &img)
{
    cameraOnline[id] = true;
    QImage overlay = drawOverlay(id, img);
    QLabel *targetLabel = id == 0 ? ui->labelCam0 : ui->labelCam1;
    QPixmap pix = QPixmap::fromImage(overlay).scaled(targetLabel->size(), Qt::KeepAspectRatio);
    targetLabel->setPixmap(pix);
}

void xingaodaApp::onWidthUpdated(int id, const WidthResult &result)
{
    Q_UNUSED(id)
    lastWidth[id] = result;
    updateFusionLabels(core.fusedWidthMM() / 10.0);
}

void xingaodaApp::onFusionUpdated(double fusedMm, double cam0Mm, double cam1Mm)
{
    ui->labelCam0WidthValue->setText(cam0Mm > 0 ? QString::number(cam0Mm / 10.0, 'f', 2) + tr(" cm") : QStringLiteral("--"));
    ui->labelCam1WidthValue->setText(cam1Mm > 0 ? QString::number(cam1Mm / 10.0, 'f', 2) + tr(" cm") : QStringLiteral("--"));
    updateFusionLabels(fusedMm / 10.0);
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

void xingaodaApp::onPumpThresholdChanged(double value)
{
    core.config()->setAutoStartThresholdMM(value * 10.0);
    core.reloadPumpConfig();
}

void xingaodaApp::onPumpStopThresholdChanged(double value)
{
    core.config()->setAutoStopThresholdMM(value * 10.0);
    core.reloadPumpConfig();
}

void xingaodaApp::onPumpDurationChanged(double value)
{
    core.config()->setPumpDurationMs(static_cast<int>(std::lround(value * 1000.0)));
    core.reloadPumpConfig();
}

void xingaodaApp::onPumpPrecheckChanged(double value)
{
    core.config()->setAutoPrecheckMs(static_cast<int>(std::lround(value * 1000.0)));
    core.reloadPumpConfig();
}

void xingaodaApp::onPumpMonitorChanged(double value)
{
    core.config()->setAutoMonitorMs(static_cast<int>(std::lround(value * 1000.0)));
    core.reloadPumpConfig();
}

void xingaodaApp::onPumpCooldownChanged(double value)
{
    core.config()->setAutoCooldownMs(static_cast<int>(std::lround(value * 1000.0)));
    core.reloadPumpConfig();
}

void xingaodaApp::onPumpMinInflationChanged(double value)
{
    core.config()->setMinInflationMM(value * 10.0);
    core.reloadPumpConfig();
}

void xingaodaApp::onPumpPortChanged(const QString &port)
{
    core.config()->setPumpPort(port);
    core.reloadPumpConfig();
}

void xingaodaApp::onPushStatusChanged(int failures)
{
    if (!pushStatusLabel) return;
    const auto cfgPush = core.config()->pushConfig();
    if (!cfgPush.enabled || cfgPush.url.isEmpty()) {
        const QString text = tr("推送未开启");
        pushStatusLabel->setText(text);
        ui->labelPushStatus->setText(text);
        pushStatusLabel->setStyleSheet(QLatin1String(""));
        return;
    }
    if (failures > 0) {
        const QString text = tr("推送失败%1次").arg(failures);
        pushStatusLabel->setText(text);
        ui->labelPushStatus->setText(text);
        pushStatusLabel->setStyleSheet(QLatin1String("color: orange;"));
    } else {
        const QString text = tr("推送通道正常");
        pushStatusLabel->setText(text);
        ui->labelPushStatus->setText(text);
        pushStatusLabel->setStyleSheet(QLatin1String("color: green;"));
    }
}

void xingaodaApp::onPushFailureAlarm(int failures)
{
    if (!pushStatusLabel) return;
    pushStatusLabel->setText(tr("推送连续失败%1次").arg(failures));
    pushStatusLabel->setStyleSheet(QLatin1String("color: red;"));
    onMessage(tr("推送连续失败，已超过阈值"));
}

void xingaodaApp::applyLineRatio(int id, int value)
{
    CameraConfig cfgCam = core.config()->camera(id);
    cfgCam.lineRatio = value / 100.0;
    core.config()->setCameraConfig(id, cfgCam);
}

void xingaodaApp::populateCameraIndexes()
{
    const QList<int> indices = core.availableCameraIndices();
    auto fillCombo = [&](QComboBox *combo) {
        combo->blockSignals(true);
        combo->clear();
        combo->addItem(tr("禁用"), -1);
        for (int idx : indices) {
            combo->addItem(tr("USB %1").arg(idx), idx);
        }
        combo->blockSignals(false);
    };
    fillCombo(ui->comboCamera0);
    fillCombo(ui->comboCamera1);
}

void xingaodaApp::populatePumpPorts()
{
    ui->comboPumpPort->blockSignals(true);
    ui->comboPumpPort->clear();
    ui->comboPumpPort->addItem(QString());
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &p : ports) {
        ui->comboPumpPort->addItem(p.portName());
    }
    ui->comboPumpPort->blockSignals(false);
}

void xingaodaApp::syncPumpUi()
{
    const auto cfgApp = core.config()->config();
    ui->chkAutoPump->setChecked(cfgApp.autoPumpEnabled);
    ui->spinPumpThreshold->setValue(cfgApp.autoStartThresholdMM / 10.0);
    ui->spinPumpStopThreshold->setValue(cfgApp.autoStopThresholdMM / 10.0);
    ui->spinPumpDuration->setValue(cfgApp.pumpDurationMs / 1000.0);
    ui->spinPrecheck->setValue(cfgApp.autoPrecheckMs / 1000.0);
    ui->spinMonitor->setValue(cfgApp.autoMonitorMs / 1000.0);
    ui->spinCooldown->setValue(cfgApp.autoCooldownMs / 1000.0);
    ui->spinMinInflation->setValue(cfgApp.autoMinInflationMM / 10.0);
    populatePumpPorts();
    int idx = ui->comboPumpPort->findText(cfgApp.pumpPort);
    if (idx < 0 && !cfgApp.pumpPort.isEmpty()) {
        ui->comboPumpPort->addItem(cfgApp.pumpPort);
        idx = ui->comboPumpPort->count() - 1;
    }
    if (idx >= 0) {
        ui->comboPumpPort->setCurrentIndex(idx);
    }
}

void xingaodaApp::syncPushUi()
{
    const auto cfgPush = core.config()->pushConfig();
    ui->chkPushEnabled->blockSignals(true);
    ui->editWebhook->blockSignals(true);
    ui->chkPushEnabled->setChecked(cfgPush.enabled);
    ui->editWebhook->setText(cfgPush.url);
    ui->chkPushEnabled->blockSignals(false);
    ui->editWebhook->blockSignals(false);
    onPushStatusChanged(0);
}

void xingaodaApp::updateFusionLabels(double fusedCm)
{
    if (fusedCm > 0) {
        ui->labelFusionValue->setText(tr("融合宽度 %1 cm").arg(fusedCm, 0, 'f', 2));
    } else {
        ui->labelFusionValue->setText(tr("融合宽度 --"));
    }
}


void xingaodaApp::onPushEnabledChanged(bool enabled)
{
    PushConfig cfgPush = core.config()->pushConfig();
    cfgPush.enabled = enabled;
    core.config()->setPushConfig(cfgPush);
    if (core.pushManager()) {
        core.pushManager()->reloadConfig();
    }
    onPushStatusChanged(0);
}

void xingaodaApp::onPushUrlEdited(const QString &text)
{
    PushConfig cfgPush = core.config()->pushConfig();
    cfgPush.url = text;
    core.config()->setPushConfig(cfgPush);
    if (core.pushManager()) {
        core.pushManager()->reloadConfig();
    }
    onPushStatusChanged(0);
}

void xingaodaApp::onTestPush()
{
    if (core.pushManager()) {
        core.pushManager()->sendCustomMessage(tr("推送通道测试"));
    }
}

void xingaodaApp::syncCameraUi(int id)
{
    CameraConfig cfgCam = core.config()->camera(id);
    QSlider *slider = id == 0 ? ui->sliderLine0 : ui->sliderLine1;
    QSpinBox *spinLine = id == 0 ? ui->spinLine0 : ui->spinLine1;
    QSpinBox *spinBand = id == 0 ? ui->spinBand0 : ui->spinBand1;
    QComboBox *comboRotation = id == 0 ? ui->comboRotation0 : ui->comboRotation1;
    QComboBox *comboCamera = id == 0 ? ui->comboCamera0 : ui->comboCamera1;
    QCheckBox *flipH = id == 0 ? ui->chkFlipH0 : ui->chkFlipH1;
    QCheckBox *flipV = id == 0 ? ui->chkFlipV0 : ui->chkFlipV1;
    QGroupBox *group = id == 0 ? ui->groupCam0 : ui->groupCam1;

    slider->blockSignals(true);
    spinLine->blockSignals(true);
    slider->setValue(static_cast<int>(cfgCam.lineRatio * 100));
    spinLine->setValue(static_cast<int>(cfgCam.lineRatio * 100));
    slider->blockSignals(false);
    spinLine->blockSignals(false);

    spinBand->setValue(cfgCam.widthRegionHeight);
    comboRotation->setCurrentIndex(comboRotation->findData(cfgCam.rotation));
    int camIdx = comboCamera->findData(cfgCam.enabled ? cfgCam.index : -1);
    if (camIdx < 0 && cfgCam.enabled) {
        comboCamera->addItem(tr("USB %1").arg(cfgCam.index), cfgCam.index);
        camIdx = comboCamera->count() - 1;
    }
    if (camIdx >= 0) {
        comboCamera->setCurrentIndex(camIdx);
    }
    flipH->setChecked(cfgCam.flipHorizontal);
    flipV->setChecked(cfgCam.flipVertical);
    group->setTitle(cfgCam.name.isEmpty() ? (id == 0 ? tr("摄像头0") : tr("摄像头1")) : cfgCam.name);
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
    }

    painter.setPen(Qt::white);
    QColor bg(0, 0, 0, 170);
    QFont baseFont = painter.font();
    QFont bigFont = baseFont;
    bigFont.setPointSizeF(baseFont.pointSizeF() * 1.6);
    QFontMetrics fm(baseFont);
    QFontMetrics fmBig(bigFont);

    QString pixelText = lastWidth[id].valid ? tr("像素宽度: %1 px").arg(lastWidth[id].widthPixels, 0, 'f', 1) : tr("像素宽度: --");
    const double widthCm = lastWidth[id].valid ? lastWidth[id].widthMM / 10.0 : 0.0;
    QString realText = lastWidth[id].valid ? tr("真实宽度: %1 cm").arg(widthCm, 0, 'f', 2) : tr("真实宽度: --");
    QString bandText = tr("检测线高度: %1 px").arg(band);

    int infoWidth = qMax(qMax(fm.horizontalAdvance(pixelText), fm.horizontalAdvance(bandText)), fmBig.horizontalAdvance(realText)) + 16;
    int infoHeight = fm.height() * 2 + fmBig.height() + 20;
    QRect infoRect(image.width() - infoWidth - 12, image.height() - infoHeight - 12, infoWidth, infoHeight);
    painter.fillRect(infoRect, bg);

    int textY = infoRect.top() + 8 + fm.ascent();
    painter.setFont(baseFont);
    painter.drawText(infoRect.left() + 8, textY, pixelText);
    textY += fm.height();
    painter.drawText(infoRect.left() + 8, textY, bandText);
    textY += fm.height();
    painter.setFont(bigFont);
    painter.drawText(infoRect.left() + 8, textY + fmBig.ascent(), realText);

    QStringList tags;
    tags << (cfgCam.enabled && cameraOnline[id] ? tr("在线") : tr("离线"));
    if (cfgCam.flipHorizontal) tags << tr("水平翻转");
    if (cfgCam.flipVertical) tags << tr("垂直翻转");
    tags << tr("检测线%1%").arg(static_cast<int>(cfgCam.lineRatio * 100));
    int y = 8;
    for (const QString &tag : tags) {
        int w = fm.horizontalAdvance(tag) + 14;
        int h = fm.height() + 6;
        QRect r(8, y, w, h);
        painter.fillRect(r, bg);
        painter.setFont(baseFont);
        painter.drawText(r.adjusted(6, 4, -6, -4).bottomLeft() - QPoint(0, fm.descent()), tag);
        y += h + 4;
    }

    return image;
}

