#include "pushmanager.h"
#include "logmanager.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QUrl>

PushManager::PushManager(QObject *parent)
    : QObject(parent)
{
}

void PushManager::setConfig(const PushConfig &cfg)
{
    config = cfg;
}

void PushManager::sendStartup()
{
    sendPayload("系统已启动", "应用启动完成");
}

void PushManager::sendShutdown()
{
    sendPayload("系统已关闭", "应用已正常退出");
}

void PushManager::sendException(const QString &message)
{
    sendPayload("系统异常", message);
}

void PushManager::sendCustom(const QString &event, const QString &content)
{
    sendPayload(event, content);
}

void PushManager::sendPayload(const QString &event, const QString &content, int retries)
{
    if (!config.enabled || config.url.isEmpty()) {
        return;
    }
    for (int i = 0; i < retries; ++i) {
        if (postOnce(event, content)) {
            if (consecutiveFailures > 0) {
                consecutiveFailures = 0;
                emit pushRecovered();
            }
            return;
        }
    }
    consecutiveFailures++;
    QString reason = tr("Push failed after %1 retries").arg(retries);
    LogManager::instance().logError(reason);
    emit pushFailed(reason);
}

bool PushManager::postOnce(const QString &event, const QString &content)
{
    QNetworkRequest req((QUrl(config.url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject payload{
        {"token", config.token},
        {"event", event},
        {"content", content},
        {"template", config.templateText}
    };
    QNetworkReply *reply = network.post(req, QJsonDocument(payload).toJson());
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    bool success = reply->error() == QNetworkReply::NoError;
    if (!success) {
        LogManager::instance().logWarn(tr("Push send error: %1").arg(reply->errorString()));
    }
    reply->deleteLater();
    return success;
}
