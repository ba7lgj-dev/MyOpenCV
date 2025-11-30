#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QPainter>
#include <QPen>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QtGlobal>
#include <QFont>
#include <QPoint>
#include <QStringList>

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    ui->chartView->setChart(core.trendChart()->chart());
    createMenus();
    setupConnections();
    core.initialize();
    ui->spinRealWidth->setValue(lastCalibrationWidthCm);
    ui->spinAutoThreshold->setValue(core.config()->config().autoStartThresholdMM / 10.0);
    updateFusionLabels(core.fusedWidthMM() / 10.0);
    updateLineSliders();

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

void xingaodaApp::createMenus()
{
    auto menuSystem = menuBar()->addMenu(tr("系统设置"));
    actionStart = menuSystem->addAction(tr("启动采集"), this, &xingaodaApp::onStart);
    actionStop = menuSystem->addAction(tr("停止采集"), this, &xingaodaApp::onStop);
    menuSystem->addSeparator();
    menuSystem->addAction(tr("重新加载配置"), this, &xingaodaApp::reloadConfig);
    menuSystem->addAction(tr("保存配置"), this, &xingaodaApp::saveConfig);

    auto menuCamera = menuBar()->addMenu(tr("摄像头管理"));
    menuCamera->addAction(tr("打开摄像头管理"), this, &xingaodaApp::openCameraManager);

    auto menuPump = menuBar()->addMenu(tr("自动加气"));
    actionAutoPump = menuPump->addAction(tr("启用自动加气"));
    actionAutoPump->setCheckable(true);
    connect(actionAutoPump, &QAction::triggered, this, &xingaodaApp::toggleAutoPump);
    menuPump->addAction(tr("自动加气配置"), this, &xingaodaApp::openPumpSettings);

    auto menuPush = menuBar()->addMenu(tr("告警与通知"));
    menuPush->addAction(tr("推送与告警配置"), this, &xingaodaApp::openPushSettings);

    auto menuDebug = menuBar()->addMenu(tr("调试工具"));
    menuDebug->addAction(tr("查看推送状态"), [this]() { onPushStatusChanged(0); });
    menuDebug->addAction(tr("重启摄像头"), &core, &ApplicationCore::reloadCamerasFromConfig);
    menuDebug->addAction(tr("打开调试面板"), this, &xingaodaApp::openDebugTools);

    updateAutoPumpAction();
}

void xingaodaApp::setupConnections()
{
    connect(ui->btnCalibrateAll, &QPushButton::clicked, this, &xingaodaApp::onCalibrateAll);
    connect(ui->spinRealWidth, &QDoubleSpinBox::editingFinished, this, &xingaodaApp::onCalibrateAll);
    connect(ui->btnApplyThreshold, &QPushButton::clicked, this, &xingaodaApp::onAutoThresholdEdited);
    connect(ui->spinAutoThreshold, &QDoubleSpinBox::editingFinished, this, &xingaodaApp::onAutoThresholdEdited);
    connect(ui->sliderCam0Line, &QSlider::valueChanged, this, [this](int value) { onLineSliderChanged(0, value); });
    connect(ui->sliderCam1Line, &QSlider::valueChanged, this, [this](int value) { onLineSliderChanged(1, value); });

    connect(&core, &ApplicationCore::cameraFrame, this, &xingaodaApp::onCameraFrame);
    connect(&core, &ApplicationCore::widthUpdated, this, &xingaodaApp::onWidthUpdated);
    connect(&core, &ApplicationCore::fusedWidthUpdated, this, &xingaodaApp::onFusionUpdated);
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
    cameraOnline[0] = false;
    cameraOnline[1] = false;
    onMessage(tr("Cameras stopped"));
}

void xingaodaApp::onCalibrateAll()
{
    const double realWidth = ui->spinRealWidth->value();
    lastCalibrationWidthCm = realWidth;
    QString error;
    const double realWidthMm = realWidth * 10.0;
    if (!core.calibrateAllCameras(realWidthMm, error)) {
        onMessage(error);
        QMessageBox::warning(this, tr("校准失败"), error);
    } else {
        onMessage(tr("宽度校准完成"));
    }
}

void xingaodaApp::onAutoThresholdEdited()
{
    const double thresholdCm = ui->spinAutoThreshold->value();
    core.config()->setAutoStartThresholdMM(thresholdCm * 10.0);
    core.reloadPumpConfig();
    onMessage(tr("自动加气阈值已更新为 %1 cm").arg(thresholdCm, 0, 'f', 2));
}

void xingaodaApp::openCameraManager()
{
    if (!cameraDialog) {
        cameraDialog = new CameraManagerDialog(&core, this);
    }
    cameraDialog->show();
    cameraDialog->raise();
    cameraDialog->activateWindow();
}

void xingaodaApp::openPumpSettings()
{
    if (!pumpDialog) {
        pumpDialog = new PumpSettingsDialog(&core, this);
    }
    pumpDialog->show();
    pumpDialog->raise();
    pumpDialog->activateWindow();
}

void xingaodaApp::openPushSettings()
{
    if (!pushDialog) {
        pushDialog = new PushSettingsDialog(&core, this);
    }
    pushDialog->show();
    pushDialog->raise();
    pushDialog->activateWindow();
}

void xingaodaApp::openDebugTools()
{
    QMessageBox::information(this, tr("调试工具"), tr("调试工具将包含原始画面、帧率、串口日志等工程信息。"));
}

void xingaodaApp::toggleAutoPump()
{
    const bool enabled = actionAutoPump && actionAutoPump->isChecked();
    core.setAutoPumpEnabled(enabled);
    updateAutoPumpAction();
}

