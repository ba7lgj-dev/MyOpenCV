#include "pushsettingsdialog.h"
#include "applicationcore.h"
#include "logmanager.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDateTime>
#include <algorithm>

PushSettingsDialog::PushSettingsDialog(ApplicationCore *core, QWidget *parent)
    : QDialog(parent), core(core)
{
    cfg = core ? core->config() : nullptr;
    push = core ? core->pushManager() : nullptr;
    setWindowTitle(tr("推送配置"));
    setMinimumWidth(420);

    auto layout = new QVBoxLayout(this);
    auto form = new QFormLayout();

    editUrl = new QLineEdit(this);
    chkEnabled = new QCheckBox(tr("开启推送"), this);
    spinMaxFailures = new QSpinBox(this);
    spinThrottleSeconds = new QSpinBox(this);
    labelStatus = new QLabel(this);

    spinMaxFailures->setRange(1, 100);
    spinMaxFailures->setSuffix(tr(" 次"));
    spinThrottleSeconds->setRange(1, 600);
    spinThrottleSeconds->setSuffix(tr(" 秒"));

    form->addRow(tr("Webhook URL"), editUrl);
    form->addRow(tr("推送开关"), chkEnabled);
    form->addRow(tr("失败阈值"), spinMaxFailures);
    form->addRow(tr("限流窗口"), spinThrottleSeconds);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto btnTest = new QPushButton(tr("测试推送"), this);
    buttons->addButton(btnTest, QDialogButtonBox::ActionRole);

    layout->addLayout(form);
    layout->addWidget(labelStatus);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &PushSettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &PushSettingsDialog::reject);
    connect(btnTest, &QPushButton::clicked, this, &PushSettingsDialog::onTest);

    loadConfig();
}

void PushSettingsDialog::loadConfig()
{
    if (!cfg) return;
    auto pcfg = cfg->pushConfig();
    editUrl->setText(pcfg.url);
    chkEnabled->setChecked(pcfg.enabled);
    spinMaxFailures->setValue(pcfg.maxFailures);
    spinThrottleSeconds->setValue(std::max(1, pcfg.throttleWindowMs / 1000));
    labelStatus->setText(QString());
}

void PushSettingsDialog::onTest()
{
    if (!push) {
        labelStatus->setText(tr("无法测试推送：推送模块未初始化"));
        return;
    }

    PushConfig cfgTemp = cfg ? cfg->pushConfig() : PushConfig();
    cfgTemp.url = editUrl->text();
    cfgTemp.enabled = chkEnabled->isChecked();
    cfgTemp.maxFailures = spinMaxFailures->value();
    cfgTemp.throttleWindowMs = spinThrottleSeconds->value() * 1000;
    push->setConfig(cfgTemp);
    bool result = push->sendCustomMessage(tr("测试推送：%1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")), false);
    push->reloadConfig();
    LogManager::instance().logInfo(result ? tr("测试推送成功") : tr("测试推送失败，请查看上方响应记录"));
    labelStatus->setText(result ? tr("测试推送成功") : tr("测试推送失败，详情见日志"));
}

void PushSettingsDialog::onAccept()
{
    if (!cfg) {
        accept();
        return;
    }
    PushConfig p = cfg->pushConfig();
    p.url = editUrl->text();
    p.enabled = chkEnabled->isChecked();
    p.maxFailures = spinMaxFailures->value();
    p.throttleWindowMs = spinThrottleSeconds->value() * 1000;
    cfg->setPushConfig(p);
    cfg->save(cfg->configPath());
    if (push) {
        push->reloadConfig();
    }
    accept();
}

