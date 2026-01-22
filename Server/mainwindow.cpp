#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPixmap>                    /* to show the image on the Qlabel */
#include <QDebug>                     /* to write debugging message on the screen */
#include <QMouseEvent>
#include <QtEndian>                   /* for qFromBigEndian */

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)                                        /* create new UI object */
    , server(new QTcpServer(this))                                  /* create new tcp server object */
    , clientSocket(nullptr)                                         /* initially there is no clients */
{
    ui->setupUi(this);                                              /* bind all the UI with the actual window */

    /* ✅ Install event filter ONLY on the label that displays client screen */
    ui->labelScreen->installEventFilter(this);
    ui->labelScreen->setMouseTracking(true);                        /* enable mouse move events without clicking */

    connect(server, &QTcpServer::newConnection,
            this, &MainWindow::onNewConnection);                    /* call onNewConnection when a client requests connection */

    if (!server->listen(QHostAddress::Any, 45454))                  /* listen to the port 45454 */
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

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onNewConnection()
{
    clientSocket = server->nextPendingConnection();
    connect(clientSocket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    qDebug() << "Client connected!";
}

void MainWindow::onReadyRead()
{
    recvBuffer.append(clientSocket->readAll());
    int offset = 0;
    const int total = recvBuffer.size();
    const uchar *dataPtr = (const uchar*)recvBuffer.constData();

    while (true)
    {
        if (total - offset < 4) break;
        qint32 packetType = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset));
        offset += 4;

        if (packetType == 1) /* IMAGE PACKET */
        {
            if (total - offset < 4) { offset -= 4; break; }
            qint32 imgSize = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset));
            offset += 4;
            if (total - offset < imgSize) { offset -= 8; break; }

            QByteArray imgData = recvBuffer.mid(offset, imgSize);
            offset += imgSize;

            QPixmap pix;
            if (pix.loadFromData(imgData, "JPG")) {
                currentPixmap = pix;
            }
        }
        else {
            break;
        }
    }
    if (offset > 0) recvBuffer.remove(0, offset);
}

/* This function captures mouse events on the label and triggers the packet sending */
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->labelScreen && (event->type() == QEvent::MouseMove ||
                                   event->type() == QEvent::MouseButtonPress ||
                                   event->type() == QEvent::MouseButtonRelease))
    {
        QMouseEvent *mouse = static_cast<QMouseEvent*>(event);

        // 1. نحسب النسبة المئوية لمكان الماوس جوه الـ Label (من 0 إلى 1)
        double normX = (double)mouse->position().x() / ui->labelScreen->width();
        double normY = (double)mouse->position().y() / ui->labelScreen->height();

        // 2. نضرب في 10000 عشان نبعتها كـ Integer دقيق
        int sendX = normX * 10000;
        int sendY = normY * 10000;

        /* Send the data */
        sendControlPacket(sendX, sendY, mouse->button(), event->type());

        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}

/* Private helper function to build and send the control packet */
void MainWindow::sendControlPacket(int x, int y, int button, int eventType)
{
    if (!clientSocket || clientSocket->state() != QAbstractSocket::ConnectedState)
        return;

    QByteArray ctrlPacket;
    QDataStream out(&ctrlPacket, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);

    /* Packet type 2 = Control */
    out << (qint32)2;
    out << (qint32)x;
    out << (qint32)y;
    out << (qint32)button;
    out << (qint32)eventType;

    clientSocket->write(ctrlPacket);
    clientSocket->flush();

    qDebug() << "[SERVER] Sent Control -> x:" << x << "y:" << y << "type:" << eventType;
}
