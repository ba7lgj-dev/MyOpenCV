#include "pushmanager.h"
#include "logmanager.h"
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

PushManager::PushManager(ConfigManager *cfg, QObject *parent)
    : QObject(parent), config(cfg)
{
    reloadConfig();
}

bool PushManager::sendText(const QString &text, bool force)
{
    QMutexLocker locker(&mutex);
    if ((!enabled && !force) || url.isEmpty()) {
        return true;
    }

    QJsonObject body;
    body.insert("msgtype", "text");
    QJsonObject textObj;
    textObj.insert("content", text);
    body.insert("text", textObj);

    QString payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    bool success = postMessage(payload);
    if (success) {
        failureCount = 0;
    } else {
        failureCount++;
    }
    emit failureCountChanged(failureCount, maxFailures);
    return success;
}

bool PushManager::testCurrentConfig()
{
    return sendText(tr("测试推送"), true);
}

void PushManager::reloadConfig()
{
    if (!config) return;
    PushConfig cfg = config->pushConfig();
    QMutexLocker locker(&mutex);
    url = cfg.url;
    enabled = cfg.enabled;
    maxFailures = cfg.maxFailures > 0 ? cfg.maxFailures : 3;
}

bool PushManager::postMessage(const QString &payload)
{
    QNetworkRequest req(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    constexpr int maxAttempts = 3;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        QNetworkReply *reply = network.post(req, payload.toUtf8());
        bool ok = waitForReply(reply);
        if (ok && reply->error() == QNetworkReply::NoError) {
            reply->deleteLater();
            return true;
        }
        if (reply->error() != QNetworkReply::NoError) {
            LogManager::instance().logWarn(tr("Push failed (attempt %1): %2").arg(attempt + 1).arg(reply->errorString()));
        }
        reply->deleteLater();
        QThread::msleep(200);
    }
    return false;
}

bool PushManager::waitForReply(QNetworkReply *reply)
{
    if (!reply) return false;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(5000);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();
    if (timer.isActive()) {
        timer.stop();
        return true;
    }
    reply->abort();
    return false;
}

