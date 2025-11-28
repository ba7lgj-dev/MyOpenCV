#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QColorDialog>
#include <QPainter>
#include <QtMath>

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    ui->chartView->setChart(core.trendChart()->chart());
    setupConnections();
    core.initialize();
    refreshUiFromConfig();
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
    connect(ui->comboCam0, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){ onCameraIndexChanged(0, ui->comboCam0->currentData().toInt());});
    connect(ui->comboCam1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){ onCameraIndexChanged(1, ui->comboCam1->currentData().toInt());});
    connect(ui->editName0, &QLineEdit::textEdited, this, [this](const QString &t){ onCameraNameEdited(0, t);});
    connect(ui->editName1, &QLineEdit::textEdited, this, [this](const QString &t){ onCameraNameEdited(1, t);});
    connect(ui->comboRotate0, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){ onRotationChanged(0, ui->comboRotate0->currentData().toInt());});
    connect(ui->comboRotate1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){ onRotationChanged(1, ui->comboRotate1->currentData().toInt());});
    connect(ui->sliderLine0, &QSlider::valueChanged, this, [this](int v){ onLineRatioChanged(0, v);});
    connect(ui->sliderLine1, &QSlider::valueChanged, this, [this](int v){ onLineRatioChanged(1, v);});
    connect(ui->btnColor0, &QPushButton::clicked, this, [this](){ onLineColorChanged(0);});
    connect(ui->btnColor1, &QPushButton::clicked, this, [this](){ onLineColorChanged(1);});
    connect(ui->spinLineHeight0, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){ onLineHeightChanged(0, v);});
    connect(ui->spinLineHeight1, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){ onLineHeightChanged(1, v);});
    connect(ui->spinWidthRegion0, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){ onWidthRegionChanged(0, v);});
    connect(ui->spinWidthRegion1, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){ onWidthRegionChanged(1, v);});
    connect(ui->btnSavePump, &QPushButton::clicked, this, &xingaodaApp::onPumpConfigUpdated);
    connect(ui->btnSavePush, &QPushButton::clicked, this, &xingaodaApp::onPushConfigUpdated);
    connect(ui->btnRestore, &QPushButton::clicked, this, &xingaodaApp::onRestoreDefaults);
    connect(ui->btnTestPush, &QPushButton::clicked, this, &xingaodaApp::onTestPush);

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
    QString trend = "→";
    if (lastResult[id].valid) {
        if (result.widthMM > lastResult[id].widthMM + 0.1) trend = "↑";
        else if (result.widthMM < lastResult[id].widthMM - 0.1) trend = "↓";
    }
    QString text = QString("宽度: %1 px / %2 mm %3").arg(result.widthPixels, 0, 'f', 1).arg(result.widthMM, 0, 'f', 1).arg(trend);
    QString detail = tr("检测行 %1, 左=%2, 右=%3").arg(result.usedRow).arg(result.leftX).arg(result.rightX);
    if (id == 0) {
        ui->labelWidth0->setText(text);
        ui->labelStatus0->setText(detail);
    } else {
        ui->labelWidth1->setText(text);
        ui->labelStatus1->setText(detail);
    }
}

