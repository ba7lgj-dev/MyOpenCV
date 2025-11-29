#include "xingaodaapp.h"
#include "configmanager.h"
#include "logmanager.h"
#include "pushmanager.h"

#include <QApplication>
#include <exception>

static void sendEmergencyPush(const QString &msg)
{
    ConfigManager cfg;
    cfg.load("config.json");
    PushManager push(&cfg);
    push.reloadConfig();
    push.sendText(msg, true);
}

int main(int argc, char *argv[])
{
    try {
        QApplication a(argc, argv);
        xingaodaApp w;
        w.show();
        return a.exec();
    } catch (const std::exception &ex) {
        const QString err = QStringLiteral("系统异常：%1").arg(ex.what());
        LogManager::instance().logError(err);
        sendEmergencyPush(err);
        return 1;
    } catch (...) {
        const QString err = QStringLiteral("系统异常：未知错误");
        LogManager::instance().logError(err);
        sendEmergencyPush(err);
        return 1;
    }
}
