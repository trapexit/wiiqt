#-------------------------------------------------
#
# Project created by QtCreator 2011-12-12T00:35:52
#
#-------------------------------------------------

QT       += core

#QT       -= gui

TARGET = symbolizer
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += main.cpp\
	../WiiQt/tools.cpp \
	../WiiQt/aes.c \
	elfparser.cpp \
	dol.cpp \
	be.cpp \
    ppc_disasm.c

HEADERS  += ../WiiQt/tools.h \
	../WiiQt/aes.h \
	elfparser.h \
	dol.h \
	be.h \
    ppc_disasm.h
