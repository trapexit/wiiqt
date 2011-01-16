#-------------------------------------------------
#
# Project created by QtCreator 2011-01-05T10:05:50
#
#-------------------------------------------------

QT       += core gui multimedia

TARGET = thp_player
TEMPLATE = app


SOURCES += main.cpp\
        thpwindow.cpp \
    gcvid.cpp

HEADERS  += thpwindow.h \
    gcvid.h

FORMS    += thpwindow.ui

CONFIG += static
win32 {
	message("win32 build")
    INCLUDEPATH += . ./libpng/include
    LIBS += -L./libpng/lib -ljpeg
}
else {
    LIBS += -ljpeg
}

RESOURCES += \
    rc.qrc
