#-------------------------------------------------
#
# Project created by QtCreator 2011-01-05T10:05:50
#
#-------------------------------------------------

QT       += core gui multimedia

TARGET = thp_player
TEMPLATE = app


SOURCES += main.cpp\
        thpwindow.cpp \
    gcvid.cpp

HEADERS  += thpwindow.h \
    gcvid.h

FORMS    += thpwindow.ui

CONFIG += static
LIBS += -ljpeg

RESOURCES += \
    rc.qrc
