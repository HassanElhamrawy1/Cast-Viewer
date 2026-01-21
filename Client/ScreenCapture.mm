#include "ScreenCapture.h"
#include <QDebug>
#include <QScreen>
#include <QGuiApplication>

QPixmap ScreenCapture::captureScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qDebug() << "[ScreenCapture] No primary screen found";
        return QPixmap();
    }

    QPixmap pixmap = screen->grabWindow(0);

    if (pixmap.isNull()) {
        qDebug() << "[ScreenCapture] Failed to grab screen";
        return QPixmap();
    }

    qDebug() << "[ScreenCapture] Screen captured successfully:" << pixmap.width() << "x" << pixmap.height();
    return pixmap;
}

bool ScreenCapture::checkPermission()
{
    qDebug() << "[ScreenCapture] Permission check (Qt native - no permission needed)";
    return true;
}

void ScreenCapture::requestPermission()
{
    qDebug() << "[ScreenCapture] Permission request (Qt native - no permission needed)";
}
