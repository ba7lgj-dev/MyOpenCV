#include "pushsettingsdialog.h"
#include "applicationcore.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>

PushSettingsDialog::PushSettingsDialog(ApplicationCore *core, QWidget *parent)
    : QDialog(parent), core(core)
{
    cfg = core ? core->config() : nullptr;
    setWindowTitle(tr("推送配置"));
    setMinimumWidth(460);

    auto layout = new QVBoxLayout(this);
    auto form = new QFormLayout();

    editUrl = new QLineEdit(this);
    chkEnabled = new QCheckBox(tr("启用推送"), this);
    spinMaxFailures = new QSpinBox(this);
    spinMaxFailures->setRange(1, 100);
    spinMaxFailures->setSuffix(tr(" 次"));
    labelStatus = new QLabel(this);

    form->addRow(tr("推送 URL"), editUrl);
    form->addRow(tr("连续失败上限"), spinMaxFailures);
    form->addRow(QString(), chkEnabled);

    auto btnTest = new QPushButton(tr("测试推送"), this);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    layout->addLayout(form);
    layout->addWidget(btnTest);
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
    const auto push = cfg->pushConfig();
    editUrl->setText(push.url);
    chkEnabled->setChecked(push.enabled);
    spinMaxFailures->setValue(push.maxFailures > 0 ? push.maxFailures : 3);
}

void PushSettingsDialog::onAccept()
{
    if (!cfg) {
        accept();
        return;
    }

    PushConfig push = cfg->pushConfig();
    push.url = editUrl->text();
    push.enabled = chkEnabled->isChecked();
    push.maxFailures = spinMaxFailures->value();
    cfg->setPushConfig(push);
    if (core) {
        core->reloadPushConfig();
    }
    accept();
}

void PushSettingsDialog::onTest()
{
    if (!core) {
        labelStatus->setText(tr("无法测试：核心未初始化"));
        return;
    }
    PushConfig push = cfg ? cfg->pushConfig() : PushConfig();
    push.url = editUrl->text();
    push.enabled = chkEnabled->isChecked();
    push.maxFailures = spinMaxFailures->value();
    if (cfg) {
        cfg->setPushConfig(push);
    }
    core->reloadPushConfig();
    bool ok = core->pushManager()->testCurrentConfig();
    labelStatus->setText(ok ? tr("测试推送成功") : tr("测试推送失败"));
}

