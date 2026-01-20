#include "FramesSender.h"



FramesSender::FramesSender(QObject* parent) : QObject(parent)
{
}

void FramesSender::setSocket(QTcpSocket* s)
{
    socket = s;
}


void FramesSender::encodeFrame(const QPixmap& pixmap)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
        return;

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "JPG", 30);                            /* saving the image in bytes with quality 30% (compressed Image)*/

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);             /* make the packet ready to be sent */
    out.setByteOrder(QDataStream::BigEndian);                   /* set the byte order to be compatible with the server */

    // **Packet Type = 1 (image)**
    out << static_cast<qint32>(1);

    /* send the image length first to garantee the image will be sent compeletly to the server */
    out << static_cast<qint32>(bytes.size());

    /* appent the message data after the length */
    packet.append(bytes);

    /* send the message on the on the TCP if the socket is connected */
    emit frameEncoded(packet);
}
