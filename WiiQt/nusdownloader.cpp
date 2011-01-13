#include "nusdownloader.h"
#include "tools.h"


NusDownloader::NusDownloader( QObject *parent, const QString &cPath ) : QObject( parent ), cachePath( cPath ), curTmd( QByteArray() )
{
    currentJob.tid = 0;
    currentJob.version = 0;
    totalJobs = 0;
    running = false;
}

//change the cache path
void NusDownloader::SetCachePath( const QString &cPath )
{
	//qDebug() << "NusDownloader::SetCachePath" << cPath;
    cachePath = cPath;
}

//add a single job to the list
void NusDownloader::GetTitle( const NusJob &job )
{
    //qDebug() << "NusDownloader::GetTitle";
    jobList.append( job );
    totalJobs++;

    if( !running )
    {
        //qDebug() << "no job is running, starting this one";
        QTimer::singleShot( 0, this, SLOT( StartNextJob() ) );
    }
    running = true;
}

//add a list of jobs to the list
void NusDownloader::GetTitles( const QList<NusJob> &jobs )
{
    //qDebug() << "NusDownloader::GetTitles";
    jobList.append( jobs );
    totalJobs += jobs.size();

    if( !running )
        QTimer::singleShot( 0, this, SLOT( StartNextJob() ) );

    running = true;
}

//add a new job to the list
void NusDownloader::Get( quint64 tid, bool decrypt, quint16 version )
{
    NusJob j;
    j.tid = tid;
    j.decrypt = decrypt;
    j.version = version;

    GetTitle( j );
}

//get how much of this title is already downloaded
quint32 NusDownloader::TitleSizeDownloaded()
{
    quint32 ret = 0;
    for( int i = 0; i < currentJob.data.size(); i++ )
        ret += currentJob.data.at( i ).size();
    return ret;
}

//start downloading the next title in the list
void NusDownloader::StartNextJob()
{
    //qDebug() << "NusDownloader::StartNextJob";
    if( jobList.isEmpty() )//nothing else to do
    {
        currentJob.tid = 0;
        totalJobs = 0;
        emit SendTitleProgress( 100 );
        emit SendTotalProgress( 100 );
        running = false;
        //qDebug() << "done";
        emit SendDone();
        return;
    }
    //pull the first title from the list
    currentJob = jobList.takeFirst();
    SendTitleProgress( 0 );
    downloadJob tmdJob;
    tmdJob.tid = QString( "%1" ).arg( currentJob.tid, 16, 16, QChar( '0' ) );
    tmdJob.index = IDX_TMD;
    if( currentJob.version != TITLE_LATEST_VERSION )
    {
        tmdJob.name = QString( "tmd.%1" ).arg( currentJob.version );
        QByteArray stuff = GetDataFromCache( tmdJob );
        //DbgJoB( currentJob );
        if( !stuff.isEmpty() )
        {
            //qDebug() << "tmdJob.data size:" << hex << stuff.size();
            //DbgJoB( currentJob );
            ReadTmdAndGetTicket( stuff );
        }
        else
        {
            dlJob = tmdJob;
			QTimer::singleShot( 50, this, SLOT( StartDownload() ) );
        }
    }
    else//download the latest tmd to get the version
    {
        tmdJob.name = "tmd";
        dlJob = tmdJob;
        QTimer::singleShot( 500, this, SLOT( StartDownload() ) );
    }

}

//tries to read data for the job from the PC
QByteArray NusDownloader::GetDataFromCache( downloadJob job )
{
    //qDebug() << "NusDownloader::GetDataFromCache";
    if( cachePath.isEmpty() || currentJob.version == TITLE_LATEST_VERSION )
        return QByteArray();

    QFileInfo fi( cachePath );
    if( !fi.exists() || !fi.isDir() )
    {
        //qWarning() << "NusDownloader::GetDataFromCache -> cachePath is not a directory";
        return QByteArray();
    }

    QFile f( GetCachePath( job.index ) );
    if( !f.exists() || !f.open( QIODevice::ReadOnly ) )
    {
        //qWarning() << "NusDownloader::GetDataFromCache -> file cant be opened for reading" << QFileInfo( f ).absoluteFilePath();
        return QByteArray();
    }

    //qDebug() << "reading data from PC";
    QByteArray ret = f.readAll();
    f.close();
    //qDebug() << "read" << hex << ret.size() << "bytes of data from" << QFileInfo( f ).absoluteFilePath();
    return ret;
}

//load the tmd and try to get the ticket
void NusDownloader::ReadTmdAndGetTicket( const QByteArray &ba )
{
    //qDebug() << "NusDownloader::ReadTmdAndGetTicket" << hex << ba.size();
    curTmd = Tmd( ba );
    if( curTmd.Tid() != currentJob.tid )
    {
        qDebug() << curTmd.Tid() << currentJob.tid;
        CurrentJobErrored( tr( "TID in TMD doesn't match expected." ) );
        return;
    }
    if( currentJob.version == TITLE_LATEST_VERSION )
    {
        currentJob.version = qFromBigEndian( curTmd.payload()->title_version );
    }
    else if( currentJob.version != qFromBigEndian( curTmd.payload()->title_version ) )
    {
        CurrentJobErrored( tr( "Version in TMD doesn't match expected." ) );
        return;
    }
    //add the tmd data to the current job return
    currentJob.data << ba;

    //calculate the total size for this title
    totalTitleSize = 0;
    for( quint32 i = 0; i < qFromBigEndian( curTmd.payload()->num_contents ); i++ )
    {
        totalTitleSize += curTmd.Size( i );
    }

    totalTitleSize += ba.size() + 0x9a4;//ticket size for ios 9.  should be good enough for everything else


    //now get the ticket
    downloadJob tikJob = CreateJob( "cetk", IDX_CETK );
    QByteArray stuff = GetDataFromCache( tikJob );
    if( stuff.isEmpty() )
    {
        dlJob = tikJob;
        QTimer::singleShot( 0, this, SLOT( StartDownload() ) );
    }
    else
    {
        Ticket t( stuff );
        //set this key to decrypt contents
        decKey = t.DecryptedKey();
        //AesSetKey( t.DecryptedKey() );
        //add the ticket data to the return
        currentJob.data << stuff;
        QTimer::singleShot( 0, this, SLOT( GetNextItemForCurrentTitle() ) );
    }
}

//save data downloaded from the internet to local HDD for future downloads
bool NusDownloader::SaveDataToCache( const QString &path, const QByteArray &stuff )
{
    //make sure there is all the parent folders needed to hold this folder
    if( path.count( "/" ) < 4  || !path.startsWith( cachePath + "/" ))
    {
        qWarning() << "NusDownloader::SaveDataToCache -> bad path" << path << cachePath;
        return false;
    }
	QFileInfo fi( path );
	QString parent = fi.absolutePath();
	if( !QDir( parent ).exists() && !QDir().mkpath( parent ) )
    {
		qWarning() << "NusDownloader::SaveDataToCache -> cant create directory" << parent;
        return false;
    }
    QFile f( path );
    if( f.exists() )
    {
        qWarning() << "NusDownloader::SaveDataToCache -> file already exists" << path;
        return false;
    }
    if( !f.open( QIODevice::WriteOnly ) )
    {
        qWarning() << "NusDownloader::SaveDataToCache -> can't create file" << path;
        return false;
    }
    f.write( stuff );//probably should check the return values on these.  but if they dont go right, then the person has bigger things to worry about
    f.close();
    qDebug() << "saved" << hex << stuff.size() << "bytes to" << path;
    return true;
}

downloadJob NusDownloader::CreateJob( const QString &name, quint16 index )
{
    downloadJob r;
    r.tid = QString( "%1" ).arg( currentJob.tid, 16, 16, QChar( '0' ) );
    r.name = name;
    r.index = index;
    r.data = QByteArray();
    return r;
}

//send an error about the current job and move to the next
void NusDownloader::CurrentJobErrored( const QString &str )
{
    qWarning() << "NusDownloader::CurrentJobErrored ->" << str;
    emit SendError( str, currentJob );
    QTimer::singleShot( 0, this, SLOT( StartNextJob() ) );
}

