#include "nandthread.h"

NandThread::NandThread( QObject *parent ) : QThread( parent )
{
    abort = false;
    itemToExtract = NULL;

    connect( &nandBin, SIGNAL( SendError( QString ) ), this, SLOT( GetError( QString ) ) );
    connect( &nandBin, SIGNAL( SendText( QString ) ), this, SLOT( GetStatusUpdate( QString ) ) );
}

void NandThread::ForceQuit()
{
    mutex.lock();
    abort = true;
    mutex.unlock();
}

NandThread::~NandThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    wait();

    if( itemToExtract )
	delete itemToExtract;
}

bool NandThread::IsRunning()
{
    return isRunning();
}

bool NandThread::SetPath( const QString &path )
{
    if( isRunning() )
    {
	emit SendError( tr( "Wait till the current job is done" ) );
	return false;
    }
    return nandBin.SetPath( path );
}

const Blocks0to7 NandThread::BootBlocks()
{
    if( isRunning() )
    {
	emit SendError( tr( "Wait till the current job is done" ) );
	return Blocks0to7();
    }
    return nandBin.BootBlocks();
}

const QList<Boot2Info> NandThread::Boot2Infos()
{
    if( isRunning() )
    {
	emit SendError( tr( "Wait till the current job is done" ) );
	return QList<Boot2Info>();
    }
    return nandBin.Boot2Infos();
}

quint8 NandThread::Boot1Version()
{
    if( isRunning() )
    {
	emit SendError( tr( "Wait till the current job is done" ) );
	return 0;
    }
    return nandBin.Boot1Version();
}

QTreeWidgetItem *NandThread::GetTree()
{
    if( isRunning() )
    {
	emit SendError( tr( "Wait till the current job is done" ) );
	return NULL;
    }
    return nandBin.GetTree();
}

bool NandThread::InitNand( QIcon dirs, QIcon files )
{
    if( isRunning() )
    {
	emit SendError( tr( "Wait till the current job is done" ) );
	return false;
    }
    return nandBin.InitNand( dirs, files );
}

const QList<quint16> NandThread::GetFats()
{
    return nandBin.GetFats();
}

const QList<quint16> NandThread::GetFatsForFile( quint16 i )
{
    return nandBin.GetFatsForFile( i );
}

void NandThread::Extract( QTreeWidgetItem *item, const QString &path )
{
    if( isRunning() )
    {
	emit SendError( tr( "This thread is already doing something.  Please wait." ) );
	return;
    }
    if( !item )
    {
	emit SendError( tr( "Oh Noez!! I was told to extract with a pointer to NULL :( ." ) );
	return;
    }
    abort = false;
    extractPath = path;
    itemToExtract = item->clone();



    start( NormalPriority );
}

void NandThread::run()
{
    if( abort )
    {
	qDebug( "NandThread::run -> Thread abort" );
	return;
    }
    mutex.lock();
    if( extractPath.isEmpty() )
    {
	qDebug() << "NandThread::run -> its empty";
	return;
    }
    fileCnt = FileCount( itemToExtract );
    idx = 0;
    mutex.unlock();
    emit SendProgress( 0 );

    nandBin.ExtractToDir( itemToExtract, extractPath );

    delete itemToExtract;
    itemToExtract = NULL;
    emit SendProgress( 100 );
    emit SendExtractDone();
}

quint32 NandThread::FileCount( QTreeWidgetItem *item )
{
    if( item->text( 6 ) == "00" )//its a folder, recurse through it and count all the files
    {
	quint32 ret = 0;
	quint16 cnt = item->childCount();
	for( quint16 i = 0; i < cnt; i++ )
	{
	    ret += FileCount( item->child( i ) ) ;
	}
	return ret;
    }
    //not a folder, must be a file.  just return 1
    return 1;
}

void NandThread::GetStatusUpdate( QString s )
{
    emit SendText( s );
    emit SendProgress( (int)( ( (float)( idx++ ) / (float)fileCnt ) * (float)100 ) );
}

//just send errors from the nand object to the mainwindow
void NandThread::GetError( QString str )
{
    emit SendError( str );
}



