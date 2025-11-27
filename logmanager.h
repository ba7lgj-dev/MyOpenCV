#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QString>
#include <QFile>
#include <QMutex>

class LogManager {
public:
    static LogManager &instance();
    void logInfo(const QString &msg);
    void logWarn(const QString &msg);
    void logError(const QString &msg);

private:
    LogManager();
    QString makePrefix(const QString &level) const;
    void rotateIfNeeded();
    void writeLine(const QString &line);

    QFile currentFile;
    mutable QMutex mutex;
    QString currentDate;
};

#endif // LOGMANAGER_H
