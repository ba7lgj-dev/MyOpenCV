#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include <QColorDialog>
#include <QSignalBlocker>
#include <QComboBox>

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
    ui->chartView->setChart(core.trendChart()->chart());
    setupConnections();
    core.initialize();
    syncCameraUi();
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

    connect(ui->btnRescan, &QPushButton::clicked, this, &xingaodaApp::onRescanCameras);
    connect(ui->btnSwap, &QPushButton::clicked, this, &xingaodaApp::onSwapCameras);
    connect(ui->comboCamera0, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &xingaodaApp::onCameraIndexChanged0);
    connect(ui->comboCamera1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &xingaodaApp::onCameraIndexChanged1);
    connect(ui->chkDualMode, &QCheckBox::toggled, this, &xingaodaApp::onDualModeToggled);
    connect(ui->sliderLine0, &QSlider::valueChanged, this, &xingaodaApp::onLineRatio0);
    connect(ui->sliderLine1, &QSlider::valueChanged, this, &xingaodaApp::onLineRatio1);
    connect(ui->spinRegion0, QOverload<int>::of(&QSpinBox::valueChanged), this, &xingaodaApp::onRegionHeight0);
    connect(ui->spinRegion1, QOverload<int>::of(&QSpinBox::valueChanged), this, &xingaodaApp::onRegionHeight1);
    connect(ui->comboRotation0, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &xingaodaApp::onRotation0);
    connect(ui->comboRotation1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &xingaodaApp::onRotation1);
    connect(ui->btnColor0, &QPushButton::clicked, this, &xingaodaApp::onLineColor0);
    connect(ui->btnColor1, &QPushButton::clicked, this, &xingaodaApp::onLineColor1);

    connect(ui->editName0, &QLineEdit::editingFinished, [this](){ core.setCameraName(0, ui->editName0->text()); syncCameraUi();});
    connect(ui->editName1, &QLineEdit::editingFinished, [this](){ core.setCameraName(1, ui->editName1->text()); syncCameraUi();});

    connect(&core, &ApplicationCore::cameraFrame, this, &xingaodaApp::onCameraFrame);
    connect(&core, &ApplicationCore::widthUpdated, this, &xingaodaApp::onWidthUpdated);
    connect(&core, &ApplicationCore::message, this, &xingaodaApp::onMessage);
    connect(&core, &ApplicationCore::safetyModeEnabled, this, &xingaodaApp::onSafety);
    connect(&core, &ApplicationCore::availableCamerasChanged, this, &xingaodaApp::onAvailableCameras);
    connect(&core, &ApplicationCore::cameraStatus, this, &xingaodaApp::onCameraStatus);
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
    QPixmap pix = QPixmap::fromImage(img).scaled(400, 250, Qt::KeepAspectRatio);
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

void xingaodaApp::onRescanCameras()
{
    core.rescanCameras();
}

void xingaodaApp::onCameraIndexChanged0(int)
{
    int idx = ui->comboCamera0->currentData().toInt();
    core.setCameraIndex(0, idx);
}

void xingaodaApp::onCameraIndexChanged1(int)
{
    int idx = ui->comboCamera1->currentData().toInt();
    core.setCameraIndex(1, idx);
}

void xingaodaApp::onSwapCameras()
{
    core.swapCameras();
    syncCameraUi();
}

void xingaodaApp::onDualModeToggled(bool enabled)
{
    core.setDualCameraMode(enabled);
    ui->groupCam1->setEnabled(enabled);
}

void xingaodaApp::onLineRatio0(int value)
{
    core.setLineRatio(0, value / 100.0);
}

void xingaodaApp::onLineRatio1(int value)
{
    core.setLineRatio(1, value / 100.0);
}

void xingaodaApp::onRegionHeight0(int value)
{
    core.setRegionHeight(0, value);
}

void xingaodaApp::onRegionHeight1(int value)
{
    core.setRegionHeight(1, value);
}

