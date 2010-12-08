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
    savebanner.cpp \
    savelistitem.cpp \
    saveloadthread.cpp \
    tools.cpp

HEADERS  += mainwindow.h \
    includes.h \
    savebanner.h \
    savelistitem.h \
    saveloadthread.h \
    tools.h

FORMS    += mainwindow.ui

RESOURCES += \
    rc.qrc
