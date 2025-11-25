#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class WeChatNotifier : public QObject {
    Q_OBJECT
public:
    explicit WeChatNotifier(QObject *parent = nullptr);

    void setWebhook(const QString &url);
    void sendAlarm(const QString &title, const QString &content);

signals:
    void notifyFinished(const QString &result);

private slots:
    void onFinished(QNetworkReply *reply);

private:
    QString m_webhookUrl;
    QNetworkAccessManager m_manager;
};

