#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "../WiiQt/settingtxtdialog.h"
#include "../WiiQt/tiktmd.h"
#include "../WiiQt/tools.h"
#include "../WiiQt/wad.h"


MainWindow::MainWindow( QWidget *parent ) : QMainWindow( parent ), ui( new Ui::MainWindow ), nus ( this )
{
    ui->setupUi(this);
    ui->mainToolBar->setVisible( false );//hide toolbar for now

    //resize buttons to be same size
    QFontMetrics fm( fontMetrics() );
    int max = fm.width( ui->pushButton_CachePathBrowse->text() );
    max = MAX( max, fm.width( ui->pushButton_decFolder->text() ) );
    max = MAX( max, fm.width( ui->pushButton_GetTitle->text() ) );
    max = MAX( max, fm.width( ui->pushButton_nandPath->text() ) );
    max = MAX( max, fm.width( ui->pushButton_wad->text() ) );

    max += 15;
    ui->pushButton_CachePathBrowse->setMinimumWidth( max );
    ui->pushButton_decFolder->setMinimumWidth( max );
    ui->pushButton_GetTitle->setMinimumWidth( max );
    ui->pushButton_nandPath->setMinimumWidth( max );
    ui->pushButton_wad->setMinimumWidth( max );

    Wad::SetGlobalCert( QByteArray( (const char*)&certs_dat, CERTS_DAT_SIZE ) );

    //connect to the nus object so we can respond to what it is saying with pretty stuff in the gui
    connect( &nus, SIGNAL( SendDownloadProgress( int ) ), ui->progressBar_dl, SLOT( setValue( int ) ) );
    connect( &nus, SIGNAL( SendTitleProgress( int ) ), ui->progressBar_title, SLOT( setValue( int ) ) );
    connect( &nus, SIGNAL( SendTotalProgress( int ) ), ui->progressBar_whole, SLOT( setValue( int ) ) );
    connect( &nus, SIGNAL( SendText( QString ) ), ui->statusBar, SLOT( showMessage( QString ) ) );
    connect( &nus, SIGNAL( SendError( const QString &, const NusJob & ) ), this, SLOT( GetError( const QString &, const NusJob & ) ) );
    connect( &nus, SIGNAL( SendDone() ), this, SLOT( NusIsDone() ) );
    connect( &nus, SIGNAL( SendData( const NusJob & ) ), this, SLOT( ReceiveTitleFromNus( const NusJob & ) ) );

    LoadSettings();
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
    s.beginGroup( "nusDownloader" );
    //window geometry
    s.setValue( "size", size() );
    s.setValue( "pos", pos() );

    //which radio button is selected
    quint8 val = 0;
    if( ui->radioButton_folder->isChecked() )
        val = 1;
    else if( ui->radioButton_wad->isChecked() )
        val = 2;
    s.setValue( "radio", val );

    s.setValue( "folder", ui->lineEdit_extractPath->text() );
    s.setValue( "nuswads", ui->lineEdit_wad->text() );

    s.endGroup();

    //settings shared in multiple programs
    //paths
    s.beginGroup( "paths" );
    s.setValue( "nusCache", ui->lineEdit_cachePath->text() );
    s.setValue( "sneek", ui->lineEdit_nandPath->text() );
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
    s.beginGroup( "nusDownloader" );
    resize( s.value("size", QSize( 585, 457 ) ).toSize() );
    move( s.value("pos", QPoint( 2, 72 ) ).toPoint() );

    quint8 radio = s.value( "radio", 0 ).toInt();
    if( radio == 1 )
        ui->radioButton_folder->setChecked( true );
    else if( radio == 2 )
        ui->radioButton_wad->setChecked( true );

    ui->lineEdit_extractPath->setText( s.value( "folder", PATH_PREFIX + "/downloaded" ).toString() );
    ui->lineEdit_wad->setText( s.value( "nuswads", PATH_PREFIX + "/wads" ).toString() );

    s.endGroup();

    //settings shared in multiple programs
    s.beginGroup( "paths" );

    QString cachePath = s.value( "nusCache", PATH_PREFIX + "/NUS_cache" ).toString();
    QString nandPath = s.value( "sneek" ).toString();
    ui->lineEdit_cachePath->setText( cachePath );
    ui->lineEdit_nandPath->setText( nandPath );

    if( !nandPath.isEmpty() )
        nand.SetPath( QFileInfo( nandPath ).absoluteFilePath() );
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
    if( ui->radioButton_folder->isChecked() )
    {
        ui->lineEdit_extractPath->setEnabled( true );
        ui->pushButton_decFolder->setEnabled( true );
    }
    else if( ui->radioButton_nand->isChecked() )
    {
        //check if IOS35 is present in nand dump - needed for sneek
        QByteArray tmdBA = nand.GetFile( "/title/00000001/00000023/content/title.tmd" );
        if( tmdBA.isEmpty() )
        {
            ui->plainTextEdit_log->appendHtml( tr( "IOS35 not found on nand.  Getting it now...") );
            nus.Get( 0x100000023ull, true );
            return;
        }
        ui->lineEdit_nandPath->setEnabled( true );
        ui->pushButton_nandPath->setEnabled( true );

        //write the uid.sys and content.map to disc
        ShowMessage( tr( "Flushing nand..." ) );
        nand.Flush();

        //make sure there is a setting.txt
        QByteArray set = nand.GetSettingTxt();
        if( set.isEmpty() )
        {
            quint8 reg = SETTING_TXT_UNK;
            if( ui->lineEdit_tid->text().endsWith( "e", Qt::CaseInsensitive ) && ui->lineEdit_tid->text().size() == 4 )
                reg = SETTING_TXT_PAL;
            if( ui->lineEdit_tid->text().endsWith( "j", Qt::CaseInsensitive ) && ui->lineEdit_tid->text().size() == 4 )
                reg = SETTING_TXT_JAP;
            if( ui->lineEdit_tid->text().endsWith( "k", Qt::CaseInsensitive ) && ui->lineEdit_tid->text().size() == 4 )
                reg = SETTING_TXT_KOR;
            set = SettingTxtDialog::Edit( this, QByteArray(), reg );
            if( !set.isEmpty() )
                nand.SetSettingTxt( set );
        }
        /*QMap< quint64, quint16 > t = nand.GetInstalledTitles();
    QMap< quint64, quint16 >::iterator i = t.begin();
    while( i != t.end() )
    {
        QString title = QString( "%1v%2" ).arg( i.key(), 16, 16, QChar( '0' ) ).arg( i.value() );
        qDebug() << "title:" << title;
        i++;
    }*/
    }
    else if( ui->radioButton_wad->isChecked() )
    {
        ui->lineEdit_wad->setEnabled( true );
        ui->pushButton_wad->setEnabled( true );
    }

    ui->radioButton_folder->setEnabled( true );
    ui->radioButton_nand->setEnabled( true );
    ui->radioButton_wad->setEnabled( true );
}

