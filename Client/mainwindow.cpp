#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScreen>                      /* to get a screenshot (APIs to capture the screen image) */
#include <QGuiApplication>              /* to get a screenshot (APIs to capture the screen image) */
#include <QBuffer>                      /* to store the image into buffer before sending it  */
#include <QDebug>                       /* to write debig messages on the screen */
#include "FramesSender.h"
#include <QThread>
#include <QDataStream>                  /* to read the packets */
#include <QCursor>
#include <QApplication>
#include <QWindow>
#include <QWidget>
#include <QMouseEvent>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , socket(new QTcpSocket(this))
    , timer(new QTimer(this))
{
    ui->setupUi(this);

    qApp->installEventFilter(this);                  /* if we use this here we are telling the Qt any Event occurs in the MainWindow send it to me first
                                                        but we need to monitor everything in the app so we need o use qApp */

    /* default: connect to localhost; edit UI later to change IP */
    socket->connectToHost("127.0.0.1", 45454);


    /* Create a separate thread for sending frames */
    senderThread = new QThread(this);
    frameSender = new FramesSender(nullptr);
    frameSender->setSocket(socket);

    /* Move worker to thread */
    frameSender->moveToThread(senderThread);

    /* Start thread */
    senderThread->start();

    /* Connect signal to worker */
    connect(this, &MainWindow::frameReady, frameSender, &FramesSender::encodeFrame);

    //connect(frameSender, &FramesSender::frameEncoded, this, [this](const QByteArray& packet){ socket->write(packet);});

    connect(frameSender, &FramesSender::frameEncoded, this, [this](const QByteArray &packet){
        if (socket && socket->state() == QAbstractSocket::ConnectedState)
        {
            socket->write(packet);
            qDebug() << "[CLIENT] Sent frame:" << packet.size() << "bytes";
        }
    });

    /* when we receive readyRead signal on the network the onReadyRead will be called */
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);

    connect(timer, &QTimer::timeout, this, &MainWindow::sendScreen);  /* call sendScreen on every timeout of the timer */
    timer->start(100); /* 10 frames every 1 second */
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::sendScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();         /* capter the mainscreen */
    if (!screen || socket->state() != QAbstractSocket::ConnectedState) return;   /* return if the socket is not connected to the server  or if there is no screen */

    QPixmap pix = screen->grabWindow(0);                       /* 0 means we capturing the whole screen */
    /* scaling down the image before compressing */
    pix = pix.scaled(pix.width()/2, pix.height()/2, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    emit frameReady(pix);                             /* send the frame worker to the thread */

}


void MainWindow::onReadyRead()
{
    /* Append any new bytes to the receive buffer */
    recvBuffer.append(socket->readAll());

    /* Try to parse as many full packets as possible */
    int offset = 0;
    const int total = recvBuffer.size();
    const uchar *dataPtr = (const uchar*)recvBuffer.constData();

    while (true)
    {
        /* Need at least 4 bytes for packetType */
        if (total - offset < 4)
            break;

        qint32 packetType = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset));
        offset += 4;

        /***************************** CASE 1: IMAGE PACKET *****************************/
        if (packetType == 1)
        {
            /* Image packet: need 4 bytes length + that many bytes */
            if (total - offset < 4)
            {
                offset -= 4; /* unread packetType */
                break;
            }

            qint32 imgSize = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset));
            offset += 4;

            /* Wait until full image arrives */
            if (total - offset < imgSize)
            {
                offset -= 8; /* unread type + size */
                break;
            }

            QByteArray imgData = recvBuffer.mid(offset, imgSize);
            offset += imgSize;

            /* Process the image (display) */
            QPixmap pix;
            if (pix.loadFromData(imgData, "JPG"))
            {
                ui->labelInfo->setPixmap(
                    pix.scaled(ui->labelInfo->size(),
                               Qt::KeepAspectRatio,
                               Qt::SmoothTransformation)
                    );
            }
            else
            {
                qDebug() << "[CLIENT] Failed to load image data";
            }
        }
        /***************************** NO MORE CONTROL PACKETS HERE *****************************/
        else
        {
            qDebug() << "[CLIENT] Unknown packetType:" << packetType;
            /* If unknown, stop to avoid infinite loop */
            break;
        }
    }

    /* Remove processed bytes from recvBuffer */
    if (offset > 0)
        recvBuffer.remove(0, offset);
}





