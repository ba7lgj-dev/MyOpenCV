#ifndef PUSHMANAGER_H
#define PUSHMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "configmanager.h"

class PushManager : public QObject {
    Q_OBJECT
public:
    explicit PushManager(QObject *parent = nullptr);
    void setConfig(const PushConfig &cfg);
    void sendEvent(const QString &title, const QString &body);
    int failureCount() const;

signals:
    void warningRaised(const QString &msg);

private:
    bool sendOnce(const QString &title, const QString &body);
    QByteArray buildPayload(const QString &title, const QString &body) const;

    QNetworkAccessManager nam;
    PushConfig config;
    int consecutiveFailures {0};
};

#endif // PUSHMANAGER_H
