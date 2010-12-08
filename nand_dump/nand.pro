#-------------------------------------------------
#
# Project created by QtCreator 2010-12-02T23:30:12
#
#-------------------------------------------------

QT       += core gui\
	network

TARGET = nand
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    tools.cpp \
    uidmap.cpp \
    sharedcontentmap.cpp \
    sha1.c \
    tiktmd.cpp \
    aes.c \
    nusdownloader.cpp \
    nanddump.cpp \
    settingtxtdialog.cpp \
    wad.cpp

HEADERS  += mainwindow.h \
    tools.h \
    includes.h \
    uidmap.h \
    sharedcontentmap.h \
    sha1.h \
    tiktmd.h \
    aes.h \
    nusdownloader.h \
    nanddump.h \
    settingtxtdialog.h \
    wad.h

FORMS    += mainwindow.ui \
    settingtxtdialog.ui
