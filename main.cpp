#include "xingaodaapp.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    xingaodaApp w;
    w.show();
    return a.exec();
}
