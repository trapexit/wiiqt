#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ngdialog.h"
#include "textdialog.h"

#include "../WiiQt/tools.h"
#include "../WiiQt/savebanner.h"
#include "../WiiQt/savedatabin.h"

#include "quazip.h"
#include "quazipfile.h"


//TODO...  get these from settings and dont use global variables
#ifdef Q_WS_MAC
static QString sneekPath = "/Volumes/VMware Shared Folders/host-c/QtWii/test";
#else
static QString sneekPath = "/media/SDHC_4GB";
#endif

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), bannerthread( this )
{
    ui->setupUi(this);
#ifdef Q_WS_MAC
    ui->listWidget_sneekSaves->setFixedWidth( 630 );
    ui->listWidget_pcSaves->setFixedWidth( 630 );
#endif
	ClearSneekGuiInfo();
	ClearPcGuiInfo();
    progressBar.setVisible( false );
    ui->statusBar->addPermanentWidget( &progressBar, 0 );
    ngID = 0;
    ngKeyID = 0;

    //sneekIconTimer.setInterval( 150 );//delay of icon image animation

    connect( &bannerthread, SIGNAL( SendProgress( int ) ), this, SLOT( GetProgressUpdate( int ) ) );
	connect( &bannerthread, SIGNAL( SendDone( int ) ), this, SLOT( LoadThreadIsDone( int ) ) );
	connect( &bannerthread, SIGNAL( SendSneekItem( QByteArray, const QString&, int ) ), this, SLOT( ReceiveSneekBanner( QByteArray, const QString&, int ) ) );
	connect( &bannerthread, SIGNAL( SendPcItem( PcSaveInfo ) ), this, SLOT( ReceivePcItem( PcSaveInfo ) ) );

	connect( &sneekIconTimer, SIGNAL( timeout() ), this, SLOT( ShowNextSneekIcon() ) );
	connect( &pcIconTimer, SIGNAL( timeout() ), this, SLOT( ShowNextPcIcon() ) );

	//GetSavesFromSneek( "/media/WiiFat500" );
	GetSavesFromSneek( sneekPath );
	//GetSavesFromPC( "./saveBackups" );

}

MainWindow::~MainWindow()
{
    delete ui;
}

//get the saves from a nand directory
void MainWindow::GetSavesFromSneek( const QString &path )
{
    qDebug() << "MainWindow::GetSavesFromSneek" << path;
    if( !QFileInfo( path ).exists() )
        return;

    if( bannerthread.isRunning() )
    {
        ui->statusBar->showMessage( tr( "Wait for the current job to finish" ) );
        return;
    }
    sneekPath = path;
    ui->listWidget_sneekSaves->clear();
    progressBar.setValue( 0 );
    progressBar.setVisible( true );
    if( !bannerthread.SetNandPath( path ) )
    {
        qWarning() << "MainWindow::GetSavesFromSneek -> error sotteng path" << path;
        return;
    }
    bannerthread.GetBanners();
}

void MainWindow::GetSavesFromPC( const QString &path )
{
    pcPath = path;
    if( !QFileInfo( path ).exists() )
        return;

    if( bannerthread.isRunning() )
    {
        ui->statusBar->showMessage( tr( "Wait for the current job to finish" ) );
        return;
    }
	//remove all currently loaded saves
	ui->listWidget_pcSaves->clear();
	pcInfos.clear();
    progressBar.setValue( 0 );
    progressBar.setVisible( true );
    bannerthread.GetBanners( pcPath );
}

void MainWindow::ReceivePcItem( PcSaveInfo info )
{
	//qDebug() << "received a pc save";
	if( !info.sizes.size() || info.sizes.size() != info.descriptions.size() || info.sizes.size() != info.paths.size() )//invalid
		return;
	SaveBanner sb( info.banner );
	new SaveListItem( sb, info.tid, 0, ui->listWidget_pcSaves );
	pcInfos << info;
}

