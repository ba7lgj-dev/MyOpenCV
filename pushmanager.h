#ifndef PUSHMANAGER_H
#define PUSHMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include "configmanager.h"

class PushManager : public QObject
{
    Q_OBJECT
public:
    explicit PushManager(QObject *parent = nullptr);

    void configure(const PushConfig &cfg);
    bool sendText(const QString &content);
    bool testPush();
    int consecutiveFailures() const { return failureCount; }

signals:
    void failureThresholdReached(int count);
    void recovered();

private:
    bool attemptSend(const QJsonObject &payload);
    void markFailure();
    void markSuccess();

    QNetworkAccessManager nam;
    PushConfig config;
    int failureCount {0};
    int maxAttempts {3};
};

#endif // PUSHMANAGER_H

