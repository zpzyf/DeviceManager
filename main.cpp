#include "mainwindow.h"
#include <QApplication>
#include <QTextCodec>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("GB2312"));
    a.setWindowIcon(QIcon(":/images/app.ico"));
    MainWindow w;
    w.show();

    return a.exec();
}
