QT += widgets network gui

CONFIG += c++17

SOURCES += main.cpp \
    framessender.cpp \
    mainwindow.cpp

HEADERS += mainwindow.h \
    FramesSender.h \
    ScreenCapture.h

FORMS += mainwindow.ui

macx {
    OBJECTIVE_SOURCES += ScreenCapture.mm

    LIBS += -framework AppKit
    LIBS += -framework CoreGraphics
    LIBS += -framework CoreMedia
    LIBS += -framework ApplicationServices

    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.13

    # Add entitlements for screen recording
    QMAKE_INFO_PLIST = Info.plist
}

DISTFILES += \
    Info.plist

