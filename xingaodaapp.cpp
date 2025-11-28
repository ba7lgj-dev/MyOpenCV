#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QPainter>
#include <QPen>
#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QTabWidget>
#include <QTextEdit>
#include <QGroupBox>
#include <QPushButton>
#include <opencv2/videoio.hpp>

namespace {

QList<int> availableCameraPorts()
{
    QList<int> ports;
    for (int i = 0; i < 10; ++i) {
        cv::VideoCapture cap;
        if (cap.open(i)) {
            ports.append(i);
            cap.release();
        }
    }
    if (ports.isEmpty()) {
        ports.append(0);
    }
    return ports;
}

}

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    ui->chartView->setChart(core.trendChart()->chart());
    ui->sliderLine0->setValue(static_cast<int>(core.config()->camera(0).lineRatio * 100));
    ui->sliderLine1->setValue(static_cast<int>(core.config()->camera(1).lineRatio * 100));
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
    connect(ui->sliderLine0, &QSlider::valueChanged, this, &xingaodaApp::onLine0Changed);
    connect(ui->sliderLine1, &QSlider::valueChanged, this, &xingaodaApp::onLine1Changed);
    connect(ui->actionResetConfig, &QAction::triggered, this, &xingaodaApp::onResetDefaults);
    connect(ui->actionCamera, &QAction::triggered, this, &xingaodaApp::onCameraSettings);
    connect(ui->actionDetect, &QAction::triggered, this, &xingaodaApp::onDetectSettings);
    connect(ui->actionPump, &QAction::triggered, this, &xingaodaApp::onPumpSettings);
    connect(ui->actionPush, &QAction::triggered, this, &xingaodaApp::onPushSettings);
    connect(core.config(), &ConfigManager::configReloaded, this, &xingaodaApp::onConfigReloaded);

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

void xingaodaApp::onLine0Changed(int value)
{
    double ratio = value / 100.0;
    AppConfig appCfg = core.config()->config();
    appCfg.cameras[0].lineRatio = ratio;
    core.config()->setConfig(appCfg);
    onMessage(tr("左摄像头检测线高度已调整"));
}

void xingaodaApp::onLine1Changed(int value)
{
    double ratio = value / 100.0;
    AppConfig appCfg = core.config()->config();
    appCfg.cameras[1].lineRatio = ratio;
    core.config()->setConfig(appCfg);
    onMessage(tr("右摄像头检测线高度已调整"));
}

void xingaodaApp::onResetDefaults()
{
    core.config()->resetDefaults();
    core.config()->save("config.json");
    onMessage(tr("已恢复出厂配置"));
}

void xingaodaApp::onCameraSettings()
{
    showCameraDialog();
}

void xingaodaApp::onDetectSettings()
{
    showDetectDialog();
}

void xingaodaApp::onPumpSettings()
{
    showPumpDialog();
}

void xingaodaApp::onPushSettings()
{
    showPushDialog();
}

void xingaodaApp::onConfigReloaded()
{
    ui->sliderLine0->setValue(static_cast<int>(core.config()->camera(0).lineRatio * 100));
    ui->sliderLine1->setValue(static_cast<int>(core.config()->camera(1).lineRatio * 100));
    ui->chkAutoPump->setChecked(core.config()->config().autoPumpEnabled);
}

QWidget *xingaodaApp::buildCameraGroup(int idx, const CameraConfig &cfg, QMap<QString, QWidget *> &widgets)
{
    QGroupBox *box = new QGroupBox(idx == 0 ? tr("左摄像头") : tr("右摄像头"));
    QFormLayout *form = new QFormLayout(box);

    QComboBox *indexCombo = new QComboBox(box);
    indexCombo->setEditable(true);
    QList<int> ports = availableCameraPorts();
    for (int port : ports) {
        indexCombo->addItem(QString::number(port));
    }
    int portIndex = ports.indexOf(cfg.index);
    if (portIndex >= 0) {
        indexCombo->setCurrentIndex(portIndex);
    } else {
        indexCombo->setCurrentText(QString::number(cfg.index));
    }
    QLineEdit *nameEdit = new QLineEdit(cfg.name, box);
    QComboBox *rotationCombo = new QComboBox(box);
    rotationCombo->addItems({QStringLiteral("0°"), QStringLiteral("90°"), QStringLiteral("180°"), QStringLiteral("270°")});
    rotationCombo->setCurrentIndex(cfg.rotation / 90);
    QCheckBox *flipH = new QCheckBox(tr("水平翻转"), box);
    flipH->setChecked(cfg.flipHorizontal);
    QCheckBox *flipV = new QCheckBox(tr("垂直翻转"), box);
    flipV->setChecked(cfg.flipVertical);

    form->addRow(tr("摄像头端口号"), indexCombo);
    form->addRow(tr("名称"), nameEdit);
    form->addRow(tr("旋转"), rotationCombo);
    form->addRow(flipH);
    form->addRow(flipV);

    widgets.insert(QString("cam%1.index").arg(idx), indexCombo);
    widgets.insert(QString("cam%1.name").arg(idx), nameEdit);
    widgets.insert(QString("cam%1.rotation").arg(idx), rotationCombo);
    widgets.insert(QString("cam%1.flipH").arg(idx), flipH);
    widgets.insert(QString("cam%1.flipV").arg(idx), flipV);
    return box;
}

