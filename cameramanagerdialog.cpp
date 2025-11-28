#include "cameramanagerdialog.h"
#include "applicationcore.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

CameraManagerDialog::CameraManagerDialog(ApplicationCore *core, QWidget *parent)
    : QDialog(parent), core(core)
{
    cfg = core ? core->config() : nullptr;
    setWindowTitle(tr("摄像头管理"));
    setMinimumWidth(400);

    auto layout = new QVBoxLayout(this);
    auto form0 = new QFormLayout();
    auto form1 = new QFormLayout();

    comboIndex0 = new QComboBox(this);
    comboIndex1 = new QComboBox(this);
    editName0 = new QLineEdit(this);
    editName1 = new QLineEdit(this);
    comboRotation0 = new QComboBox(this);
    comboRotation1 = new QComboBox(this);
    chkDualMode = new QCheckBox(tr("启用双摄像头模式"), this);

    for (int deg : {0, 90, 180, 270}) {
        comboRotation0->addItem(QString::number(deg), deg);
        comboRotation1->addItem(QString::number(deg), deg);
    }

    form0->addRow(tr("摄像头0索引"), comboIndex0);
    form0->addRow(tr("摄像头0名称"), editName0);
    form0->addRow(tr("摄像头0旋转"), comboRotation0);

    form1->addRow(tr("摄像头1索引"), comboIndex1);
    form1->addRow(tr("摄像头1名称"), editName1);
    form1->addRow(tr("摄像头1旋转"), comboRotation1);

    auto hlayout = new QHBoxLayout();
    auto btnRescan = new QPushButton(tr("重新扫描"), this);
    auto btnSwap = new QPushButton(tr("左右互换"), this);
    hlayout->addWidget(btnRescan);
    hlayout->addWidget(btnSwap);
    hlayout->addStretch();

    layout->addLayout(form0);
    layout->addLayout(form1);
    layout->addWidget(chkDualMode);
    layout->addLayout(hlayout);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(btnRescan, &QPushButton::clicked, this, &CameraManagerDialog::onRescan);
    connect(btnSwap, &QPushButton::clicked, this, &CameraManagerDialog::onSwap);
    connect(buttons, &QDialogButtonBox::accepted, this, &CameraManagerDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &CameraManagerDialog::reject);

    onRescan();
    loadFromConfig();
}

void CameraManagerDialog::populateIndices()
{
    comboIndex0->clear();
    comboIndex1->clear();
    for (int idx : indices) {
        comboIndex0->addItem(QString::number(idx), idx);
        comboIndex1->addItem(QString::number(idx), idx);
    }
}

void CameraManagerDialog::loadFromConfig()
{
    if (!cfg) return;
    CameraConfig c0 = cfg->camera(0);
    CameraConfig c1 = cfg->camera(1);
    int idx0 = comboIndex0->findData(c0.index);
    int idx1 = comboIndex1->findData(c1.index);
    if (idx0 >= 0) comboIndex0->setCurrentIndex(idx0);
    if (idx1 >= 0) comboIndex1->setCurrentIndex(idx1);
    editName0->setText(c0.name);
    editName1->setText(c1.name);
    comboRotation0->setCurrentIndex(comboRotation0->findData(c0.rotation));
    comboRotation1->setCurrentIndex(comboRotation1->findData(c1.rotation));
    chkDualMode->setChecked(cfg->config().dualCameraMode);
}

void CameraManagerDialog::onRescan()
{
    if (!core) return;
    indices = core->availableCameraIndices();
    if (indices.isEmpty()) {
        indices = {0, 1};
    }
    populateIndices();
}

void CameraManagerDialog::onSwap()
{
    int idx0 = comboIndex0->currentIndex();
    int idx1 = comboIndex1->currentIndex();
    comboIndex0->setCurrentIndex(idx1);
    comboIndex1->setCurrentIndex(idx0);

    QString name0 = editName0->text();
    editName0->setText(editName1->text());
    editName1->setText(name0);

    int rot0 = comboRotation0->currentIndex();
    comboRotation0->setCurrentIndex(comboRotation1->currentIndex());
    comboRotation1->setCurrentIndex(rot0);
}

void CameraManagerDialog::onAccept()
{
    if (!cfg || !core) {
        accept();
        return;
    }
    CameraConfig c0 = cfg->camera(0);
    CameraConfig c1 = cfg->camera(1);
    c0.index = comboIndex0->currentData().toInt();
    c1.index = comboIndex1->currentData().toInt();
    c0.name = editName0->text();
    c1.name = editName1->text();
    c0.rotation = comboRotation0->currentData().toInt();
    c1.rotation = comboRotation1->currentData().toInt();

    cfg->setCameraConfig(0, c0);
    cfg->setCameraConfig(1, c1);
    cfg->setDualCameraMode(chkDualMode->isChecked());
    cfg->save(cfg->configPath());
    core->reloadCamerasFromConfig();
    accept();
}