//sneek save item changed
void MainWindow::on_listWidget_sneekSaves_currentItemChanged( QListWidgetItem* current, QListWidgetItem* previous )
{
    Q_UNUSED( previous );
    if( !current )
	{
		ClearSneekGuiInfo();
        return;
	}

    SaveListItem *i = static_cast< SaveListItem * >( current );
    ShowSneekSaveDetails( i );
}

//get an item from the thread loading all the data and turn it into a banner
void MainWindow::ReceiveSneekBanner( QByteArray stuff, const QString& tid, int size )
{
	QByteArray copy = stuff;
	SaveBanner sb( copy );
	new SaveListItem( sb, tid, size, ui->listWidget_sneekSaves );
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

    ui->listWidget_sneekSaves->clear();
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

//slots for cycling animated icons
void MainWindow::ShowNextSneekIcon()
{
	if( ++currentSneekIcon >= sneekIcon.size() )
		currentSneekIcon = 0;

	ui->label_sneek_icon->setPixmap( sneekIcon.at( currentSneekIcon ) );
}

void MainWindow::ShowNextPcIcon()
{
	if( ++currentPcIcon >= pcIcon.size() )
		currentPcIcon = 0;

	ui->label_PC_icon->setPixmap( pcIcon.at( currentPcIcon ) );
}

//clicked button to delete save from sneek
void MainWindow::on_pushButton_sneekDelete_clicked()
{
    QList<QListWidgetItem*>selected = ui->listWidget_sneekSaves->selectedItems();
    quint16 cnt = selected.size();
    if( !cnt )
        return;

    if( QMessageBox::question( this, tr( "Are you sure?" ), \
        tr( "You are about to delete %1 %2 from the SNEEK nand" ).arg( cnt )\
        .arg( cnt == 1 ? tr( "save" ) : tr( "saves" ) ),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel ) != QMessageBox::Ok )
        return;

    quint16 del = 0;
    for( quint16 i = 0; i < cnt; i++ )
    {
        QListWidgetItem* item = selected.takeFirst();
        SaveListItem *si = static_cast< SaveListItem * >( item );
        if( !si )
        {
            qDebug() << "MainWindow::on_pushButton_sneekDelete_clicked() -> error casting" << item->text();
            delete item;
            continue;
        }

        bool ok = false;
        quint64 tid = si->Tid().toLongLong( &ok, 16 );
        if( !ok )
        {
            qDebug() << "MainWindow::on_pushButton_sneekDelete_clicked() -> error converting" << si->Tid() << "to int";
            delete item;
            continue;
        }
        delete item;
        if( !bannerthread.DeleteSaveFromSneekNand( tid ) )
            qDebug() << "MainWindow::on_pushButton_sneekDelete_clicked() -> error deleting" << tid;

        else
            del++;

    }

    ui->statusBar->showMessage( tr( "Deleted %1 of %2 saves" ).arg( del ).arg( cnt ), 5000 );
}

QString MainWindow::GetSaveName( quint64 tid )
{
    if( pcPath.isEmpty() )
        return QString();

    QString tidStr = QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
    tidStr.insert( 8, "/" );

    QString parent = pcPath + "/" + tidStr;
    QFileInfo fi( parent );
    if( fi.exists() && fi.isFile() )
    {
        qWarning() << "MainWindow::GetSaveName ->" << fi.absoluteFilePath() << "is a file";
        return QString();
    }
    if( !fi.exists() && !QDir().mkpath( fi.absoluteFilePath() ) )
    {
        qWarning() << "MainWindow::GetSaveName -> error creating" << fi.absoluteFilePath();
        return QString();
    }
    quint32 i = 1;
    QString name = QString( "1.zip" );
    QDir dir( fi.absoluteFilePath() );
    while( dir.exists( name ) )
    {
        name = QString( "%1.zip" ).arg( ++i );
    }
    return fi.absoluteFilePath() + "/" + name;
}

//quick sanity checks on the NG stuff
bool MainWindow::NG_Ok()
{
    return ( ngID && ngKeyID && ngSig.size() == 60 && ngMac.size() == 6 && ngPriv.size() == 30 );
}

//button to extract a save from sneek clicked
void MainWindow::on_pushButton_sneekExtract_clicked()
{
	if( pcPath.isEmpty() )
	{
		ui->statusBar->showMessage( tr( "Set a path to extract saves first" ) );
		return;
	}
    QList<QListWidgetItem*>selected = ui->listWidget_sneekSaves->selectedItems();
    quint16 cnt = selected.size();
    if( !cnt )
    {
        ui->statusBar->showMessage( tr( "No saves are selected to extract" ) );
        return;
    }
    if( !NG_Ok() )
    {
        on_actionSet_NG_Keys_triggered();
        if( !NG_Ok() )
        {
            qDebug() << "invalid keys.  can't create data.bins";
            ui->statusBar->showMessage( tr( "I Need valid keys to extract saves" ) );
            return;
        }
    }

    bool promptForDetails = true;
    //keep from prompting for details for multiple saves
    if( cnt > 1 && QMessageBox::question( this, tr( "Enter Details?" ), \
        tr( "You are about to extract %1 saves from the SNEEK nand.<br>Do you want to enter details for each of them?" )
        .arg( cnt ), QMessageBox::No | QMessageBox::Yes, QMessageBox::No ) == QMessageBox::No )
        promptForDetails = false;

    quint16 done = 0;
    for( quint16 i = 0; i < cnt; i++ )
    {
        QListWidgetItem* item = selected.at( i );
        SaveListItem *si = static_cast< SaveListItem * >( item );
        if( !si )
        {
            qDebug() << "MainWindow::on_pushButton_sneekExtract_clicked() -> error casting" << item->text();
            delete item;
            continue;
        }

        bool ok = false;
        quint64 tid = si->Tid().toLongLong( &ok, 16 );
        if( !ok )
        {
            qDebug() << "MainWindow::on_pushButton_sneekExtract_clicked() -> error converting" << si->Tid() << "to int";
            continue;
        }

        //get a save destination
        QString fn = GetSaveName( tid );
        if( fn.isEmpty() )
            continue;

        //extract the save
        SaveGame sg = bannerthread.GetSave( tid );
        if( !IsValidSave( sg ) )
        {
            ui->statusBar->showMessage( tr( "Error extracting save for %1").arg( tid, 16, 16, QChar( '0' ) ) );
            qDebug() << "MainWindow::on_pushButton_sneekExtract_clicked() -> invalid save" << QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
            continue;
        }

        //convert to data.bin
        QByteArray ba = SaveDataBin::DataBinFromSaveStruct( sg, ngPriv, ngSig, ngMac, ngID, ngKeyID );
        if( ba.isEmpty() )
        {
            ui->statusBar->showMessage( tr( "Error encoding save for %1").arg( tid, 16, 16, QChar( '0' ) ) );
            qDebug() << "MainWindow::on_pushButton_sneekExtract_clicked() -> error converting" << QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
            continue;
        }

        QString fullDesc = QString( "SaveInfo\nversion=%1\ntid=%2\n" ).arg( DESC_VERSION ).arg( tid, 16, 16, QChar( '0' ) );
        QDate date = QDate::currentDate();
        QTime time = QTime::currentTime();
        QString desc = QString( "title=%1\ndate=%2\ntime=%3\n" ).arg( si->Banner()->Title() ).arg( date.toString() ).arg( time.toString() );

        if( promptForDetails )
        {
            fullDesc += "desc=\n" + TextDialog::GetText( this, desc ) + "\nend_desc\n";
        }
        else
        {
            fullDesc += "desc=\n" + desc + "\nend_desc\n";
        }
        //qDebug() << "description:\n" << fullDesc;

        //write to file
        if( !WriteZipFile(  ba, fullDesc.toLatin1(), fn ) )
        {
            ui->statusBar->showMessage( tr( "Error writing save for %1").arg( tid, 16, 16, QChar( '0' ) ) );
            qDebug() << "MainWindow::on_pushButton_sneekExtract_clicked() -> error writing" << QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
            continue;
        }
		//item extracted ok, now add the info to the tab displaying the PC
		SaveBanner banner = *si->Banner();
		AddNewPCSave( fullDesc, si->Tid(), si->Size(), fn, banner );
        done++;

    }

    ui->statusBar->showMessage( tr( "Extracted %1 of %2 saves" ).arg( done ).arg( cnt ), 5000 );
}

void MainWindow::AddNewPCSave( const QString &desc, const QString &tid, quint32 size, const QString &path, SaveBanner banner )
{
	//look to see if there is already some pc saves from the same game
	quint32 cnt = pcInfos.size();
	for( quint32 i = 0; i < cnt; i++ )
	{
		if( tid == pcInfos.at( i ).tid )
		{
			PcSaveInfo info = pcInfos.at( i );
			info.sizes << size;
			info.descriptions << desc;
			info.paths << path;

			pcInfos.replace( i, info );
			if( currentPcSave == i )
			{
				QString version = path;
				version.remove( 0, version.lastIndexOf( "/" ) + 1 );
				ui->comboBox_pcSelect->addItem( version );
			}
			return;
		}
	}
	//not found, create a new one
	PcSaveInfo info;
	info.tid = tid;
	info.sizes << size;
	info.descriptions << desc;
	info.paths << path;

	pcInfos << info;
	new SaveListItem( banner, tid, 0, ui->listWidget_pcSaves );
}

//try to write a zip file with a data.bin and a descriptive text file
bool MainWindow::WriteZipFile( const QByteArray &dataBin, const QByteArray &desc, const QString &path )
{
    qDebug() << "MainWindow::WriteZipFile" << path;
    QuaZip zip( path );
    if( !zip.open(QuaZip::mdCreate ) )
    {
        qWarning( "error creating zip file: %d", zip.getZipError() );
        return false;
    }
    zip.setComment( "Created with giantpune's saveToy" );
    QuaZipFile outFile( &zip );
    for( quint8 i = 0; i < 2; i++ )
    {
        QString fn;
        QByteArray stuff;
        if( i == 0 )
        {
            stuff = dataBin;
            fn = "data.bin";
        }
        else
        {
            stuff = desc;
            fn = "info.txt";
        }

        if( !outFile.open( QIODevice::WriteOnly, QuaZipNewInfo( fn, fn ) ) )
        {
            qWarning("MainWindow::WriteZipFile: outFile.open(): %d", outFile.getZipError() );
            goto error;
        }
        if( outFile.write( stuff ) != stuff.size() )
        {
            qWarning() << "MainWindow::WriteZipFile: wrong size written to zip file";
            goto error;
        }
        outFile.close();
        if( outFile.getZipError() !=UNZ_OK )
        {
            qWarning("MainWindow::WriteZipFile: outFile.close(): %d", outFile.getZipError() );
            goto error;
        }
    }
    zip.close();
    if( zip.getZipError() != 0 )
    {
        qWarning("MainWindow::WriteZipFile: zip.close(): %d", zip.getZipError());
        goto error;
    }
    return true;

error:
    zip.close();
    if( QFile::exists( path ) )
        QFile::remove( path );

    return false;

}

//tools -> set ng keys
void MainWindow::on_actionSet_NG_Keys_triggered()
{
    qDebug() << hex << ngID;
    NgDialog d( this );

    d.ngID = ngID;
    d.ngKeyID = ngKeyID;
    d.ngMac = ngMac;
    d.ngPriv = ngPriv;
    d.ngSig = ngSig;

    if( !d.exec() )
    {
        //qDebug() << "not accepted";
        return;
    }
    ngID = d.ngID;
    ngKeyID = d.ngKeyID;
    ngMac = d.ngMac;
    ngPriv = d.ngPriv;
    ngSig = d.ngSig;
	qDebug() << "accepted";
    qDebug() << hex << d.ngID
            << "\n" << d.ngKeyID
            << "\n" << d.ngMac.toHex()
            << "\n" << d.ngPriv.toHex()
			<< "\n" << d.ngSig.toHex();
}

//PC list item changed
void MainWindow::on_listWidget_pcSaves_currentItemChanged( QListWidgetItem* current, QListWidgetItem* previous )
{
	Q_UNUSED( previous );
	if( !current )
	{
		ClearPcGuiInfo();
		return;
	}

	SaveListItem *i = static_cast< SaveListItem * >( current );
	ShowPCSaveDetails( i );
}

//show detials for a save backed up on the PC
void MainWindow::ShowPCSaveDetails( SaveListItem *item )
{
	//qDebug() << "MainWindow::ShowPCSaveDetails";
	pcIconTimer.stop();
	currentPcIcon = 0;
	pcIcon.clear();
	ui->comboBox_pcSelect->clear();
	//find the item in the list of infos that matches this item
	currentPcSave = 0xffffffff;
	quint32 cnt = pcInfos.size();
	for( quint32 i = 0; i < cnt; i++ )
	{
		if( item->Tid() == pcInfos.at( i ).tid )
		{
			currentPcSave = i;
			break;
		}
	}
	if( currentPcSave == 0xffffffff )
	{
		qWarning() << "MainWindow::ShowPCSaveDetails -> tid not found";
		return;
	}

	SaveBanner *sb = item->Banner();

	ui->label_pc_title->setText( sb->Title() );

	if( !sb->SubTitle().isEmpty() && sb->Title() != sb->SubTitle() )
		ui->label_pc_title2->setText( sb->SubTitle() );
	else
		ui->label_pc_title2->clear();

	QString tid = item->Tid();
	tid.insert( 8, "/" );
	tid.prepend( "/" );
	//ui->label_sneek_path->setText( tid );

	QString id;
	tid = item->Tid().right( 8 );
	quint32 num = qFromBigEndian( (quint32) tid.toInt( NULL, 16 ) );
	for( int i = 0; i < 4; i++ )
		id += ascii( (char)( num >> ( 8 * i ) ) & 0xff );

	ui->label_pc_id->setText( id );

	foreach( QImage im, sb->IconImgs() )
		pcIcon << QPixmap::fromImage( im );

	currentPcIcon = 0;
	ui->label_PC_icon->setPixmap( pcIcon.at( 0 ) );
	if( pcIcon.size() > 1 )
	{
		pcIconTimer.setInterval( 1000 / pcIcon.size() );//delay of icon image animation
		pcIconTimer.start();
	}

	//add combobox entries for each of the different saves for this game
	cnt = pcInfos.at( currentPcSave ).sizes.size();
	for( quint32 i = 0; i < cnt; i++ )
	{
		QString version = pcInfos.at( currentPcSave ).paths.at( i );
		version.remove( 0, version.lastIndexOf( "/" ) + 1 );
		ui->comboBox_pcSelect->addItem( version );
	}
}

//pc combobox index changed
void MainWindow::on_comboBox_pcSelect_currentIndexChanged( int index )
{
	//qDebug() << "MainWindow::on_comboBox_pcSelect_currentIndexChanged" << index;
	if( index < 0 )
		return;
	ui->plainTextEdit_pcDesc->clear();
	if( currentPcSave >= (quint32)pcInfos.size() || index >= pcInfos.at( currentPcSave ).sizes.size() )
	{
		qWarning() << "MainWindow::on_comboBox_pc_date_currentIndexChanged -> index is out of range";
		return;
	}

	int size = pcInfos.at( currentPcSave ).sizes.at( index );
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

	ui->label_pc_size->setText( si );
	QString path = pcInfos.at( currentPcSave ).paths.at( index );
	if( path.size() >= 30 )
	{
		path = path.right( 27 );
		path.prepend( "..." );
	}
	ui->label_pc_path->setText( path );
	ui->plainTextEdit_pcDesc->clear();
	ui->plainTextEdit_pcDesc->insertPlainText( pcInfos.at( currentPcSave ).descriptions.at( index ) );
}

//delete PC save button clicked
void MainWindow::on_pushButton_pcDelete_clicked()
{
	QList<QListWidgetItem*>selected = ui->listWidget_pcSaves->selectedItems();
	quint32 cnt = selected.size();
	if( cnt != 1 )
		return;

	SaveListItem *si = static_cast< SaveListItem * >( selected.at( 0 ) );

	int i = ui->comboBox_pcSelect->currentIndex();

	//find the item in the list of infos that matches this item
	currentPcSave = 0xffffffff;
	cnt = pcInfos.size();
	for( quint32 i = 0; i < cnt; i++ )
	{
		if( si->Tid() == pcInfos.at( i ).tid )
		{
			currentPcSave = i;
			break;
		}
	}
	if( currentPcSave == 0xffffffff )
	{
		qWarning() << "MainWindow::on_pushButton_pcDelete_clicked() -> tid not found";
		return;
	}

	if( QMessageBox::question( this, tr( "Are you sure?" ), \
		tr( "You are about to delete the backed up save<br>\"%1\"?" ).arg( pcInfos.at( currentPcSave ).paths.at( i ) ),
		QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel ) != QMessageBox::Ok )
		return;

	//delete the file
	if( !QFile::remove( pcInfos.at( currentPcSave ).paths.at( ui->comboBox_pcSelect->currentIndex() ) ) )
	{
		ui->statusBar->showMessage( tr( "Error deleting file" ), 5000 );
		return;
	}
	if( pcInfos.at( currentPcSave ).sizes.size() == 1 )//there are no more saves for this game
	{
		pcInfos.takeAt( currentPcSave );							//remove it entirely from the list of knowns
		si = static_cast< SaveListItem * >( selected.takeFirst() );//delete its banner from the list
		delete si;
	}
	else															//theres more saves for this game on the PC, just remove the selected one
	{
		//remove entries for this save from the list
		pcInfos[ currentPcSave ].descriptions.takeAt( i );
		pcInfos[ currentPcSave ].paths.takeAt( i );
		pcInfos[ currentPcSave ].sizes.takeAt( i );
		ui->comboBox_pcSelect->removeItem( i );						//remove it from the combobox last
	}
	ui->statusBar->showMessage( tr( "Deleted 1 save from the PC" ), 5000 );

}

void MainWindow::on_actionSet_Local_Path_triggered()
{
	QString p = QFileDialog::getExistingDirectory( this, tr( "Select Save Backup path" ), pcPath );
	if( p.isEmpty() )
		return;
	GetSavesFromPC( p );
}

void MainWindow::ClearSneekGuiInfo()
{
	ui->label_sneek_icon->clear();
	ui->label_sneek_id->clear();
	ui->label_sneek_path->clear();
	ui->label_sneek_size->clear();
	ui->label_sneek_title->clear();
	ui->label_sneek_title2->clear();
}

void MainWindow::ClearPcGuiInfo()
{
	ui->label_PC_icon->clear();
	ui->label_pc_id->clear();
	ui->label_pc_path->clear();
	ui->label_pc_size->clear();
	ui->label_pc_title->clear();
	ui->label_pc_title2->clear();
	ui->plainTextEdit_pcDesc->clear();
	ui->comboBox_pcSelect->clear();
}
