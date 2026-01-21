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

#endif // SCREENCAPTURE_H
