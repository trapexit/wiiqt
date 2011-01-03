#include "mainwindow.h"
#include "newnandbin.h"
#include "ui_mainwindow.h"

#include "../WiiQt/settingtxtdialog.h"
#include "../WiiQt/tiktmd.h"
#include "../WiiQt/tools.h"
#include "../WiiQt/wad.h"


MainWindow::MainWindow( QWidget *parent ) : QMainWindow( parent ), ui( new Ui::MainWindow ), nus ( this ), nand ( this )
{
    ui->setupUi(this);
    nandInited = false;
    root = NULL;
    uidDirty = false;
    sharedDirty = false;
    nandDirty = false;
    Wad::SetGlobalCert( QByteArray( (const char*)&certs_dat, CERTS_DAT_SIZE ) );

    //connect to the nus object so we can respond to what it is saying with pretty stuff in the gui
    connect( &nus, SIGNAL( SendDownloadProgress( int ) ), ui->progressBar_dl, SLOT( setValue( int ) ) );
    connect( &nus, SIGNAL( SendTitleProgress( int ) ), ui->progressBar_title, SLOT( setValue( int ) ) );
    connect( &nus, SIGNAL( SendTotalProgress( int ) ), ui->progressBar_whole, SLOT( setValue( int ) ) );
    connect( &nus, SIGNAL( SendText( QString ) ), ui->statusBar, SLOT( showMessage( QString ) ) );
    connect( &nus, SIGNAL( SendError( const QString &, NusJob ) ), this, SLOT( GetError( const QString &, NusJob ) ) );
    connect( &nus, SIGNAL( SendDone() ), this, SLOT( NusIsDone() ) );
    connect( &nus, SIGNAL( SendData( NusJob ) ), this, SLOT( ReceiveTitleFromNus( NusJob) ) );

    //connect to the nand.bin to get text and crap from it
    connect( &nand, SIGNAL( SendError( const QString & ) ), this, SLOT( GetError( const QString & ) ) );
    connect( &nand, SIGNAL( SendText( QString ) ), ui->statusBar, SLOT( showMessage( QString ) ) );

    //TODO, really get these paths from settings
#ifdef Q_WS_WIN
    QString cachePath = "../../NUS_cache";
#else
    QString cachePath = "../NUS_cache";
#endif
    QString nandPath = "./testNand.bin";


    ui->lineEdit_cachePath->setText( cachePath );
    ui->lineEdit_nandPath->setText( nandPath );

    nus.SetCachePath( cachePath );
}

MainWindow::~MainWindow()
{
    delete ui;
}

//some slots to respond to the NUS downloader
void MainWindow::GetError( const QString &message, NusJob job )
{
    QString dataStuff = QString( "%1 items:" ).arg( job.data.size() );
    for( int i = 0; i < job.data.size(); i++ )
		dataStuff += QString( " %1" ).arg( job.data.at( i ).size(), 0, 16, QChar( ' ' ) );

    QString str = tr( "<b>Error getting title from NUS: %1</b>" ).arg( message );
    QString j = QString( "NusJob( %1, %2, %3, %4 )<br>" )
				.arg( job.tid, 16, 16, QChar( '0' ) )
				.arg( job.version ).arg( job.decrypt ? "decrypted" : "encrypted" )
				.arg( dataStuff );


    ui->plainTextEdit_log->appendHtml( str );
    ui->plainTextEdit_log->appendHtml( j );
}

//get error from nand.bin
void MainWindow::GetError( const QString &message )
{
    QString str = tr( "<b>Nand object Error: %1</b>" ).arg( message );
    ui->plainTextEdit_log->appendHtml( str );
}

void MainWindow::ShowMessage( const QString& mes )
{
    QString str = mes + "<br>";
    ui->plainTextEdit_log->appendHtml( str );
}

