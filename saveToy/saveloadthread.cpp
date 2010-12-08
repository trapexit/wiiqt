#include "saveloadthread.h"

SaveLoadThread::SaveLoadThread( QObject *parent ) : QThread( parent )
{
    abort = false;
    //qRegisterMetaType< SaveListItem* >();
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


void SaveLoadThread::GetBanners( const QString &bPath, int mode )
{
    basePath = bPath;
    type = mode;
    start( NormalPriority );
}

void SaveLoadThread::run()
{
    if ( abort )
    {
	qDebug( "SaveLoadThread::run -> Thread abort" );
	return;
    }
    mutex.lock();
    if( basePath.isEmpty() )
    {
	qDebug() << "SaveLoadThread::run -> its empty";
	return;
    }
    mutex.unlock();

    QFileInfo fi( basePath );
    if( !fi.exists() || !fi.isDir() )
    {
	qWarning() << "SaveLoadThread::run ->" << basePath << "is not a directory";
	return;
    }

    fi.setFile(basePath + "/title/00010000" );
    if( !fi.exists() || !fi.isDir() )
    {
	qWarning() << "SaveLoadThread::run ->" << QString( basePath + "/title/00010000" ) << "is not a directory";
	return;
    }


    QDir subDir( basePath + "/title/00010000" );
    subDir.setFilter( QDir::Dirs | QDir::NoDotAndDotDot );
    QFileInfoList fiL = subDir.entryInfoList();
    quint32 cnt = fiL.size();

    int i = 0;
    subDir.setPath( basePath + "/title/00010001" );
    QFileInfoList fiL2 = subDir.entryInfoList();
    cnt += fiL2.size();

    foreach( QFileInfo f, fiL )
    {
	i++;
	emit SendProgress( (int)( ( (float)( i ) / (float)cnt ) * (float)100 ) );

	QFile ff( f.absoluteFilePath() + "/data/banner.bin" );
	if( !ff.exists() || !ff.open( QIODevice::ReadOnly ) )
	    continue;

	QByteArray stuff = ff.readAll();
	ff.close();

	quint32 size = GetFolderSize( f.absoluteFilePath() + "/data" );
	emit SendItem( stuff, QString( "00010000" + f.fileName() ), type, size );
    }
    foreach( QFileInfo f, fiL2 )
    {
	i++;
	emit SendProgress( (int)( ( (float)( i ) / (float)cnt ) * (float)100 ) );

	QFile ff( f.absoluteFilePath() + "/data/banner.bin" );
	if( !ff.exists() || !ff.open( QIODevice::ReadOnly ) )
	    continue;

	QByteArray stuff = ff.readAll();
	ff.close();

	quint32 size = GetFolderSize( f.absoluteFilePath() + "/data" );
	emit SendItem( stuff, QString( "00010002" + f.fileName() ), type, size );
    }

    emit SendProgress( 100 );
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
