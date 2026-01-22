#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTimer>
#include "FramesSender.h"
#include <QThread>
#include <QtEndian>
#include "ScreenCapture.h"

class FramesSender;
class QThread;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void sendScreen();
    void onReadyRead();             /* Parse control packets mouse/keyboard */

signals:
    void frameReady(const QPixmap& pixmap);

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    QTimer *timer;
    QThread* senderThread;
    FramesSender* frameSender;
    QByteArray recvBuffer;                      /* Buffer for incoming Bytes */
    int remoteScreenWidth = 1440;
    int remoteScreenHeight = 900;

};

#endif // MAINWINDOW_H
