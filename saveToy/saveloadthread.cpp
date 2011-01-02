#include "saveloadthread.h"
#include "quazip.h"
#include "quazipfile.h"
#include "../WiiQt/savedatabin.h"

Q_DECLARE_METATYPE( PcSaveInfo )
SaveLoadThread::SaveLoadThread( QObject *parent ) : QThread( parent )
{
    abort = false;
	qRegisterMetaType< PcSaveInfo >();
}

void SaveLoadThread::ForceQuit()
{
    mutex.lock();
    abort = true;
    mutex.unlock();
}

SaveLoadThread::~SaveLoadThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    wait();
}

void SaveLoadThread::GetBanners( const QString &bPath )
{
    if( isRunning() )
    {
        qWarning() << "SaveLoadThread::GetBanners -> already running";
        return;
    }
    basePath = bPath;
    if( basePath.isEmpty() )
        type = LOAD_SNEEK;
    else
        type = LOAD_PC;
    start( NormalPriority );
}

bool SaveLoadThread::SetNandPath( const QString &bPath )
{
    if( isRunning() )
        return false;

    if( nand.BasePath() == bPath )
        return true;

    return nand.SetPath( bPath );
}

SaveGame SaveLoadThread::GetSave( quint64 tid )
{
    SaveGame ret;
    ret.tid = 0;

    if( isRunning() )
        return ret;

    return nand.GetSaveData( tid );
}


void SaveLoadThread::run()
{
    if ( abort )
    {
        qDebug( "SaveLoadThread::run -> Thread abort" );
        return;
    }
    mutex.lock();
    /*if( basePath.isEmpty() )
    {
        qDebug() << "SaveLoadThread::run -> its empty";
        return;
    }*/
    mutex.unlock();

    switch( type )
    {
    case LOAD_SNEEK:
        GetSavesFromNandDump();
        break;
    case LOAD_PC:
        if( basePath.isEmpty() )
            return;
		GetPCSaves();
        break;
    default:
        break;

    }

    emit SendDone( type );
}

int SaveLoadThread::GetFolderSize( const QString& path )
{
    //qDebug() << "SaveLoadThread::GetFolderSize" << path;
    quint32 ret = 0;
    QDir dir( path );
    dir.setFilter( QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot );
    QFileInfoList fiL = dir.entryInfoList();
    foreach( QFileInfo fi, fiL )
    {
        if( fi.isDir() )
            ret += GetFolderSize( fi.absoluteFilePath() );

        else
            ret += fi.size();
    }
    return ret;
}

void SaveLoadThread::GetSavesFromPC()
{
    emit SendProgress( 0 );

    emit SendProgress( 100 );
}

