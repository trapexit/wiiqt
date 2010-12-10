#-------------------------------------------------
#
# Project created by QtCreator 2010-12-02T02:54:54
#
#-------------------------------------------------

QT       += core gui

TARGET = saveToy
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    savelistitem.cpp \
    saveloadthread.cpp \
    ../WiiQt/tools.cpp \
    ../WiiQt/savebanner.cpp \
    ../WiiQt/aes.c \
    ../WiiQt/sha1.c

HEADERS  += mainwindow.h \
    savelistitem.h \
    saveloadthread.h\
    ../WiiQt/tools.h

FORMS    += mainwindow.ui

RESOURCES += \
    rc.qrc

INCPATH += "../WiiQt"
