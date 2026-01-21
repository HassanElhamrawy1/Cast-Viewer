#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPixmap>                    /* to show the image on the Qlabel */
#include <QDebug>                     /* to write debugging message on the screen */
#include <Qpainter>                   /* if we need to draw picture of fill spaces */
#include <QMouseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)                                        /* create new UI object */
    , server(new QTcpServer(this))                                  /* create new tcp server object and bind it to the parent */
    , clientSocket(nullptr)                                         /* initially there is no clients */
{
    ui->setupUi(this);                                              /* bind all the UI with the actual window */

    qApp->installEventFilter(this);                  /* if we use this here we are telling the Qt any Event occurs in the MainWindow send it to me first
                                                        but we need to monitor everything in the app so we need o use qApp */


    connect(server, &QTcpServer::newConnection,
            this, &MainWindow::onNewConnection);                    /* call onNewConnection when a client request new connection */

    if (!server->listen(QHostAddress::Any, 45454))       /* make sure that you listen to the port 45454 */
    {
        qDebug() << "Server failed to start!";
    } else
    {
        qDebug() << "Server listening on port 45454";
    }
    displayTimer = new QTimer(this);
    connect(displayTimer, &QTimer::timeout, this, [this]() {
        if (!currentPixmap.isNull()) {
            ui->labelScreen->setPixmap(
                currentPixmap.scaled(ui->labelScreen->size(),
                                     Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation)
                );
        }
    });
    displayTimer->start(33);

}



MainWindow::~MainWindow()             /* responsible for cleaning the memory after closing the application */
{
    delete ui;
}                                     /* note we do not need to delete the server or the clienr socket because we select them with this and Qt manage the memory automatically */

void MainWindow::onNewConnection()
{
    clientSocket = server->nextPendingConnection();            /* returning QTcpSocket of the new client "QTcpSocket" represent the client connection */
    connect(clientSocket, &QTcpSocket::readyRead,
            this, &MainWindow::onReadyRead);                   /* call "onReadyRead" directly after receiving data from the client */

    qDebug() << "Client connected!";                           /* write debug message to tell the user that the client is connected */
}

void MainWindow::onReadyRead()
{
    /* Append any new bytes to the receive buffer */
    recvBuffer.append(clientSocket->readAll());

    /* Try to parse as many full packets as possible */
    int offset = 0;
    const int total = recvBuffer.size();
    const uchar *dataPtr = (const uchar*)recvBuffer.constData();

    while (true)
    {
        /* Need at least 4 bytes for packetType */
        if (total - offset < 4)
            break;

        qint32 packetType = qFromBigEndian(*(const qint32*)(dataPtr + offset));
        offset += 4;

        /*************************** CASE 1: IMAGE PACKET ***************************/
        if (packetType == 1)
        {
            /* Image packet: need 4 bytes length + that many bytes */
            if (total - offset < 4)
            {
                offset -= 4; /* unread packetType */
                break;
            }

            qint32 imgSize = qFromBigEndian(*(const qint32*)(dataPtr + offset));
            offset += 4;

            /* Wait until full image arrives */
            if (total - offset < imgSize)
            {
                offset -= 8; /* unread type + size */
                break;
            }

            QByteArray imgData = recvBuffer.mid(offset, imgSize);
            offset += imgSize;

            /* Process the image (store in currentPixmap) */
            QPixmap pix;
            if (pix.loadFromData(imgData, "JPG"))
            {
                currentPixmap = pix;
                qDebug() << "[SERVER] Frame received:" << imgSize << "bytes";
            }
            else
            {
                qDebug() << "[SERVER] Failed to load image data";
            }
        }
        /*************************** CASE 2: CONTROL PACKET ***************************/
        else if (packetType == 2)
        {
            /* Mouse control packet - skip for now */
            qDebug() << "[SERVER] Received control packet (ignored)";
            break;
        }
        else
        {
            qDebug() << "[SERVER] Unknown packetType:" << packetType;
            break;
        }
    }

    /* Remove processed bytes from recvBuffer */
    if (offset > 0)
        recvBuffer.remove(0, offset);
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
        if (!clientSocket || clientSocket->state() != QAbstractSocket::ConnectedState)
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
        if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState)
        {
            clientSocket->write(ctrlPacket);
        }

        /* Returning true blocks local mouse interaction on the server window */
        return true;
    }

    /* Default handling for other events */
    return QMainWindow::eventFilter(obj, event);
}
