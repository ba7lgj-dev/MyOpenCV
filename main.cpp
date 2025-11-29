#include "xingaodaapp.h"
#include "pushmanager.h"
#include "logmanager.h"
#include "configmanager.h"

#include <QApplication>
#include <exception>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    try {
        xingaodaApp w;
        w.show();
        return a.exec();
    } catch (const std::exception &ex) {
        LogManager::instance().logError(QStringLiteral("捕获异常：") + ex.what());
        ConfigManager cfg;
        cfg.load("config.json");
        PushManager push(&cfg);
        push.sendException(QString::fromUtf8(ex.what()));
        return -1;
    } catch (...) {
        LogManager::instance().logError(QStringLiteral("未知异常"));
        ConfigManager cfg;
        cfg.load("config.json");
        PushManager push(&cfg);
        push.sendException(QStringLiteral("系统异常：未知错误"));
        return -1;
    }
}
