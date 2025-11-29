#ifndef ALERTRATELIMITER_H
#define ALERTRATELIMITER_H

#include <QHash>
#include <QMutex>
#include <QString>

class AlertRateLimiter
{
public:
    static AlertRateLimiter &instance();

    bool shouldAllow(const QString &key, int windowMs, qint64 nowMs) const;
    void markSent(const QString &key, qint64 timestampMs);
    qint64 lastSent(const QString &key) const;

private:
    AlertRateLimiter() = default;
    AlertRateLimiter(const AlertRateLimiter &) = delete;
    AlertRateLimiter &operator=(const AlertRateLimiter &) = delete;

    int effectiveWindow(int windowMs) const;

    mutable QMutex mutex;
    QHash<QString, qint64> lastSentMap;
};

#endif // ALERTRATELIMITER_H