void MainWindow::NusIsDone()
{
    QString str = tr( "NUS object is done working<br>" );
    ui->plainTextEdit_log->appendHtml( str );
    ui->statusBar->showMessage( tr( "Done" ), 5000 );

    //make sure there is a setting.txt
    QTreeWidgetItem *item = ItemFromPath( "/title/00000001/00000002/data/setting.txt" );
    if( !item )
    {
		quint8 reg = SETTING_TXT_UNK;
		if( ui->lineEdit_tid->text().endsWith( "e", Qt::CaseInsensitive ) && ui->lineEdit_tid->text().size() == 4 )
			reg = SETTING_TXT_PAL;
		if( ui->lineEdit_tid->text().endsWith( "j", Qt::CaseInsensitive ) && ui->lineEdit_tid->text().size() == 4 )
			reg = SETTING_TXT_JAP;
		if( ui->lineEdit_tid->text().endsWith( "k", Qt::CaseInsensitive ) && ui->lineEdit_tid->text().size() == 4 )
			reg = SETTING_TXT_KOR;
		QByteArray ba = SettingTxtDialog::Edit( this, QByteArray(), reg );	//call a dialog to create a new setting.txt
		if( !ba.isEmpty() )				//if the dialog returned anything ( cancel wasnt pressed ) write that new setting.txt to the nand
		{
			quint16 r = nand.CreateEntry( "/title/00000001/00000002/data/setting.txt", 0x1000, 1, NAND_FILE, NAND_READ, NAND_READ, NAND_READ );
			if( !r )
			{
				ShowMessage( "<b>Error creating setting.txt.  maybe some folders are missing?</b>" );
			}
			else
			{
				if( !nand.SetData( r, ba) )
				{
					ShowMessage( "<b>Error writing data for setting.txt.</b>" );
				}
				else
				{
					nandDirty = true;
					UpdateTree();
				}
			}
		}
    }

    //nand.Delete( "/title/00000001/00000002/content/title.tmd" );
    if( nandDirty )
    {
		if( !FlushNand() )
		{
			ShowMessage( "<b>Error flushing nand.  Maybe you used too much TP?</b>" );
		}
		nandDirty = false;
    }
}

void MainWindow::ReceiveTitleFromNus( NusJob job )
{
    QString str = tr( "Received a completed download from NUS" );
    //QString title = QString( "%1v%2" ).arg( job.tid, 16, 16, QChar( '0' ) ).arg( job.version );

    ui->plainTextEdit_log->appendHtml( str );

    //do something with the data we got
    if( InstallNUSItem( job ) )
		nandDirty = true;

}

//clicked the button to get a title
void MainWindow::on_pushButton_GetTitle_clicked()
{
    if( !nandInited && !InitNand( ui->lineEdit_nandPath->text() ) )
		return;

    bool ok = false;
    bool wholeUpdate = false;
    quint64 tid = 0;
    quint32 ver = 0;
    if( ui->lineEdit_tid->text().size() == 4 )
    {
		wholeUpdate = true;
    }
    else
    {
		tid = ui->lineEdit_tid->text().toLongLong( &ok, 16 );
		if( !ok )
		{
			ShowMessage( "<b>Error converting \"" + ui->lineEdit_tid->text() + "\" to a hex number.</b>" );
			return;
		}
		ver = TITLE_LATEST_VERSION;
		if( !ui->lineEdit_version->text().isEmpty() )
		{
			ver = ui->lineEdit_version->text().toInt( &ok, 10 );
			if( !ok )
			{
				ShowMessage( "<b>Error converting \"" + ui->lineEdit_version->text() + "\" to a decimal number.</b>" );
				return;
			}
			if( ver > 0xffff )
			{
				ShowMessage( tr( "<b>Version %1 is too high.  Max is 65535</b>" ).arg( ver ) );
				return;
			}
		}
    }
    //decide how we want nus to give us the title
    bool decrypt = true;
    nus.SetCachePath( ui->lineEdit_cachePath->text() );
    if( wholeUpdate )
    {
		if( !nus.GetUpdate( ui->lineEdit_tid->text(), decrypt ) )
		{
			ShowMessage( tr( "<b>I dont know the titles that were in the %1 update</b>" ).arg( ui->lineEdit_tid->text() ) );
			return;
		}
    }
    else
    {
		nus.Get( tid, decrypt, ver );
    }
}

