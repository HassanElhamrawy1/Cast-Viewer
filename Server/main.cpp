#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);     /* needed to work with every Qt app abd the ui */
    MainWindow w;                   /* create new window */
    w.show();                       /* showing the window */
    return a.exec();                /* start events loop */
}
