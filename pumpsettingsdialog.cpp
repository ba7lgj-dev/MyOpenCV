#include "pumpsettingsdialog.h"
#include "applicationcore.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSerialPort>

PumpSettingsDialog::PumpSettingsDialog(ApplicationCore *core, QWidget *parent)
    : QDialog(parent), core(core)
{
    cfg = core ? core->config() : nullptr;
    setWindowTitle(tr("自动加气配置"));
    setMinimumWidth(420);

    auto layout = new QVBoxLayout(this);
    auto form = new QFormLayout();

    comboPort = new QComboBox(this);
    spinDuration = new QSpinBox(this);
    spinCooldown = new QSpinBox(this);
    labelStatus = new QLabel(this);

    spinDuration->setRange(50, 20000);
    spinDuration->setSuffix(tr(" ms"));
    spinCooldown->setRange(0, 600000);
    spinCooldown->setSuffix(tr(" ms"));

    form->addRow(tr("加气串口"), comboPort);
    form->addRow(tr("单次加气时长"), spinDuration);
    form->addRow(tr("加气冷却时间"), spinCooldown);

    auto btnRefresh = new QPushButton(tr("刷新串口"), this);
    auto btnTest = new QPushButton(tr("测试串口"), this);
    auto hbox = new QHBoxLayout();
    hbox->addWidget(btnRefresh);
    hbox->addWidget(btnTest);
    hbox->addStretch();

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    layout->addLayout(form);
    layout->addLayout(hbox);
    layout->addWidget(labelStatus);
    layout->addWidget(buttons);

    connect(btnRefresh, &QPushButton::clicked, this, &PumpSettingsDialog::onRefreshPorts);
    connect(btnTest, &QPushButton::clicked, this, &PumpSettingsDialog::onTestPort);
    connect(buttons, &QDialogButtonBox::accepted, this, &PumpSettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &PumpSettingsDialog::reject);

    onRefreshPorts();
    loadConfig();
}

void PumpSettingsDialog::populatePorts()
{
    comboPort->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &p : ports) {
        comboPort->addItem(p.portName());
    }
    if (comboPort->count() == 0) {
        comboPort->addItem("COM1");
    }
}

void PumpSettingsDialog::onRefreshPorts()
{
    populatePorts();
}

void PumpSettingsDialog::loadConfig()
{
    if (!cfg) return;
    const auto appCfg = cfg->config();
    int idx = comboPort->findText(appCfg.pumpPort);
    if (idx >= 0) {
        comboPort->setCurrentIndex(idx);
    } else if (!appCfg.pumpPort.isEmpty()) {
        comboPort->addItem(appCfg.pumpPort);
        comboPort->setCurrentIndex(comboPort->count() - 1);
    }
    spinDuration->setValue(appCfg.pumpDurationMs);
    spinCooldown->setValue(appCfg.pumpCooldownMs);
}

void PumpSettingsDialog::onTestPort()
{
    QSerialPort port(comboPort->currentText());
    port.setBaudRate(QSerialPort::Baud9600);
    if (port.open(QIODevice::ReadWrite)) {
        labelStatus->setText(tr("串口可用"));
        port.close();
    } else {
        labelStatus->setText(tr("串口不可用: %1").arg(port.errorString()));
    }
}

void PumpSettingsDialog::onAccept()
{
    if (!cfg) {
        accept();
        return;
    }
    cfg->setPumpPort(comboPort->currentText());
    cfg->setPumpDurationMs(spinDuration->value());
    cfg->setPumpCooldownMs(spinCooldown->value());
    cfg->save(cfg->configPath());
    if (core) {
        core->reloadPumpConfig();
    }
    accept();
}
