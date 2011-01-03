#-------------------------------------------------
#
# Project created by QtCreator 2010-12-02T23:30:12
#
#-------------------------------------------------

QT       += core gui\
	network

TARGET = ohneschwanzenegger
TEMPLATE = app
DEFINES += NAND_BIN_CAN_WRITE


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
    ../WiiQt/sha1.c \
    newnandbin.cpp \
    ../WiiQt/nandbin.cpp \
    ../WiiQt/nandspare.cpp\
    ../WiiQt/blocks0to7.cpp

HEADERS  += mainwindow.h \
    ../WiiQt/tools.h \
    ../WiiQt/uidmap.h \
    ../WiiQt/sharedcontentmap.h \
    ../WiiQt/tiktmd.h \
    ../WiiQt/nusdownloader.h \
    ../WiiQt/uidmap.h \
    ../WiiQt/nanddump.h \
    ../WiiQt/settingtxtdialog.h \
    ../WiiQt/wad.h \
    newnandbin.h \
    ../WiiQt/nandbin.h \
    ../WiiQt/nandspare.h\
    ../WiiQt/blocks0to7.h

FORMS    += mainwindow.ui \
    ../WiiQt/settingtxtdialog.ui \
    newnandbin.ui

RESOURCES += \
    rc.qrc