void xingaodaApp::onRotation0(int index)
{
    static const int rotations[] = {0, 90, 180, 270};
    if (index >= 0 && index < 4) {
        core.setCameraRotation(0, rotations[index]);
    }
}

void xingaodaApp::onRotation1(int index)
{
    static const int rotations[] = {0, 90, 180, 270};
    if (index >= 0 && index < 4) {
        core.setCameraRotation(1, rotations[index]);
    }
}

void xingaodaApp::onLineColor0()
{
    QColor c = QColorDialog::getColor(Qt::red, this, tr("选择检测线颜色0"));
    if (c.isValid()) {
        core.setLineColor(0, c);
        ui->btnColor0->setStyleSheet(QString("background:%1").arg(c.name()));
    }
}

void xingaodaApp::onLineColor1()
{
    QColor c = QColorDialog::getColor(Qt::red, this, tr("选择检测线颜色1"));
    if (c.isValid()) {
        core.setLineColor(1, c);
        ui->btnColor1->setStyleSheet(QString("background:%1").arg(c.name()));
    }
}

void xingaodaApp::onAvailableCameras(const QList<int> &indexes)
{
    auto fillCombo = [](QComboBox *combo, const QList<int> &idxs, int current){
        QSignalBlocker block(combo);
        combo->clear();
        for (int idx : idxs) {
            combo->addItem(QString::number(idx), idx);
        }
        if (combo->findData(current) < 0) {
            combo->addItem(QString::number(current), current);
        }
        int pos = combo->findData(current);
        combo->setCurrentIndex(pos >= 0 ? pos : 0);
    };
    auto cam0 = core.config()->camera(0);
    auto cam1 = core.config()->camera(1);
    fillCombo(ui->comboCamera0, indexes, cam0.index);
    fillCombo(ui->comboCamera1, indexes, cam1.index);
}

void xingaodaApp::onCameraStatus(int id, const QString &msg, bool error)
{
    updateCameraStatusLabel(id, msg, error);
}

void xingaodaApp::syncCameraUi()
{
    auto cam0 = core.config()->camera(0);
    auto cam1 = core.config()->camera(1);
    ui->groupCam0->setTitle(cam0.name);
    ui->groupCam1->setTitle(cam1.name);
    ui->editName0->setText(cam0.name);
    ui->editName1->setText(cam1.name);
    ui->chkDualMode->setChecked(core.config()->config().dualCameraMode);

    auto setRotation = [](QComboBox *combo, int rotation){
        int index = 0;
        switch (rotation) {
        case 90: index = 1; break;
        case 180: index = 2; break;
        case 270: index = 3; break;
        default: index = 0; break;
        }
        QSignalBlocker block(combo);
        combo->setCurrentIndex(index);
    };
    setRotation(ui->comboRotation0, cam0.rotation);
    setRotation(ui->comboRotation1, cam1.rotation);

    {
        QSignalBlocker b0(ui->sliderLine0);
        ui->sliderLine0->setValue(static_cast<int>(cam0.lineRatio * 100));
    }
    {
        QSignalBlocker b1(ui->sliderLine1);
        ui->sliderLine1->setValue(static_cast<int>(cam1.lineRatio * 100));
    }
    {
        QSignalBlocker b2(ui->spinRegion0);
        ui->spinRegion0->setValue(cam0.widthRegionHeight);
    }
    {
        QSignalBlocker b3(ui->spinRegion1);
        ui->spinRegion1->setValue(cam1.widthRegionHeight);
    }

    ui->btnColor0->setStyleSheet(QString("background:%1").arg(cam0.lineColor.name()));
    ui->btnColor1->setStyleSheet(QString("background:%1").arg(cam1.lineColor.name()));

    updateCameraStatusLabel(0, "", false);
    updateCameraStatusLabel(1, "", false);
}

void xingaodaApp::updateCameraStatusLabel(int id, const QString &msg, bool error)
{
    QLabel *lbl = id == 0 ? ui->labelCameraStatus0 : ui->labelCameraStatus1;
    lbl->setText(msg);
    lbl->setVisible(error);
}