QWidget *xingaodaApp::buildDetectGroup(int idx, const CameraConfig &cfg, QMap<QString, QWidget *> &widgets)
{
    QGroupBox *box = new QGroupBox(idx == 0 ? tr("左摄像头检测") : tr("右摄像头检测"));
    QFormLayout *form = new QFormLayout(box);

    QDoubleSpinBox *line = new QDoubleSpinBox(box);
    line->setRange(0.0, 1.0);
    line->setSingleStep(0.01);
    line->setValue(cfg.lineRatio);

    QDoubleSpinBox *mmPerPx = new QDoubleSpinBox(box);
    mmPerPx->setRange(0.01, 10.0);
    mmPerPx->setDecimals(3);
    mmPerPx->setValue(cfg.mmPerPixel);

    QPushButton *colorBtn = new QPushButton(cfg.detectLineColor.name(QColor::HexRgb), box);
    colorBtn->setStyleSheet(QStringLiteral("background:%1").arg(cfg.detectLineColor.name()));
    connect(colorBtn, &QPushButton::clicked, box, [colorBtn]( ){
        QColor chosen = QColorDialog::getColor(QColor(colorBtn->text()));
        if (chosen.isValid()) {
            colorBtn->setText(chosen.name(QColor::HexRgb));
            colorBtn->setStyleSheet(QStringLiteral("background:%1").arg(chosen.name()));
        }
    });

    form->addRow(tr("检测线比例"), line);
    form->addRow(tr("毫米/像素"), mmPerPx);
    form->addRow(tr("检测线颜色"), colorBtn);

    widgets.insert(QString("cam%1.line").arg(idx), line);
    widgets.insert(QString("cam%1.mmpp").arg(idx), mmPerPx);
    widgets.insert(QString("cam%1.color").arg(idx), colorBtn);
    return box;
}

void xingaodaApp::showCameraDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("摄像头管理"));
    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    AppConfig appCfg = core.config()->config();
    QMap<QString, QWidget *> widgets;

    QCheckBox *dual = new QCheckBox(tr("双摄像头模式"), &dlg);
    dual->setChecked(appCfg.dualCameraMode);
    layout->addWidget(dual);

    layout->addWidget(buildCameraGroup(0, appCfg.cameras[0], widgets));
    layout->addWidget(buildCameraGroup(1, appCfg.cameras[1], widgets));

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        appCfg.dualCameraMode = dual->isChecked();
        for (int i = 0; i < 2; ++i) {
            CameraConfig cfg = appCfg.cameras[i];
            cfg.index = qobject_cast<QComboBox*>(widgets.value(QString("cam%1.index").arg(i)))->currentText().toInt();
            cfg.name = qobject_cast<QLineEdit*>(widgets.value(QString("cam%1.name").arg(i)))->text();
            cfg.rotation = qobject_cast<QComboBox*>(widgets.value(QString("cam%1.rotation").arg(i)))->currentIndex() * 90;
            cfg.flipHorizontal = qobject_cast<QCheckBox*>(widgets.value(QString("cam%1.flipH").arg(i)))->isChecked();
            cfg.flipVertical = qobject_cast<QCheckBox*>(widgets.value(QString("cam%1.flipV").arg(i)))->isChecked();
            appCfg.cameras[i] = cfg;
        }
        core.config()->setConfig(appCfg);
        core.config()->save("config.json");
        onMessage(tr("摄像头配置已保存"));
    }
}

void xingaodaApp::showDetectDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("检测参数"));
    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    AppConfig appCfg = core.config()->config();
    QMap<QString, QWidget *> widgets;

    layout->addWidget(buildDetectGroup(0, appCfg.cameras[0], widgets));
    layout->addWidget(buildDetectGroup(1, appCfg.cameras[1], widgets));

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        for (int i = 0; i < 2; ++i) {
            CameraConfig cfg = appCfg.cameras[i];
            cfg.lineRatio = qobject_cast<QDoubleSpinBox*>(widgets.value(QString("cam%1.line").arg(i)))->value();
            cfg.mmPerPixel = qobject_cast<QDoubleSpinBox*>(widgets.value(QString("cam%1.mmpp").arg(i)))->value();
            QColor c(qobject_cast<QPushButton*>(widgets.value(QString("cam%1.color").arg(i)))->text());
            if (c.isValid()) cfg.detectLineColor = c;
            appCfg.cameras[i] = cfg;
        }
        core.config()->setConfig(appCfg);
        core.config()->save("config.json");
        onMessage(tr("检测参数已保存"));
    }
}

