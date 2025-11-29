#ifndef PUSHMANAGER_H
#define PUSHMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include "configmanager.h"

class PushManager : public QObject
{
    Q_OBJECT
public:
    explicit PushManager(ConfigManager *cfg, QObject *parent = nullptr);

    void reloadConfig();
    void setConfig(const PushConfig &cfg);
    void sendStartup();
    void sendShutdown();
    void sendException(const QString &errorMsg);
    void sendPumpTriggered(int cameraId, double widthMm);
    bool sendCustomMessage(const QString &text);

signals:
    void consecutiveFailuresExceeded(int count);
    void statusUpdated(int consecutiveFailures);

private:
    bool postMessage(const QString &text);
    bool isEnabled() const;

    ConfigManager *cfg {nullptr};
    PushConfig current;
    QNetworkAccessManager network;
    int consecutiveFailures {0};
    const int retryTimes {3};
};

#endif // PUSHMANAGER_H

