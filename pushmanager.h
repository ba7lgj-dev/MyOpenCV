#ifndef PUSHMANAGER_H
#define PUSHMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslSocket>
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
    bool sendCustomMessage(const QString &text, bool countFailure = true);

signals:
    void consecutiveFailuresExceeded(int count);
    void statusUpdated(int consecutiveFailures);

private:
    bool postMessage(const QString &text, bool countFailure = true);
    bool isEnabled() const;
    bool ensureSslAvailable();

    ConfigManager *cfg {nullptr};
    PushConfig current;
    QNetworkAccessManager network;
    int consecutiveFailures {0};
    const int retryTimes {3};
    bool sslAvailable {true};
};

#endif // PUSHMANAGER_H

