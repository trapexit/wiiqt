#include "mainwindow.h"
#include "newnandbin.h"
#include "svnrev.h"
#include "ui_mainwindow.h"

#include "../WiiQt/settingtxtdialog.h"
#include "../WiiQt/tiktmd.h"
#include "../WiiQt/tools.h"
#include "../WiiQt/wad.h"

//on 1 of my wiis, disc 123J owns the test directory & testlog.  this wii came from the factory as 3.2u
//#define NAND_TEST_OWNER 0x100003132334aull

//on my later wiis, disc 121J owns the test directory & testlog.  all came with 4.2u or later
#define NAND_TEST_OWNER 0x100003132314aull

//the group of the test dir/files seems to always be 4
#define NAND_TEST_GROUP 4

MainWindow::MainWindow( QWidget *parent ) : QMainWindow( parent ), ui( new Ui::MainWindow ), nus ( this ), nand ( this )
{
    ui->setupUi(this);
    nandInited = false;
    root = NULL;
    uidDirty = false;
    sharedDirty = false;
    nandDirty = false;

    ui->mainToolBar->setVisible( false );//hide toolbar for now

    //resize buttons to be same size
    QFontMetrics fm( fontMetrics() );
    int max = fm.width( ui->pushButton_CachePathBrowse->text() );
    max = MAX( max, fm.width( ui->pushButton_GetTitle->text() ) );
    max = MAX( max, fm.width( ui->pushButton_initNand->text() ) );
    max = MAX( max, fm.width( ui->pushButton_nandPath->text() ) );

    max += 20;
    ui->pushButton_CachePathBrowse->setFixedWidth( max );
    ui->pushButton_GetTitle->setFixedWidth( max );
    ui->pushButton_initNand->setFixedWidth( max );
    ui->pushButton_nandPath->setFixedWidth( max );

    Wad::SetGlobalCert( QByteArray( (const char*)&certs_dat, CERTS_DAT_SIZE ) );

    //connect to the nus object so we can respond to what it is saying with pretty stuff in the gui
    connect( &nus, SIGNAL( SendDownloadProgress( int ) ), ui->progressBar_dl, SLOT( setValue( int ) ) );
    connect( &nus, SIGNAL( SendTitleProgress( int ) ), ui->progressBar_title, SLOT( setValue( int ) ) );
    connect( &nus, SIGNAL( SendTotalProgress( int ) ), ui->progressBar_whole, SLOT( setValue( int ) ) );
    connect( &nus, SIGNAL( SendText( QString ) ), ui->statusBar, SLOT( showMessage( QString ) ) );
    connect( &nus, SIGNAL( SendError( const QString &, const NusJob & ) ), this, SLOT( GetError( const QString &, const NusJob & ) ) );
    connect( &nus, SIGNAL( SendDone() ), this, SLOT( NusIsDone() ) );
    connect( &nus, SIGNAL( SendData( const NusJob & ) ), this, SLOT( ReceiveTitleFromNus( const NusJob & ) ) );

    //connect to the nand.bin to get text and crap from it
    connect( &nand, SIGNAL( SendError( const QString & ) ), this, SLOT( GetError( const QString & ) ) );
    connect( &nand, SIGNAL( SendText( QString ) ), ui->statusBar, SLOT( showMessage( QString ) ) );

    LoadSettings();
    ui->lineEdit_nandPath->setText( "./testNand.bin" );
}

MainWindow::~MainWindow()
{
    SaveSettings();
    delete ui;
}

void MainWindow::SaveSettings()
{
    QSettings s( QSettings::IniFormat, QSettings::UserScope, "WiiQt", "examples", this );

    //settings specific to this program
    s.beginGroup( "ohneschwanzenegger" );
    s.setValue( "size", size() );
    s.setValue( "pos", pos() );

    s.endGroup();

    //settings shared in multiple programs
    //paths
    s.beginGroup( "paths" );
    s.setValue( "nusCache", ui->lineEdit_cachePath->text() );
    s.endGroup();
}

#ifdef Q_WS_WIN
#define PATH_PREFIX QString("../..")
#else
#define PATH_PREFIX QString("..")
#endif
void MainWindow::LoadSettings()
{
    QSettings s( QSettings::IniFormat, QSettings::UserScope, "WiiQt", "examples", this );

    //settings specific to this program
    s.beginGroup( "ohneschwanzenegger" );
    resize( s.value("size", QSize( 654, 507 ) ).toSize() );
    move( s.value("pos", QPoint( 2, 72 ) ).toPoint() );
    s.endGroup();

    s.beginGroup( "paths" );

    QString cachePath = s.value( "nusCache", PATH_PREFIX + "/NUS_cache" ).toString();
    ui->lineEdit_cachePath->setText( cachePath );
    if( !cachePath.isEmpty() )
        nus.SetCachePath( QFileInfo( cachePath ).absoluteFilePath() );
    s.endGroup();
}

