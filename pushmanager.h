#ifndef PUSHMANAGER_H
#define PUSHMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "configmanager.h"

class PushManager : public QObject
{
    Q_OBJECT
public:
    explicit PushManager(ConfigManager *cfg, QObject *parent = nullptr);
    void sendEvent(const QString &title, const QString &detail = QString());
    void setLastError(const QString &err);
    int failureCount() const { return consecutiveFailures; }

signals:
    void message(const QString &msg);
    void alarm(const QString &msg);

private slots:
    void onFinished(QNetworkReply *reply);

private:
    void post(const QString &payload, int retries = 3);
    QString renderTemplate(const QString &title, const QString &detail) const;

    ConfigManager *cfg {nullptr};
    QNetworkAccessManager manager;
    int consecutiveFailures {0};
};

#endif // PUSHMANAGER_H
