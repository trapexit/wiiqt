#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "tools.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), bannerthread( this )
{
    ui->setupUi(this);
    progressBar.setVisible( false );
    ui->statusBar->addPermanentWidget( &progressBar, 0 );

    //sneekIconTimer.setInterval( 150 );//delay of icon image animation

    connect( &bannerthread, SIGNAL( SendProgress( int ) ), this, SLOT( GetProgressUpdate( int ) ) );
    connect( &bannerthread, SIGNAL( SendDone( int ) ), this, SLOT( LoadThreadIsDone( int ) ) );
    connect( &bannerthread, SIGNAL( SendItem( QByteArray, const QString&, int, int ) ), this, SLOT( ReceiveSaveItem( QByteArray, const QString&, int, int ) ) );

    connect( &sneekIconTimer, SIGNAL(timeout() ), this, SLOT( ShowNextSneekIcon() ) );



    //GetSavesFromSneek( "/media/WiiFat500" );
    GetSavesFromSneek( sneekPath );
}

MainWindow::~MainWindow()
{
    delete ui;
}

//get the saves from a nand directory
void MainWindow::GetSavesFromSneek( const QString &path )
{
    sneekPath = path;
    ui->listWidget_sneekSaves->clear();
    progressBar.setValue( 0 );
    progressBar.setVisible( true );
    bannerthread.GetBanners( path, LOAD_SNEEK );
}
/*
//sneek save clicked
void MainWindow::on_listWidget_sneekSaves_itemClicked(QListWidgetItem* item)
{
    if( !item )
	return;

    SaveListItem *i = static_cast< SaveListItem * >( item );
    qDebug() << "item clicked" << i->Tid();
}

//sneek save double clicked
void MainWindow::on_listWidget_sneekSaves_itemActivated( QListWidgetItem* item )
{
    if( !item )
	return;

    SaveListItem *i = static_cast< SaveListItem * >( item );
    qDebug() << "item activated" << i->Tid();
}*/
//sneek save item changed
void MainWindow::on_listWidget_sneekSaves_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
    Q_UNUSED( previous );
    if( !current )
	return;

    SaveListItem *i = static_cast< SaveListItem * >( current );

    ShowSneekSaveDetails( i );
}

//get an item from the thread loading all the data and turn it into a banner
void MainWindow::ReceiveSaveItem( QByteArray stuff, const QString& tid, int type, int size )
{
    switch( type )
    {
    case LOAD_SNEEK:
	{
	    QByteArray copy = stuff;
	    SaveBanner sb( copy );
	    SaveListItem *item = new SaveListItem( sb, tid, size, ui->listWidget_sneekSaves );
	    ui->listWidget_sneekSaves->addItem( item );
	}
	break;
    default:
	break;
    }


}

//get a pregress update from something that is doing work
void MainWindow::GetProgressUpdate( int i )
{
    progressBar.setValue( i );
}

//something is done working.  respond somehow
void MainWindow::LoadThreadIsDone( int type )
{
    Q_UNUSED( type );
    progressBar.setVisible( false );
}

//tools -> set sneek path
void MainWindow::on_actionSet_Sneek_Path_triggered()
{
    QString p = QFileDialog::getExistingDirectory( this, tr( "Select SNEEK root" ), "/media" );
    if( p.isEmpty() )
	return;

    GetSavesFromSneek( p );
}

//show the details for a save in the sneek nand
void MainWindow::ShowSneekSaveDetails( SaveListItem *item )
{
    sneekIconTimer.stop();
    currentSneekIcon = 0;
    sneekIcon.clear();

    SaveBanner *sb = item->Banner();

    ui->label_sneek_title->setText( sb->Title() );

    if( !sb->SubTitle().isEmpty() && sb->Title() != sb->SubTitle() )
	ui->label_sneek_title2->setText( sb->SubTitle() );
    else
	ui->label_sneek_title2->clear();

    QString tid = item->Tid();
    tid.insert( 8, "/" );
    tid.prepend( "/" );
    ui->label_sneek_path->setText( tid );

    QString id;
    tid = item->Tid().right( 8 );
    quint32 num = qFromBigEndian( (quint32) tid.toInt( NULL, 16 ) );
    for( int i = 0; i < 4; i++ )
	id += ascii( (char)( num >> ( 8 * i ) ) & 0xff );

    ui->label_sneek_id->setText( id );
    int size = item->Size();
    QString sizeStr;
    if( size < 0x400 )
	sizeStr = tr( "%1 B" ).arg( size, 3 );
    else if( size < 0x100000 )
    {
	float kib = (float)size / 1024.00f;
	sizeStr = tr( "%1 KiB" ).arg( kib, 3, 'f', 2 );
    }
    else//assume there wont be any 1GB saves
    {
	float mib = (float)size / 1048576.00f;
	sizeStr = tr( "%1 MiB" ).arg( mib, 3, 'f', 2 );
    }
    int blocks = RU( 0x20000, size) / 0x20000;
    QString si = QString( "%1 %2 (%3)").arg( blocks ).arg( blocks == 1 ? tr( "Block" ) : tr( "Blocks" ) ).arg( sizeStr );

    ui->label_sneek_size->setText( si );

    foreach( QImage im, sb->IconImgs() )
	sneekIcon << QPixmap::fromImage( im );

    currentSneekIcon = 0;
    ui->label_sneek_icon->setPixmap( sneekIcon.at( 0 ) );
    if( sneekIcon.size() > 1 )
    {
	sneekIconTimer.setInterval( 1000 / sneekIcon.size() );//delay of icon image animation
	sneekIconTimer.start();
    }
}

void MainWindow::ShowNextSneekIcon()
{
    if( ++currentSneekIcon >= sneekIcon.size() )
	currentSneekIcon = 0;

    ui->label_sneek_icon->setPixmap( sneekIcon.at( currentSneekIcon ) );
}
