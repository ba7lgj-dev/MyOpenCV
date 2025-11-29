#ifndef PUSHMANAGER_H
#define PUSHMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMutex>
#include "configmanager.h"

class PushManager : public QObject
{
    Q_OBJECT
public:
    explicit PushManager(ConfigManager *cfg, QObject *parent = nullptr);

    bool sendText(const QString &text, bool force = false);
    bool testCurrentConfig();
    void reloadConfig();
    int failureThreshold() const { return maxFailures; }
    int consecutiveFailures() const { return failureCount; }

signals:
    void failureCountChanged(int count, int threshold);

private:
    bool postMessage(const QString &payload);
    bool waitForReply(QNetworkReply *reply);

    ConfigManager *config {nullptr};
    QNetworkAccessManager network;
    mutable QMutex mutex;
    QString url;
    bool enabled {false};
    int maxFailures {3};
    int failureCount {0};
};

#endif // PUSHMANAGER_H
