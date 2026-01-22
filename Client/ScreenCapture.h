#ifndef SCREENCAPTURE_H
#define SCREENCAPTURE_H

#include <QPixmap>
#include <QGuiApplication>
#include <QScreen>

class ScreenCapture
{
public:
    static QPixmap captureScreen();
    static bool checkPermission();
    static void requestPermission();
};


#ifdef Q_OS_MAC
void injectMouseClick(int x, int y, bool isPress);
#endif


#endif // SCREENCAPTURE_H