void MainWindow::processControlPacket( QByteArray &data)
{
    QDataStream in(&data, QIODevice::ReadOnly);
    in.setByteOrder(QDataStream::BigEndian);

    /* If the buffer might contain multiple packets, you should loop; for simplicity we assume single packet.*/
    /* Read packet type */
    qint32 packetType = 0;
    if (in.status() != QDataStream::Ok) return;
    in >> packetType;

    if (packetType == 2) // Mouse event
    {
        int x = 0, y = 0, button = 0, action = 0;
        in >> x >> y >> button >> action;

        /* DEBUG: print what we received */
        qDebug() << "[CLIENT] Mouse Event Received: pos=" << x << y << " button=" << button << " action=" << action;

        /* Map server coordinates to client screen coordinates if resolutions differ.
           Here we assume server sends screen coordinates relative to server screen size.
           If you know serverScreenSize (wServer,hServer), scale to local:
             int localX = x * (localWidth / (double)wServer);
             int localY = y * (localHeight / (double)hServer);
           For now, assume same resolution: */
        int localX = x;
        int localY = y;

        /* Move cursor */
        QCursor::setPos(localX, localY);

        /* Simulate mouse press/release depending on action:
                action==0 -> move only; action==1 -> press; action==2 -> release; action==3 -> click */
        if (action == 1 || action == 2 || action == 3)
        {
            /* find the widget under cursor */
            QPoint globalPos(localX, localY);
            QWidget *w = QApplication::widgetAt(globalPos);
            if (!w) w = this; /* fallback */

            /* Translate global position to widget-local coordinates */
            QPoint posInWidget = w->mapFromGlobal(globalPos);

            Qt::MouseButton mb = Qt::LeftButton;
            if (button == 2) mb = Qt::RightButton;
            else if (button == 3) mb = Qt::MiddleButton;

            if (action == 1) // press
            {
                QMouseEvent pressEv(QEvent::MouseButtonPress, posInWidget, globalPos, mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &pressEv);
            }
            else if (action == 2) // release
            {
                QMouseEvent releaseEv(QEvent::MouseButtonRelease, posInWidget, globalPos, mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &releaseEv);
            }
            else if (action == 3) // click = press+release
            {
                QMouseEvent pressEv(QEvent::MouseButtonPress, posInWidget, globalPos, mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &pressEv);
                QMouseEvent releaseEv(QEvent::MouseButtonRelease, posInWidget, globalPos, mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &releaseEv);
            }
        }
    }
    else
    {
        qDebug() << "[CLIENT] Unknown packetType:" << packetType;
        /* If packetType==1 (frame) or other, you should forward data to your frame parser. */
    }
}

/* this function will be called everytime we event happened */
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    /* Handle only mouse-related events */
    if (event->type() == QEvent::MouseMove ||
        event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease)
    {
        /* Convert the generic event into a QMouseEvent */
        QMouseEvent *mouse = static_cast<QMouseEvent*>(event);

        /* Ensure socket is valid and connected */
        if (!socket || socket->state() != QAbstractSocket::ConnectedState)
            return false;

        /* Prepare a control packet to send mouse input to the client */
        QByteArray ctrlPacket;
        QDataStream out(&ctrlPacket, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::BigEndian);

        /* Packet type = 2 (mouse control packet) */
        out << static_cast<qint32>(2);

        /* Send mouse coordinates */
        out << static_cast<qint32>(mouse->position().x());
        out << static_cast<qint32>(mouse->position().y());

        /* Send mouse button */
        out << static_cast<qint32>(mouse->button());

        /* Send event type (press / release / move) */
        out << static_cast<qint32>(event->type());

        /* Send the packet to the client */
        if (socket && socket->state() == QAbstractSocket::ConnectedState)
        {
            socket->write(ctrlPacket);
        }




        /* Returning true blocks local mouse interaction on the server window */
        return true;
    }

    /* Default handling for other events */
    return QMainWindow::eventFilter(obj, event);
}