//get the next content for the current title
void NusDownloader::GetNextItemForCurrentTitle()
{
    //qDebug() << "NusDownloader::GetNextItemForCurrentTitle" << currentJob.data.size() - 2;
    //DbgJoB( currentJob );
    if( currentJob.data.size() < 2 )
    {
        qDebug() << "currentJob.data.size() < 2 )" << currentJob.data.size();
        CurrentJobErrored( tr( "Tried to download contents without having the TMD & Ticket") );
        return;
    }

    quint32 alreadyHave = currentJob.data.size() - 2;//number of contest from this title already gotten

    if( alreadyHave >= qFromBigEndian( curTmd.payload()->num_contents ) )//WTF
    {
        qDebug() << "alreadyHave >= qFromBigEndian( curTmd.payload()->num_contents )" << alreadyHave << qFromBigEndian( curTmd.payload()->num_contents );
        CurrentJobErrored( tr( "Tried to download more contents then this title has." ) );
        return;
    }
    //send progress about how much of this title we already have
    int prog = (int)( (float)( (float)TitleSizeDownloaded() / (float)totalTitleSize ) * 100.0f );
    //qDebug() << "titleProg:" << hex << TitleSizeDownloaded() << totalTitleSize << prog;
    emit SendTitleProgress( prog );

    downloadJob appJob = CreateJob( curTmd.Cid( alreadyHave ), alreadyHave );
    QByteArray stuff = GetDataFromCache( appJob );
    if( stuff.isEmpty() )
    {
        dlJob = appJob;
        QTimer::singleShot( 0, this, SLOT( StartDownload() ) );
        //StartDownload( appJob );
    }
    else
    {
        //hexdump( stuff );
        if( !DecryptCheckHashAndAppendData( stuff, appJob.index ) )
        {
            CurrentJobErrored( tr( "Cached data has a different hash than expected." ) );
            return;
        }
        //qDebug() << "hash matched for index" << alreadyHave;
        if( alreadyHave + 1 < qFromBigEndian( curTmd.payload()->num_contents ) )
            QTimer::singleShot( 0, this, SLOT( GetNextItemForCurrentTitle() ) );//next content

        else
        {
            int progress = (int)( ( (float)( totalJobs - jobList.size() ) / (float)totalJobs ) * 100.0f );
            emit SendTotalProgress( progress );
            emit SendTitleProgress( 100 );
            emit SendData( currentJob );
            QTimer::singleShot( 0, this, SLOT( StartNextJob() ) );//start next job
        }
    }
}

//get a path for an item in the cache
QString NusDownloader::GetCachePath( quint32 idx )
{
    //qDebug() << "NusDownloader::GetCachePath" << currentJob.version << currentJob.tid;
    if( currentJob.version == TITLE_LATEST_VERSION || !currentJob.tid )//c'mon guy
        return QString();

    QString path = cachePath;
    if( path.endsWith( "/" ) )
        path.resize( path.size() - 1 );
    QString idPath = QString( "/%1" ).arg( currentJob.tid, 16, 16, QChar( '0' ) );
    idPath.insert( 9, "/" );
    QString verPath = QString( "/v%1/" ).arg( currentJob.version );
    path += idPath + verPath;
    switch( idx )
    {
    case IDX_CETK:
        path += "cetk";
        break;
    case IDX_TMD:
        path += QString( "tmd.%1" ).arg( currentJob.version );
        break;
    default:
        path += curTmd.Cid( idx );
        break;
    }
    return path;
}

//print info about a job
void NusDownloader::DbgJoB( NusJob job )
{
    QString dataStuff = QString( "%1 items:" ).arg( job.data.size() );
    for( int i = 0; i < job.data.size(); i++ )
        dataStuff += QString( " %1" ).arg( job.data.at( i ).size(), 0, 16, QChar( ' ' ) );

    qDebug() << QString( "NusJob( %1, %2, %3, %4 )" )
            .arg( job.tid, 16, 16, QChar( '0' ) )
            .arg( job.version ).arg( job.decrypt ? "decrypted" : "encrypted" )
            .arg( dataStuff );
}

//check a hash and add the data to the return item
bool NusDownloader::DecryptCheckHashAndAppendData( const QByteArray &encData, quint16 idx )
{
    //seems like a waste to keep setting the key, but for now im doing it this way
    //so multiple objects can be decrypting titles at the same time by different objects
    AesSetKey( decKey );

    //qDebug() << "NusDownloader::DecryptCheckHashAndAppendData" << hex << encData.size() << idx;
    QByteArray paddedEncrypted = PaddedByteArray( encData, 0x40 );
    QByteArray decData = AesDecrypt( idx, paddedEncrypted );
    decData.resize( curTmd.Size( idx ) );
    QByteArray realHash = GetSha1( decData );
    if( realHash != curTmd.Hash( idx ) )
    {
        qWarning() << "NusDownloader::DecryptCheckHashAndAppendData -> hash doesnt match for content" << hex << idx;
        //CurrentJobErrored( tr( "Downloaded data has a different hash than expected." ) );
        hexdump( realHash );
        hexdump( curTmd.Hash( idx ) );
        return false;
    }
    //add whatever data is requested to the return
    if( currentJob.decrypt )
        currentJob.data << decData;
    else
        currentJob.data << encData;

    return true;
}

//something is done downloading
void NusDownloader::FileIsFinishedDownloading( downloadJob job )
{
    //qDebug() << "NusDownloader::FileIsFinishedDownloading" << job.index;
    if( job.data.isEmpty() )
    {
        qWarning() << "NusDownloader::FileIsFinishedDownloading -> got empty data in return";
        CurrentJobErrored( tr( "Error downloading, returned empty data" ) );
        return;
    }

    //this is kinda ugly, but we need to get the path to save the data in the cache
    //and since we are using all these asyncronous signals and slots, we have to get cPath at different times for different situations
    QString cPath;
    switch( job.index )
    {
    case IDX_TMD:
        {
            ReadTmdAndGetTicket( job.data );
            cPath = GetCachePath( job.index );
        }
        break;
    case IDX_CETK:
        {
            Ticket t( job.data );
            decKey = t.DecryptedKey();
            //set this key to decrypt contents
            //AesSetKey( t.DecryptedKey() );
            //add the ticket data to the return
            currentJob.data << job.data;
            //start downloading the contents
            GetNextItemForCurrentTitle();

            cPath = GetCachePath( job.index );
        }
        break;
    default:
        {
            if( job.index > qFromBigEndian( curTmd.payload()->num_contents ) )
            {
                qWarning() << "NusDownloader::FileIsFinishedDownloading -> received data that doesnt fit anywhere";
                CurrentJobErrored( tr( "I have confused myself and cannot find where some downloaded data goes." ) );
                return;
            }
            if( job.index != currentJob.data.size() - 2 )
            {
                qWarning() << "NusDownloader::FileIsFinishedDownloading -> index doesnt match what it should";
                CurrentJobErrored( tr( "I have confused myself and cannot find where some downloaded data goes." ) );
                return;
            }
            if( !DecryptCheckHashAndAppendData( job.data, job.index ) )
            {
                CurrentJobErrored( tr( "Downloaded data has a different hash than expected." ) );
                return;
            }


            if( job.index == qFromBigEndian( curTmd.payload()->num_contents ) - 1 )//this is the last content for this title
            {
                int progress = (int)( ( (float)( totalJobs - jobList.size() ) / (float)totalJobs ) * 100.0f );
                emit SendTotalProgress( progress );
                emit SendTitleProgress( 100 );
                emit SendData( currentJob );
                QTimer::singleShot( 0, this, SLOT( StartNextJob() ) );//move on to next job
            }

            else
                QTimer::singleShot( 0, this, SLOT( GetNextItemForCurrentTitle() ) );//next content

            cPath = GetCachePath( job.index );
        }
        break;
    }

    //try to save this data to the cache
    if( cPath.isEmpty() )
        return;

    SaveDataToCache( cPath, job.data );
}

//get something from somewhere
void NusDownloader::StartDownload()
{
    //qDebug() << "NusDownloader::StartDownload" << dlJob.index;
    emit SendDownloadProgress( 0 );
    QString dlUrl = NUS_BASE_URL + dlJob.tid + "/" + dlJob.name;
    qDebug() << "url" << dlUrl;
    currentJobText = dlUrl;

    QUrl url( dlUrl );

    QNetworkRequest request( url );
    request.setRawHeader("User-Agent", UPDATING_USER_AGENT );

    currentDownload = manager.get( request );
    connect( currentDownload, SIGNAL( downloadProgress( qint64, qint64 ) ), this, SLOT( downloadProgress( qint64, qint64 ) ) );
    connect( currentDownload, SIGNAL( finished() ), this, SLOT( downloadFinished() ) );
    connect( currentDownload, SIGNAL( readyRead() ), this, SLOT( downloadReadyRead() ) );

    downloadTime.start();
}

//get a progress update from a download and turn it into some signals with text and numbers
void NusDownloader::downloadProgress( qint64 bytesReceived, qint64 bytesTotal )
{
    //qDebug() << "NusDownloader::downloadProgress" << bytesTotal;
    Q_UNUSED( bytesTotal );
    // calculate the download speed
    double speed = bytesReceived * 1000.0 / downloadTime.elapsed();
    QString unit;
    if( speed < 1024 )
    {
        unit = "bytes/sec";
    }
    else if( speed < 1024 * 1024 )
    {
        speed /= 1024;
        unit = "kB/s";
    }
    else
    {
        speed /= 1024*1024;
        unit = "MB/s";
    }
    emit SendText( currentJobText + "   " + QString::fromLatin1( "%1 %2" ).arg( speed, 3, 'f', 1 ).arg( unit ) );
    int progress = (int)( ( (float)bytesReceived / (float)bytesTotal ) * 100.0f );
    emit SendDownloadProgress( progress );
}

//file is done downloading
void NusDownloader::downloadFinished()
{
    //qDebug() << "NusDownloader::downloadFinished";
    if( currentDownload->error() )
    {
        qDebug() << "currentDownload->error()";
        CurrentJobErrored( tr( "Error downloading part of the title." ) );
    }
    else
    {
        emit SendDownloadProgress( 100 );
        FileIsFinishedDownloading( dlJob );

    }
    currentDownload->deleteLater();
}