void SaveLoadThread::GetPCSaves()
{
	emit SendProgress( 0 );
    if( basePath.isEmpty() )
		return;
	int total = 0;
	int done = 0;
	//first count all the saves
    //list all subdirs of the base ( TID upper )
    QDir base( basePath );
    QFileInfoList fi1 = base.entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot );
	foreach( QFileInfo upper, fi1 )
	{
		//qDebug() << "looking in" << upper.absoluteFilePath();
		//get all subdirectories of the subfolder ( TID lower )
		QDir uDir( upper.absoluteFilePath() );
		QFileInfoList fi2 = uDir.entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot );
		foreach( QFileInfo lower, fi2 )
		{
			//qDebug() << "looking in" << lower.absoluteFilePath();
			//list all .zip files in the game's folder
			QDir lDir( lower.absoluteFilePath() );
			QFileInfoList fi3 = lDir.entryInfoList( QStringList() << "*.zip", QDir::Files );
			//qDebug() << "found" << fi3.size() << "zip files in here";
			total += fi3.size();
		}
	}

	//qDebug() << "SaveLoadThread::GetPCSaves(): will open" << total << "zip archives";
	//now actually read/send the saves
	foreach( QFileInfo upper, fi1 )
	{
		//get all subdirectories of the subfolder ( TID lower )
		QDir uDir( upper.absoluteFilePath() );
		QFileInfoList fi2 = uDir.entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot );
		foreach( QFileInfo lower, fi2 )
		{
			//list all .zip files in the game's folder
			QDir lDir( lower.absoluteFilePath() );
			QFileInfoList fi3 = lDir.entryInfoList( QStringList() << "*.zip", QDir::Files );
			quint32 cnt = fi3.size();
			//QStringList descriptions;
			//QByteArray banner;
			//quint32 size = 0;
			PcSaveInfo info;
			info.tid = upper.fileName() + lower.fileName();
			for( quint32 i = 0; i < cnt; i++ )
			{
				//qDebug() << "opening archive" << fi3.at( i ).absoluteFilePath();
				bool bnrOk = false;
				bool txtOk = false;
				quint32 sSize = 0;
				QString desc;
				QuaZip zip( fi3.at( i ).absoluteFilePath() );
				if( !zip.open( QuaZip::mdUnzip ) )
				{
					qWarning("SaveLoadThread::GetPCSaves(): zip.open(): %d", zip.getZipError() );
					continue;
				}
				zip.setFileNameCodec("IBM866");
				if( zip.getEntriesCount() != 2 )
				{
					qWarning() << "SaveLoadThread::GetPCSaves() -> save contains more than 2 entries" << fi3.at( i ).absoluteFilePath();
					zip.close();
					continue;
				}
				QuaZipFile file(&zip);
				QString name;
				for( bool more = zip.goToFirstFile(); more; more = zip.goToNextFile() )
				{
					if( !file.open( QIODevice::ReadOnly ) )
					{
						qWarning("SaveLoadThread::GetPCSaves(): file.open(): %d", file.getZipError() );
						zip.close();
						continue;
					}
					name = file.getActualFileName();
					//qDebug() << "extracting" << name << "from the archive";
					if( file.getZipError() != UNZ_OK )
					{
						qWarning("SaveLoadThread::GetPCSaves(): file.getFileName(): %d", file.getZipError());
						file.close();
						zip.close();
						continue;
					}
					if( name != "data.bin" && name != "info.txt" )
					{
						qWarning() << "SaveLoadThread::GetPCSaves() -> zip contians bad filename" << fi3.at( i ).absoluteFilePath() << name;
						file.close();
						zip.close();
						continue;
					}
					/*if( name == "data.bin" && banner.size() )//theres already a banner been extracted, no need to get another one
					{
						file.close();
						continue;
					}*/
					QByteArray unc = file.readAll();
					//qDebug() << "read" << hex << unc.size();
					if( file.getZipError() != UNZ_OK )
					{
						qWarning("SaveLoadThread::GetPCSaves(): file.getFileName(): %d", file.getZipError());
						file.close();
						zip.close();
						continue;
					}
					file.close();
					//read the data.bin to get the banner
					if( name == "data.bin" )
					{
						if( info.banner.isEmpty() )
							info.banner = SaveDataBin::GetBanner( unc );

						sSize = SaveDataBin::GetSize( unc );
						bnrOk = true;
					}
					else if( name == "info.txt" )
					{
						desc = QString( unc );
						txtOk = true;
					}
				}
				if( txtOk && bnrOk )
				{
					info.descriptions << desc;
					info.sizes << sSize;
					info.paths << fi3.at( i ).absoluteFilePath();
				}
				zip.close();
				emit SendProgress( (int)( ( (float)( ++done) / (float)total ) * (float)100 ) );
			}
			//QString tidStr = upper.fileName() + lower.fileName();
			//emit SendPcItem( banner, tidStr, descriptions, size );
			if( info.sizes.size() )
				emit SendPcItem( info );
		}
	}
	emit SendProgress( 100 );
}

void SaveLoadThread::GetSavesFromNandDump()
{
    emit SendProgress( 0 );
    QString base = nand.BasePath();
    if( base.isEmpty() )
        return;

    QMap< quint64, quint32 > list = nand.GetSaveList();
    QMap< quint64, quint32 >::Iterator it = list.begin();
    int i = 0;
    int s = list.size();
    while( it != list.end() && !abort )
    {
        quint64 tid = it.key();
        quint32 size = it.value();
        it++;

        QString tidStr = QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
        QString bnrPath = tidStr;
        bnrPath.insert( 8, "/" );
        bnrPath.prepend( "/title/" );
        bnrPath.append( "/data/banner.bin" );

        QByteArray bnr = nand.GetFile( bnrPath );
        emit SendProgress( (int)( ( (float)( ++i ) / (float)s ) * (float)100 ) );
        if( bnr.isEmpty() )
            continue;

		emit SendSneekItem( bnr, tidStr, size );
    }

    emit SendProgress( 100 );
}

bool SaveLoadThread::DeleteSaveFromSneekNand( quint64 tid )
{
    if( isRunning() )
        return false;
    return nand.DeleteSave( tid );
}
