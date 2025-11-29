#ifndef PUSHNOTIFIER_H
#define PUSHNOTIFIER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "configmanager.h"

class PushNotifier : public QObject
{
    Q_OBJECT
public:
    explicit PushNotifier(ConfigManager *cfg, QObject *parent = nullptr);

    void reload();
    void sendStartup();
    void sendShutdown();
    void sendException(const QString &detail);
    void sendPumpTriggered(double widthMM);
    void sendTest(const QString &text);
    int failureCount() const { return consecutiveFailures; }

signals:
    void pushFailed(const QString &msg, int failures);
    void pushSucceeded();
    void pushRecovered();

private:
    bool sendWithRetry(const QString &text, bool blocking = true);
    bool sendSingle(const QString &text, bool blocking);
    QString payloadForText(const QString &text) const;
    QString pushUrl() const;
    QString overridePath() const;

    ConfigManager *cfg {nullptr};
    QNetworkAccessManager manager;
    int consecutiveFailures {0};
};

#endif // PUSHNOTIFIER_H
