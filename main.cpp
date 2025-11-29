#include "xingaodaapp.h"
#include "configmanager.h"
#include "pushmanager.h"
#include "logmanager.h"

#include <QApplication>
#include <exception>

static void sendEmergencyPush(const QString &message)
{
    ConfigManager cfg;
    cfg.load("config.json");
    PushManager pusher;
    PushConfig pushCfg = cfg.pushConfig();
    pushCfg.enabled = true; // 异常情况下强制推送
    pusher.configure(pushCfg);
    pusher.sendText(message);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    try {
        xingaodaApp w;
        w.show();
        return a.exec();
    } catch (const std::exception &e) {
        const QString msg = QObject::tr("系统异常：%1").arg(e.what());
        LogManager::instance().logError(msg);
        sendEmergencyPush(msg);
    } catch (...) {
        const QString msg = QObject::tr("系统异常：未知错误");
        LogManager::instance().logError(msg);
        sendEmergencyPush(msg);
    }
    return -1;
}
