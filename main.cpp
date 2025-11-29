#include "xingaodaapp.h"
#include "configmanager.h"
#include "pushnotifier.h"
#include "logmanager.h"

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
        LogManager::instance().logError(QString("Fatal exception: %1").arg(ex.what()));
        ConfigManager cfg;
        cfg.load("config.json");
        PushNotifier notifier(&cfg);
        notifier.sendException(QString::fromUtf8(ex.what()));
    } catch (...) {
        LogManager::instance().logError("Fatal unknown exception");
        ConfigManager cfg;
        cfg.load("config.json");
        PushNotifier notifier(&cfg);
        notifier.sendException(QObject::tr("未知异常"));
    }
    return -1;
}
