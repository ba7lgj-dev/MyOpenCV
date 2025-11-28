#include "cameramanagerdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QGroupBox>
#include <QLabel>
#include <QPalette>

CameraManagerDialog::CameraManagerDialog(ApplicationCore *core, QWidget *parent)
    : QDialog(parent), core(core), cfg(core->config())
{
    setWindowTitle(tr("摄像头管理"));
    buildUi();
    refreshAvailableIndices();
    populateCamera(0, camWidgets[0]);
    populateCamera(1, camWidgets[1]);
}

void CameraManagerDialog::buildUi()
{
    auto mainLayout = new QVBoxLayout(this);

    dualModeCheck = new QCheckBox(tr("启用双摄像头模式"));
    dualModeCheck->setChecked(cfg->config().dualCameraMode);
    mainLayout->addWidget(dualModeCheck);

    rescanBtn = new QPushButton(tr("重新扫描可用摄像头"));
    connect(rescanBtn, &QPushButton::clicked, this, &CameraManagerDialog::onRescan);
    mainLayout->addWidget(rescanBtn);

    auto camerasLayout = new QHBoxLayout();
    for (int i = 0; i < 2; ++i) {
        QGroupBox *box = new QGroupBox(tr("摄像头 %1 配置").arg(i));
        auto form = new QFormLayout();
        camWidgets[i].nameEdit = new QLineEdit();
        camWidgets[i].indexCombo = new QComboBox();
        camWidgets[i].indexCombo->setEditable(true);
        camWidgets[i].rotationCombo = new QComboBox();
        camWidgets[i].rotationCombo->addItems({"0", "90", "180", "270"});
        camWidgets[i].lineSlider = new QSlider(Qt::Horizontal);
        camWidgets[i].lineSlider->setRange(0, 100);
        camWidgets[i].lineValue = new QLabel();
        camWidgets[i].colorBtn = new QPushButton(tr("选择颜色"));
        camWidgets[i].regionHeight = new QSpinBox();
        camWidgets[i].regionHeight->setRange(10, 2000);
        camWidgets[i].regionHeight->setSingleStep(10);

        auto lineLayout = new QHBoxLayout();
        lineLayout->addWidget(camWidgets[i].lineSlider);
        lineLayout->addWidget(camWidgets[i].lineValue);

        form->addRow(tr("名称"), camWidgets[i].nameEdit);
        form->addRow(tr("摄像头索引"), camWidgets[i].indexCombo);
        form->addRow(tr("旋转(度)"), camWidgets[i].rotationCombo);
        form->addRow(tr("检测线位置"), lineLayout);
        form->addRow(tr("检测区域高度(px)"), camWidgets[i].regionHeight);
        form->addRow(tr("检测线颜色"), camWidgets[i].colorBtn);

        connect(camWidgets[i].lineSlider, &QSlider::valueChanged, this, [this, i](int v){
            camWidgets[i].lineValue->setText(QString::number(v) + "%");
        });
        connect(camWidgets[i].colorBtn, &QPushButton::clicked, this, [this, i](){
            QColor current = cfg->camera(i).lineColor;
            QColor chosen = selectColor(current);
            if (chosen.isValid()) {
                QPalette pal = camWidgets[i].colorBtn->palette();
                pal.setColor(QPalette::Button, chosen);
                camWidgets[i].colorBtn->setAutoFillBackground(true);
                camWidgets[i].colorBtn->setPalette(pal);
                camWidgets[i].colorBtn->update();
                cfg->setLineColor(i, chosen);
            }
        });

        box->setLayout(form);
        camerasLayout->addWidget(box);
    }
    mainLayout->addLayout(camerasLayout);

    QPushButton *swapBtn = new QPushButton(tr("互换摄像头顺序"));
    connect(swapBtn, &QPushButton::clicked, this, &CameraManagerDialog::onSwap);
    mainLayout->addWidget(swapBtn);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &CameraManagerDialog::applyAndClose);
    connect(buttons, &QDialogButtonBox::rejected, this, &CameraManagerDialog::reject);
    mainLayout->addWidget(buttons);
}

void CameraManagerDialog::populateCamera(int id, CameraWidgets &widgets)
{
    CameraConfig cam = cfg->camera(id);
    widgets.nameEdit->setText(cam.name);
    widgets.indexCombo->clear();
    for (int index : currentIndices) {
        widgets.indexCombo->addItem(QString::number(index));
    }
    widgets.indexCombo->setCurrentText(QString::number(cam.index));
    int rotIndex = widgets.rotationCombo->findText(QString::number(cam.rotation));
    widgets.rotationCombo->setCurrentIndex(rotIndex < 0 ? 0 : rotIndex);
    widgets.lineSlider->setValue(static_cast<int>(cam.lineRatio * 100));
    widgets.lineValue->setText(QString::number(static_cast<int>(cam.lineRatio * 100)) + "%");
    widgets.regionHeight->setValue(cam.widthRegionHeight > 0 ? cam.widthRegionHeight : 200);
    QPalette pal = widgets.colorBtn->palette();
    pal.setColor(QPalette::Button, cam.lineColor);
    widgets.colorBtn->setAutoFillBackground(true);
    widgets.colorBtn->setPalette(pal);
}

QColor CameraManagerDialog::selectColor(const QColor &current)
{
    return QColorDialog::getColor(current, this, tr("选择检测线颜色"));
}

void CameraManagerDialog::syncConfigFromWidgets(int id, const CameraWidgets &widgets)
{
    CameraConfig cam = cfg->camera(id);
    cam.name = widgets.nameEdit->text();
    cam.index = widgets.indexCombo->currentText().toInt();
    cam.rotation = widgets.rotationCombo->currentText().toInt();
    cam.lineRatio = widgets.lineSlider->value() / 100.0;
    cam.widthRegionHeight = widgets.regionHeight->value();
    cfg->setCameraConfig(id, cam);
}

void CameraManagerDialog::refreshAvailableIndices()
{
    currentIndices = core->availableCameras();
    if (currentIndices.isEmpty()) {
        currentIndices = {0, 1, 2, 3};
    }
}

void CameraManagerDialog::onSwap()
{
    CameraConfig c0 = cfg->camera(0);
    CameraConfig c1 = cfg->camera(1);
    cfg->setCameraConfig(0, c1);
    cfg->setCameraConfig(1, c0);
    populateCamera(0, camWidgets[0]);
    populateCamera(1, camWidgets[1]);
}

void CameraManagerDialog::onRescan()
{
    core->rescanCameras();
    refreshAvailableIndices();
    populateCamera(0, camWidgets[0]);
    populateCamera(1, camWidgets[1]);
}

void CameraManagerDialog::applyAndClose()
{
    cfg->setDualCameraMode(dualModeCheck->isChecked());
    syncConfigFromWidgets(0, camWidgets[0]);
    syncConfigFromWidgets(1, camWidgets[1]);
    cfg->save(cfg->lastConfigPath().isEmpty() ? QStringLiteral("config.json") : cfg->lastConfigPath());
    core->reloadCameraConfig();
    accept();
}
