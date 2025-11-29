#include "alertratelimiter.h"

#include <QMutexLocker>
#include <QtGlobal>

AlertRateLimiter &AlertRateLimiter::instance()
{
    static AlertRateLimiter inst;
    return inst;
}

bool AlertRateLimiter::shouldAllow(const QString &key, int windowMs, qint64 nowMs) const
{
    const int window = effectiveWindow(windowMs);
    QMutexLocker locker(&mutex);
    const qint64 last = lastSentMap.value(key, -1);
    if (last < 0) {
        return true;
    }
    return nowMs - last >= window;
}

void AlertRateLimiter::markSent(const QString &key, qint64 timestampMs)
{
    QMutexLocker locker(&mutex);
    lastSentMap.insert(key, timestampMs);
}

qint64 AlertRateLimiter::lastSent(const QString &key) const
{
    QMutexLocker locker(&mutex);
    return lastSentMap.value(key, -1);
}

int AlertRateLimiter::effectiveWindow(int windowMs) const
{
    constexpr int defaultWindow = 10000; // 10 seconds
    if (windowMs <= 0) {
        return defaultWindow;
    }
    return windowMs;
}
