#include <QtGui/QApplication>
#include "thpwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
	Q_INIT_RESOURCE( rc );
    ThpWindow w;
    w.show();

    return a.exec();
}
