#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTimer>
#include "FramesSender.h"
#include <QThread>
#include <QtEndian>

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
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void sendScreen();
    void onReadyRead();             /* Parse control packets mouse/keyboard */
    void processControlPacket(QByteArray &data);       /* slot for socket readyRead */

signals:
    void frameReady(const QPixmap& pixmap);

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    QTimer *timer;
    QThread* senderThread;
    FramesSender* frameSender;
    QByteArray recvBuffer;                      /* Buffer for incoming Bytes */

};

#endif // MAINWINDOW_H
