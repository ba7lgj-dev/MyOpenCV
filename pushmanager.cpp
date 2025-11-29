#include "pushmanager.h"
#include "logmanager.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>

PushManager::PushManager(QObject *parent)
    : QObject(parent)
{
}

void PushManager::configure(const PushConfig &cfg)
{
    config = cfg;
    if (config.maxFailures < 1) {
        config.maxFailures = 1;
    }
}

bool PushManager::sendText(const QString &content)
{
    if (!config.enabled || config.url.isEmpty()) {
        return true;
    }
    QJsonObject payload;
    payload.insert("msgtype", "text");
    QJsonObject textObj;
    textObj.insert("content", content);
    payload.insert("text", textObj);

    bool ok = attemptSend(payload);
    if (ok) {
        markSuccess();
    } else {
        markFailure();
    }
    return ok;
}

bool PushManager::testPush()
{
    return sendText(QObject::tr("推送通道测试：这是一条测试消息"));
}

bool PushManager::attemptSend(const QJsonObject &payload)
{
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        QNetworkRequest req(QUrl(config.url));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QJsonDocument doc(payload);
        QNetworkReply *reply = nam.post(req, doc.toJson());

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(5000);
        loop.exec();

        bool timedOut = !reply->isFinished();
        if (timedOut) {
            reply->abort();
        }

        bool ok = false;
        if (!timedOut && reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument respDoc = QJsonDocument::fromJson(data);
            if (!respDoc.isObject() || respDoc.object().value("errcode").toInt(0) == 0) {
                ok = true;
            }
        }
        reply->deleteLater();

        if (ok) {
            return true;
        }
    }
    return false;
}

void PushManager::markFailure()
{
    failureCount++;
    LogManager::instance().logWarn(tr("微信推送失败，连续失败次数：%1").arg(failureCount));
    if (failureCount > config.maxFailures) {
        emit failureThresholdReached(failureCount);
    }
}

void PushManager::markSuccess()
{
    if (failureCount > 0) {
        failureCount = 0;
        emit recovered();
    }
}

