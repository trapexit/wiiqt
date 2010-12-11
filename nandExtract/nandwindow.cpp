#include "nandwindow.h"
#include "ui_nandwindow.h"

NandWindow::NandWindow( QWidget *parent ) : QMainWindow( parent ), ui( new Ui::NandWindow ), nThread( this )
{
    ui->setupUi( this );
    freeSpace = 0;

    //setup the block map
    ui->graphicsView_blocks->setScene( &gv );
    ui->graphicsView_blocks->setAlignment( Qt::AlignRight );
    ui->graphicsView_blocks->setRenderHint( QPainter::Antialiasing );

    QPixmap blue( ":/blue.png" );
    QPixmap green( ":/green.png" );
    QPixmap pink( ":/pink.png" );
    QPixmap grey( ":/grey.png" );

    quint16 i = 0;
    for( quint16 y = 0; y < 352; y += 11 )
    {
	for( quint16 x = 0; x < 1408; x += 11 )
	{
	    pmi[ i ] = new QGraphicsPixmapItem( grey );
	    pmi[ i ]->setPos( x, y );
	    gv.addItem( pmi[ i ] );//items belong to this now.  no need to delete them

	    i++;
	}
    }

    //put the progressbar on the status bar
    ui->progressBar->setVisible( false );
    ui->statusBar->addPermanentWidget( ui->progressBar, 0 );

    //ui->treeWidget->header()->resizeSection( 0, 300 );//name
    QFontMetrics fm( fontMetrics() );
    ui->treeWidget->header()->resizeSection( 0, fm.width( QString( 22, 'W' ) ) );//name
    ui->treeWidget->header()->resizeSection( 1, fm.width( "WWWWW" ) );//entry #
    ui->treeWidget->header()->resizeSection( 2, fm.width( "WWWWW" ) );//size
    ui->treeWidget->header()->resizeSection( 3, fm.width( "WWWWWWWWW" ) );//uid
    ui->treeWidget->header()->resizeSection( 4, fm.width( "WWWWWWWWW" ) );//gid
    ui->treeWidget->header()->resizeSection( 5, fm.width( "WWWWWWWWW" ) );//x3
    ui->treeWidget->header()->resizeSection( 6, fm.width( "WWWWW" ) );//mode
    ui->treeWidget->header()->resizeSection( 7, fm.width( "WWWWW" ) );//attr


    //connect( &nandBin, SIGNAL( SendError( QString ) ), this, SLOT( GetError( QString ) ) );
    //connect( &nandBin, SIGNAL( SendText( QString ) ), this, SLOT( GetStatusUpdate( QString ) ) );
    connect( &nThread, SIGNAL( SendError( QString ) ), this, SLOT( GetError( QString ) ) );
    connect( &nThread, SIGNAL( SendText( QString ) ), this, SLOT( GetStatusUpdate( QString ) ) );
    connect( &nThread, SIGNAL( SendExtractDone() ), this, SLOT( ThreadIsDone() ) );
    connect( &nThread, SIGNAL( SendProgress( int ) ), ui->progressBar, SLOT( setValue( int ) ) );
}

NandWindow::~NandWindow()
{
    delete ui;
}

void NandWindow::ThreadIsDone()
{
    qDebug() << "NandWindow::ThreadIsDone";
    ui->progressBar->setVisible( false );
    ui->statusBar->showMessage( "Done extracting", 5000 );
}

void NandWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void NandWindow::GetStatusUpdate( QString s )
{
    ui->statusBar->showMessage( s );
}

void NandWindow::GetError( QString str )
{
    QMessageBox::warning( this, tr( "Error" ), str, QMessageBox::Ok );
    qWarning() << str;
}

//nand window right-clicked
void NandWindow::on_treeWidget_customContextMenuRequested( QPoint pos )
{
    QPoint globalPos = ui->treeWidget->viewport()->mapToGlobal( pos );
    QTreeWidgetItem* item = ui->treeWidget->itemAt( pos );
    if( !item )//right-clicked in the partition window, but not on an item
    {
	qDebug() << "no item selected";
	return;
    }

    QMenu myMenu( this );
    QAction extractA( tr( "Extract" ), &myMenu );
    myMenu.addAction( &extractA );

    QAction* s = myMenu.exec( globalPos );
    //respond to what was selected
    if( s )
    {
	// something was chosen, do stuff
	if( s == &extractA )//extract a file
	{
	    QString path = QFileDialog::getExistingDirectory( this, tr("Select a destination") );
	    if( path.isEmpty() )
		return;

	    ui->progressBar->setVisible( true );
	    nThread.Extract( item, path );
	}
    }
}

//file->open
void NandWindow::on_actionOpen_Nand_triggered()
{
    QString path = QFileDialog::getOpenFileName( this, tr( "Select a Nand to open" ) );
    if( path.isEmpty() )
	return;

    blocks.clear();
    if( !nThread.SetPath( path ) )
    {
	qDebug() << " error in nandBin.SetPath";
	ui->statusBar->showMessage( "Error setting path to " + path );
	return;
    }
    ui->statusBar->showMessage( "Loading " + path );
    QIcon groupIcon;
    QIcon keyIcon;
    groupIcon.addPixmap( style()->standardPixmap( QStyle::SP_DirClosedIcon ), QIcon::Normal, QIcon::Off );
    groupIcon.addPixmap( style()->standardPixmap( QStyle::SP_DirOpenIcon ), QIcon::Normal, QIcon::On );
    keyIcon.addPixmap( style()->standardPixmap( QStyle::SP_FileIcon ) );

    if( !nThread.InitNand( groupIcon, keyIcon ) )
    {
	qDebug() << " error in nandBin.InitNand()";
	ui->statusBar->showMessage( "Error reading " + path );
	return;
    }

    ui->treeWidget->clear();
    GetBlocksfromNand();
    DrawBlockMap();

    //get an item holding a tree with all the items of the nand
    //QTreeWidgetItem* tree = nandBin.GetTree();
    QTreeWidgetItem* tree = nThread.GetTree();

    //take the actual contents of the nand from the made up root and add them to the gui widget
    ui->treeWidget->addTopLevelItems( tree->takeChildren() );

    //delete the made up root item
    delete tree;

    //expand the root item
    if( ui->treeWidget->topLevelItemCount() )
	ui->treeWidget->topLevelItem( 0 )->setExpanded( true );

    ui->statusBar->showMessage( "Loaded " + path, 5000 );
}

