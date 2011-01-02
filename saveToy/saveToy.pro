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
    savelistitem.cpp \
    saveloadthread.cpp \
    ../WiiQt/tools.cpp \
    ../WiiQt/savebanner.cpp \
    ../WiiQt/aes.c \
    ../WiiQt/sha1.c \
    ../WiiQt/md5.cpp \
    ../WiiQt/sharedcontentmap.cpp \
    ../WiiQt/tiktmd.cpp \
    ../WiiQt/uidmap.cpp \
    ../WiiQt/nanddump.cpp \
    ../WiiQt/wad.cpp \
    ../WiiQt/bn.cpp \
    ../WiiQt/ec.cpp \
    ../WiiQt/savedatabin.cpp \
    ../WiiQt/keysbin.cpp \
    ngdialog.cpp \
    textdialog.cpp

HEADERS  += mainwindow.h \
    savelistitem.h \
    saveloadthread.h\
    ../WiiQt/tools.h \
    ../WiiQt/savebanner.h \
    ../WiiQt/sharedcontentmap.h \
    ../WiiQt/tiktmd.h \
    ../WiiQt/uidmap.h \
    ../WiiQt/nanddump.h \
    ../WiiQt/wad.h \
    ../WiiQt/savedatabin.h \
    ../WiiQt/ec.h \
    ../WiiQt/keysbin.h \
    quazip/include/zip.h \
    quazip/include/unzip.h \
    quazip/include/quazipnewinfo.h \
    quazip/include/quazipfileinfo.h \
    quazip/include/quazipfile.h \
    quazip/include/quazip.h \
    quazip/include/quacrc32.h \
    quazip/include/quachecksum32.h \
    quazip/include/quaadler32.h \
    quazip/include/JlCompress.h \
    quazip/include/ioapi.h \
    quazip/include/crypt.h \
    ngdialog.h \
    textdialog.h

FORMS    += mainwindow.ui \
    ngdialog.ui \
    textdialog.ui

RESOURCES += \
    rc.qrc

INCLUDEPATH += . ./quazip/include
#different paths for different zip libraries
win32 {
	message("win32 build")
    LIBS += -L./quazip/lib/win32 -lquazip
}
macx{
        message("mac build")
    LIBS += -L./quazip/lib/mac -lquazip
}
else unix {
    !contains(QMAKE_HOST.arch, x86_64) {
        message("x86 build")
    LIBS += -L./quazip/lib/linux_x86 -lquazip
    } else {
        message("x86_64 build")
    LIBS += -L./quazip/lib/linux_x64 -lquazip
    }
}

