#include "pumpsettingsdialog.h"
#include "applicationcore.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
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
    spinCooldown = new QSpinBox(this);
    spinPushFailures = new QSpinBox(this);
    editPushUrl = new QLineEdit(this);
    chkPushEnabled = new QCheckBox(tr("启用微信推送"), this);
    labelStatus = new QLabel(this);

    spinDuration->setRange(50, 20000);
    spinDuration->setSuffix(tr(" ms"));
    spinCooldown->setRange(0, 600000);
    spinCooldown->setSuffix(tr(" ms"));
    spinPushFailures->setRange(1, 20);
    spinPushFailures->setValue(3);

    form->addRow(tr("加气串口"), comboPort);
    form->addRow(tr("单次加气时长"), spinDuration);
    form->addRow(tr("加气冷却时间"), spinCooldown);
    form->addRow(tr("推送 Webhook"), editPushUrl);
    form->addRow(tr("推送失败阈值"), spinPushFailures);
    form->addRow(QString(), chkPushEnabled);

    auto btnRefresh = new QPushButton(tr("刷新串口"), this);
    auto btnTest = new QPushButton(tr("测试串口"), this);
    auto btnTestPush = new QPushButton(tr("测试推送"), this);
    auto hbox = new QHBoxLayout();
    hbox->addWidget(btnRefresh);
    hbox->addWidget(btnTest);
    hbox->addWidget(btnTestPush);
    hbox->addStretch();

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    layout->addLayout(form);
    layout->addLayout(hbox);
    layout->addWidget(labelStatus);
    layout->addWidget(buttons);

    connect(btnRefresh, &QPushButton::clicked, this, &PumpSettingsDialog::onRefreshPorts);
    connect(btnTest, &QPushButton::clicked, this, &PumpSettingsDialog::onTestPort);
    connect(btnTestPush, &QPushButton::clicked, this, [this](){
        if (!core) {
            labelStatus->setText(tr("无法测试推送：核心未初始化"));
            return;
        }
        bool ok = core->sendTestPush(tr("推送通道测试：请确认收到消息"));
        labelStatus->setText(ok ? tr("推送测试成功") : tr("推送测试失败"));
    });
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
    editPushUrl->setText(appCfg.push.url);
    chkPushEnabled->setChecked(appCfg.push.enabled);
    spinPushFailures->setValue(appCfg.push.maxFailures);
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
    cfg->setPumpCooldownMs(spinCooldown->value());
    PushConfig pushCfg = cfg->pushConfig();
    pushCfg.url = editPushUrl->text();
    pushCfg.enabled = chkPushEnabled->isChecked();
    pushCfg.maxFailures = spinPushFailures->value();
    cfg->setPushConfig(pushCfg);
    cfg->save(cfg->configPath());
    if (core) {
        core->reloadPumpConfig();
        core->reloadPushConfig();
    }
    accept();
}