void xingaodaApp::reloadConfig()
{
    core.stopCameras();
    core.initialize();
    updateAutoPumpAction();
    updateLineSliders();
    onMessage(tr("配置已重新加载"));
}

void xingaodaApp::saveConfig()
{
    if (core.config()->save(core.config()->configPath())) {
        onMessage(tr("配置已保存"));
    }
}

void xingaodaApp::onCameraFrame(int id, const QImage &img)
{
    cameraOnline[id] = true;
    QImage overlay = drawOverlay(id, img);
    QLabel *targetLabel = id == 0 ? ui->labelCam0 : ui->labelCam1;
    QPixmap pix = QPixmap::fromImage(overlay).scaled(targetLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    targetLabel->setPixmap(pix);
}

void xingaodaApp::onWidthUpdated(int id, const WidthResult &result)
{
    lastWidth[id] = result;
    updateFusionLabels(core.fusedWidthMM() / 10.0);
}

void xingaodaApp::onFusionUpdated(double fusedMm, double cam0Mm, double cam1Mm)
{
    const QString cam0Text = cam0Mm > 0 ? QString::number(cam0Mm / 10.0, 'f', 2) + tr(" cm") : QStringLiteral("--");
    const QString cam1Text = cam1Mm > 0 ? QString::number(cam1Mm / 10.0, 'f', 2) + tr(" cm") : QStringLiteral("--");
    ui->labelCam0WidthLarge->setText(cam0Text);
    ui->labelCam1WidthLarge->setText(cam1Text);
    updateFusionLabels(fusedMm / 10.0);
}

void xingaodaApp::onMessage(const QString &msg)
{
    statusBar()->showMessage(msg, 5000);
}

void xingaodaApp::onSafety()
{
    if (actionAutoPump) {
        actionAutoPump->setChecked(false);
    }
    onMessage(tr("自动加气安全模式，已关闭自动加气"));
}

void xingaodaApp::onPushStatusChanged(int failures)
{
    if (!pushStatusLabel) return;
    const auto cfgPush = core.config()->pushConfig();
    if (!cfgPush.enabled || cfgPush.url.isEmpty()) {
        const QString text = tr("推送未开启");
        pushStatusLabel->setText(text);
        pushStatusLabel->setStyleSheet(QLatin1String(""));
        return;
    }
    if (failures > 0) {
        const QString text = tr("推送失败%1次").arg(failures);
        pushStatusLabel->setText(text);
        pushStatusLabel->setStyleSheet(QLatin1String("color: orange;"));
    } else {
        const QString text = tr("推送通道正常");
        pushStatusLabel->setText(text);
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

void xingaodaApp::updateAutoPumpAction()
{
    if (!actionAutoPump) return;
    actionAutoPump->blockSignals(true);
    actionAutoPump->setChecked(core.config()->config().autoPumpEnabled);
    actionAutoPump->blockSignals(false);
}

void xingaodaApp::updateFusionLabels(double fusedCm)
{
    if (fusedCm > 0) {
        ui->labelFusionValue->setText(tr("综合宽度 %1 cm").arg(fusedCm, 0, 'f', 2));
    } else {
        ui->labelFusionValue->setText(tr("综合宽度 --"));
    }
}

void xingaodaApp::updateLineSliders()
{
    auto cfgManager = core.config();
    if (!cfgManager) return;

    auto setSliderValue = [cfgManager](QSlider *slider, int camId) {
        if (!slider) return;
        CameraConfig camCfg = cfgManager->camera(camId);
        int sliderValue = qBound(0, static_cast<int>(camCfg.lineRatio * 100.0 + 0.5), 100);
        slider->blockSignals(true);
        slider->setValue(sliderValue);
        slider->blockSignals(false);
    };

    setSliderValue(ui->sliderCam0Line, 0);
    setSliderValue(ui->sliderCam1Line, 1);
}

void xingaodaApp::onLineSliderChanged(int id, int value)
{
    if (id < 0 || id > 1) return;
    const int bounded = qBound(0, value, 100);
    const double ratio = bounded / 100.0;
    CameraConfig camCfg = core.config()->camera(id);
    camCfg.lineRatio = ratio;
    core.config()->setCameraConfig(id, camCfg);
    onMessage(tr("摄像头%1检测线高度调整为%2%").arg(id).arg(static_cast<int>(ratio * 100)));
}

QImage xingaodaApp::drawOverlay(int id, const QImage &src)
{
    QImage image = src.copy();
    CameraConfig cfgCam = core.config()->camera(id);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(cfgCam.lineColor, 3));

    double lineY = cfgCam.lineRatio * image.height();
    painter.drawLine(0, lineY, image.width(), lineY);
    int band = cfgCam.widthRegionHeight > 0 ? cfgCam.widthRegionHeight : image.height() / 10;
    QRect bandRect(0, static_cast<int>(lineY - band / 2), image.width(), band);
    painter.drawRect(bandRect);

    if (lastWidth[id].valid) {
        painter.setPen(QPen(Qt::green, 3));
        painter.drawLine(lastWidth[id].leftX, 0, lastWidth[id].leftX, image.height());
        painter.drawLine(lastWidth[id].rightX, 0, lastWidth[id].rightX, image.height());
    }

    painter.setPen(Qt::white);
    QColor bg(0, 0, 0, 170);
    QFont baseFont = painter.font();
    QFont bigFont = baseFont;
    bigFont.setPointSizeF(baseFont.pointSizeF() * 2.0);
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
    tags << tr("旋转%1°").arg(cfgCam.rotation);
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
