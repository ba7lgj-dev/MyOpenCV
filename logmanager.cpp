#include "logmanager.h"
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QMutexLocker>

LogManager &LogManager::instance()
{
    static LogManager inst;
    return inst;
}

LogManager::LogManager()
{
    rotateIfNeeded();
}

QString LogManager::makePrefix(const QString &level) const
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss ") + level + " ";
}

void LogManager::rotateIfNeeded()
{
    QString date = QDate::currentDate().toString("yyyy-MM-dd");
    if (date == currentDate && currentFile.isOpen()) {
        return;
    }
    currentDate = date;
    if (currentFile.isOpen()) currentFile.close();
    QDir().mkpath("logs");
    currentFile.setFileName("logs/" + currentDate + ".log");
    currentFile.open(QIODevice::Append | QIODevice::Text);
}

void LogManager::writeLine(const QString &line)
{
    QMutexLocker locker(&mutex);
    rotateIfNeeded();
    if (!currentFile.isOpen()) return;
    QTextStream ts(&currentFile);
    ts << line << "\n";
    currentFile.flush();
}

void LogManager::logInfo(const QString &msg)
{
    writeLine(makePrefix("[INFO]") + msg);
}

void LogManager::logWarn(const QString &msg)
{
    writeLine(makePrefix("[WARN]") + msg);
}

void LogManager::logError(const QString &msg)
{
    writeLine(makePrefix("[ERROR]") + msg);
}

