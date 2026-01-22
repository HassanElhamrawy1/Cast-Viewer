#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ScreenCapture.h"
#include <QScreen>
#include <QGuiApplication>
#include <QBuffer>
#include <QDebug>
#include "FramesSender.h"
#include <QThread>
#include <QDataStream>

#ifdef Q_OS_MAC
#include <ApplicationServices/ApplicationServices.h>
#endif
#include <QtEndian>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , socket(new QTcpSocket(this))
    , timer(new QTimer(this))
{
    ui->setupUi(this);

    /* Connect to server (localhost for testing) */
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

    connect(frameSender, &FramesSender::frameEncoded, this, [this](const QByteArray &packet){
        if (socket && socket->state() == QAbstractSocket::ConnectedState)
        {
            socket->write(packet);
            // qDebug() << "[CLIENT] Sent frame:" << packet.size() << "bytes";  /* uncomment for debugging */
        }
    });

    /* When we receive data from server, call onReadyRead */
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);

    /* Start screen capture timer (10 FPS) */
    connect(timer, &QTimer::timeout, this, &MainWindow::sendScreen);
    timer->start(100);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::sendScreen()
{
    /* Check if socket is connected */
    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
        return;

    /* Use ScreenCapture wrapper (handles macOS + fallback) */
    QPixmap pix = ScreenCapture::captureScreen();

    if (pix.isNull()) {
        qDebug() << "[CLIENT] Failed to capture screen";
        return;
    }

    /* Scale down the image before compressing */
    pix = pix.scaled(pix.width()/2, pix.height()/2,
                     Qt::KeepAspectRatio, Qt::SmoothTransformation);

    /* Convert to JPEG */
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    pix.save(&buffer, "JPG", 85);
    buffer.close();

    /* Send packet: [packetType=1][imgSize][imgData] */
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);

    out << static_cast<qint32>(1);  // packetType = 1 (image)
    out << static_cast<qint32>(jpegData.size());
    packet.append(jpegData);

    /* Send to server */
    socket->write(packet);
    socket->flush();
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
            if (total - offset < 4) { offset -= 4; break; }
            qint32 imgSize = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset));
            offset += 4;
            if (total - offset < imgSize) { offset -= 8; break; }

            QByteArray imgData = recvBuffer.mid(offset, imgSize);
            offset += imgSize;

            /* Process the image (display) */
            QPixmap pix;
            if (pix.loadFromData(imgData, "JPG"))
            {
                ui->labelInfo->setPixmap(pix.scaled(ui->labelInfo->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        /***************************** CASE 2: CONTROL PACKET *****************************/
        else if (packetType == 2)
        {
            /* Control packet: need 16 bytes (x, y, button, eventType) */
            if (total - offset < 16) { offset -= 4; break; }

            qint32 x = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset));
            offset += 4;
            qint32 y = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset));
            offset += 4;
            qint32 button = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset));
            offset += 4;
            qint32 eventType = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset));
            offset += 4;

            /* DEBUG: Print what we received */
            qDebug() << "[CLIENT] Received Control: x=" << x << "y=" << y << "button=" << button << "eventType=" << eventType;

            /* NATIVE MACOS MOUSE INJECTION */
/* NATIVE MACOS MOUSE INJECTION (NORMALIZED) */
#ifdef Q_OS_MAC
            // 1. نرجع النسبة المئوية لأصلها
            double normX = x / 10000.0;
            double normY = y / 10000.0;

            // 2. نجيب حجم الشاشة الـ Logical (Points) بتاعة الماك حالياً
            QScreen *screen = QGuiApplication::primaryScreen();
            if (screen) {
                QRect screenRect = screen->geometry();

                // 3. نحسب المكان النهائي بناءً على حجم الشاشة الحقيقي
                double finalX = screenRect.width() * normX;
                double finalY = screenRect.height() * normY;

                CGPoint point = CGPointMake(finalX, finalY);
                CGEventRef eventRef = nullptr;

                // تحديد نوع الـ Event
                if (eventType == 5) { // MouseMove
                    eventRef = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, point, kCGMouseButtonLeft);
                    qDebug() << "[CLIENT] Injecting MouseMove at" << finalX << finalY;
                } else if (eventType == 2) { // MouseButtonPress
                    eventRef = CGEventCreateMouseEvent(nullptr, kCGEventLeftMouseDown, point, kCGMouseButtonLeft);
                    qDebug() << "[CLIENT] Injecting MousePress at" << finalX << finalY;
                } else if (eventType == 3) { // MouseButtonRelease
                    eventRef = CGEventCreateMouseEvent(nullptr, kCGEventLeftMouseUp, point, kCGMouseButtonLeft);
                    qDebug() << "[CLIENT] Injecting MouseRelease at" << finalX << finalY;
                }

                if (eventRef) {
                    CGEventPost(kCGHIDEventTap, eventRef);
                    CFRelease(eventRef);
                    qDebug() << "[CLIENT] Event posted successfully";
                } else {
                    qDebug() << "[CLIENT] Failed to create CGEvent for type:" << eventType;
                }
            }
#else
            qDebug() << "[CLIENT] Not on macOS, skipping injection";
#endif
        }
        else
        {
            qDebug() << "[CLIENT] Unknown packetType:" << packetType;
            break;
        }
    }

    if (offset > 0)
        recvBuffer.remove(0, offset);
}