void xingaodaApp::showPumpDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("自动加气配置"));
    QFormLayout *form = new QFormLayout(&dlg);
    AppConfig appCfg = core.config()->config();

    QLineEdit *portEdit = new QLineEdit(appCfg.pumpPort, &dlg);
    QCheckBox *autoEnabled = new QCheckBox(tr("启用自动加气"), &dlg);
    autoEnabled->setChecked(appCfg.autoPumpEnabled);

    QSpinBox *cooldown0 = new QSpinBox(&dlg);
    cooldown0->setRange(0, 10000);
    cooldown0->setValue(appCfg.cameras[0].cooldownMs);
    QSpinBox *cooldown1 = new QSpinBox(&dlg);
    cooldown1->setRange(0, 10000);
    cooldown1->setValue(appCfg.cameras[1].cooldownMs);

    QDoubleSpinBox *threshold0 = new QDoubleSpinBox(&dlg);
    threshold0->setRange(0, 5000);
    threshold0->setValue(appCfg.cameras[0].thresholdMM);
    QDoubleSpinBox *threshold1 = new QDoubleSpinBox(&dlg);
    threshold1->setRange(0, 5000);
    threshold1->setValue(appCfg.cameras[1].thresholdMM);

    QSpinBox *pulse0 = new QSpinBox(&dlg);
    pulse0->setRange(0, 10000);
    pulse0->setValue(appCfg.cameras[0].pulseMs);
    QSpinBox *pulse1 = new QSpinBox(&dlg);
    pulse1->setRange(0, 10000);
    pulse1->setValue(appCfg.cameras[1].pulseMs);

    form->addRow(tr("串口号"), portEdit);
    form->addRow(autoEnabled);
    form->addRow(tr("左冷却(ms)"), cooldown0);
    form->addRow(tr("右冷却(ms)"), cooldown1);
    form->addRow(tr("左阈值(mm)"), threshold0);
    form->addRow(tr("右阈值(mm)"), threshold1);
    form->addRow(tr("左脉冲(ms)"), pulse0);
    form->addRow(tr("右脉冲(ms)"), pulse1);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        appCfg.pumpPort = portEdit->text();
        appCfg.autoPumpEnabled = autoEnabled->isChecked();
        appCfg.cameras[0].cooldownMs = cooldown0->value();
        appCfg.cameras[1].cooldownMs = cooldown1->value();
        appCfg.cameras[0].thresholdMM = threshold0->value();
        appCfg.cameras[1].thresholdMM = threshold1->value();
        appCfg.cameras[0].pulseMs = pulse0->value();
        appCfg.cameras[1].pulseMs = pulse1->value();
        core.config()->setConfig(appCfg);
        core.config()->save("config.json");
        onMessage(tr("自动加气配置已保存"));
    }
}

void xingaodaApp::showPushDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("推送配置"));
    QFormLayout *form = new QFormLayout(&dlg);
    AppConfig appCfg = core.config()->config();

    QLineEdit *urlEdit = new QLineEdit(appCfg.push.url, &dlg);
    QLineEdit *tokenEdit = new QLineEdit(appCfg.push.token, &dlg);
    QTextEdit *templateEdit = new QTextEdit(appCfg.push.templateContent, &dlg);
    QCheckBox *enabled = new QCheckBox(tr("启用推送"), &dlg);
    enabled->setChecked(appCfg.push.enabled);
    QSpinBox *maxRetry = new QSpinBox(&dlg);
    maxRetry->setRange(1, 10);
    maxRetry->setValue(appCfg.push.maxRetries);
    QSpinBox *failThreshold = new QSpinBox(&dlg);
    failThreshold->setRange(1, 10);
    failThreshold->setValue(appCfg.push.failureThreshold);

    form->addRow(tr("推送URL"), urlEdit);
    form->addRow(tr("Token"), tokenEdit);
    form->addRow(tr("模板"), templateEdit);
    form->addRow(enabled);
    form->addRow(tr("最大重试"), maxRetry);
    form->addRow(tr("失败阈值"), failThreshold);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        appCfg.push.url = urlEdit->text();
        appCfg.push.token = tokenEdit->text();
        appCfg.push.templateContent = templateEdit->toPlainText();
        appCfg.push.enabled = enabled->isChecked();
        appCfg.push.maxRetries = maxRetry->value();
        appCfg.push.failureThreshold = failThreshold->value();
        core.config()->setConfig(appCfg);
        core.config()->save("config.json");
        onMessage(tr("推送配置已保存"));
    }
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