//some slots to respond to the NUS downloader
void MainWindow::GetError( const QString &message, const NusJob &job )
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
    if( !item && ItemFromPath( "/title/00000001/00000002/data" ) )//only try to make setting.txt if it is missing and there is a folder to hold it
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
                if( !nand.SetData( r, ba ) )
                {
                    ShowMessage( "<b>Error writing data for setting.txt.</b>" );
                }
                else
                {
                    nandDirty = true;
                    UpdateTree();
                    ShowMessage( tr( "Saved setting.txt" ) );
                }
            }
        }
    }

    if( nandDirty )
    {
        if( !FlushNand() )
        {
            ShowMessage( "<b>Error flushing nand.  Maybe you used too much TP?</b>" );
        }
        nandDirty = false;
    }
}

void MainWindow::ReceiveTitleFromNus( const NusJob &job )
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
    nus.SetCachePath( ui->lineEdit_cachePath->text() );
    if( wholeUpdate )
    {
        if( !nus.GetUpdate( ui->lineEdit_tid->text(), true ) )
        {
            ShowMessage( tr( "<b>I dont know the titles that were in the %1 update</b>" ).arg( ui->lineEdit_tid->text() ) );
            return;
        }
    }
    else
    {
        nus.Get( tid, true, ver );
    }
}

//search for a path to use as the nand basepath
void MainWindow::on_pushButton_nandPath_clicked()
{
    QString f = QFileDialog::getOpenFileName( this, tr( "Select nand.bin" ) );
    if( f.isEmpty() )
        return;

    ui->lineEdit_nandPath->setText( f );
    InitNand( ui->lineEdit_nandPath->text() );
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

    if( !ItemFromPath( "/title/00000001/00000002/data" ) )
    {
        ShowMessage( tr( "<b>You need to have a system menu before you can create a setting.txt</b>" ) );
        return;
    }

    QByteArray ba;
    if( ItemFromPath( "/title/00000001/00000002/data/setting.txt" ) )
        ba = nand.GetData( "/title/00000001/00000002/data/setting.txt" );	//read the current setting.txt

    ba = SettingTxtDialog::Edit( this, ba );	//call a dialog to edit that existing file and store the result in the same bytearray
    if( !ba.isEmpty() )				//if the dialog returned anything ( cancel wasnt pressed ) write that new setting.txt to the nand dump
    {
        quint16 r = CreateIfNeeded( "/title/00000001/00000002/data/setting.txt", 0x1000, 1, NAND_FILE, NAND_READ, NAND_READ, NAND_READ );
        if( !r || !nand.SetData( r, ba )
                || !nand.WriteMetaData() )
        {
            ShowMessage( tr( "<b>Error writing setting.txt</b>" ) );
        }
        else
            ShowMessage( tr( "Saved setting.txt" ) );
    }
}

