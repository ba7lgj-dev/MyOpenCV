#ifndef XINGAODAAPP_H
#define XINGAODAAPP_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class xingaodaApp; }
QT_END_NAMESPACE

class xingaodaApp : public QMainWindow
{
    Q_OBJECT

public:
    xingaodaApp(QWidget *parent = nullptr);
    ~xingaodaApp();

private:
    Ui::xingaodaApp *ui;
};
#endif // XINGAODAAPP_H
