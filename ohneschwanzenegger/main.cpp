#include <QtGui/QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
	Q_INIT_RESOURCE( rc );
	QApplication a( argc, argv );
	QApplication::setWindowIcon( QIcon( ":/icon.png" ) );
    MainWindow w;
    w.show();

    return a.exec();
}
