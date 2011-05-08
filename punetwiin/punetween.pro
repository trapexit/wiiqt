#-------------------------------------------------
#
# Project created by QtCreator 2010-12-17T16:22:53
#
#-------------------------------------------------

TARGET = punetwiin
CONFIG   += console
CONFIG   -= app_bundle
DEFINES += NAND_BIN_CAN_WRITE

TEMPLATE = app


SOURCES += main.cpp \
	../WiiQt/blocks0to7.cpp \
    ../WiiQt/nandbin.cpp \
	../WiiQt/tools.cpp \
    ../WiiQt/aes.c \
	../WiiQt/tiktmd.cpp \
	../WiiQt/sha1.c \
	../WiiQt/nandspare.cpp

HEADERS += ../WiiQt/nandbin.h \
    ../WiiQt/tools.h \
    ../WiiQt/blocks0to7.h \
	../WiiQt/nandspare.h
