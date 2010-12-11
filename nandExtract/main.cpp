#include <QtGui/QApplication>
#include "nandwindow.h"

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE( rc );

    QApplication a(argc, argv);
    NandWindow w;
    w.show();
    return a.exec();
}
