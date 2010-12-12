#include "nandwindow.h"
#include "ui_nandwindow.h"
#include "../WiiQt/tools.h"

NandWindow::NandWindow( QWidget *parent ) : QMainWindow( parent ), ui( new Ui::NandWindow ), nThread( this )
{
    ui->setupUi( this );

    //setup the block map
    SetUpBlockMap();

    //put the progressbar on the status bar
    ui->progressBar->setVisible( false );
    ui->statusBar->addPermanentWidget( ui->progressBar, 0 );

    QFontMetrics fm( fontMetrics() );
    ui->treeWidget->header()->resizeSection( 0, fm.width( QString( 22, 'W' ) ) );//name
    ui->treeWidget->header()->resizeSection( 1, fm.width( "WWWWW" ) );//entry #
    ui->treeWidget->header()->resizeSection( 2, fm.width( "WWWWW" ) );//size
    ui->treeWidget->header()->resizeSection( 3, fm.width( "WWWWWWWWW" ) );//uid
    ui->treeWidget->header()->resizeSection( 4, fm.width( "WWWWWWWWW" ) );//gid
    ui->treeWidget->header()->resizeSection( 5, fm.width( "WWWWWWWWW" ) );//x3
    ui->treeWidget->header()->resizeSection( 6, fm.width( "WWWWW" ) );//mode
    ui->treeWidget->header()->resizeSection( 7, fm.width( "WWWWW" ) );//attr

    connect( &nThread, SIGNAL( SendError( QString ) ), this, SLOT( GetError( QString ) ) );
    connect( &nThread, SIGNAL( SendText( QString ) ), this, SLOT( GetStatusUpdate( QString ) ) );
    connect( &nThread, SIGNAL( SendExtractDone() ), this, SLOT( ThreadIsDone() ) );
    connect( &nThread, SIGNAL( SendProgress( int ) ), ui->progressBar, SLOT( setValue( int ) ) );

}

NandWindow::~NandWindow()
{
    delete ui;
}