//read from the file that is downloading
void NusDownloader::downloadReadyRead()
{
    //qDebug() << "NusDownloader::downloadReadyRead";
    dlJob.data += currentDownload->readAll();
}

bool NusDownloader::GetUpdate( const QString & upd, bool decrypt )
{
    QString s = upd.toLower();
    QMap< quint64, quint16 > titles;

    //hell, give everybody these.
    titles.insert( 0x1000248414741ull, 0x3 );//news channel HAGA
    titles.insert( 0x1000248414641ull, 0x3 );//Weather Channel HAFA

    if( s == "2.1e" ) titles = List21e();
    else if( s == "3.0e" ) titles = List30e();
    else if( s == "3.1e" ) titles = List31e();
    else if( s == "3.3e" ) titles = List33e();
    else if( s == "3.4e" ) titles = List34e();
    else if( s == "4.0e" ) titles = List40e();
    else if( s == "4.1e" ) titles = List41e();
    else if( s == "4.2e" ) titles = List42e();
    else if( s == "4.3e" ) titles = List43e();

    else if( s == "2.0u" ) titles = List20u();
    else if( s == "3.0u" ) titles = List30u();
    else if( s == "3.1u" ) titles = List31u();
    else if( s == "3.2u" ) titles = List32u();
    else if( s == "3.3u" ) titles = List33u();
    else if( s == "3.4u" ) titles = List34u();
    else if( s == "4.0u" ) titles = List40u();
    else if( s == "4.1u" ) titles = List41u();
    else if( s == "4.2u" ) titles = List42u();
    else if( s == "4.3u" ) titles = List43u();

    else if( s == "3.5k" ) titles = List35k();
    else if( s == "4.1k" ) titles = List41k();
    else if( s == "4.2k" ) titles = List42k();
    else if( s == "4.3k" ) titles = List43k();

    else if( s == "2.0j" ) titles = List20j();
    else if( s == "3.1j" ) titles = List31j();
    else if( s == "3.3j" ) titles = List33j();
    else if( s == "3.4j" ) titles = List34j();
    else if( s == "4.0j" ) titles = List40j();
    else if( s == "4.1j" ) titles = List41j();
    else if( s == "4.2j" ) titles = List42j();
    else if( s == "4.3j" ) titles = List43j();

    else return false;//unknown update

    QMap< quint64, quint16 >::ConstIterator i = titles.begin();
    while( i != titles.end() )
    {
        Get( i.key(), decrypt, i.value() );
        i++;
    }
    return true;
}
QMap< quint64, quint16 > NusDownloader::List20j()
{
    QMap< quint64, quint16 > titles;
    //titles.insert( 0x100000001ull, 2 );//boot2
    titles.insert( 0x100000002ull, 128 );//sys menu
    titles.insert( 0x10000000bull, 10 );//11v10
    titles.insert( 0x10000000cull, 6 );//12v6
    titles.insert( 0x10000000dull, 10 );//13v10
    titles.insert( 0x10000000full, 257 );//15v257
    titles.insert( 0x100000011ull, 512 );//17v512
    titles.insert( 0x100000023ull, 0xc10 ); 	// IOS35 - not really part of this update, but needed for sneek
    titles.insert( 0x100000100ull, 0x2 );//bcv2
    titles.insert( 0x100000101ull, 0x4 );//miosv4
    titles.insert( 0x1000848414B4aull, 0 );//EULA - HAKJ
    titles.insert( 0x1000848414C4aull, 0x2 );//regsel  //region select isnt in the paper mario update, but putting it here just to be safe
    titles.insert( 0x1000248414341ull, 0x2 );//nigaoeNRv2 - MII
    titles.insert( 0x1000248414141ull, 0x1 );//photov1
    titles.insert( 0x1000248414241ull, 0x4 );//shoppingv4
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List31j()
{
    QMap< quint64, quint16 > titles = List20j();//TODO - missing a few in here
    titles.insert( 0x100000002ull, 256 );//sys menu
    titles.insert( 0x100000014ull, 12 );//20v12
    titles.insert( 0x100000015ull, 514 );//21v514
    titles.insert( 0x100000016ull, 777 );//22v777 - should be v772
    titles.insert( 0x10000001cull, 1292 );//28v1292 - should be 1228
    titles.insert( 0x10000001eull, 1040 );//30v1040
    titles.insert( 0x10000001full, 1040 );//31v1040
    titles.insert( 0x100000021ull, 1040 );//33v1040
    titles.insert( 0x100000022ull, 1039 );//34v1039
    titles.insert( 0x100000023ull, 1040 );//35v1040
    titles.insert( 0x100000024ull, 1042 );//36v1042
    //titles.insert( 0x100000025ull, 2070 );//37v2070	//3.1u has this one but not 3.1j??
    titles.insert( 0x1000248415941ull, 0x1 );//photo2v1
    titles.insert( 0x1000848414B4aull, 0 );//EULA - HAKJ
    titles.insert( 0x100024841464aull, 0x7 );// forcast v7 HAFJ
    titles.insert( 0x100000101ull, 5 );//miosv5
    titles.insert( 0x1000848414C4aull, 0x2 );//regsel  //region select isnt in the paper mario update, but putting it here just to be safe
    titles.insert( 0x1000248414341ull, 0x2 );//nigaoeNRv2 - MII
    titles.insert( 0x1000248414141ull, 0x1 );//photov1
    titles.insert( 0x1000248414241ull, 7 );//shoppingv7
    titles.insert( 0x100024841474aull, 0x7 ); 		// news v7 HAGJ
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List33j()
{
    QMap< quint64, quint16 > titles = List31j();//TODO - missing 3.2j
    titles.insert( 0x100000002ull, 352 );//sys menu
    titles.insert( 0x10000000bull, 10 );//11v10
    titles.insert( 0x10000000cull, 6 );//12v6
    titles.insert( 0x10000000dull, 10 );//13v10
    titles.insert( 0x10000000full, 257 );//15v257
    titles.insert( 0x100000011ull, 512 );//17v512
    titles.insert( 0x10000001eull, 2576 );//30v2576
    titles.insert( 0x10000001full, 2576 );//31v2576
    titles.insert( 0x100000025ull, 2070 );//37v2070
    titles.insert( 0x100000100ull, 0x4 );//bcv4
    titles.insert( 0x1000248415941ull, 0x1 );//photo2v1
    titles.insert( 0x1000848414B4aull, 2 );//EULA - HAKJ
    titles.insert( 0x100000101ull, 8 );//miosv8
    titles.insert( 0x1000248414341ull, 5 );//nigaoeNRv5 - MII
    titles.insert( 0x1000248414241ull, 10 );//shoppingv10
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List34j()
{
    QMap< quint64, quint16 > titles;
    titles.insert( 0x100000004ull, 0xff00 ); 	// IOS4
    titles.insert( 0x100000009ull, 0x208 ); 	// IOS9
    titles.insert( 0x10000000aull, 0x300 ); 	// IOS10
    titles.insert( 0x10000000bull, 0x100 ); 	// IOS11
    titles.insert( 0x10000000cull, 0xb ); 		// IOS12
    titles.insert( 0x10000000dull, 0xf ); 		// IOS13
    titles.insert( 0x10000000eull, 0x106 ); 	// IOS14
    titles.insert( 0x10000000full, 0x109 ); 	// IOS15
    titles.insert( 0x100000011ull, 0x205 ); 	// IOS17
    titles.insert( 0x100000014ull, 0x100 ); 	// IOS20
    titles.insert( 0x100000015ull, 0x20a ); 	// IOS21
    titles.insert( 0x100000016ull, 0x309 ); 	// IOS22
    titles.insert( 0x10000001cull, 0x50c ); 	// IOS28
    titles.insert( 0x10000001eull, 0xb00 ); 	// IOS30
    titles.insert( 0x10000001full, 0xc10 ); 	// IOS31
    titles.insert( 0x100000021ull, 0xb10 ); 	// IOS33
    titles.insert( 0x100000022ull, 0xc0f ); 	// IOS34
    titles.insert( 0x100000023ull, 0xc10 ); 	// IOS35
    titles.insert( 0x100000024ull, 0xc12 ); 	// IOS36
    titles.insert( 0x100000025ull, 0xe19 ); 	// IOS37
    titles.insert( 0x100000032ull, 0x1319 ); 	// IOS50
    titles.insert( 0x100000033ull, 0x1219 ); 	// IOS51
    titles.insert( 0x1000000feull, 0x2 ); 		// IOS254
    titles.insert( 0x100000002ull, 0x180 );		// SystemMenu 3.4J
    titles.insert( 0x100000100ull, 0x5 ); 		// BC
    titles.insert( 0x100000101ull, 0x9 ); 		// MIOS
    titles.insert( 0x1000248414141ull, 0x2 ); 	// Channel HAAA
    titles.insert( 0x1000248414241ull, 0xd ); 	// Channel HABA
    titles.insert( 0x1000248414341ull, 0x6 ); 	// Channel HACA
    titles.insert( 0x100024841464aull, 0x7 ); 	// Channel HAFJ
    titles.insert( 0x100024841474aull, 0x7 ); 	// Channel HAGJ
    titles.insert( 0x1000248415941ull, 0x2 ); 	// Channel HAYA
    titles.insert( 0x1000848414b4aull, 0x2 ); 	// Channel HAKJ
    titles.insert( 0x1000848414c4aull, 0x2 ); 	// Channel HALJ
    titles.insert( 0x100084843434aull, 0x0); 	// Channel HCCJ
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List40j()
{
    QMap< quint64, quint16 > titles = List34j();
    titles.insert( 0x100000009ull, 0x209 ); 	// IOS9
    titles.insert( 0x10000000cull, 0xc ); 		// IOS12
    titles.insert( 0x10000000dull, 0x10 ); 		// IOS13
    titles.insert( 0x10000000eull, 0x107 ); 	// IOS14
    titles.insert( 0x10000000full, 0x10a ); 	// IOS15
    titles.insert( 0x100000010ull, 0x200 ); 	// IOS16
    titles.insert( 0x100000011ull, 0x206 ); 	// IOS17
    titles.insert( 0x100000015ull, 0x20d ); 	// IOS21
    titles.insert( 0x100000016ull, 0x30c ); 	// IOS22
    titles.insert( 0x10000001cull, 0x50d ); 	// IOS28
    titles.insert( 0x10000001full, 0xc14 ); 	// IOS31
    titles.insert( 0x100000021ull, 0xb12 ); 	// IOS33
    titles.insert( 0x100000022ull, 0xc13 ); 	// IOS34
    titles.insert( 0x100000023ull, 0xc14 ); 	// IOS35
    titles.insert( 0x100000024ull, 0xc16 ); 	// IOS36
    titles.insert( 0x100000025ull, 0xe1c ); 	// IOS37
    titles.insert( 0x100000026ull, 0xe1a ); 	// IOS38
    titles.insert( 0x100000032ull, 0x1400 ); 	// IOS50
    titles.insert( 0x100000033ull, 0x1300 ); 	// IOS51
    titles.insert( 0x100000035ull, 0x141d ); 	// IOS53
    titles.insert( 0x100000037ull, 0x141d ); 	// IOS55
    titles.insert( 0x10000003cull, 0x181e ); 	// IOS60
    titles.insert( 0x10000003dull, 0x131a ); 	// IOS61
    titles.insert( 0x1000000feull, 0x3 ); 		// IOS254
    titles.insert( 0x1000248414241ull, 0x10 ); 	// Channel HABA
    titles.insert( 0x1000248415941ull, 0x3 );	// Channel HAYA
    titles.insert( 0x100084843434aull, 0x1 );	// Channel HCCJ
    titles.insert( 0x100000002ull, 0x1a0 );		// SystemMenu 4.0J
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List41j()
{
    QMap< quint64, quint16 > titles = List40j();
    titles.insert( 0x100000002ull, 0x1c0 );		// SystemMenu 4.1E
    titles.insert( 0x100084843434aull, 0x2 );	// Channel HCCJ
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List42j()
{
    QMap< quint64, quint16 > titles = List41j();
    //titles.insert( 0x100000001ull, 0x4 );//make people really ask for the boot2 update if they want it
    titles.insert( 0x100000009ull, 0x30a ); 	// IOS9
    titles.insert( 0x10000000cull, 0x10d ); 	// IOS12
    titles.insert( 0x10000000dull, 0x111 ); 	// IOS13
    titles.insert( 0x10000000eull, 0x208 ); 	// IOS14
    titles.insert( 0x10000000full, 0x20b ); 	// IOS15
    titles.insert( 0x100000011ull, 0x307 ); 	// IOS17
    titles.insert( 0x100000015ull, 0x30e ); 	// IOS21
    titles.insert( 0x100000016ull, 0x40d ); 	// IOS22
    titles.insert( 0x10000001cull, 0x60e ); 	// IOS28
    titles.insert( 0x10000001full, 0xd15 ); 	// IOS31
    titles.insert( 0x100000021ull, 0xc13 ); 	// IOS33
    titles.insert( 0x100000022ull, 0xd14 ); 	// IOS34
    titles.insert( 0x100000023ull, 0xd15 ); 	// IOS35
    titles.insert( 0x100000024ull, 0xd17 ); 	// IOS36
    titles.insert( 0x100000025ull, 0xf1d ); 	// IOS37
    titles.insert( 0x100000026ull, 0xf1b ); 	// IOS38
    titles.insert( 0x100000035ull, 0x151e ); 	// IOS53
    titles.insert( 0x100000037ull, 0x151e ); 	// IOS55
    titles.insert( 0x100000038ull, 0x151d ); 	// IOS56
    titles.insert( 0x100000039ull, 0x161d ); 	// IOS57
    titles.insert( 0x10000003cull, 0x1900 ); 	// IOS60
    titles.insert( 0x10000003dull, 0x151d ); 	// IOS61
    titles.insert( 0x100000046ull, 0x1a1f ); 	// IOS70
    titles.insert( 0x1000000deull, 0xff00 ); 	// IOS222
    titles.insert( 0x1000000dfull, 0xff00 ); 	// IOS223
    titles.insert( 0x1000000f9ull, 0xff00 ); 	// IOS249
    titles.insert( 0x1000000faull, 0xff00 ); 	// IOS250
    titles.insert( 0x1000000feull, 0x104 ); 	// IOS254
    titles.insert( 0x100000100ull, 0x6 ); 		// BC
    titles.insert( 0x100000101ull, 0xa ); 		// MIOS
    titles.insert( 0x1000248414241ull, 0x11 );	// Channel HABA
    titles.insert( 0x1000248414241ull, 0x12 );	// ShopChannel
    titles.insert( 0x100000002ull, 0x1e0 );		// SystemMenu 4.2J
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List43j()
{
    QMap< quint64, quint16 > titles = List42j();
    titles.insert( 0x100000009ull, 0x40a ); 	// IOS9
    titles.insert( 0x10000000cull, 0x20e ); 	// IOS12
    titles.insert( 0x10000000dull, 0x408 ); 	// IOS13
    titles.insert( 0x10000000eull, 0x408 ); 	// IOS14
    titles.insert( 0x10000000full, 0x408 ); 	// IOS15
    titles.insert( 0x100000011ull, 0x408 ); 	// IOS17
    titles.insert( 0x100000015ull, 0x40f ); 	// IOS21
    titles.insert( 0x100000016ull, 0x50e ); 	// IOS22
    titles.insert( 0x10000001cull, 0x70f ); 	// IOS28
    titles.insert( 0x10000001full, 0xe18 ); 	// IOS31
    titles.insert( 0x100000021ull, 0xe18 ); 	// IOS33
    titles.insert( 0x100000022ull, 0xe18 ); 	// IOS34
    titles.insert( 0x100000023ull, 0xe18 ); 	// IOS35
    titles.insert( 0x100000024ull, 0xe18 ); 	// IOS36
    titles.insert( 0x100000025ull, 0x161f ); 	// IOS37
    titles.insert( 0x100000026ull, 0x101c ); 	// IOS38
    titles.insert( 0x100000028ull, 0xc00 ); 	// IOS40
    titles.insert( 0x100000029ull, 0xe17 ); 	// IOS41
    titles.insert( 0x10000002bull, 0xe17 ); 	// IOS43
    titles.insert( 0x10000002dull, 0xe17 ); 	// IOS45
    titles.insert( 0x10000002eull, 0xe17 ); 	// IOS46
    titles.insert( 0x100000030ull, 0x101c ); 	// IOS48
    titles.insert( 0x100000034ull, 0x1700 ); 	// IOS52
    titles.insert( 0x100000035ull, 0x161f ); 	// IOS53
    titles.insert( 0x100000037ull, 0x161f ); 	// IOS55
    titles.insert( 0x100000038ull, 0x161e ); 	// IOS56
    titles.insert( 0x100000039ull, 0x171f ); 	// IOS57
    titles.insert( 0x10000003aull, 0x1820 ); 	// IOS58
    titles.insert( 0x10000003dull, 0x161e ); 	// IOS61
    titles.insert( 0x100000046ull, 0x1b00 ); 	// IOS70
    titles.insert( 0x100000050ull, 0x1b20 ); 	// IOS80
    titles.insert( 0x1000000feull, 0xff00 ); 	// IOS254
    titles.insert( 0x1000848414b4aull, 0x3 );	// EULA
    titles.insert( 0x1000248414241ull, 0x14 );	// ShopChannel
    titles.insert( 0x100000002ull, 0x200 );		// SystemMenu 4.3J
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List21e()
{
    QMap< quint64, quint16 > titles;
    //( from metroid 3 )
    //titles.insert( 0x100000001ull, 2 );//boot2
    titles.insert( 0x100000002ull, 162 );//sys menu
    titles.insert( 0x10000000bull, 10 );//11v10
    titles.insert( 0x10000000cull, 6 );//12v6
    titles.insert( 0x10000000dull, 10 );//13v10
    titles.insert( 0x10000000full, 257 );//15v257
    titles.insert( 0x100000011ull, 512 );//17v512
    titles.insert( 0x100000014ull, 12 );//20v12
    titles.insert( 0x100000015ull, 514 );//21v514
    titles.insert( 0x100000016ull, 777 );//22v772   //should be getting v772 but it isnt available on NUS, get 777 instead
    titles.insert( 0x10000001cull, 1292 );//28v1228 //should be getting v1288 but it isnt on NUS
    titles.insert( 0x100000023ull, 0xc10 ); 	// IOS35 - not really part of this update, but needed for sneek
    titles.insert( 0x100000100ull, 0x2 );//bcv2
    titles.insert( 0x100000101ull, 0x4 );//miosv4
    titles.insert( 0x1000848414B50ull, 0 );//EULA - HAKP
    titles.insert( 0x1000848414C50ull, 0x2 );//regsel  //region select isnt in the paper mario update, but putting it here just to be safe
    titles.insert( 0x1000248414341ull, 0x2 );//nigaoeNRv2 - MII
    titles.insert( 0x1000248414141ull, 0x1 );//photov1
    titles.insert( 0x1000248414241ull, 0x4 );//shoppingv4
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List30e()
{
    QMap< quint64, quint16 > titles = List21e();
    //( from GH 3 )
    titles.insert( 0x100000002ull, 226 );//sys menu
    titles.insert( 0x100000100ull, 0x2 );//bcv2
    titles.insert( 0x10000001eull, 1039 );//30v1039
    titles.insert( 0x10000001full, 1039 );//31v1039
    titles.insert( 0x100000101ull, 5 );//miosv5
    titles.insert( 0x1000848414B50ull, 2 );//EULA - HAKP
    titles.insert( 0x1000248414650ull, 0x7 ); 		// forcast v7 HAFP
    titles.insert( 0x1000248414750ull, 0x7 ); 		// news v7 HAGP
    titles.insert( 0x1000848414C50ull, 0x2 );//regsel
    titles.insert( 0x1000248414341ull, 4 );//nigaoeNRv4 - MII
    titles.insert( 0x1000248414141ull, 0x1 );//photov1
    titles.insert( 0x1000248414241ull, 7 );//shoppingv7
    titles.insert( 0x1000248414741ull, 0x3 );//news channel HAGA
    titles.insert( 0x1000248414641ull, 0x3 );//Weather Channel HAFA
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List31e()
{
    QMap< quint64, quint16 > titles = List30e();
    //( from rayman raving rabbids tv party )
    //titles.insert( 0x10000000eull, 257 );//14v257 //dunno where this one came from?
    titles.insert( 0x10000001eull, 1040 );//30v1040
    titles.insert( 0x10000001full, 1040 );//31v1040
    titles.insert( 0x100000021ull, 1040 );//33v1040
    titles.insert( 0x100000022ull, 1039 );//34v1039
    titles.insert( 0x100000023ull, 1040 );//35v1040
    titles.insert( 0x100000024ull, 1042 );//36v1042
    titles.insert( 0x100000002ull, 258 );//sys menu
    titles.insert( 0x1000248415941ull, 0x2 ); 		// photo channel 1.1 HAYA
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List33e()
{
    QMap< quint64, quint16 > titles = List31e();
    titles.insert( 0x100000002ull, 354 );//RVL-WiiSystemmenu-v354.wad
    titles.insert( 0x10000000bull, 10 );//11v10
    titles.insert( 0x10000000cull, 6 );//12v6
    titles.insert( 0x10000000dull, 10 );//13v10
    titles.insert( 0x10000000eull, 262 );//14v262 - should actually be 14v257 but that version isnt available on NUS
    titles.insert( 0x10000000full, 257 );//15v257
    titles.insert( 0x100000011ull, 512 );//17v512
    titles.insert( 0x100000014ull, 12 );//20v12
    titles.insert( 0x100000015ull, 514 );//21v514
    titles.insert( 0x100000016ull, 777 );//22v777 - should be v772
    titles.insert( 0x10000001cull, 1292 );//28v1292 - should be 1228
    titles.insert( 0x10000001eull, 2576 );//30v2576
    titles.insert( 0x10000001full, 2576 );//31v2576
    titles.insert( 0x100000025ull, 2070 );//37v2070
    titles.insert( 0x100000100ull, 4 );//bcv4
    titles.insert( 0x100000101ull, 8 );//miosv8
    titles.insert( 0x1000248414341ull, 5 );//nigaoeNRv5 - MII
    titles.insert( 0x1000248414241ull, 10 );//shoppingv10
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List34e()
{
    QMap< quint64, quint16 > titles;
    titles.insert( 0x100000004ull, 0xff00 ); 	// IOS4
    titles.insert( 0x100000009ull, 0x208 ); 	// IOS9
    titles.insert( 0x10000000aull, 0x300 ); 	// IOS10
    titles.insert( 0x10000000bull, 0x100 ); 	// IOS11
    titles.insert( 0x10000000cull, 0xb ); 		// IOS12
    titles.insert( 0x10000000dull, 0xf ); 		// IOS13
    titles.insert( 0x10000000eull, 0x106 ); 	// IOS14
    titles.insert( 0x10000000full, 0x109 ); 	// IOS15
    titles.insert( 0x100000011ull, 0x205 ); 	// IOS17
    titles.insert( 0x100000014ull, 0x100 ); 	// IOS20
    titles.insert( 0x100000015ull, 0x20a ); 	// IOS21
    titles.insert( 0x100000016ull, 0x309 ); 	// IOS22
    titles.insert( 0x10000001cull, 0x50c ); 	// IOS28
    titles.insert( 0x10000001eull, 0xb00 ); 	// IOS30
    titles.insert( 0x10000001full, 0xc10 ); 	// IOS31
    titles.insert( 0x100000021ull, 0xb10 ); 	// IOS33
    titles.insert( 0x100000022ull, 0xc0f ); 	// IOS34
    titles.insert( 0x100000023ull, 0xc10 ); 	// IOS35
    titles.insert( 0x100000024ull, 0xc12 ); 	// IOS36
    titles.insert( 0x100000025ull, 0xe19 ); 	// IOS37
    titles.insert( 0x100000032ull, 0x1319 ); 	// IOS50
    titles.insert( 0x100000033ull, 0x1219 ); 	// IOS51
    titles.insert( 0x1000000feull, 0x2 ); 		// IOS254
    titles.insert( 0x100000002ull, 0x182 );		// SystemMenu 3.4E
    titles.insert( 0x100000100ull, 0x5 ); 		// BC
    titles.insert( 0x100000101ull, 0x9 ); 		// MIOS
    titles.insert( 0x1000248414141ull, 0x2 ); 		// Channel HAAA
    titles.insert( 0x1000248414241ull, 0xd ); 		// Channel HABA
    titles.insert( 0x1000248414341ull, 0x6 ); 		// Channel HACA
    titles.insert( 0x1000248414650ull, 0x7 ); 		// Channel HAFP
    titles.insert( 0x1000248414750ull, 0x7 ); 		// Channel HAGP
    titles.insert( 0x1000248415941ull, 0x2 ); 		// Channel HAYA
    titles.insert( 0x1000848414b50ull, 0x2 ); 		// Channel HAKP
    titles.insert( 0x1000848414c50ull, 0x2 ); 		// Channel HALP
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List40e()
{
    QMap< quint64, quint16 > titles = List34e();
    titles.insert( 0x100000009ull, 0x209 ); 	// IOS9
    titles.insert( 0x10000000cull, 0xc ); 		// IOS12
    titles.insert( 0x10000000dull, 0x10 ); 		// IOS13
    titles.insert( 0x10000000eull, 0x107 ); 	// IOS14
    titles.insert( 0x10000000full, 0x10a ); 	// IOS15
    titles.insert( 0x100000010ull, 0x200 ); 	// IOS16
    titles.insert( 0x100000011ull, 0x206 ); 	// IOS17
    titles.insert( 0x100000015ull, 0x20d ); 	// IOS21
    titles.insert( 0x100000016ull, 0x30c ); 	// IOS22
    titles.insert( 0x10000001cull, 0x50d ); 	// IOS28
    titles.insert( 0x10000001full, 0xc14 ); 	// IOS31
    titles.insert( 0x100000021ull, 0xb12 ); 	// IOS33
    titles.insert( 0x100000022ull, 0xc13 ); 	// IOS34
    titles.insert( 0x100000023ull, 0xc14 ); 	// IOS35
    titles.insert( 0x100000024ull, 0xc16 ); 	// IOS36
    titles.insert( 0x100000025ull, 0xe1c ); 	// IOS37
    titles.insert( 0x100000026ull, 0xe1a ); 	// IOS38
    titles.insert( 0x100000032ull, 0x1400 ); 	// IOS50
    titles.insert( 0x100000033ull, 0x1300 ); 	// IOS51
    titles.insert( 0x100000035ull, 0x141d ); 	// IOS53
    titles.insert( 0x100000037ull, 0x141d ); 	// IOS55
    titles.insert( 0x10000003cull, 0x181e ); 	// IOS60
    titles.insert( 0x10000003dull, 0x131a ); 	// IOS61
    titles.insert( 0x1000000feull, 0x3 ); 		// IOS254
    titles.insert( 0x1000248414241ull, 0x10 ); 	// Channel HABA
    titles.insert( 0x1000248415941ull, 0x3 );	// Channel HAYA
    titles.insert( 0x100000002ull, 0x1a2 );		// SystemMenu 4.0E
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List41e()
{
    QMap< quint64, quint16 > titles = List40e();
    titles.insert( 0x100000002ull, 0x1c2 );		// SystemMenu 4.1E
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List42e()
{
    QMap< quint64, quint16 > titles = List41e();
    //titles.insert( 0x100000001ull, 0x4 );//make people really ask for the boot2 update if they want it
    titles.insert( 0x100000009ull, 0x30a ); 	// IOS9
    titles.insert( 0x10000000cull, 0x10d ); 	// IOS12
    titles.insert( 0x10000000dull, 0x111 ); 	// IOS13
    titles.insert( 0x10000000eull, 0x208 ); 	// IOS14
    titles.insert( 0x10000000full, 0x20b ); 	// IOS15
    titles.insert( 0x100000011ull, 0x307 ); 	// IOS17
    titles.insert( 0x100000015ull, 0x30e ); 	// IOS21
    titles.insert( 0x100000016ull, 0x40d ); 	// IOS22
    titles.insert( 0x10000001cull, 0x60e ); 	// IOS28
    titles.insert( 0x10000001full, 0xd15 ); 	// IOS31
    titles.insert( 0x100000021ull, 0xc13 ); 	// IOS33
    titles.insert( 0x100000022ull, 0xd14 ); 	// IOS34
    titles.insert( 0x100000023ull, 0xd15 ); 	// IOS35
    titles.insert( 0x100000024ull, 0xd17 ); 	// IOS36
    titles.insert( 0x100000025ull, 0xf1d ); 	// IOS37
    titles.insert( 0x100000026ull, 0xf1b ); 	// IOS38
    titles.insert( 0x100000035ull, 0x151e ); 	// IOS53
    titles.insert( 0x100000037ull, 0x151e ); 	// IOS55
    titles.insert( 0x100000038ull, 0x151d ); 	// IOS56
    titles.insert( 0x100000039ull, 0x161d ); 	// IOS57
    titles.insert( 0x10000003cull, 0x1900 ); 	// IOS60
    titles.insert( 0x10000003dull, 0x151d ); 	// IOS61
    titles.insert( 0x100000046ull, 0x1a1f ); 	// IOS70
    titles.insert( 0x1000000deull, 0xff00 ); 	// IOS222
    titles.insert( 0x1000000dfull, 0xff00 ); 	// IOS223
    titles.insert( 0x1000000f9ull, 0xff00 ); 	// IOS249
    titles.insert( 0x1000000faull, 0xff00 ); 	// IOS250
    titles.insert( 0x1000000feull, 0x104 ); 	// IOS254
    titles.insert( 0x100000100ull, 0x6 ); 		// BC
    titles.insert( 0x100000101ull, 0xa ); 		// MIOS
    titles.insert( 0x1000248414241ull, 0x11 );	// Channel HABA
    titles.insert( 0x1000248414241ull, 0x12 );	// ShopChannel
    titles.insert( 0x100000002ull, 0x1e2 );		// SystemMenu 4.2E
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List43e()
{
    QMap< quint64, quint16 > titles = List42e();
    titles.insert( 0x100000009ull, 0x40a ); 	// IOS9
    titles.insert( 0x10000000cull, 0x20e ); 	// IOS12
    titles.insert( 0x10000000dull, 0x408 ); 	// IOS13
    titles.insert( 0x10000000eull, 0x408 ); 	// IOS14
    titles.insert( 0x10000000full, 0x408 ); 	// IOS15
    titles.insert( 0x100000011ull, 0x408 ); 	// IOS17
    titles.insert( 0x100000015ull, 0x40f ); 	// IOS21
    titles.insert( 0x100000016ull, 0x50e ); 	// IOS22
    titles.insert( 0x10000001cull, 0x70f ); 	// IOS28
    titles.insert( 0x10000001full, 0xe18 ); 	// IOS31
    titles.insert( 0x100000021ull, 0xe18 ); 	// IOS33
    titles.insert( 0x100000022ull, 0xe18 ); 	// IOS34
    titles.insert( 0x100000023ull, 0xe18 ); 	// IOS35
    titles.insert( 0x100000024ull, 0xe18 ); 	// IOS36
    titles.insert( 0x100000025ull, 0x161f ); 	// IOS37
    titles.insert( 0x100000026ull, 0x101c ); 	// IOS38
    titles.insert( 0x100000028ull, 0xc00 ); 	// IOS40
    titles.insert( 0x100000029ull, 0xe17 ); 	// IOS41
    titles.insert( 0x10000002bull, 0xe17 ); 	// IOS43
    titles.insert( 0x10000002dull, 0xe17 ); 	// IOS45
    titles.insert( 0x10000002eull, 0xe17 ); 	// IOS46
    titles.insert( 0x100000030ull, 0x101c ); 	// IOS48
    titles.insert( 0x100000034ull, 0x1700 ); 	// IOS52
    titles.insert( 0x100000035ull, 0x161f ); 	// IOS53
    titles.insert( 0x100000037ull, 0x161f ); 	// IOS55
    titles.insert( 0x100000038ull, 0x161e ); 	// IOS56
    titles.insert( 0x100000039ull, 0x171f ); 	// IOS57
    titles.insert( 0x10000003aull, 0x1820 ); 	// IOS58
    titles.insert( 0x10000003dull, 0x161e ); 	// IOS61
    titles.insert( 0x100000046ull, 0x1b00 ); 	// IOS70
    titles.insert( 0x100000050ull, 0x1b20 ); 	// IOS80
    titles.insert( 0x1000000feull, 0xff00 ); 	// IOS254
    titles.insert( 0x1000848414b50ull, 0x3 );	// EULA
    titles.insert( 0x1000248414241ull, 0x14 );	// ShopChannel
    titles.insert( 0x100000002ull, 0x202 );		// SystemMenu 4.3E
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List20u()
{
    QMap< quint64, quint16 > titles;
    //( from paper mario )
    //titles.insert( 0x100000001ull, 2 );//boot2
    titles.insert( 0x100000002ull, 97 );//sys menu
    titles.insert( 0x10000000bull, 10 );//11v10
    titles.insert( 0x10000000cull, 6 );//12v6
    titles.insert( 0x10000000dull, 10 );//13v10
    titles.insert( 0x10000000full, 257 );//15v257
    titles.insert( 0x100000011ull, 512 );//17v512
    titles.insert( 0x100000023ull, 0xc10 ); 	// IOS35 - not really part of this update, but needed for sneek
    titles.insert( 0x100000100ull, 0x2 );//bcv2
    titles.insert( 0x100000101ull, 0x4 );//miosv4
    titles.insert( 0x1000848414B45ull, 0 );//EULA - HAKE
    titles.insert( 0x1000848414C45ull, 0x2 );//regsel  //region select isnt in the paper mario update, but putting it here just to be safe
    titles.insert( 0x1000248414341ull, 0x2 );//nigaoeNRv2 - MII
    titles.insert( 0x1000248414141ull, 0x1 );//photov1
    titles.insert( 0x1000248414241ull, 0x4 );//shoppingv4
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List30u()
{
    QMap< quint64, quint16 > titles = List20u();
    //( from GH3 )
    //titles.insert( 0x100000001ull, 2 );//boot2
    titles.insert( 0x100000002ull, 225 );//sys menu
    titles.insert( 0x100000014ull, 12 );//20v12
    titles.insert( 0x100000015ull, 514 );//21v514
    titles.insert( 0x10000001eull, 1039 );//30v1039
    titles.insert( 0x10000001full, 1039 );//31v1039
    titles.insert( 0x100000100ull, 0x2 );//bcv2
    titles.insert( 0x1000848414B45ull, 0x2 );//EULA - HAKE
    titles.insert( 0x1000248414645ull, 0x7 );//forecast
    titles.insert( 0x1000248414745ull, 0x7 );//news_USv7
    titles.insert( 0x1000248414341ull, 0x4 );//nigaoeNRv4 - MII
    titles.insert( 0x1000248414241ull, 0x7 );//shoppingv7
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List31u()
{
    QMap< quint64, quint16 > titles = List30u();
    //( from rockband2 )
    titles.insert( 0x100000002ull, 257 );//sys menu
    titles.insert( 0x10000000eull, 262 );//14v262 - should actually be 14v257 but that version isnt available on NUS
    titles.insert( 0x10000001eull, 1040 );//30v1040
    titles.insert( 0x10000001full, 1040 );//31v1040
    titles.insert( 0x100000021ull, 1040 );//33v1040
    titles.insert( 0x100000022ull, 1039 );//34v1040
    titles.insert( 0x100000023ull, 1040 );//35v1040
    titles.insert( 0x100000024ull, 1042 );//36v1040
    titles.insert( 0x100000025ull, 2070 );//37v2070
    titles.insert( 0x1000248415941ull, 0x1 );//photo2v1 ?? FIXME: this disc didnt have the photochannelv1 on it, but wiimpersonator logs say it is present is 3.2u
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List32u()
{
    QMap< quint64, quint16 > titles = List31u();
    titles.insert( 0x100000002ull, 0x121 );
    titles.insert( 0x10000000bull, 0xa );
    titles.insert( 0x10000000cull, 0x6 );
    titles.insert( 0x10000000dull, 0xa );
    titles.insert( 0x10000000full, 0x104 );
    titles.insert( 0x100000011ull, 0x200 );
    titles.insert( 0x100000014ull, 0xc );
    titles.insert( 0x100000015ull, 0x205 );
    titles.insert( 0x10000001eull, 0xa10 );
    titles.insert( 0x10000001full, 0x410 );
    titles.insert( 0x100000021ull, 0x410 );
    titles.insert( 0x100000022ull, 0x40f );
    titles.insert( 0x100000023ull, 0x410 );
    titles.insert( 0x100000025ull, 0x816 );
    titles.insert( 0x100000100ull, 0x2 );
    titles.insert( 0x100000101ull, 0x5 );
    titles.insert( 0x1000248414141ull, 0x1 );
    titles.insert( 0x1000248414241ull, 0x8 );
    titles.insert( 0x1000248414341ull, 0x4 );
    titles.insert( 0x1000248414645ull, 0x7 );
    titles.insert( 0x1000248414745ull, 0x7 );
    titles.insert( 0x1000248415941ull, 0x1 );
    titles.insert( 0x1000848414B45ull, 0x2 );
    titles.insert( 0x1000848414C45ull, 0x2 );
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List33u()
{
    QMap< quint64, quint16 > titles = List32u();
    titles.insert( 0x100000002ull, 0x161 );
    titles.insert( 0x100000004ull, 0xff00 );
    titles.insert( 0x100000009ull, 0x208 );
    titles.insert( 0x10000000aull, 0x300 );
    titles.insert( 0x10000000bull, 0x100 );
    titles.insert( 0x10000000cull, 0xb );
    titles.insert( 0x10000000dull, 0xf );
    titles.insert( 0x10000000eull, 0x106 );
    titles.insert( 0x10000000full, 0x109 );
    titles.insert( 0x100000011ull, 0x205 );
    titles.insert( 0x100000014ull, 0x100 );
    titles.insert( 0x100000015ull, 0x20a );
    titles.insert( 0x100000016ull, 0x309 );
    titles.insert( 0x10000001cull, 0x50c );
    titles.insert( 0x10000001full, 0xa10 );
    titles.insert( 0x100000021ull, 0xb10 );
    titles.insert( 0x100000022ull, 0xc0f );
    titles.insert( 0x100000023ull, 0xc10 );
    titles.insert( 0x100000024ull, 0xc12 );
    titles.insert( 0x100000033ull, 0x1219 );
    titles.insert( 0x100000100ull, 0x4 );
    titles.insert( 0x100000101ull, 0x8 );
    titles.insert( 0x1000248414341ull, 0x5 );
    titles.insert( 0x1000248414141ull, 0x2 );
    titles.insert( 0x1000248414241ull, 0xd );
    titles.insert( 0x1000248415941ull, 0x2 );
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List34u()
{
    QMap< quint64, quint16 > titles = List33u();
    titles.insert( 0x100000032ull, 0x1319 );
    titles.insert( 0x1000000feull, 0x2 );
    titles.insert( 0x100000002ull, 0x181 );
    titles.insert( 0x10000001eull, 0xb00 );
    titles.insert( 0x10000001full, 0xc10 );
    titles.insert( 0x100000025ull, 0xe19 );
    titles.insert( 0x100000100ull, 0x5 );
    titles.insert( 0x100000101ull, 0x9 );
    titles.insert( 0x1000248414341ull, 0x6 );
    titles.insert( 0x100000010ull, 0x200 );
    titles.insert( 0x100000026ull, 0xe1a );
    titles.insert( 0x100000035ull, 0x141d );
    titles.insert( 0x100000037ull, 0x141d );
    titles.insert( 0x10000003cull, 0x181e );
    titles.insert( 0x10000003dull, 0x131a );
    titles.insert( 0x100000009ull, 0x209 );
    titles.insert( 0x10000000cull, 0xc );
    titles.insert( 0x10000000dull, 0x10 );
    titles.insert( 0x10000000eull, 0x107 );
    titles.insert( 0x10000000full, 0x10a );
    titles.insert( 0x100000011ull, 0x206 );
    titles.insert( 0x100000015ull, 0x20d );
    titles.insert( 0x100000016ull, 0x309 );
    titles.insert( 0x10000001cull, 0x50d );
    titles.insert( 0x10000001full, 0xc14 );
    titles.insert( 0x100000021ull, 0xb12 );
    titles.insert( 0x100000022ull, 0xc0f );
    titles.insert( 0x100000032ull, 0x1319 );
    titles.insert( 0x100000024ull, 0xc16 );
    titles.insert( 0x100000025ull, 0xe1c );
    titles.insert( 0x1000000feull, 0x3 );
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List40u()
{
    QMap< quint64, quint16 > titles = List34u();
    titles.insert( 0x100000002ull, 0x1a1 );
    titles.insert( 0x100000032ull, 0x1400 );
    titles.insert( 0x100000033ull, 0x1300 );
    titles.insert( 0x1000248414241ull, 0x10 );
    titles.insert( 0x1000248415941ull, 0x3 );
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List41u()
{
    QMap< quint64, quint16 > titles = List40u();
    titles.insert( 0x100000002ull, 0x1c1 );
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List42u()
{
    QMap< quint64, quint16 > titles = List41u();
    //titles.insert( 0x100000001ull, 0x4 );//make people really ask for the boot2 update if they want it
    titles.insert( 0x100000038ull, 0x151d );
    titles.insert( 0x100000039ull, 0x161d );
    titles.insert( 0x100000046ull, 0x1a1f );
    titles.insert( 0x1000000deull, 0xff00 );
    titles.insert( 0x1000000dfull, 0xff00 );
    titles.insert( 0x1000000f9ull, 0xff00 );
    titles.insert( 0x1000000faull, 0xff00 );
    titles.insert( 0x100000002ull, 0x1e1 );
    titles.insert( 0x100000009ull, 0x30a );
    titles.insert( 0x10000000cull, 0x10d );
    titles.insert( 0x10000000dull, 0x111 );
    titles.insert( 0x10000000eull, 0x208 );
    titles.insert( 0x10000000full, 0x20b );
    titles.insert( 0x100000011ull, 0x307 );
    titles.insert( 0x100000015ull, 0x30e );
    titles.insert( 0x100000016ull, 0x40d );
    titles.insert( 0x10000001cull, 0x60e );
    titles.insert( 0x10000001full, 0xd15 );
    titles.insert( 0x100000021ull, 0xc13 );
    titles.insert( 0x100000022ull, 0xd14 );
    titles.insert( 0x100000023ull, 0xd15 );
    titles.insert( 0x100000024ull, 0xd17 );
    titles.insert( 0x100000025ull, 0xf1d );
    titles.insert( 0x100000026ull, 0xf1b );
    titles.insert( 0x100000035ull, 0x151e );
    titles.insert( 0x100000037ull, 0x151e );
    titles.insert( 0x10000003cull, 0x1900 );
    titles.insert( 0x10000003dull, 0x151d );
    titles.insert( 0x1000000feull, 0x104 );
    titles.insert( 0x100000100ull, 0x6 );
    titles.insert( 0x100000101ull, 0xa );
    titles.insert( 0x1000248414241ull, 0x12 );
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List43u()
{
    QMap< quint64, quint16 > titles = List42u();
    titles.insert( 0x100000009ull, 0x40a ); 	// IOS9
    titles.insert( 0x10000000cull, 0x20e ); 	// IOS12
    titles.insert( 0x10000000dull, 0x408 );		// IOS13
    titles.insert( 0x10000000eull, 0x408 );		// IOS14
    titles.insert( 0x10000000full, 0x408 );		// IOS15
    titles.insert( 0x100000011ull, 0x408 );		// IOS17
    titles.insert( 0x100000015ull, 0x40f );		// IOS21
    titles.insert( 0x100000016ull, 0x50e );		// IOS22
    titles.insert( 0x10000001cull, 0x70f );		// IOS28
    titles.insert( 0x10000001full, 0xe18 );		// IOS31
    titles.insert( 0x100000021ull, 0xe18 );		// IOS33
    titles.insert( 0x100000021ull, 0xe18 );		// IOS34
    titles.insert( 0x100000023ull, 0xe18 );		// IOS35
    titles.insert( 0x100000024ull, 0xe18 );		// IOS36
    titles.insert( 0x100000025ull, 0x161f ); 	// IOS37
    titles.insert( 0x100000026ull, 0x101c );	// IOS38
    titles.insert( 0x100000028ull, 0xc00 );		// IOS40
    titles.insert( 0x100000029ull, 0xe17 );		// IOS41
    titles.insert( 0x10000002bull, 0xe17 );		// IOS43
    titles.insert( 0x10000002dull, 0xe17 );		// IOS45
    titles.insert( 0x10000002eull, 0xe17 );		// IOS46
    titles.insert( 0x100000030ull, 0x101c );	// IOS48
    titles.insert( 0x100000034ull, 0x1700 ); 	// IOS52
    titles.insert( 0x100000035ull, 0x161f ); 	// IOS53
    titles.insert( 0x100000037ull, 0x161f ); 	// IOS55
    titles.insert( 0x100000038ull, 0x161e ); 	// IOS56
    titles.insert( 0x100000039ull, 0x171f ); 	// IOS57
    titles.insert( 0x10000003aull, 0x1820 ); 	// IOS58
    titles.insert( 0x10000003dull, 0x161e ); 	// IOS61
    titles.insert( 0x100000046ull, 0x1b00 );	// IOS70
    titles.insert( 0x100000050ull, 0x1b20 ); 	// IOS80
    titles.insert( 0x1000000feull, 0xff00 ); 	// IOS254
    titles.insert( 0x100000002ull, 0x201 );		// SystemMenu 4.3U
    titles.insert( 0x1000248414241ull, 0x14 );	// ShopChannel
    titles.insert( 0x1000848414b45ull, 0x3 );	// EULA
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List35k()
{
    QMap< quint64, quint16 > titles;
    titles.insert( 0x100000004ull, 0xff00 ); 	// IOS4
    titles.insert( 0x100000009ull, 0x209 ); 	// IOS9
    titles.insert( 0x100000015ull, 0x20d );		// IOS21
    titles.insert( 0x100000025ull, 0xe1c ); 	// IOS37
    titles.insert( 0x100000028ull, 0xc00 ); 	// IOS40
    titles.insert( 0x100000029ull, 0xb13 ); 	// IOS41
    titles.insert( 0x10000002bull, 0xb13 ); 	// IOS43
    titles.insert( 0x10000002dull, 0xb13 ); 	// IOS45
    titles.insert( 0x10000002eull, 0xb15 ); 	// IOS46
    titles.insert( 0x100000033ull, 0x1300 ); 	// IOS51
    titles.insert( 0x100000034ull, 0x161d ); 	// IOS52
    titles.insert( 0x100000035ull, 0x141d ); 	// IOS53
    titles.insert( 0x100000037ull, 0x141d ); 	// IOS55
    titles.insert( 0x10000003dull, 0x131a ); 	// IOS61
    titles.insert( 0x1000000feull, 0x3 ); 		// IOS254
    titles.insert( 0x100000100ull, 0x5 ); 		// BC
    titles.insert( 0x100000101ull, 0x9 ); 		// MIOS
    titles.insert( 0x100024841424bull, 0x10 ); 	// Channel HABK
    titles.insert( 0x1000848414c4bull, 0x2 ); 	// Channel HALK
    titles.insert( 0x100000002ull, 0x186 );		// SystemMenu 3.5K
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List41k()
{
    QMap< quint64, quint16 > titles = List35k();
    titles.insert( 0x100000010ull, 0x200 ); 	// IOS16
    titles.insert( 0x100000029ull, 0xc13 ); 	// IOS41
    titles.insert( 0x10000002bull, 0xc13 ); 	// IOS43
    titles.insert( 0x10000002dull, 0xc13 ); 	// IOS45
    titles.insert( 0x10000002eull, 0xc15 ); 	// IOS46
    titles.insert( 0x100000034ull, 0x1700 ); 	// IOS52
    titles.insert( 0x10000003cull, 0x181e ); 	// IOS60
    titles.insert( 0x100024841594bull, 0x3 ); 	// Channel HAYK
    titles.insert( 0x100000002ull, 0x1c6 );		// SystemMenu 4.1K
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List42k()
{
    QMap< quint64, quint16 > titles = List41k();
    //titles.insert( 0x100000001ull, 0x4 );//make people really ask for the boot2 update if they want it
    titles.insert( 0x100000009ull, 0x30a ); 	// IOS9
    titles.insert( 0x100000015ull, 0x30e ); 	// IOS21
    titles.insert( 0x100000024ull, 0xd17 ); 	// IOS36
    titles.insert( 0x100000025ull, 0xf1d ); 	// IOS37
    titles.insert( 0x100000029ull, 0xd14 ); 	// IOS41
    titles.insert( 0x10000002bull, 0xd14 ); 	// IOS43
    titles.insert( 0x10000002dull, 0xd14 ); 	// IOS45
    titles.insert( 0x10000002eull, 0xd16 ); 	// IOS46
    titles.insert( 0x100000035ull, 0x151e ); 	// IOS53
    titles.insert( 0x100000037ull, 0x151e ); 	// IOS55
    titles.insert( 0x100000038ull, 0x151d ); 	// IOS56
    titles.insert( 0x100000039ull, 0x161d ); 	// IOS57
    titles.insert( 0x10000003cull, 0x1900 ); 	// IOS60
    titles.insert( 0x10000003dull, 0x151d ); 	// IOS61
    titles.insert( 0x100000046ull, 0x1a1f ); 	// IOS70
    titles.insert( 0x1000000deull, 0xff00 ); 	// IOS222
    titles.insert( 0x1000000dfull, 0xff00 ); 	// IOS223
    titles.insert( 0x1000000f9ull, 0xff00 ); 	// IOS249
    titles.insert( 0x1000000faull, 0xff00 ); 	// IOS250
    titles.insert( 0x1000000feull, 0x104 ); 	// IOS254
    titles.insert( 0x100000100ull, 0x6 ); 		// BC
    titles.insert( 0x100000101ull, 0xa ); 		// MIOS
    titles.insert( 0x100024841424bull, 0x11 );	// Channel HABK
    titles.insert( 0x1000248414241ull, 0x12 );	// ShopChannel
    titles.insert( 0x100000002ull, 0x1e6 );		// SystemMenu 4.2K
    return titles;
}

QMap< quint64, quint16 > NusDownloader::List43k()
{
    QMap< quint64, quint16 > titles = List42k();
    titles.insert( 0x100000009ull, 0x40a ); 	// IOS9
    titles.insert( 0x10000000aull, 0x300 ); 	// IOS10
    titles.insert( 0x10000000bull, 0x100 ); 	// IOS11
    titles.insert( 0x10000000cull, 0x20e ); 	// IOS12
    titles.insert( 0x10000000dull, 0x408 ); 	// IOS13
    titles.insert( 0x10000000eull, 0x408 ); 	// IOS14
    titles.insert( 0x10000000full, 0x408 ); 	// IOS15
    titles.insert( 0x100000011ull, 0x408 ); 	// IOS17
    titles.insert( 0x100000014ull, 0x100 ); 	// IOS20
    titles.insert( 0x100000015ull, 0x40f ); 	// IOS21
    titles.insert( 0x100000016ull, 0x50e ); 	// IOS22
    titles.insert( 0x10000001cull, 0x70f ); 	// IOS28
    titles.insert( 0x10000001eull, 0xb00 ); 	// IOS30
    titles.insert( 0x10000001full, 0xe18 ); 	// IOS31
    titles.insert( 0x100000021ull, 0xe18 ); 	// IOS33
    titles.insert( 0x100000022ull, 0xe18 ); 	// IOS34
    titles.insert( 0x100000023ull, 0xe18 ); 	// IOS35
    titles.insert( 0x100000024ull, 0xe18 ); 	// IOS36
    titles.insert( 0x100000025ull, 0x161f ); 	// IOS37
    titles.insert( 0x100000026ull, 0x101c ); 	// IOS38
    titles.insert( 0x100000029ull, 0xe17 ); 	// IOS41
    titles.insert( 0x10000002bull, 0xe17 ); 	// IOS43
    titles.insert( 0x10000002dull, 0xe17 ); 	// IOS45
    titles.insert( 0x10000002eull, 0xe17 ); 	// IOS46
    titles.insert( 0x100000030ull, 0x101c ); 	// IOS48
    titles.insert( 0x100000032ull, 0x1400 ); 	// IOS50
    titles.insert( 0x100000035ull, 0x161f ); 	// IOS53
    titles.insert( 0x100000037ull, 0x161f ); 	// IOS55
    titles.insert( 0x100000038ull, 0x161e ); 	// IOS56
    titles.insert( 0x100000039ull, 0x171f ); 	// IOS57
    titles.insert( 0x10000003aull, 0x1820 ); 	// IOS58
    titles.insert( 0x10000003dull, 0x161e ); 	// IOS61
    titles.insert( 0x100000046ull, 0x1b00 ); 	// IOS70
    titles.insert( 0x100000050ull, 0x1b20 ); 	// IOS80
    titles.insert( 0x1000000feull, 0xff00 ); 	// IOS254
    titles.insert( 0x1000848414b4bull, 0x3 );	// EULA
    titles.insert( 0x1000248414241ull, 0x14 );	// ShopChannel
    titles.insert( 0x100000002ull, 0x206 );		// SystemMenu 4.3K
    return titles;
}