void NandWindow::GetBlocksfromNand()
{
    blocks.clear();
    freeSpace = 0;

    QList<quint16> clusters = nThread.GetFats();
    if( !clusters.size() == 0x8000 )
    {
	QMessageBox::warning( this, tr( "Error" ), tr( "Expected 0x8000 clusters from the nand, but got %1 instead!" ).arg( clusters.size(), 0, 16 ), QMessageBox::Ok );
	return;
    }
    //QString str;
    for( quint16 i = 0; i < 0x8000; i += 8 )//first cluster of each block.
    {
	quint16 thisBlock = clusters.at( i );

	//str += QString( "%1 " ).arg( thisBlock, 4, 16 );
	if( thisBlock == 0xFFFC
	    || thisBlock == 0xFFFD )
	{
	    //qDebug() << "adding" << hex << thisBlock;
	    blocks << thisBlock;
	    continue;
	}
	bool used = 0;
	for( quint16 j = i; j < i + 8; j++ )//each individual cluster
	{
	    thisBlock = clusters.at( i );
	    if( thisBlock == 0xFFFE )
		freeSpace += 0x800;

	    else used = true;
	}
	blocks << ( used ? 1 : 0xfffe ); // just put 1 for used blocks
    }

}

QList<quint16> NandWindow::ToBlocks( QList<quint16> clusters )
{
    //qDebug() << "NandWindow::ToBlocks" << clusters;
    QList<quint16> ret;
    quint16 size = clusters.size();
    for( quint16 i = 0; i < size; i++ )
    {
	quint16 block = ( clusters.at( i ) / 8 );
	if( !ret.contains( block ) )
	    ret << block;
    }
    return ret;
}

//expects blocks, not clusters!!
void NandWindow::DrawBlockMap( QList<quint16> newFile )
{
    if( blocks.size() != 0x1000 )
    {
	qWarning() << "NandWindow::DrawBlockMap -> current blocks are fucked up, son" << hex << blocks.size();
	return;
    }
    QPixmap blue( ":/blue.png" );
    QPixmap green( ":/green.png" );
    QPixmap pink( ":/pink.png" );
    QPixmap grey( ":/grey.png" );
    QPixmap black( ":/black.png" );

    for( quint16 i = 0; i < 0x1000; i++ )
    {
	quint16 thisBlock;
	if( !newFile.contains( i ) )
	{
	    thisBlock = blocks.at( i );
	}
	else
	{
	    thisBlock = 2;
	}
	switch( thisBlock )
	{
	default:
	case 1://used, but not in this file
	    pmi[ i ]->setPixmap( green );
	    break;
	case 2://used in this file
	    pmi[ i ]->setPixmap( pink );
	    break;
	case 0xFFFE://free block
	    pmi[ i ]->setPixmap( grey );
	    break;
	case 0xFFFC://reserved
	    pmi[ i ]->setPixmap( blue );
	    break;
	case 0xFFFD: // bad block
	    pmi[ i ]->setPixmap( black );
	    break;
	}
    }


}

//show nand usage
void NandWindow::on_actionShow_Usage_triggered()
{
    //128x32
    /*QList<quint16> clusters = nThread.GetFats();//each of these is 0x4000 bytes ( neglecting the ecc )
    quint16 badBlocks = 0;
    quint16 reserved = 0;
    quint16 freeBlocks = 0;
    QList<quint16>badOnes;
    for( quint16 i = 0; i < 0x8000; i++ )
    {
	quint16 fat = GetFAT( i );
	if( 0xfffc == fat )
	    reserved++;
	else if( 0xfffd == fat )
	{
	    badBlocks++;
	    if( i % 8 == 0 )
	    {
		badOnes << ( i / 8 );
	    }
	}
	else if( 0xfffe == fat )
	    freeBlocks++;
    }
    if( badBlocks )
	 badBlocks /= 8;

    if( reserved )
	 reserved /= 8;

    if( freeBlocks )
	 freeBlocks /= 8;*/
}

//some item in the nand tree was clicked
void NandWindow::on_treeWidget_currentItemChanged( QTreeWidgetItem* current, QTreeWidgetItem* previous )
{
    Q_UNUSED( previous );

    if( !current || current->text( 6 ) == "00" )
	return;
    bool ok = false;
    quint16 entry = current->text( 1 ).toInt( &ok );
    if( !ok )
    {
	qDebug() << "NandWindow::on_treeWidget_currentItemChanged ->" << current->text( 1 ) << "isnt a decimal number";
	return;
    }

    QList<quint16> blocks = ToBlocks( nThread.GetFatsForFile( entry ) );
    //qDebug() << "blocks for" << current->text( 0 ) << blocks;
    DrawBlockMap( blocks );
}