//search for a path to use as the nand basepath
void MainWindow::on_pushButton_nandPath_clicked()
{
    QString f = QFileDialog::getOpenFileName( this, tr( "Select nand.bin" ) );
    if( f.isEmpty() )
		return;

    ui->lineEdit_nandPath->setText( f );
}

void MainWindow::on_pushButton_CachePathBrowse_clicked()
{
    QString f = QFileDialog::getExistingDirectory( this, tr( "Select NUS Cache base folder" ) );
    if( f.isEmpty() )
		return;

    ui->lineEdit_cachePath->setText( f );
    nus.SetCachePath( ui->lineEdit_cachePath->text() );
}

//nand-dump -> setting.txt
void MainWindow::on_actionSetting_txt_triggered()
{
    if( !nandInited )
		return;

    QTreeWidgetItem *it = ItemFromPath( "/title/00000001/00000002/data/setting.txt" );
    if( !it )
    {
		ShowMessage( tr( "<b>There is no setting.txt found in %1</b>" )
					 .arg( QFileInfo( ui->lineEdit_nandPath->text() ).absoluteFilePath() ) );
		return;
    }
    QByteArray ba = nand.GetData( "/title/00000001/00000002/data/setting.txt" );	//read the current setting.txt
    ba = SettingTxtDialog::Edit( this, ba );	//call a dialog to edit that existing file and store the result in the same bytearray
    if( !ba.isEmpty() )				//if the dialog returned anything ( cancel wasnt pressed ) write that new setting.txt to the nand dump
		nand.SetData( "/title/00000001/00000002/data/setting.txt", ba );
}

//nand-dump -> flush
void MainWindow::on_actionFlush_triggered()
{
    if( !nandInited )
		FlushNand();
}

//nand-dump -> ImportWad
void MainWindow::on_actionImportWad_triggered()
{
    if( !nandInited && !InitNand( ui->lineEdit_nandPath->text() ) )
    {
		ShowMessage( tr( "<b>Error setting the basepath of the nand to %1</b>" ).arg( QFileInfo( ui->lineEdit_nandPath->text() ).absoluteFilePath() ) );
		return;
    }
    QString fn = QFileDialog::getOpenFileName( this, tr("Wad files(*.wad)"), QCoreApplication::applicationDirPath(), tr("WadFiles (*.wad)") );

    if( fn.isEmpty() )
		return;

    QByteArray data = ReadFile( fn );
    if( data.isEmpty() )
		return;

    Wad wad( data );
    if( !wad.IsOk() )
    {
		ShowMessage( tr( "Wad data not ok for \"%1\"" ).arg( fn ) );
		return;
    }

    //work smart, not hard... just turn the wad into a NUSJob and reused the same code to install it
    NusJob job;
    job.tid = wad.Tid();
    job.data << wad.getTmd();
    job.data << wad.getTik();

    Tmd t( wad.getTmd() );
    job.version = t.Version();
    quint16 cnt  = t.Count();
    for( quint16 i = 0; i < cnt; i++ )
    {
		job.data << wad.Content( i );
    }

    job.decrypt = true;
    ShowMessage( tr( "Installing %1 to nand" ).arg( fn ) );
    InstallNUSItem( job );

}

void MainWindow::on_actionNew_nand_from_keys_triggered()
{
    QString path = NewNandBin::GetNewNandPath( this );
    if( path.isEmpty() )
		return;
    InitNand( path );
    ui->lineEdit_nandPath->setText( path );
}

void MainWindow::on_pushButton_initNand_clicked()
{
    if( ui->lineEdit_nandPath->text().isEmpty() )
    {
		ShowMessage( "<b>Please enter a path for nand.bin<\b>" );
		return;
    }
    InitNand( ui->lineEdit_nandPath->text() );
}