//nand-dump -> flush
void MainWindow::on_actionFlush_triggered()
{
    if( nandInited )
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
    //QString fn = QFileDialog::getOpenFileName( this, tr("Wad files(*.wad)"), QCoreApplication::applicationDirPath(), tr("WadFiles (*.wad)") );
    QStringList fns = QFileDialog::getOpenFileNames( this, tr("Wad files(*.wad)"), QCoreApplication::applicationDirPath(), tr("WadFiles (*.wad)") );

    if( fns.isEmpty() )
        return;
    foreach( const QString &fn, fns )
    {
        QByteArray data = ReadFile( fn );
        if( data.isEmpty() )
            continue;

        Wad wad( data );
        if( !wad.IsOk() )
        {
            ShowMessage( tr( "Wad data not ok for \"%1\"" ).arg( fn ) );
            continue;
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
}

void MainWindow::on_actionNew_nand_from_keys_triggered()
{
    QString path = NewNandBin::GetNewNandPath( this );
    if( path.isEmpty() )
        return;
    InitNand( path );
    ui->lineEdit_nandPath->setText( path );

    //these titles should be in order ( not really functional, but to emulate better how the wii comes from the factory )
    if( !nand.CreateEntry( "/title/00000001", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_READ )
            || !nand.CreateEntry( "/title/00000001/00000004", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_READ )
            || !nand.CreateEntry( "/title/00000001/00000009", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_READ )
            || !nand.CreateEntry( "/title/00000001/00000002", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_READ )
            || !nand.CreateEntry( "/title/00000001/00000100", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_READ )
            || !nand.CreateEntry( "/title/00000001/00000101", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_READ ) )
    {
        ShowMessage( "<b>Error creating title subdirs<\b>" );
        return;
    }
    WriteTestLog();
#if 0
    //add some factory test logs and whatnot
    quint32 _uid = uid.GetUid( NAND_TEST_OWNER, true );
    if( !nand.CreateEntry( "/shared2/test", _uid, NAND_TEST_GROUP, NAND_DIR, NAND_RW, NAND_RW, NAND_RW )
            || !nand.CreateEntry( "/shared2/sys", _uid, NAND_TEST_GROUP, NAND_DIR, NAND_RW, NAND_RW, NAND_RW ) )
    {
        ShowMessage( "<b>Error creating folder for testlog<\b>" );
        return;
    }
    quint16 handle = nand.CreateEntry( "/shared2/test/testlog.txt", _uid, NAND_TEST_GROUP, NAND_FILE, NAND_RW, NAND_RW, NAND_RW );
    if( !handle )
    {
        ShowMessage( "<b>Error creating testlog<\b>" );
        return;
    }
    QByteArray tLog = ReadFile( ":/testlog.txt" );
    if( !nand.SetData( handle, tLog ) )
    {
        ShowMessage( "<b>Error writing to testlog.txt<\b>" );
        return;
    }
    UpdateTree();
    ShowMessage( "Created /shared2/test/testlog.txt" );
#endif
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
    ui->menuContent->setEnabled( false );
    if( !nand.SetPath( path ) || !nand.InitNand() )
        return false;

    if( !UpdateTree() )
        return false;

    //setup the uid
    QTreeWidgetItem *it = ItemFromPath( "/sys/uid.sys" );
    if( !it )
    {
        uid.CreateNew();//dont add any UID besides the system menu since we dont know what region it will be
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

    nandInited = true;
    ui->menuContent->setEnabled( true );
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
    {
        delete root;
        root = NULL;
    }
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

bool MainWindow::InstallSharedContent( const QByteArray &stuff, const QByteArray &hash )
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

    if( !CreateIfNeeded( "/title/" + upper + "/" + lower + "/data", _uid, _gid, NAND_DIR, NAND_RW, 0, 0 ) )
    { qWarning() << "can't create data folder";goto error;}

    if( !CreateIfNeeded( "/title/" + upper + "/" + lower + "/content", 0, 0, NAND_DIR, NAND_RW, NAND_RW, 0 ) )
    { qWarning() << "can't create content folder";goto error;}

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
        ShowMessage( tr( "Deleted old TMD and private contents for<br>%1" ).arg( job.tid, 16, 16, QChar( '0' ) ) );
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
                     "<br><br>IT SHOULD ONLY BE USED BY PEOPLE THAT KNOW HOW TO VERIFY THE FILES IT PRODUCES.  AND HAVE A WAY TO FIX A BRICKED WII SHOULD THIS PROGRAM HAVE BUGS"
                     "<br><br>YOU HAVE BEEN WARNED"
                     "<br>giantpune" );
    QMessageBox::critical( this, tr( "svn r%1" ).arg( CleanSvnStr( SVN_REV_STR ) ), txt );
}

#if 0
//add a default settings file if there is one laying around
void MainWindow::TryToAddDefaultSettings()
{
    if( ItemFromPath( "/shared2/sys/SYSCONF" ) )
        return;

    QByteArray stuff = ReadFile( "./default_SYSCONF" );
    if( stuff.isEmpty() )
        return;

    quint32 uiD = uid.GetUid( NAND_TEST_OWNER );
    if( !CreateIfNeeded( "/shared2/sys", uiD, NAND_TEST_GROUP, NAND_DIR, NAND_RW, NAND_RW, NAND_RW ) )
        return;

    quint16 handle = nand.CreateEntry( "/shared2/sys/SYSCONF", uiD, NAND_TEST_GROUP, NAND_FILE, NAND_RW, NAND_RW, NAND_RW );
    if( !handle || !nand.SetData( handle, stuff ) || !nand.WriteMetaData() )
    {
        ShowMessage( "<b>Error adding the default settings</b>" );
        return;
    }
    UpdateTree();
    ShowMessage( "Wrote /shared2/sys/SYSCONF" );
}
#endif

//AFAIK, the stuff in the "/meta" folder is put there by the disc that installs the IOS & system menu at the factory.
//i have not been able to get the wii to create this data on its own using any officially release system menu or other program
void MainWindow::AddStuffToMetaFolder()
{
    if( !ItemFromPath( "/meta" ) )
        return;

    if( !CreateIfNeeded( "/meta/00000001", 0x1000, 1, NAND_DIR, NAND_RW, NAND_RW, NAND_RW ) )
    {
        ShowMessage( "<b>Cannot create folder for metacrap</b>" );
        return;
    }
    bool written = false;

    //these are the bare minimum metadata files ive seen on a nand
    //i have seen some where there are full banners for the 0x10002 titles, but this is not always the case
    for( quint16 i = 0; i < 3; i++ )
    {
        quint64 tid;
        quint16 ver;
        QString desc;
        switch( i )
        {
        case 0:
            tid = 0x100000004ull;
            ver = 3;
            desc = "sd_os1_1.64";
            break;
        case 1:
            tid = 0x100000009ull;
            ver = 1;
            desc = "sd_os1_1.64";
            break;
        case 2:
            tid = 0x100000002ull;
            ver = 0;
            desc = "systemmenu.rvl.0.4";
            break;
        }
        QString tidStr = QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
        tidStr.insert( 8, "/" );
        QString path = "/meta/" + tidStr + "/title.met";
        if( ItemFromPath( path ) )//already have this metadata
            continue;

        //create subfolder
        path = "/meta/" + tidStr;
        if( !CreateIfNeeded( path, 0x1000, 1, NAND_DIR, NAND_RW, NAND_RW, NAND_RW ) )
        {
            ShowMessage( "<b>Cannot create " + path + " for metacrap</b>" );
            return;
        }
        path = "/meta/" + tidStr + "/title.met";

        //generate metacrap
        QByteArray stuff = GenMeta( desc, tid, ver );

        quint16 handle = nand.CreateEntry( path, 0x1000, 1, NAND_FILE, NAND_RW, NAND_RW, NAND_RW );
        if( !handle || !nand.SetData( handle, stuff ) || !UpdateTree() )
        {
            ShowMessage( "<b>Error writing data for " + path + " </b>" );
            return;
        }
        written = true;
    }
    if( !written )
    {
        ShowMessage( "Nothing to write in \"/meta\"" );
        return;
    }
    if( !nand.WriteMetaData() )
    {
        ShowMessage( "<b>Error writing nand metadata for \"/meta\"</b>" );
        return;
    }
    ShowMessage( "Wrote entries for \"/meta\"" );
}

QByteArray MainWindow::GenMeta( const QString &desc, quint64 tid, quint16 version )
{
    QByteArray ret( 0x40, '\0' );
    QBuffer buf( &ret );
    buf.open( QIODevice::WriteOnly );
    buf.write( desc.toLatin1().data() );
    tid = qFromBigEndian( tid );
    buf.seek( 0x20 );
    buf.write( (const char*)&tid, 8 );
    version = qFromBigEndian( version );
    buf.write( (const char*)&version, 4 );
    buf.close();
    return ret;

}

void MainWindow::on_actionWrite_meta_entries_triggered()
{
    AddStuffToMetaFolder();
}

void MainWindow::WriteTestLog()
{
    if( ( !nandInited && !InitNand( ui->lineEdit_nandPath->text() ) )
            || ItemFromPath( "/shared2/test/testlog.txt" ) )                //already exists
        return;

    quint32 _uid = uid.GetUid( NAND_TEST_OWNER, true );
    if( !CreateIfNeeded( "/shared2/test", _uid, NAND_TEST_GROUP, NAND_DIR, NAND_RW, NAND_RW, NAND_RW )
            || !CreateIfNeeded( "/shared2/sys", _uid, NAND_TEST_GROUP, NAND_DIR, NAND_RW, NAND_RW, NAND_RW ) )
    {
        ShowMessage( "<b>Error creating folder for testlog<\b>" );
        return;
    }
    quint16 handle = CreateIfNeeded( "/shared2/test/testlog.txt", _uid, NAND_TEST_GROUP, NAND_FILE, NAND_RW, NAND_RW, NAND_RW );
    if( !handle )
    {
        ShowMessage( "<b>Error creating testlog<\b>" );
        return;
    }
    QByteArray tLog = ReadFile( ":/testlog.txt" );
    if( !nand.SetData( handle, tLog ) )
    {
        ShowMessage( "<b>Error writing to testlog.txt<\b>" );
        return;
    }
    ShowMessage( "Created /shared2/test/testlog.txt" );
}

//content -> format
void MainWindow::on_actionFormat_triggered()
{
    if( nand.FilePath().isEmpty() )
        return;
    if( QMessageBox::warning( this, tr( "Format" ), \
                             tr( "You are about to format<br>%1.<br><br>This cannot be undone.  Are you sure you want to do it?" ).arg( nand.FilePath() ),\
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No ) != QMessageBox::Yes )
        return;

    setCursor( Qt::BusyCursor );
    ShowMessage( "Formatting nand..." );
    if( !nand.Format() )
    {
        unsetCursor();
        ShowMessage( "<b>Error!  This nand may be broken now :(</b>" );
        return;
    }
    unsetCursor();

    //add folders to root
    if( !nand.CreateEntry( "/sys", 0, 0, NAND_DIR, NAND_RW, NAND_RW, 0 )
            || !nand.CreateEntry( "/ticket", 0, 0, NAND_DIR, NAND_RW, NAND_RW, 0 )
            || !nand.CreateEntry( "/title", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_READ )
            || !nand.CreateEntry( "/shared1", 0, 0, NAND_DIR, NAND_RW, NAND_RW, 0 )
            || !nand.CreateEntry( "/shared2", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_RW )
            || !nand.CreateEntry( "/import", 0, 0, NAND_DIR, NAND_RW, NAND_RW, 0 )
            || !nand.CreateEntry( "/meta", 0x1000, 1, NAND_DIR, NAND_RW, NAND_RW, NAND_RW )
            || !nand.CreateEntry( "/tmp", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_RW ) )
    {
        ShowMessage( "<b>Error! Can't create base folders in the new nand.</b>" );
        return;
    }
    //add cert.sys
    quint16 handle = nand.CreateEntry( "/sys/cert.sys", 0, 0, NAND_FILE, NAND_RW, NAND_RW, NAND_READ );
    if( !handle || !nand.SetData( handle, QByteArray( (const char*)&certs_dat, CERTS_DAT_SIZE ) ) )
    {
        ShowMessage( "<b>Error! Can't cert.sys.</b>" );
        return;
    }

    //wipe all user-created entries from uid.sys
    QByteArray uidData = uid.Data();
    QBuffer buf( &uidData );
    buf.open( QIODevice::ReadOnly );

    quint64 tid;
    quint16 titles = 0;
    quint32 cnt = uidData.size() / 12;
    for( quint32 i = 0; i < cnt; i++ )
    {
        buf.seek( i * 12 );
        buf.read( (char*)&tid, 8 );
        tid = qFromBigEndian( tid );
        quint32 upper = ( ( tid >> 32 ) & 0xffffffff );
        quint32 lower = ( tid & 0xffffffff );
        //qDebug() << hex << i << QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) ) << upper << lower << QChar( ( lower >> 24 ) & 0xff ) << ( lower & 0xffffff00 );
        if( ( upper == 0x10001 && ( ( lower >> 24 ) & 0xff ) != 0x48 ) ||		//a channel, not starting with 'H'
                lower == 0x48415858 ||												//original HBC
                tid == 0x100000000ull ||											//bannerbomb -> ATD ( or any other program that uses the SU tid )
                ( upper == 0x10000 && ( ( lower & 0xffffff00 ) == 0x555000 ) ) )	//a disc update partition
            break;
        titles++;
    }
    buf.close();

    uidData.resize( 12 * titles );
    //hexdump12( uidData );
    uid = UIDmap( uidData );

    uidDirty = true;
    sharedDirty = true;
    shared = SharedContentMap();
    if( !nand.CreateEntry( "/sys/uid.sys", 0, 0, NAND_FILE, NAND_RW, NAND_RW, 0 ) )
    {
        ShowMessage( "<b>Error! Can't create /sys/uid.sys</b>" );
        return;
    }
    //clear content.map
    if( !nand.CreateEntry( "/shared1/content.map", 0, 0, NAND_FILE, NAND_RW, NAND_RW, 0 ) )
    {
        ShowMessage( "<b>Error! Can't create /shared1/content.map</b>" );
        return;
    }

    //commit
    if( !nand.WriteMetaData() || !UpdateTree() )
    {
        ShowMessage( "<b>Error finalizing formatting!</b>" );
        return;
    }
    WriteTestLog();
    ShowMessage( "Done!" );
}

// respond to keyboard events
void MainWindow::keyPressEvent( QKeyEvent *event )
{
    if( event->key() == Qt::Key_Return )
    {
        on_pushButton_GetTitle_clicked();
        event->accept();
        return;
    }

    event->ignore();
}
