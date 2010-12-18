#-------------------------------------------------
#
# Project created by QtCreator 2010-12-17T16:22:53
#
#-------------------------------------------------

#QT       += core\
#	    gui

TARGET = nandBinCheck
#CONFIG   += console
#CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    ../WiiQt/blocks0to7.cpp \
    ../WiiQt/tiktmd.cpp \
    ../WiiQt/nandbin.cpp \
    ../WiiQt/tools.cpp \
    ../WiiQt/savebanner.cpp \
    ../WiiQt/aes.c \
    ../WiiQt/sha1.c \
    ../WiiQt/uidmap.cpp \
    ../WiiQt/sharedcontentmap.cpp \
    ../WiiQt/nandspare.cpp

HEADERS += ../WiiQt/tiktmd.h \
    ../WiiQt/nandbin.h \
    ../WiiQt/tools.h \
    ../WiiQt/blocks0to7.h \
    ../WiiQt/uidmap.h \
    ../WiiQt/sharedcontentmap.h \
    ../WiiQt/nandspare.h