void NandWindow::SetUpBlockMap()
{
    ui->graphicsView_blocks->setScene( &gv );

    QPixmap grey( ":/grey.png" );

    quint16 i = 0;
    for( quint16 y = 0; y < 288; y += 9 )		//create all the blocks and make them grey
    {
	for( quint16 x = 0; x < 1152; x += 9 )
	{
	    pmi[ i ] = new QGraphicsPixmapItem( grey );
	    pmi[ i ]->setPos( x, y );
	    gv.addItem( pmi[ i ] );//items belong to this now.  no need to delete them

	    i++;
	}
    }
    QFontMetrics fm( fontMetrics() );

    quint16 y = 288;
    quint16 x = 10;
    quint16 x2 = 200;
    quint8 spacing = 5;

    fileSize = new QGraphicsTextItem();
    fileSize->setPos( x, y );
    nandSize = new QGraphicsTextItem();
    nandSize->setPos( x + 400, y );

    y += fm.height() + 2;

    QGraphicsTextItem *badText = new QGraphicsTextItem( tr( "Bad" ) );
    QGraphicsTextItem *freeText = new QGraphicsTextItem( tr( "Free" ) );
    QGraphicsTextItem *usedText = new QGraphicsTextItem( tr( "Used" ) );
    QGraphicsTextItem *used2Text = new QGraphicsTextItem( tr( "Selected file" ) );
    QGraphicsTextItem *resText = new QGraphicsTextItem( tr( "Reserved" ) );

    QGraphicsPixmapItem *greySquare = new QGraphicsPixmapItem( grey );
    QGraphicsPixmapItem *blackSquare = new QGraphicsPixmapItem( QPixmap( ":/black.png" ) );
    QGraphicsPixmapItem *blueSquare = new QGraphicsPixmapItem( QPixmap( ":/blue.png" ) );
    QGraphicsPixmapItem *greenSquare = new QGraphicsPixmapItem( QPixmap( ":/green.png" ) );
    QGraphicsPixmapItem *pinkSquare = new QGraphicsPixmapItem( QPixmap( ":/pink.png" ) );

    greySquare->setPos( x, y + fm.height() / 2 );
    freeText->setPos( greySquare->pos().x() + 8 + spacing, y );
    greenSquare->setPos( x2, y + fm.height() / 2 );
    usedText->setPos( greenSquare->pos().x() + 8 + spacing, y );
    y += fontMetrics().height() + 2;

    blackSquare->setPos( x, y + fm.height() / 2 );
    badText->setPos( blackSquare->pos().x() + 8 + spacing, y );
    pinkSquare->setPos( x2, y + fm.height() / 2 );
    used2Text->setPos( pinkSquare->pos().x() + 8 + spacing, y );
    y += fontMetrics().height() + 2;

    blueSquare->setPos( x, y + fm.height() / 2 );
    resText->setPos( blueSquare->pos().x() + 8 + spacing, y );

    gv.addItem( fileSize );
    gv.addItem( nandSize );
    gv.addItem( badText );
    gv.addItem( freeText );
    gv.addItem( usedText );
    gv.addItem( used2Text );
    gv.addItem( resText );
    gv.addItem( greySquare );
    gv.addItem( blackSquare );
    gv.addItem( blueSquare );
    gv.addItem( greenSquare );
    gv.addItem( pinkSquare );

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
    quint32 freeSpace = 0;

    QList<quint16> clusters = nThread.GetFats();
    if( !clusters.size() == 0x8000 )
    {
	QMessageBox::warning( this, tr( "Error" ), tr( "Expected 0x8000 clusters from the nand, but got %1 instead!" ).arg( clusters.size(), 0, 16 ), QMessageBox::Ok );
	return;
    }

    for( quint16 i = 0; i < 0x8000; i += 8 )//first cluster of each block.
    {
	quint16 thisBlock = clusters.at( i );

	if( thisBlock == 0xFFFC
	    || thisBlock == 0xFFFD )
	{
	    blocks << thisBlock;
	    continue;
	}
	bool used = false;
	for( quint16 j = i; j < i + 8; j++ )//each individual cluster
	{
	    if( clusters.at( j ) == 0xFFFE )
		freeSpace += 0x4000;

	    else used = true;
	}
	blocks << ( used ? 1 : 0xfffe ); // just put 1 for used blocks
    }
    quint32 used = 0x20000000 - freeSpace;
    float per = (float)((float)used/(float)0x20000000) * 100.0f;
    float usedMb = (float)((float)used/(float)0x100000);

    nandSize->setHtml( QString( "<b>%1Mib ( %2 % ) used</b>" ).arg( usedMb, 3, 'f', 2 ).arg( per, 3, 'f', 2 ) );


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

    QList<quint16> clusters = nThread.GetFatsForFile( entry );
    QList<quint16> blocks = ToBlocks( clusters );

    DrawBlockMap( blocks );
    float size = current->text( 2 ).toInt( &ok, 16 );
    if( !ok )
    {
	qDebug() << "error converting" << current->text( 2 ) << "to int";
	return;
    }
    QString unit = "bytes";
    if( size > 1024 )
    {
	unit = "KiB";
	size /= 1024.0f;
    }
    if( size > 1024 )
    {
	unit = "MiB";
	size /= 1024.0f;
    }
    QString clusterStr = clusters.size() == 1 ? tr( "%1 cluster" ).arg( 1 ) : tr( "%1 clusters" ).arg( clusters.size() );
    QString blockStr = blocks.size() == 1 ? tr( "%1 block" ).arg( 1 ) : tr( "%1 blocks" ).arg( blocks.size() );
    QString sizeStr = QString( "( %1 %2 )" ).arg( size, 0, 'f', 2 ).arg( unit );


    fileSize->setHtml( QString( "%1 - %2 in %3 %4" ).arg( current->text( 0 ) ).arg( clusterStr ).arg( blockStr ).arg( sizeStr ) );
    //qDebug() << "blocks for" << current->text( 0 ) << blocks;

}

