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


#ifdef Q_OS_MAC
#include <ApplicationServices/ApplicationServices.h>

void injectMouseClick(int x, int y, bool isPress)
{
    // 1. تحديد النقطة (الإحداثيات)
    CGPoint point;
    point.x = x;
    point.y = y;

    // 2. تحديد نوع الحدث (ضغط أم رفع الإصبع)
    CGEventType type = isPress
        ? kCGEventLeftMouseDown
        : kCGEventLeftMouseUp;

    // 3. إنشاء حدث الماوس
    CGEventRef event = CGEventCreateMouseEvent(
        NULL,
        type,
        point,
        kCGMouseButtonLeft
    );

    // 4. إرسال الحدث للنظام ليتم تنفيذه فعلياً
    CGEventPost(kCGHIDEventTap, event);

    // 5. تنظيف الذاكرة (مهم جداً في C APIs)
    if (event) {
        CFRelease(event);
    }
}
#endif
