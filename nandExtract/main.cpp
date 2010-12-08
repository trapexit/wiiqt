#include <QtGui/QApplication>
#include "nandwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    NandWindow w;
    w.show();
    return a.exec();
}
