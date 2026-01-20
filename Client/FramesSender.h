#ifndef FRAMESSENDER_H
#define FRAMESSENDER_H

#include <QObject>
#include <QTcpSocket>
#include <QPixmap>
#include <QBuffer>
#include <QDataStream>

class FramesSender : public QObject
{
    Q_OBJECT

public:
    explicit FramesSender(QObject* parent);
    void setSocket(QTcpSocket* s);

signals:
    void frameEncoded(const QByteArray& packet );

public slots:
    void encodeFrame(const QPixmap& pixmap);          /*  */

private:
    QTcpSocket* socket = nullptr;
};

#endif
