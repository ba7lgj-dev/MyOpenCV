#include "pushmanager.h"
#include "logmanager.h"
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

PushManager::PushManager(ConfigManager *cfg, QObject *parent)
    : QObject(parent), cfg(cfg)
{
    reloadConfig();
}

void PushManager::reloadConfig()
{
    if (cfg) {
        current = cfg->pushConfig();
    }
}

void PushManager::setConfig(const PushConfig &cfgValue)
{
    current = cfgValue;
}

bool PushManager::isEnabled() const
{
    return current.enabled && !current.url.isEmpty();
}

void PushManager::sendStartup()
{
    postMessage(tr("系统已启动"));
}

void PushManager::sendShutdown()
{
    postMessage(tr("系统已关闭"));
}

void PushManager::sendException(const QString &errorMsg)
{
    postMessage(tr("系统异常：%1").arg(errorMsg));
}

void PushManager::sendPumpTriggered(int cameraId, double widthMm)
{
    postMessage(tr("自动加气已触发，摄像头%1 当前宽度 %2 mm").arg(cameraId).arg(widthMm, 0, 'f', 2));
}

bool PushManager::sendCustomMessage(const QString &text)
{
    return postMessage(text);
}

bool PushManager::postMessage(const QString &text)
{
    if (!isEnabled()) {
        consecutiveFailures = 0;
        emit statusUpdated(consecutiveFailures);
        return false;
    }

    bool success = false;
    for (int attempt = 0; attempt < retryTimes; ++attempt) {
        QNetworkRequest req(QUrl(current.url));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QJsonObject payload;
        payload.insert("msgtype", "text");
        QJsonObject textObj;
        textObj.insert("content", text);
        payload.insert("text", textObj);

        QNetworkReply *reply = network.post(req, QJsonDocument(payload).toJson());
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (reply->error() == QNetworkReply::NoError && statusCode.toInt() / 100 == 2) {
            success = true;
            reply->deleteLater();
            break;
        }
        LogManager::instance().logWarn(tr("推送失败（尝试 %1/%2）：%3").arg(attempt + 1).arg(retryTimes).arg(reply->errorString()));
        reply->deleteLater();
    }

    if (success) {
        consecutiveFailures = 0;
    } else {
        consecutiveFailures++;
        if (consecutiveFailures >= current.maxFailures) {
            emit consecutiveFailuresExceeded(consecutiveFailures);
        }
    }
    emit statusUpdated(consecutiveFailures);
    return success;
}

