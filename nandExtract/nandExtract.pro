# -------------------------------------------------
# Project created by QtCreator 2010-12-06T03:40:50
# -------------------------------------------------
TARGET = nandExtract
TEMPLATE = app
SOURCES += main.cpp \
    nandwindow.cpp \
    ../WiiQt/nandbin.cpp \
    ../WiiQt/tools.cpp \
    ../WiiQt/savebanner.cpp \
    ../WiiQt/aes.c \
    ../WiiQt/sha1.c

HEADERS += nandwindow.h \
    ../WiiQt/nandbin.h \
    ../WiiQt/tools.h \
    ../WiiQt/tools.h

FORMS += nandwindow.ui
