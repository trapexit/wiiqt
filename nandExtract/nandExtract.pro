# -------------------------------------------------
# Project created by QtCreator 2010-12-06T03:40:50
# -------------------------------------------------
TARGET = nandExtract
TEMPLATE = app
SOURCES += main.cpp \
    nandwindow.cpp \
    ../WiiQt/blocks0to7.cpp \
    ../WiiQt/tiktmd.cpp \
    ../WiiQt/nandbin.cpp \
    ../WiiQt/tools.cpp \
    ../WiiQt/savebanner.cpp \
    ../WiiQt/aes.c \
    ../WiiQt/sha1.c \
    nandthread.cpp \
    boot2infodialog.cpp

HEADERS += nandwindow.h \
    ../WiiQt/tiktmd.h \
    ../WiiQt/nandbin.h \
    ../WiiQt/tools.h \
    nandthread.h \
    boot2infodialog.h

FORMS += nandwindow.ui \
    boot2infodialog.ui

RESOURCES += \
    rc.qrc
