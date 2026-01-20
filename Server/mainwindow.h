#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onNewConnection();             /* will be called when we have new client */
    void onReadyRead();                 /* will be called when data recieved from the client */

private:
    Ui::MainWindow *ui;                 /* the user interface object */
    QTcpServer *server;                 /*TCP IP server */
    QTcpSocket *clientSocket = nullptr;           /* connecting to the the client through the soclet */
    //QByteArray buffer;                  /* assemble the data before making the image the in tcp ip we are not sure that the data will be send in one time before showing it */
};

#endif // MAINWINDOW_H