bool MainWindow::InitNand( const QString &path )
{
    nandInited = false;
    sharedDirty = false;
    nandDirty = false;
    if( !nand.SetPath( path ) || !nand.InitNand() )
		return false;

    if( !UpdateTree() )
		return false;

    //setup the uid
    QTreeWidgetItem *it = ItemFromPath( "/sys/uid.sys" );
    if( !it )
    {
		uid.CreateNew( true );
		if( !nand.CreateEntry( "/sys/uid.sys", 0, 0, NAND_FILE, NAND_RW, NAND_RW, 0 ) )
		{
			ShowMessage( "<b>Error creating new uid.sys</b>" );
			return false;
		}
		uidDirty = true;
    }
    else
    {
		QByteArray ba = nand.GetData( "/sys/uid.sys" );
		uid = UIDmap( ba );
    }
    //set up the shared map
    it = ItemFromPath( "/shared1/content.map" );
    if( !it )
    {
		if( !nand.CreateEntry( "/shared1/content.map", 0, 0, NAND_FILE, NAND_RW, NAND_RW, 0 ) )
		{
			sharedDirty = true;
			ShowMessage( "<b>Error creating new content.map</b>" );
			return false;
		}
    }
    else
    {
		QByteArray ba = nand.GetData( "/shared1/content.map" );
		shared = SharedContentMap( ba );
		if( !shared.Check() )//i really dont want to create a new one and rewrite all the contents to match it
			ShowMessage( "<b>Something about the shared map isnt right, but im using it anyways</b>" );
    }
    //nand.Delete( "/title/00000001/00000002/content/title.tmd" );

    nandInited = true;
    ShowMessage( "Set path to nand as " + path );
    return true;
}

//this one is kinda important
// it is in charge of writing the uid, content map, and all metadata to the nand.
//failing to do this will result in any changes not being applied
bool MainWindow::FlushNand()
{
    //qDebug() << "MainWindow::FlushNand()";
    bool r1 = true;
    bool r2 = true;
    if( uidDirty && !nand.SetData( "/sys/uid.sys", uid.Data() ) )
		r1 = false;
    else
		uidDirty = false;


    if( sharedDirty && !nand.SetData( "/shared1/content.map", shared.Data() ) )
		r2 = false;
    else
		sharedDirty = false;

    return ( nand.WriteMetaData() && r1 && r2 );
}

QTreeWidgetItem *MainWindow::FindItem( const QString &s, QTreeWidgetItem *parent )
{
    int cnt = parent->childCount();
    for( int i = 0; i <cnt; i++ )
    {
		QTreeWidgetItem *r = parent->child( i );
		if( r->text( 0 ) == s )
		{
			return r;
		}
    }
    return NULL;
}

QTreeWidgetItem *MainWindow::ItemFromPath( const QString &path )
{
    QTreeWidgetItem *item = root;
    if( !path.startsWith( "/" ) || path.contains( "//" ))
    {
		return NULL;
    }
    int slash = 1;
    while( slash )
    {
		int nextSlash = path.indexOf( "/", slash + 1 );
		QString lookingFor = path.mid( slash, nextSlash - slash );
		item = FindItem( lookingFor, item );
		if( !item )
		{
			//qWarning() << "ItemFromPath ->item not found" << path;
			return NULL;
		}
		slash = nextSlash + 1;
    }
    return item;
}

QString MainWindow::PathFromItem( QTreeWidgetItem *item )
{
    QString ret;
    while( item )
    {
		ret.prepend( "/" + item->text( 0 ) );
		item = item->parent();
		if( item->text( 0 ) == "/" )// dont add the root
			break;
    }
    return ret;
}

bool MainWindow::UpdateTree()
{
    //set up the tree so we know what all is in the nand without asking for it every time
    if( root )
		delete root;
    QTreeWidgetItem *r = nand.GetTree();
    if( r->childCount() != 1 || r->child( 0 )->text( 0 ) != "/" )
    {
		ShowMessage( "The nand FS is seriously broken.  I Couldn't even find a correct root" );
		return false;
    }
    root = r->takeChild( 0 );
    delete r;

    return true;
}

