#include "pushmanager.h"
#include "logmanager.h"
#include <QDebug>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

PushManager::PushManager(ConfigManager *cfg, QObject *parent)
    : QObject(parent), cfg(cfg)
{
    reloadConfig();
}

void PushManager::reloadConfig()
{
    if (cfg) {
        current = cfg->pushConfig();
    }
}

void PushManager::setConfig(const PushConfig &cfgValue)
{
    current = cfgValue;
}

bool PushManager::isEnabled() const
{
    return current.enabled && !current.url.isEmpty();
}

void PushManager::sendStartup()
{
    postMessage(tr("系统已启动"));
}

void PushManager::sendShutdown()
{
    postMessage(tr("系统已关闭"));
}

bool PushManager::sendException(const QString &key, const QString &errorMsg)
{
    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    const QString text = tr("系统异常（%1）：%2").arg(timestamp, errorMsg);
    return sendThrottled(key, text);
}

void PushManager::sendPumpTriggered(int cameraId, double widthMm)
{
    postMessage(tr("自动加气已触发，摄像头%1 当前宽度 %2 mm").arg(cameraId).arg(widthMm, 0, 'f', 2));
}

bool PushManager::sendCustomMessage(const QString &text, bool countFailure)
{
    return postMessage(text, countFailure);
}

bool PushManager::sendThrottled(const QString &key, const QString &text, bool countFailure)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!AlertRateLimiter::instance().shouldAllow(key, throttleWindowMs(), nowMs)) {
        const QString skipped = tr("异常重复，已限流：%1").arg(key);
        LogManager::instance().logInfo(skipped);
        qInfo().noquote() << skipped;
        return false;
    }

    const bool success = postMessage(text, countFailure);
    if (success) {
        AlertRateLimiter::instance().markSent(key, nowMs);
    } else {
        const QString failed = tr("推送失败未更新限流窗口（%1）：%2")
                              .arg(timestampString(nowMs))
                              .arg(key);
        LogManager::instance().logWarn(failed);
    }
    return success;
}

bool PushManager::postMessage(const QString &text, bool countFailure)
{

    const QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    const QString finalText = ensureTimestamp(text, timeStr);
    qDebug()<< QSslSocket::sslLibraryBuildVersionString();

    if (!isEnabled()) {
        if (countFailure) {
            consecutiveFailures = 0;
            emit statusUpdated(consecutiveFailures);
        }
        return false;
    }

    if (!ensureSslAvailable()) {
        if (countFailure) {
            consecutiveFailures++;
            if (consecutiveFailures >= current.maxFailures) {
                emit consecutiveFailuresExceeded(consecutiveFailures);
            }
            emit statusUpdated(consecutiveFailures);
        }
        return false;
    }

    bool success = false;
    for (int attempt = 0; attempt < retryTimes; ++attempt) {
        QNetworkRequest req(QUrl(current.url));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QJsonObject payload;
        payload.insert("msgtype", "text");
        QJsonObject textObj;
        textObj.insert("content", finalText);
        payload.insert("text", textObj);

        QNetworkReply *reply = network.post(req, QJsonDocument(payload).toJson());
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        QByteArray body = reply->readAll();
        const QString bodyText = QString::fromUtf8(body);
        if (reply->error() == QNetworkReply::NoError && statusCode.toInt() / 100 == 2) {
            const QString successMsg = tr("推送成功（%1，HTTP %2）：%3")
                                         .arg(timeStr)
                                         .arg(statusCode.toInt())
                                         .arg(bodyText);
            LogManager::instance().logInfo(successMsg);
            qInfo().noquote() << successMsg;
            success = true;
            reply->deleteLater();
            break;
        }
        const QString errorMsg = tr("推送失败（%1，尝试 %2/%3，HTTP %4）：%5 | 响应：%6")
                                    .arg(timeStr)
                                    .arg(attempt + 1)
                                    .arg(retryTimes)
                                    .arg(statusCode.toInt())
                                    .arg(reply->errorString())
                                    .arg(bodyText);
        LogManager::instance().logWarn(errorMsg);
        qWarning().noquote() << errorMsg;
        reply->deleteLater();
    }

    if (countFailure) {
        if (success) {
            consecutiveFailures = 0;
        } else {
            consecutiveFailures++;
            if (consecutiveFailures >= current.maxFailures) {
                emit consecutiveFailuresExceeded(consecutiveFailures);
            }
        }
        emit statusUpdated(consecutiveFailures);
    }
    return success;
}

bool PushManager::ensureSslAvailable()
{
    qDebug()<< QSslSocket::sslLibraryBuildVersionString();
    if (!QSslSocket::supportsSsl()) {
        if (sslAvailable) {
            LogManager::instance().logError(tr("推送失败：当前环境不支持HTTPS，请确认已安装 OpenSSL 库 (例如 libcrypto-1_1-x64.dll 和 libssl-1_1-x64.dll)") );
        }
        sslAvailable = false;
        return false;
    }

    if (!sslAvailable) {
        LogManager::instance().logInfo(tr("已检测到HTTPS支持恢复，可正常发送推送"));
    }
    sslAvailable = true;
    return true;
}

QString PushManager::ensureTimestamp(const QString &text, const QString &timestamp) const
{
    static const QRegularExpression tsPattern(QStringLiteral("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}"));
    if (text.contains(tsPattern)) {
        return text;
    }
    const QString ts = timestamp.isEmpty() ? QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") : timestamp;
    return QStringLiteral("%1 %2").arg(ts, text);
}

int PushManager::throttleWindowMs() const
{
    return current.throttleWindowMs > 0 ? current.throttleWindowMs : 10000;
}

QString PushManager::timestampString(qint64 msSinceEpoch) const
{
    return QDateTime::fromMSecsSinceEpoch(msSinceEpoch).toString("yyyy-MM-dd HH:mm:ss");
}

