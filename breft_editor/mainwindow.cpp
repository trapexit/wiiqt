#include "mainwindow.h"

#include "../WiiQt/tools.h"
#include "breft.h"

MainWindow::MainWindow( QWidget *parent ) : QMainWindow( parent)
{
	setObjectName("MainWindow");
	resize(1024, 768);

	setUnifiedTitleAndToolBarOnMac(true);

	QFrame *centralwidget = new QFrame;
	Layout = new QGridLayout(centralwidget);

	setupForms();

	setupActions();
	setupMenu();

	setCentralWidget(centralwidget);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupForms()
{
	QVBoxLayout* mainLayout = new QVBoxLayout;

	plainTextEdit_log = new QPlainTextEdit;
	mainLayout->addWidget(plainTextEdit_log);

	Layout->addLayout(mainLayout, 0, 0);

	ShowMessage( tr("Done creating forms") );
}

void MainWindow::setupActions()
{
	actionQuit = new QAction(this);
	actionQuit->setText("Quit");
	actionQuit->setToolTip("Quits the program.");
	connect(actionQuit, SIGNAL(triggered()), this, SLOT(on_actionQuit_triggered()));

	actionLoad = new QAction(this);
	actionLoad->setText("Load");
	actionLoad->setToolTip("Loads a breft.");
	connect(actionLoad, SIGNAL(triggered()), this, SLOT(on_actionLoad_triggered()));

	ShowMessage( tr("Done creating actions") );
}

void MainWindow::setupMenu()
{
	menubar = new QMenuBar(this);

	menuFile = new QMenu(tr("&File"), this);
	menuFile->addAction(actionLoad);
	menuFile->addAction(actionQuit);
	menubar->addAction(menuFile->menuAction());
	menubar->addMenu(menuFile);

	setMenuBar(menubar);

	ShowMessage( tr("Done creating menu") );
}

void MainWindow::on_actionQuit_triggered()
{
	exit(EXIT_SUCCESS);
}

void MainWindow::on_actionLoad_triggered()
{
	//DO SOMETHING COOL
	QString fn = QFileDialog::getOpenFileName( this,
			tr("Effects files(*.breft)"),
			QCoreApplication::applicationDirPath(),
			tr("BreftFiles (*.breft)"));
	if(fn == "") {
		ShowMessage( tr("No breft selected") );
		return;
	}

	QByteArray data = ReadFile( fn );
	if( data.isEmpty() ) {
		ShowMessage( tr("Bad breft file") );
	    return;
	}

	QDataStream stream(data);
	header head;
	stream >> head;
	ShowMessage( tr("Header loaded") );
	reft rft;
	stream >> rft; 
	ShowMessage( tr("Reft loaded") );
	qint64 curr_pos = stream.device()->pos();
	projct proj;
	stream >> proj;
	ShowMessage( tr("Project loaded") );
	char* project_name = new char[proj.str_length];
	stream.readRawData(project_name, proj.str_length);
	ShowMessage( QString("Project name: %1").arg(project_name) );
	delete[] project_name;
	stream.device()->seek(proj.length + curr_pos);

	qint64 temp_pos = stream.device()->pos();
	section1 sec1;
	stream >> sec1;

	for(quint32 ii=0; ii<sec1.count; ii++) {
		quint16 len;
		stream >> len;
		char* string_name = new char[len];
		stream.readRawData(string_name, len);
		ShowMessage( tr("String Name: %1").arg(string_name) );
		section1b sec1b;
		stream >> sec1b;
		curr_pos = stream.device()->pos();
		qint64 my_pos = temp_pos + sec1b.offset;
		stream.device()->seek(my_pos);
		picture_header pic_head;
		stream >> pic_head;
		ShowMessage( tr("Picture Header read") );
		QByteArray pic_data(pic_head.size, '\x00');
		stream.readRawData( pic_data.data() , pic_head.size );

// SHOW TPL ON SCREEN

// SAVE TPL TO FILE
#if 0
		QFile tpl( "tpl_data/" + QString(string_name) + ".tpl" );
		if( !tpl.open( QIODevice::WriteOnly ) )
		{
			ShowMessage( "<b>Couldn't open " + QString(string_name) + " for writing<\b>" );
		}else{
			//Write header1
			QByteArray tplHead1;
			char tpl_head[0x14] = { 0, 0x20, 0xaf, 0x30,
				0x00, 0x00, 0x00, 0x01,
				0x00, 0x00, 0x00, 0x0c,
				0x00, 0x00, 0x00, 0x14,
				0x00, 0x00, 0x00, 0x00 };
			tplHead1.append(tpl_head, 0x14);
			tpl.write( tplHead1 );
			//Write header2
			QByteArray tplHead2;
			char tpl_head2[44] = {
				0x00, 0x80, 0x00, 0x80,	//height && width
				0x00, 0x00, 0x00, 0x04,	//format
				0x00, 0x00, 0x00, 0x40,	//offset
				0x00, 0x00, 0x00, 0x00,	//wrap_s
				0x00, 0x00, 0x00, 0x00,	//wrap_t
				0x00, 0x00, 0x00, 0x01,	//min
				0x00, 0x00, 0x00, 0x01,	//mag
				0x00, 0x00, 0x00, 0x00,	//lod_bias
				0x00, 0x00, 0x00, 0x00,	//lods and unpacked
				0x00, 0x00, 0x00, 0x00,	//padding
				0x00, 0x00, 0x00, 0x00 };
			*(quint16*)(tpl_head2 + 0) = qToBigEndian(pic_head.height);
			*(quint16*)(tpl_head2 + 2) = qToBigEndian(pic_head.width);
			*(quint32*)(tpl_head2 + 4) = qToBigEndian((quint32)pic_head.format);
			tplHead2.append(tpl_head2, 44);
			tpl.write( tplHead2 );
			//Write data
			tpl.write( pic_data );
			tpl.close();
			ShowMessage( "Saved " + QString(string_name) );
		}
#endif

		stream.device()->seek(curr_pos);
		delete[] string_name;
	}

	ShowMessage( tr("Done loading a breft") );
}

void MainWindow::ShowMessage( const QString& mes )
{
    QString str = mes;
    plainTextEdit_log->appendHtml( str );
}

