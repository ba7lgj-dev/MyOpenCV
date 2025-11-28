#include "pushmanager.h"
#include "logmanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QEventLoop>

PushManager::PushManager(QObject *parent)
    : QObject(parent)
{
}

void PushManager::setConfig(const PushConfig &cfg)
{
    config = cfg;
}

int PushManager::failureCount() const
{
    return consecutiveFailures;
}

void PushManager::sendEvent(const QString &title, const QString &body)
{
    if (!config.enabled || config.url.isEmpty()) {
        return;
    }
    int attempts = 0;
    bool ok = false;
    while (attempts < 3 && !ok) {
        attempts++;
        ok = sendOnce(title, body);
    }
    if (!ok) {
        consecutiveFailures++;
        LogManager::instance().logWarn(tr("Push failed after retries: %1").arg(title));
        if (consecutiveFailures >= config.maxFailures) {
            emit warningRaised(tr("推送连续失败 %1 次，请检查网络或配置").arg(consecutiveFailures));
        }
    } else {
        consecutiveFailures = 0;
    }
}

QByteArray PushManager::buildPayload(const QString &title, const QString &body) const
{
    QJsonObject obj;
    obj.insert("token", config.token);
    obj.insert("title", title);
    obj.insert("content", config.templateText.isEmpty() ? body : config.templateText.arg(body));
    return QJsonDocument(obj).toJson();
}

bool PushManager::sendOnce(const QString &title, const QString &body)
{
    QNetworkRequest req{QUrl(config.url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto reply = nam.post(req, buildPayload(title, body));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok) {
        LogManager::instance().logWarn(tr("Push request error: %1").arg(reply->errorString()));
    }
    reply->deleteLater();
    return ok;
}

