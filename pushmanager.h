#ifndef PUSHMANAGER_H
#define PUSHMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "configmanager.h"

class PushManager : public QObject
{
    Q_OBJECT
public:
    explicit PushManager(QObject *parent = nullptr);
    void setConfig(const PushConfig &cfg);
    void sendStartup();
    void sendShutdown();
    void sendException(const QString &message);
    void sendCustom(const QString &event, const QString &content);
    int failureCount() const { return consecutiveFailures; }

signals:
    void pushFailed(const QString &reason);
    void pushRecovered();

private:
    void sendPayload(const QString &event, const QString &content, int retries = 3);
    bool postOnce(const QString &event, const QString &content);

    QNetworkAccessManager network;
    PushConfig config;
    int consecutiveFailures {0};
};

#endif // PUSHMANAGER_H
