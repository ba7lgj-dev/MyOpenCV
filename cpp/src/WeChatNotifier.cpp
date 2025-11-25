#include "WeChatNotifier.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

WeChatNotifier::WeChatNotifier(QObject *parent) : QObject(parent) {
    connect(&m_manager, &QNetworkAccessManager::finished, this, &WeChatNotifier::onFinished);
}

void WeChatNotifier::setWebhook(const QString &url) {
    m_webhookUrl = url;
}

void WeChatNotifier::sendAlarm(const QString &title, const QString &content) {
    if (m_webhookUrl.isEmpty()) {
        return;
    }
    QJsonObject text;
    text.insert("content", title + "\n" + content);
    QJsonObject root;
    root.insert("msgtype", "text");
    root.insert("text", text);

    QNetworkRequest request(m_webhookUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_manager.post(request, QJsonDocument(root).toJson());
}

void WeChatNotifier::onFinished(QNetworkReply *reply) {
    QString result = reply->error() == QNetworkReply::NoError
                         ? QString::fromUtf8(reply->readAll())
                         : reply->errorString();
    emit notifyFinished(result);
    reply->deleteLater();
}

