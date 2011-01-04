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
    ../WiiQt/tools.cpp \
    ../WiiQt/sharedcontentmap.cpp \
    ../WiiQt/tiktmd.cpp \
    ../WiiQt/nusdownloader.cpp \
    ../WiiQt/uidmap.cpp \
    ../WiiQt/nanddump.cpp \
    ../WiiQt/settingtxtdialog.cpp \
    ../WiiQt/wad.cpp \
    ../WiiQt/aes.c \
    ../WiiQt/sha1.c

HEADERS  += mainwindow.h \
    ../WiiQt/tools.h \
    ../WiiQt/uidmap.h \
    ../WiiQt/sharedcontentmap.h \
    ../WiiQt/tiktmd.h \
    ../WiiQt/nusdownloader.h \
    ../WiiQt/uidmap.h \
    ../WiiQt/nanddump.h \
    ../WiiQt/settingtxtdialog.h \
    ../WiiQt/wad.h

FORMS    += mainwindow.ui \
    ../WiiQt/settingtxtdialog.ui

RESOURCES += \
    rc.qrc