quint16 MainWindow::CreateIfNeeded( const QString &path, quint32 uid, quint16 gid, quint8 attr, quint8 user_perm, quint8 group_perm, quint8 other_perm )
{
	//    qDebug() << "MainWindow::CreateIfNeeded" << path;
    QTreeWidgetItem *item = ItemFromPath( path );
    if( !item )
    {
		quint16 ret = nand.CreateEntry( path, uid, gid, attr, user_perm, group_perm, other_perm );
		if( ret && UpdateTree() )
			return ret;
		return 0;
    }
    //TODO - if the item already exists, check that its attributes match the expected ones
    return item->text( 1 ).toInt();
}

bool MainWindow::InstallSharedContent( const QByteArray stuff, const QByteArray hash )
{
    //qDebug() << "MainWindow::InstallSharedContent";
    QByteArray h;
    if( hash.isEmpty() )
		h = GetSha1( stuff );
    else
		h = hash;

    QString cid = shared.GetAppFromHash( hash );
    if( !cid.isEmpty() )			    //this one is already installed
		return true;
    //qDebug() << "will create new";

    //get next available cid in the shared map
    cid = shared.GetNextEmptyCid();
    shared.AddEntry( cid, h );
    sharedDirty = true;
    //qDebug() << "next cid" << cid;

    //create the file
    quint16 r = CreateIfNeeded( "/shared1/" + cid + ".app", 0, 0, NAND_FILE, NAND_RW, NAND_RW, 0 );
    if( !r )
		return false;

    //write the data to the file
    return nand.SetData( r, stuff );
}