void xingaodaApp::onWidthUpdated(int id, const WidthResult &result)
{
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

void xingaodaApp::onCameraIndexChanged(int id, int index)
{
    core.config()->updateCameraIndex(id, index);
    core.config()->save("config.json");
    core.applyCameraConfig(id);
    core.startCameras();
    onMessage(tr("已切换摄像头%1为索引%2").arg(id).arg(index));
}

void xingaodaApp::onCameraNameEdited(int id, const QString &name)
{
    core.config()->updateCameraName(id, name);
    core.config()->save("config.json");
}

void xingaodaApp::onRotationChanged(int id, int value)
{
    core.config()->updateCameraRotation(id, value);
    core.config()->save("config.json");
    core.applyCameraConfig(id);
    core.startCameras();
}

void xingaodaApp::onLineRatioChanged(int id, int sliderValue)
{
    double ratio = sliderValue / 100.0;
    core.config()->updateCameraLineRatio(id, ratio);
    core.config()->save("config.json");
}

void xingaodaApp::onLineColorChanged(int id)
{
    QColor chosen = askColor(core.config()->camera(id).lineColor);
    if (!chosen.isValid()) return;
    core.config()->updateCameraLineColor(id, chosen);
    core.config()->save("config.json");
}

void xingaodaApp::onLineHeightChanged(int id, int value)
{
    core.config()->updateCameraLineHeight(id, value);
    core.config()->save("config.json");
}

void xingaodaApp::onWidthRegionChanged(int id, int value)
{
    core.config()->updateCameraWidthRegion(id, value);
    core.config()->save("config.json");
}

void xingaodaApp::onPumpConfigUpdated()
{
    core.config()->updatePumpConfig(ui->editPumpPort->text(), ui->spinPumpDuration->value(), ui->doublePumpThreshold->value(), ui->spinPumpCooldown->value());
    ui->chkAutoPump->setChecked(core.config()->config().autoPumpEnabled);
    onMessage(tr("自动加气参数已保存"));
}

void xingaodaApp::onPushConfigUpdated()
{
    PushConfig push = core.config()->pushConfig();
    push.url = ui->editPushUrl->text();
    push.token = ui->editPushToken->text();
    push.templateText = ui->editPushTemplate->toPlainText();
    push.enabled = ui->chkPushEnabled->isChecked();
    push.maxFailures = ui->spinPushMaxFail->value();
    core.config()->updatePushConfig(push);
    core.config()->save("config.json");
    onMessage(tr("推送配置已保存"));
}

void xingaodaApp::onRestoreDefaults()
{
    core.config()->restoreDefaults();
    core.config()->save("config.json");
    refreshUiFromConfig();
    onMessage(tr("已恢复出厂设置"));
}

void xingaodaApp::onTestPush()
{
    core.sendPush(tr("测试推送"), tr("按钮触发的推送测试"));
    onMessage(tr("已执行推送测试"));
}

QPixmap xingaodaApp::drawOverlay(int id, const QImage &img) const
{
    QPixmap pix = QPixmap::fromImage(img);
    QPainter p(&pix);
    CameraConfig cfg = core.config()->camera(id);
    p.setPen(QPen(cfg.lineColor, 2));
    int lineY = static_cast<int>(cfg.lineRatio * img.height());
    p.drawLine(0, lineY, img.width(), lineY);
    if (cfg.lineHeightPx > 0) {
        p.drawLine(0, lineY + cfg.lineHeightPx, img.width(), lineY + cfg.lineHeightPx);
    }
    if (cfg.widthRegionHeight > 0) {
        QRect rect(0, lineY - cfg.widthRegionHeight / 2, img.width(), cfg.widthRegionHeight);
        p.drawRect(rect);
    }
    const WidthResult &res = lastResult[id];
    if (res.valid) {
        p.setPen(QPen(Qt::green, 2));
        p.drawLine(res.leftX, lineY - 10, res.leftX, lineY + 10);
        p.drawLine(res.rightX, lineY - 10, res.rightX, lineY + 10);
        p.drawText(res.leftX, lineY - 15, tr("左边界"));
        p.drawText(res.rightX, lineY - 15, tr("右边界"));
    }
    return pix;
}

QColor xingaodaApp::askColor(const QColor &current) const
{
    QColorDialog dialog(current, const_cast<xingaodaApp*>(this));
    dialog.setWindowTitle(tr("选择检测线颜色"));
    if (dialog.exec() == QDialog::Accepted) {
        return dialog.selectedColor();
    }
    return QColor();
}

void xingaodaApp::refreshUiFromConfig()
{
    ui->comboCam0->clear();
    ui->comboCam1->clear();
    QList<int> cams = core.camerasDetected();
    if (cams.isEmpty()) {
        cams = {0,1};
    }
    for (int idx : cams) {
        ui->comboCam0->addItem(QString::number(idx), idx);
        ui->comboCam1->addItem(QString::number(idx), idx);
    }
    auto c0 = core.config()->camera(0);
    auto c1 = core.config()->camera(1);
    for (int i = 0; i < ui->comboRotate0->count(); ++i) {
        int angle = i == 0 ? 0 : (i == 1 ? 90 : (i == 2 ? 180 : 270));
        ui->comboRotate0->setItemData(i, angle);
        ui->comboRotate1->setItemData(i, angle);
    }
    ui->comboCam0->setCurrentIndex(ui->comboCam0->findData(c0.index));
    ui->comboCam1->setCurrentIndex(ui->comboCam1->findData(c1.index));
    ui->editName0->setText(c0.name);
    ui->editName1->setText(c1.name);
    ui->comboRotate0->setCurrentIndex(ui->comboRotate0->findData(c0.rotation));
    ui->comboRotate1->setCurrentIndex(ui->comboRotate1->findData(c1.rotation));
    ui->sliderLine0->setValue(static_cast<int>(c0.lineRatio * 100));
    ui->sliderLine1->setValue(static_cast<int>(c1.lineRatio * 100));
    ui->spinLineHeight0->setValue(c0.lineHeightPx);
    ui->spinLineHeight1->setValue(c1.lineHeightPx);
    ui->spinWidthRegion0->setValue(c0.widthRegionHeight);
    ui->spinWidthRegion1->setValue(c1.widthRegionHeight);
    ui->chkAutoPump->setChecked(core.config()->config().autoPumpEnabled);
    ui->editPumpPort->setText(core.config()->config().pumpPort);
    ui->spinPumpDuration->setValue(core.config()->config().pumpDurationMs);
    ui->doublePumpThreshold->setValue(core.config()->config().pumpThresholdMM);
    ui->spinPumpCooldown->setValue(core.config()->config().pumpCooldownMs);
    auto push = core.config()->pushConfig();
    ui->editPushUrl->setText(push.url);
    ui->editPushToken->setText(push.token);
    ui->editPushTemplate->setPlainText(push.templateText);
    ui->chkPushEnabled->setChecked(push.enabled);
    ui->spinPushMaxFail->setValue(push.maxFailures);
}

