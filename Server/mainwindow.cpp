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
    static QByteArray buffer;                               /* the data can be received partially so we need a buffer to assemble the parts in it*/
    buffer.append(clientSocket->readAll());                 /* append the upcoming data on the client socket in it*/

    while (buffer.size() >= 4)                              /* the first 4 bytes represent the photo length so we need to read it first */
    {
        QDataStream stream(buffer);
        stream.setByteOrder(QDataStream::BigEndian);        /* to read the length prefix in a correct way here it is BigEndian*/
        qint32 imgSize;                                     /* variable to dave the image size */
        stream >> imgSize;                                  /* storing the image size in the variable */

        if (buffer.size() < imgSize + 4) break;             /* if the image is not reveived completely  break the loop and wait for the new data */

        QByteArray imgData = buffer.mid(4, imgSize);        /* extract the image data after the first 4 bytes */
        buffer.remove(0, 4 + imgSize);                      /* remove the data from the buffer after processing it */

        QPixmap pix;
        pix.loadFromData(imgData, "JPG");                   /* transform the data into a form can be presented by Qt */

        /* show the image in the Qlabel */
        ui->labelScreen->setPixmap(pix.scaled(
            ui->labelScreen->size(),
            Qt::KeepAspectRatio,                /* to keep the wide and hite of the image */
            Qt::SmoothTransformation            /* make the image smooth when we change the size */
            ));
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
