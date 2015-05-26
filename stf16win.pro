#-------------------------------------------------
#
# Project created by QtCreator 2015-05-20T17:32:43
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = stf16win
TEMPLATE = app

#RESOURCES     = stf16win.qrc
win32:RC_FILE = stf16win.rc


SOURCES += main.cpp\
        widget.cpp

HEADERS  += widget.h

FORMS    += widget.ui

LIBMODBUS_PATH = $$PWD/../3rdparty/libmodbus/src
INCLUDEPATH += $$LIBMODBUS_PATH
LIBS += -L$$LIBMODBUS_PATH/win32
CONFIG(debug, debug|release) {
    LIBS += -lmodbusd
} else {
    LIBS += -lmodbus
}

DISTFILES += \
    README.md

OTHER_FILES += \
    stf16win.rc \
    3216.png \
    3216.ico
