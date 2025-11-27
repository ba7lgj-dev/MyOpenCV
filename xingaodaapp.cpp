#include "xingaodaapp.h"
#include "ui_xingaodaapp.h"

xingaodaApp::xingaodaApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::xingaodaApp)
{
    ui->setupUi(this);
}

xingaodaApp::~xingaodaApp()
{
    delete ui;
}

