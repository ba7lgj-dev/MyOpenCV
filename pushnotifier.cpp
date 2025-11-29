#include "pushnotifier.h"
#include "logmanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QNetworkRequest>

PushNotifier::PushNotifier(ConfigManager *cfg, QObject *parent)
    : QObject(parent), cfg(cfg)
{
    reload();
}

void PushNotifier::reload()
{
    if (!cfg) return;
    cfg->loadPushOverride();
}

QString PushNotifier::payloadForText(const QString &text) const
{
    QJsonObject obj;
    obj.insert("msgtype", "text");
    QJsonObject textObj;
    textObj.insert("content", text);
    obj.insert("text", textObj);
    QJsonDocument doc(obj);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

QString PushNotifier::pushUrl() const
{
    if (!cfg) return {};
    const PushConfig pushCfg = cfg->pushConfig();
    QString url = pushCfg.url;
    if (url.isEmpty()) {
        const QString overrideFile = overridePath();
        QFile f(overrideFile);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                url = doc.object().value("url").toString();
            }
        }
    }
    return url;
}

QString PushNotifier::overridePath() const
{
    if (!cfg) return QString();
    QFileInfo fi(cfg->configPath());
    if (fi.isDir()) {
        return fi.filePath() + "/push.json";
    }
    return fi.absoluteDir().filePath("push.json");
}

bool PushNotifier::sendSingle(const QString &text, bool blocking)
{
    if (!cfg) return false;
    const PushConfig pushCfg = cfg->pushConfig();
    if (!pushCfg.enabled) {
        return true;
    }

    const QString url = pushUrl();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const QByteArray data = payloadForText(text).toUtf8();
    QNetworkReply *reply = manager.post(request, data);

    if (!blocking) {
        return true;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(5000);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();
    const bool ok = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();
    return ok;
}

bool PushNotifier::sendWithRetry(const QString &text, bool blocking)
{
    if (!cfg || !cfg->pushConfig().enabled) {
        return true;
    }
    const QString url = pushUrl();
    if (url.isEmpty()) {
        consecutiveFailures++;
        const QString msg = tr("未配置推送URL");
        emit pushFailed(msg, consecutiveFailures);
        LogManager::instance().logWarn(tr("Push skipped: empty url"));
        return false;
    }

    QString payloadText = text;
    if (cfg) {
        const QString tpl = cfg->pushConfig().templateText;
        if (!tpl.isEmpty()) {
            payloadText = tpl.arg(text);
        }
    }
    for (int i = 0; i < 3; ++i) {
        if (sendSingle(payloadText, blocking)) {
            const bool wasFailing = consecutiveFailures > 0;
            consecutiveFailures = 0;
            emit pushSucceeded();
            if (wasFailing) emit pushRecovered();
            return true;
        }
    }
    consecutiveFailures++;
    const QString msg = tr("推送失败，连续失败 %1 次").arg(consecutiveFailures);
    emit pushFailed(msg, consecutiveFailures);
    LogManager::instance().logError(msg + ": " + payloadText);
    return false;
}

void PushNotifier::sendStartup()
{
    sendWithRetry(tr("系统已启动"));
}

void PushNotifier::sendShutdown()
{
    sendWithRetry(tr("系统已关闭"));
}

void PushNotifier::sendException(const QString &detail)
{
    sendWithRetry(tr("系统异常：%1").arg(detail));
}

void PushNotifier::sendPumpTriggered(double widthMM)
{
    sendWithRetry(tr("检测到宽度偏低，触发加气，当前宽度 %1 mm").arg(widthMM, 0, 'f', 1), true);
}

void PushNotifier::sendTest(const QString &text)
{
    sendWithRetry(text.isEmpty() ? tr("测试推送") : text);
}

