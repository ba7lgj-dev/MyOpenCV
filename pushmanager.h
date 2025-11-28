#ifndef PUSHMANAGER_H
#define PUSHMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include "configmanager.h"
#include "logmanager.h"

class WechatPushManager : public QObject
{
    Q_OBJECT
public:
    explicit WechatPushManager(ConfigManager *config, QObject *parent = nullptr);

    void notifyStartup();
    void notifyShutdown();
    void notifyException(const QString &error);
    void sendTestMessage(const QString &text);

private:
    bool sendPayload(const QString &content, int retries = 3);

    ConfigManager *cfg {nullptr};
    QNetworkAccessManager nam;
    int consecutiveFailures {0};
};

#endif // PUSHMANAGER_H
