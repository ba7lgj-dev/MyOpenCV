#include "pushmanager.h"
#include <QEventLoop>
#include <QTimer>

WechatPushManager::WechatPushManager(ConfigManager *config, QObject *parent)
    : QObject(parent), cfg(config)
{
}

void WechatPushManager::notifyStartup()
{
    sendPayload(tr("系统已启动"));
}

void WechatPushManager::notifyShutdown()
{
    sendPayload(tr("系统已关闭"));
}

void WechatPushManager::notifyException(const QString &error)
{
    sendPayload(tr("系统异常：%1").arg(error));
}

void WechatPushManager::sendTestMessage(const QString &text)
{
    sendPayload(text.isEmpty() ? tr("测试推送") : text);
}

bool WechatPushManager::sendPayload(const QString &content, int retries)
{
    if (!cfg) return false;
    PushConfig push = cfg->pushConfig();
    if (!push.enabled || push.url.isEmpty()) {
        LogManager::instance().logWarn(tr("Push disabled or URL empty, skip push: %1").arg(content));
        return false;
    }

    QJsonObject payload;
    payload.insert("token", push.token);
    payload.insert("template", push.templateText);
    payload.insert("content", content);
    QNetworkRequest req(QUrl(push.url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    for (int attempt = 0; attempt < retries; ++attempt) {
        QNetworkReply *reply = nam.post(req, QJsonDocument(payload).toJson());
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(5000);
        loop.exec();
        bool timedOut = !reply->isFinished();
        if (timedOut) {
            reply->abort();
        }
        QByteArray data = reply->readAll();
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (!timedOut && status >= 200 && status < 300) {
            consecutiveFailures = 0;
            LogManager::instance().logInfo(tr("Push success: %1").arg(content));
            return true;
        }
        LogManager::instance().logWarn(tr("Push failed (attempt %1/%2): %3, status %4")
                                     .arg(attempt + 1)
                                     .arg(retries)
                                     .arg(QString::fromUtf8(data))
                                     .arg(status));
    }

    consecutiveFailures++;
    if (consecutiveFailures >= push.maxFailures) {
        LogManager::instance().logError(tr("Push failure exceeded limit: %1").arg(consecutiveFailures));
    }
    return false;
}
