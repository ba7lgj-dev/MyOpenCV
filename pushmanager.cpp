#include "pushmanager.h"
#include "logmanager.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QTimer>

PushManager::PushManager(ConfigManager *cfg, QObject *parent)
    : QObject(parent), cfg(cfg)
{
    connect(&manager, &QNetworkAccessManager::finished, this, &PushManager::onFinished);
}

void PushManager::sendEvent(const QString &title, const QString &detail)
{
    auto pushCfg = cfg->pushConfig();
    if (!pushCfg.enabled || pushCfg.url.isEmpty()) return;
    QString body = renderTemplate(title, detail);
    post(body, 3);
}

QString PushManager::renderTemplate(const QString &title, const QString &detail) const
{
    auto pushCfg = cfg->pushConfig();
    QString tpl = pushCfg.templateText.isEmpty() ? QStringLiteral("[事件] %1\n%2") : pushCfg.templateText;
    QString text = tpl.arg(title, detail);
    QJsonObject obj;
    obj.insert("token", pushCfg.token);
    obj.insert("text", text);
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void PushManager::post(const QString &payload, int retries)
{
    auto pushCfg = cfg->pushConfig();
    if (!pushCfg.enabled) return;
    QNetworkRequest req(pushCfg.url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QByteArray data = payload.toUtf8();
    QNetworkReply *reply = manager.post(req, data);
    reply->setProperty("retries", retries);
    reply->setProperty("payload", data);
}

void PushManager::setLastError(const QString &err)
{
    LogManager::instance().logError(err);
    emit message(err);
}

void PushManager::onFinished(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        int retries = reply->property("retries").toInt();
        if (retries > 0) {
            QTimer::singleShot(1000, this, [this, reply]() {
                int r = reply->property("retries").toInt();
                QByteArray payload = reply->property("payload").toByteArray();
                post(QString::fromUtf8(payload), r - 1);
            });
        } else {
            consecutiveFailures++;
            QString msg = tr("推送失败: %1").arg(reply->errorString());
            LogManager::instance().logError(msg);
            emit message(msg);
            if (consecutiveFailures >= cfg->pushConfig().maxFailures) {
                emit alarm(tr("推送连续失败，已超过阈值"));
            }
        }
    } else {
        consecutiveFailures = 0;
        QString msg = tr("推送成功: %1").arg(QString::fromUtf8(data));
        LogManager::instance().logInfo(msg);
        emit message(msg);
    }
    reply->deleteLater();
}
