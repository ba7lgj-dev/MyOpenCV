#include "pumpsettingsdialog.h"
#include "applicationcore.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QApplication>
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
    spinPrecheck = new QSpinBox(this);
    spinMonitor = new QSpinBox(this);
    spinCooldown = new QSpinBox(this);
    spinStartThreshold = new QDoubleSpinBox(this);
    spinStopThreshold = new QDoubleSpinBox(this);
    spinMinInflation = new QDoubleSpinBox(this);
    labelStatus = new QLabel(this);

    spinDuration->setRange(50, 20000);
    spinDuration->setSuffix(tr(" ms"));
    spinPrecheck->setRange(1000, 60000);
    spinPrecheck->setSuffix(tr(" ms"));
    spinMonitor->setRange(1000, 10000);
    spinMonitor->setSuffix(tr(" ms"));
    spinCooldown->setRange(0, 600000);
    spinCooldown->setSuffix(tr(" ms"));
    spinStartThreshold->setRange(10.0, 15000.0);
    spinStartThreshold->setDecimals(1);
    spinStopThreshold->setRange(10.0, 20000.0);
    spinStopThreshold->setDecimals(1);
    spinMinInflation->setRange(0.0, 5000.0);
    spinMinInflation->setDecimals(1);

    form->addRow(tr("加气串口"), comboPort);
    form->addRow(tr("单次加气时长"), spinDuration);
    form->addRow(tr("启动加气阈值 (mm)"), spinStartThreshold);
    form->addRow(tr("停止加气阈值 (mm)"), spinStopThreshold);
    form->addRow(tr("预判时间"), spinPrecheck);
    form->addRow(tr("监控窗口"), spinMonitor);
    form->addRow(tr("最小膨胀幅度 (mm)"), spinMinInflation);
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
    spinStartThreshold->setValue(appCfg.autoStartThresholdMM);
    spinStopThreshold->setValue(appCfg.autoStopThresholdMM);
    spinPrecheck->setValue(appCfg.autoPrecheckMs);
    spinMonitor->setValue(appCfg.autoMonitorMs);
    spinMinInflation->setValue(appCfg.autoMinInflationMM);
    spinCooldown->setValue(appCfg.autoCooldownMs);
}

void PumpSettingsDialog::onTestPort()
{
    if (!core) {
        labelStatus->setText(tr("无法测试串口：核心未初始化"));
        return;
    }

    const QString portName = comboPort->currentText();
    const int duration = spinDuration->value();
    labelStatus->setText(tr("正在测试 %1 ...").arg(portName));
    QApplication::processEvents();

    if (core->testPumpPulse(portName, duration)) {
        labelStatus->setText(tr("已发送加气脉冲：端口 %1，低电平 %2 ms").arg(portName).arg(duration));
    } else {
        labelStatus->setText(tr("串口不可用或触发失败"));
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
    cfg->setAutoStartThresholdMM(spinStartThreshold->value());
    cfg->setAutoStopThresholdMM(spinStopThreshold->value());
    cfg->setAutoPrecheckMs(spinPrecheck->value());
    cfg->setAutoMonitorMs(spinMonitor->value());
    cfg->setMinInflationMM(spinMinInflation->value());
    cfg->setAutoCooldownMs(spinCooldown->value());
    cfg->save(cfg->configPath());
    if (core) {
        core->reloadPumpConfig();
    }
    accept();
}