void MainWindow::ReceiveTitleFromNus( const NusJob &job )
{
    QString str = tr( "Received a completed download from NUS" );
    QString title = QString( "%1v%2" ).arg( job.tid, 16, 16, QChar( '0' ) ).arg( job.version );

    ui->plainTextEdit_log->appendHtml( str );

    //do something with the data we got
    if( ui->radioButton_folder->isChecked() )//copy its decrypted contents to a folder
    {
        SaveJobToFolder( job );
    }
    else if( ui->radioButton_nand->isChecked() )//install this title to a decrypted nand dump for sneek/dolphin
    {
        bool ok = nand.InstallNusItem( job );
        if( ok )
            ShowMessage( tr( "Installed %1 title to nand" ).arg( title ) );
        else
            ShowMessage( tr( "<b>Error installing %1 title to nand</b>" ).arg( title ) );
    }
    else if( ui->radioButton_wad->isChecked() )
    {
        SaveJobToWad( job );
    }
}

//clicked the button to get a title
void MainWindow::on_pushButton_GetTitle_clicked()
{
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
    if( ui->radioButton_folder->isChecked() )
    {
        if( ui->lineEdit_extractPath->text().isEmpty() )
        {
            ShowMessage( tr( "<b>No path given to save downloads in.</b>" ) );
            return;
        }
        ui->lineEdit_extractPath->setEnabled( false );
        ui->pushButton_decFolder->setEnabled( false );
    }
    else if( ui->radioButton_nand->isChecked() )
    {
        if( nand.GetPath() != ui->lineEdit_nandPath->text() && !nand.SetPath( ui->lineEdit_nandPath->text() ) )
        {
            ShowMessage( tr( "<b>Error setting the basepath of the nand to %1</b>" )
                         .arg( QFileInfo( ui->lineEdit_nandPath->text() ).absoluteFilePath() ) );
            return;
        }
        if( ui->lineEdit_nandPath->text().isEmpty() )
        {
            ShowMessage( tr( "<b>No path given for nand dump base.</b>" ) );
            return;
        }
        ui->lineEdit_nandPath->setEnabled( false );
        ui->pushButton_nandPath->setEnabled( false );
    }
    else if( ui->radioButton_wad->isChecked() )
    {
        if( ui->lineEdit_wad->text().isEmpty() )
        {
            ShowMessage( tr( "<b>No path given to save wads in.</b>" ) );
            return;
        }
        decrypt = false;
        ui->lineEdit_wad->setEnabled( false );
        ui->pushButton_wad->setEnabled( false );

    }

    ui->radioButton_folder->setEnabled( false );
    ui->radioButton_nand->setEnabled( false );
    ui->radioButton_wad->setEnabled( false );
    //dont set these to 0 in case the button is pressed while something else is already being downloaded
    //ui->progressBar_dl->setValue( 0 );
    //ui->progressBar_title->setValue( 0 );
    //ui->progressBar_whole->setValue( 0 );
    nus.SetCachePath( QFileInfo( ui->lineEdit_cachePath->text() ).absoluteFilePath() );

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

//ratio buttons toggled
void MainWindow::on_radioButton_nand_toggled( bool checked )
{
    ui->lineEdit_nandPath->setEnabled( checked );
    ui->pushButton_nandPath->setEnabled( checked );
}

void MainWindow::on_radioButton_folder_toggled( bool checked )
{
    ui->lineEdit_extractPath->setEnabled( checked );
    ui->pushButton_decFolder->setEnabled( checked );
}

void MainWindow::on_radioButton_wad_toggled( bool checked )
{
    ui->lineEdit_wad->setEnabled( checked );
    ui->pushButton_wad->setEnabled( checked );
}

//search for a path to use as the nand basepath
void MainWindow::on_pushButton_nandPath_clicked()
{
    QString path = ui->lineEdit_nandPath->text().isEmpty() ? "/media" : ui->lineEdit_nandPath->text();
    QString f = QFileDialog::getExistingDirectory( this, tr( "Select Nand Base Folder" ), path );
    if( f.isEmpty() )
        return;

    ui->lineEdit_nandPath->setText( f );
    nus.SetCachePath( ui->lineEdit_cachePath->text() );
}

void MainWindow::on_pushButton_decFolder_clicked()
{
    QString path = ui->lineEdit_extractPath->text().isEmpty() ? QDir::currentPath() : ui->lineEdit_extractPath->text();
    QString f = QFileDialog::getExistingDirectory( this, tr( "Select folder to save decrypted titles" ), path );
    if( f.isEmpty() )
        return;

    ui->lineEdit_extractPath->setText( f );
}

void MainWindow::on_pushButton_wad_clicked()
{
    QString path = ui->lineEdit_wad->text().isEmpty() ? QDir::currentPath() : ui->lineEdit_wad->text();
    QString f = QFileDialog::getExistingDirectory( this, tr( "Select folder to save wads to" ), path );
    if( f.isEmpty() )
        return;

    ui->lineEdit_wad->setText( f );
}

//nand-dump -> setting.txt
void MainWindow::on_actionSetting_txt_triggered()
{
    if( nand.GetPath() != ui->lineEdit_nandPath->text() && !nand.SetPath( ui->lineEdit_nandPath->text() ) )
    {
        ShowMessage( tr( "<b>Error setting the basepath of the nand to %1</b>" )
                     .arg( QFileInfo( ui->lineEdit_nandPath->text() ).absoluteFilePath() ) );
        return;
    }
    QByteArray ba = nand.GetSettingTxt();	//read the current setting.txt
    ba = SettingTxtDialog::Edit( this, ba );	//call a dialog to edit that existing file and store the result in the same bytearray
    if( !ba.isEmpty() )				//if the dialog returned anything ( cancel wasnt pressed ) write that new setting.txt to the nand dump
        nand.SetSettingTxt( ba );
}

//nand-dump -> flush
void MainWindow::on_actionFlush_triggered()
{
    if( !nand.GetPath().isEmpty() )
        nand.Flush();
}

//nand-dump -> ImportWad
void MainWindow::on_actionImportWad_triggered()
{
    if( nand.GetPath() != ui->lineEdit_nandPath->text() &&
        !nand.SetPath( ui->lineEdit_nandPath->text() ) )
    {
        ShowMessage( tr( "<b>Error setting the basepath of the nand to %1</b>" ).arg( QFileInfo( ui->lineEdit_nandPath->text() ).absoluteFilePath() ) );
        return;
    }
    QString path = ui->lineEdit_wad->text().isEmpty() ?
                   QCoreApplication::applicationDirPath() : ui->lineEdit_wad->text();
    QString fn = QFileDialog::getOpenFileName( this,
                                               tr("Wad files(*.wad)"),
                                               path,
                                               tr("WadFiles (*.wad)"));
    if(fn == "") return;

    QByteArray data = ReadFile( fn );
    if( data.isEmpty() )
        return;

    Wad wad(data);
    if( !wad.IsOk() ) {
        ShowMessage( tr( "Wad data not ok" ) );;
        return;
    }
    bool ok = nand.InstallWad( wad );
    if( ok )
        ShowMessage( tr( "Installed %1 title to nand" ).arg( wad.WadName() ) );
    else
        ShowMessage( tr( "<b>Error %1 title to nand</b>" ).arg( wad.WadName() ) );
}

//save a NUS job to a folder
void MainWindow::SaveJobToFolder( NusJob job )
{
    QString title = QString( "%1v%2" ).arg( job.tid, 16, 16, QChar( '0' ) ).arg( job.version );
    QFileInfo fi( ui->lineEdit_extractPath->text() );
    if( fi.isFile() )
    {
        ShowMessage( "<b>" + ui->lineEdit_extractPath->text() + " is a file.  I need a folder<\b>" );
        return;
    }
    if( !fi.exists() )
    {
        ShowMessage( "<b>" + fi.absoluteFilePath() + " is not a folder!\nTrying to create it...<\b>" );
        if( !QDir().mkpath( fi.absoluteFilePath() ) )
        {
            ShowMessage( "<b>Failed to make the directory!<\b>" );
            return;
        }
    }
    QString newFName = title;
    int i = 1;
    while( QFileInfo( fi.absoluteFilePath() + "/" + newFName ).exists() )//find a folder that doesnt exist and try to create it
    {
        newFName = QString( "%1 (copy%2)" ).arg( title ).arg( i++ );
    }
    if( !QDir().mkpath( fi.absoluteFilePath() + "/" + newFName ) )
    {
        ShowMessage( "<b>Can't create" + fi.absoluteFilePath() + "/" + newFName + " to save this title into!<\b>" );
        return;
    }
    //start writing all this stuff to the HDD
    QDir d( fi.absoluteFilePath() + "/" + newFName );
    QByteArray tmdDat = job.data.takeFirst();	    //remember the tmd and use it for getting the names of the .app files
    if( !WriteFile( d.absoluteFilePath( "title.tmd" ), tmdDat ) )
    {
        ShowMessage( "<b>Error writing " + d.absoluteFilePath( "title.tmd" ) + "!<\b>" );
        return;
    }
    if( !WriteFile( d.absoluteFilePath( "cetk" ), job.data.takeFirst() ) )
    {
        ShowMessage( "<b>Error writing " + d.absoluteFilePath( "cetk" ) + "!<\b>" );
        return;
    }
    Tmd t( tmdDat );
    quint16 cnt = t.Count();
    if( job.data.size() != cnt )
    {
        ShowMessage( "<b>Error! Number of contents in the TMD dont match the number received from NUS!<\b>" );
        return;
    }
    for( quint16 i = 0; i < cnt; i++ )//write all the contents in the new folder. if the job is decrypted, append ".app" to the end of their names
    {
        QString appName = t.Cid( i );
        QByteArray stuff = job.data.takeFirst();
        if( job.decrypt )
        {
            appName += ".app";
            //qDebug() << "resizing from" << hex << stuff.size() << "to" << (quint32)t.Size( i );
            //stuff.resize( t.Size( i ) );
        }
        if( !WriteFile( d.absoluteFilePath( appName ), stuff ) )
        {
            ShowMessage( "<b>Error writing " + d.absoluteFilePath( appName ) + "!<\b>" );
            return;
        }
    }
    ShowMessage( tr( "Wrote title to %1" ).arg( fi.absoluteFilePath() + "/" + newFName ) );
}

//save a completed job to wad
void MainWindow::SaveJobToWad( NusJob job )
{
    QString title = QString( "%1v%2" ).arg( job.tid, 16, 16, QChar( '0' ) ).arg( job.version );
    Wad wad( job.data );
    if( !wad.IsOk() )
    {
        ShowMessage( "<b>Error making a wad from " + title + "<\b>" );
        return;
    }
    QFileInfo fi( ui->lineEdit_wad->text() );
    if( fi.isFile() )
    {
        ShowMessage( "<b>" + ui->lineEdit_wad->text() + " is a file.  I need a folder<\b>" );
        return;
    }
    if( !fi.exists()  )
    {
        ShowMessage( "<b>" + fi.absoluteFilePath() + " is not a folder!\nTrying to create it...<\b>" );
        if( !QDir().mkpath( ui->lineEdit_wad->text() ) )
        {
            ShowMessage( "<b>Failed to make the directory!<\b>" );
            return;
        }
    }
    QByteArray w = wad.Data();
    if( w.isEmpty() )
    {
        ShowMessage( "<b>Error creating wad<br>" + wad.LastError() + "<\b>" );
        return;
    }

    QString name = wad.WadName( fi.absoluteFilePath() );
    if( name.isEmpty() )
    {
        name = QFileDialog::getSaveFileName( this, tr( "Filename for %1" ).arg( title ), fi.absoluteFilePath() );
        if( name.isEmpty() )
        {
            ShowMessage( "<b>No save name given, aborting<\b>" );
            return;
        }
    }
    QFile file( fi.absoluteFilePath() + "/" + name );
    if( !file.open( QIODevice::WriteOnly ) )
    {
        ShowMessage( "<b>Cant open " + fi.absoluteFilePath() + "/" + name + " for writing<\b>" );
        return;
    }
    file.write( w );
    file.close();
    ShowMessage( "Saved " + title + " to " + fi.absoluteFilePath() + "/" + name );


}

void MainWindow::on_pushButton_CachePathBrowse_clicked()
{
    QString f = QFileDialog::getExistingDirectory( this, tr( "Select NUS Cache base folder" ) );
    if( f.isEmpty() )
        return;

    ui->lineEdit_cachePath->setText( f );
    nus.SetCachePath( ui->lineEdit_cachePath->text() );
}
