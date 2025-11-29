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
#include <cmath>

PumpSettingsDialog::PumpSettingsDialog(ApplicationCore *core, QWidget *parent)
    : QDialog(parent), core(core)
{
    cfg = core ? core->config() : nullptr;
    setWindowTitle(tr("自动加气配置"));
    setMinimumWidth(420);

    auto layout = new QVBoxLayout(this);
    auto form = new QFormLayout();

    comboPort = new QComboBox(this);
    spinDuration = new QDoubleSpinBox(this);
    spinPrecheck = new QDoubleSpinBox(this);
    spinMonitor = new QDoubleSpinBox(this);
    spinCooldown = new QDoubleSpinBox(this);
    spinStartThreshold = new QDoubleSpinBox(this);
    spinStopThreshold = new QDoubleSpinBox(this);
    spinMinInflation = new QDoubleSpinBox(this);
    labelStatus = new QLabel(this);

    spinDuration->setRange(0.05, 20.0);
    spinDuration->setDecimals(3);
    spinDuration->setSuffix(tr(" s"));
    spinPrecheck->setRange(1.0, 60.0);
    spinPrecheck->setDecimals(3);
    spinPrecheck->setSuffix(tr(" s"));
    spinMonitor->setRange(1.0, 10.0);
    spinMonitor->setDecimals(3);
    spinMonitor->setSuffix(tr(" s"));
    spinCooldown->setRange(0.0, 600.0);
    spinCooldown->setDecimals(3);
    spinCooldown->setSuffix(tr(" s"));
    spinStartThreshold->setRange(1.0, 2000.0);
    spinStartThreshold->setDecimals(2);
    spinStartThreshold->setSuffix(tr(" cm"));
    spinStopThreshold->setRange(1.0, 2000.0);
    spinStopThreshold->setDecimals(2);
    spinStopThreshold->setSuffix(tr(" cm"));
    spinMinInflation->setRange(0.0, 500.0);
    spinMinInflation->setDecimals(2);
    spinMinInflation->setSuffix(tr(" cm"));

    form->addRow(tr("加气串口"), comboPort);
    form->addRow(tr("单次加气时长"), spinDuration);
    form->addRow(tr("启动加气阈值 (cm)"), spinStartThreshold);
    form->addRow(tr("停止加气阈值 (cm)"), spinStopThreshold);
    form->addRow(tr("预判时间"), spinPrecheck);
    form->addRow(tr("监控窗口"), spinMonitor);
    form->addRow(tr("最小膨胀幅度 (cm)"), spinMinInflation);
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
    spinDuration->setValue(appCfg.pumpDurationMs / 1000.0);
    spinStartThreshold->setValue(appCfg.autoStartThresholdMM / 10.0);
    spinStopThreshold->setValue(appCfg.autoStopThresholdMM / 10.0);
    spinPrecheck->setValue(appCfg.autoPrecheckMs / 1000.0);
    spinMonitor->setValue(appCfg.autoMonitorMs / 1000.0);
    spinMinInflation->setValue(appCfg.autoMinInflationMM / 10.0);
    spinCooldown->setValue(appCfg.autoCooldownMs / 1000.0);
}

void PumpSettingsDialog::onTestPort()
{
    if (!core) {
        labelStatus->setText(tr("无法测试串口：核心未初始化"));
        return;
    }

    const QString portName = comboPort->currentText();
    const double durationSec = spinDuration->value();
    const int duration = static_cast<int>(std::lround(durationSec * 1000.0));
    labelStatus->setText(tr("正在测试 %1 ...").arg(portName));
    QApplication::processEvents();

    if (core->testPumpPulse(portName, duration)) {
        labelStatus->setText(tr("已发送加气脉冲：端口 %1，低电平 %2 s").arg(portName).arg(durationSec, 0, 'f', 3));
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
    cfg->setPumpDurationMs(static_cast<int>(std::lround(spinDuration->value() * 1000.0)));
    cfg->setAutoStartThresholdMM(spinStartThreshold->value() * 10.0);
    cfg->setAutoStopThresholdMM(spinStopThreshold->value() * 10.0);
    cfg->setAutoPrecheckMs(static_cast<int>(std::lround(spinPrecheck->value() * 1000.0)));
    cfg->setAutoMonitorMs(static_cast<int>(std::lround(spinMonitor->value() * 1000.0)));
    cfg->setMinInflationMM(spinMinInflation->value() * 10.0);
    cfg->setAutoCooldownMs(static_cast<int>(std::lround(spinCooldown->value() * 1000.0)));
    cfg->save(cfg->configPath());
    if (core) {
        core->reloadPumpConfig();
    }
    accept();
}
