#include "../WiiQt/includes.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE( rc );
	QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