bool MainWindow::InstallNUSItem( NusJob job )
{
    QString title = QString( "%1 v%2" ).arg( job.tid, 16, 16, QChar( '0' ) ).arg( job.version );
    qDebug() << "MainWindow::InstallNUSItem" << title;

    quint16 r;
    quint32 _uid;
    quint16 _gid;
    quint16 cnt;
    bool deleted = false;
    QTreeWidgetItem *content;
    if( !job.tid || !job.data.size() > 2 )
    {
		qWarning() << "bad sizes";
		ShowMessage( "<b>Error installing title " + title + " to nand</b>" );
		return false;
    }
    QString tid = QString( "%1" ).arg( job.tid, 16, 16, QChar( '0' ) );
    QString upper = tid.left( 8 );
    QString lower = tid.right( 8 );

    Tmd t( job.data.takeFirst() );
    Ticket ticket( job.data.takeFirst() );
    if( t.Tid() != job.tid || ticket.Tid() != job.tid )
    {
		qWarning() << "bad tid";
		goto error;
    }
    cnt = t.Count();
    if( job.data.size() != cnt )
    {
		qWarning() << "content count";
		goto error;
    }
    //qDebug() << "uidDirty" << uidDirty;
    if( !uidDirty )
    {
		uidDirty = !uid.GetUid( job.tid, false );
    }
    //qDebug() << "uidDirty" << uidDirty;
    _uid = uid.GetUid( job.tid );

    _gid = t.Gid();
    if( !_uid )
    {
		qWarning() << "no uid";
		goto error;
    }

    //create all the folders
    if( !CreateIfNeeded( "/ticket/" + upper, 0, 0, NAND_DIR, NAND_RW, NAND_RW, 0 ) )
    { qWarning() << "can't create ticket+upper folder";goto error;}

    if( !CreateIfNeeded( "/title/" + upper, 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_READ ) )
    { qWarning() << "can't create title+upper folder";goto error;}

    if( !CreateIfNeeded( "/title/" + upper + "/" + lower, 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_READ ) )
    { qWarning() << "can't create title+upper+lower folder";goto error;}

    if( !CreateIfNeeded( "/title/" + upper + "/" + lower + "/content", 0, 0, NAND_DIR, NAND_RW, NAND_RW, 0 ) )
    { qWarning() << "can't create content folder";goto error;}

    if( !CreateIfNeeded( "/title/" + upper + "/" + lower + "/data", _uid, _gid, NAND_DIR, NAND_RW, 0, 0 ) )
    { qWarning() << "can't create data folder";goto error;}

    //delete old tmd/.apps and whatever else in the content folder
    content = ItemFromPath( "/title/" + upper + "/" + lower + "/content" );
    cnt = content->childCount();
    for ( quint16 i = 0; i < cnt; i++ )
    {
		if( !nand.Delete( "/title/" + upper + "/" + lower + "/content/" + content->child( i )->text( 0 ) )  )
	    { qWarning() << "error deleting old title"; goto error; }
		deleted = true;
    }
    if( deleted )
    {
		//nand.WriteMetaData();
		UpdateTree();
		ShowMessage( tr( "Deleted old TMD and private contents for<br>%1" ).arg( title ) );
    }

    cnt = t.Count();

    //install ticket
    r = CreateIfNeeded( "/ticket/" + upper + "/" + lower + ".tik", 0, 0, NAND_FILE, NAND_RW, NAND_RW, 0 );
    if( !r )
	{ qWarning() << "can't create ticket";goto error;}
    if( !nand.SetData( r, ticket.Data() ) )
	{ qWarning() << "can't write to ticket";goto error;}

    //install tmd
    r = CreateIfNeeded( "/title/" + upper + "/" + lower + "/content/title.tmd", 0, 0, NAND_FILE, NAND_RW, NAND_RW, 0 );
    if( !r )
	{ qWarning() << "can't create tmd";goto error;}
    if( !nand.SetData( r, t.Data() ) )
	{ qWarning() << "can't write to tmd";goto error;}

    //install contents
    //qDebug() << "will install" << cnt << "contents";
    for( quint16 i = 0; i < cnt; i++ )
    {
		//qDebug() << "installing" << i;
		//make sure the data is decrypted
		QByteArray decData;
		QByteArray hash;
		if( job.decrypt )
		{
			decData = job.data.takeFirst();
			if( (quint32)decData.size() != t.Size( i ) )
			{
				qDebug() << "wtf - size";
				decData.resize( t.Size( i ) );
			}
		}
		else
		{
			//decrypt the data
			QByteArray encData = job.data.takeFirst();
			AesSetKey( ticket.DecryptedKey() );
			decData = AesDecrypt( i, encData );
			decData.resize( t.Size( i ) );
		}

		//check the hash
		hash = GetSha1( decData );
		if( hash != t.Hash( i ) )
		{
			qWarning() << "hash" << i << "\n" << hash.toHex() << "\n" << t.Hash( i ).toHex();
			goto error;
		}

		//install the content
		if( t.Type( i ) == 1 )//private
		{
			r = CreateIfNeeded( "/title/" + upper + "/" + lower + "/content/" + t.Cid( i ) + ".app" , 0, 0, NAND_FILE, NAND_RW, NAND_RW, 0 );
			if( !r )
			{
				qWarning() << "cant create content" << i;
				goto error;
			}
			if ( !nand.SetData( r, decData ) )
			{
				qWarning() << "cant write content" << i;
				goto error;
			}
		}
		else if( t.Type( i ) == 0x8001 )//shared
		{
			if ( !InstallSharedContent( decData, hash ) )
			{
				qWarning() << "error installing shared" << i << hash.toHex();
				goto error;
			}
		}
		else
		{
			qWarning() << "type" << hex << t.Type( i );
			goto error;
		}
    }
    qDebug() << "done installing";
    ShowMessage( "Installed title " + title + " to nand" );

    //nand.Delete( "/title/" + upper );
    //UpdateTree();
    return true;
	error:
    ShowMessage( "<b>Error installing title " + title + " to nand</b>" );
    return false;
}

//help -> about
void MainWindow::on_actionAbout_triggered()
{
    QString txt = tr( "This is an example program from WiiQt.  It is designed to write titles to a nand.bin and even create one from scratch."
					  "<br><br>PLEASE BE AWARE, THIS IS NOT VERY WELL TESTED AND AS OF RIGHT NOW."
					  "  IT SHOULD ONLY BE USED BY PEOPLE THAT KNOW HOW TO VERIFY THE FILES IT PRODUCES.  AND HAVE A WAY TO FIX A BRICKED WII SHOULD THIS PROGRAM HAVE BUGS"
					  "<br><br>YOU HAVE BEEN WARNED"
					  "<br>giantpune" );
    QMessageBox::critical( this, tr( "About" ), txt );
}
