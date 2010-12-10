#include "nanddump.h"
#include "tiktmd.h"
#include "tools.h"

NandDump::NandDump( const QString &path )
{
    uidDirty = true;
    cmDirty = true;
    if( !path.isEmpty() )
	SetPath( path );
}
NandDump::~NandDump()
{
    if( !basePath.isEmpty() )
	Flush();//no need to check the return, the class is destructing and we wouldnt do anything to fix it
}

bool NandDump::Flush()
{
    bool ret = FlushUID();
    return FlushContentMap() && ret;
}

bool NandDump::SetPath( const QString &path )
{
    qDebug() << "NandDump::SetPath(" << path << ")";
    //check what is already in this path and create stuff that is missing
    QFileInfo fi( path );
    basePath = fi.absoluteFilePath();
    if( fi.exists() && fi.isFile() )
    {
	qWarning() << "NandDump::SetPath ->" << path << "is a file";
	return false;
    }
    if( !fi.exists() && !QDir().mkpath( path ) )
    {
	qWarning() << "NandDump::SetPath -> cant create" << path;
	return false;
    }

    //make sure some subfolders are there
    QDir d( path );
    if( ( !d.exists( "title" ) && !d.mkdir( "title" ) )//these should be good enough
	|| ( !d.exists( "ticket" ) && !d.mkdir( "ticket" ) )
	|| ( !d.exists( "shared1" ) && !d.mkdir( "shared1" ) )
	|| ( !d.exists( "shared2" ) && !d.mkdir( "shared2" ) )
	|| ( !d.exists( "sys" ) && !d.mkdir( "sys" ) ) )
    {
	qWarning() << "NandDump::SetPath -> error creating subfolders in" << path;
	return false;
    }

    //make sure there is a valid uid.sys
    QString uidPath = fi.absoluteFilePath() + "/sys/uid.sys";
    fi.setFile( uidPath );
    if( !fi.exists() && !FlushUID() )
    {
	qWarning() << "NandDump::SetPath -> can\'t create new uid.sys";
	return false;
    }
    else
    {
	QFile f( uidPath );
	if( !f.open( QIODevice::ReadOnly ) )
	{
	    qWarning() << "NandDump::SetPath -> error opening existing uid.sys" << uidPath;
	    return false;
	}
	QByteArray u = f.readAll();
	f.close();
	uidMap = UIDmap( u );
	uidMap.Check();//not really taking any action, but it will spit out errors in the debug output
	uidDirty = false;
    }

    //make sure there is a valid content.map
    QString cmPath = basePath + "/shared1/content.map";
    fi.setFile( cmPath );
    if( !fi.exists() && !FlushContentMap() )
    {
	qWarning() << "NandDump::SetPath -> can\'t create new content map";
	return false;
    }
    else
    {
	QFile f( cmPath );
	if( !f.open( QIODevice::ReadOnly ) )
	{
	    qWarning() << "NandDump::SetPath -> error opening existing content.map" << cmPath;
	    return false;
	}
	QByteArray u = f.readAll();
	f.close();
	cMap = SharedContentMap( u );//checked automatically by the constructor
	cMap.Check( basePath + "/shared1" );//just checking to make sure everything is ok.
	cmDirty = false;
    }

    //TODO - need a setting.txt up in here



    return true;
}

//write the uid to the HDD
bool NandDump::FlushUID()
{
    if( uidDirty )
	uidDirty = !SaveData( uidMap.Data(), "/sys/uid.sys" );
    return !uidDirty;
}

//write the shared content map to the HDD
bool NandDump::FlushContentMap()
{
    if( cmDirty )
	cmDirty = !SaveData( cMap.Data(), "/shared1/content.map" );
    return !cmDirty;
}

QByteArray NandDump::GetSettingTxt()
{
    return GetFile( "/title/00000001/00000002/data/setting.txt" );
}

bool NandDump::SetSettingTxt( const QByteArray ba )
{
    if( basePath.isEmpty() )
	return false;
    QString path = basePath + "/title/00000001/00000002/data";
    if( !QFileInfo( path ).exists() && !QDir().mkpath( path ) )
	return false;
    return SaveData( ba, "/title/00000001/00000002/data/setting.txt" );
}

const QByteArray NandDump::GetFile( const QString &path )
{
    if( basePath.isEmpty() )
	return QByteArray();
    QFile f( basePath + path );
    if( !f.open( QIODevice::ReadOnly ) )
    {
	qWarning() << "NandDump::GetFile -> cant open file for reading" << path;
	return QByteArray();
    }
    QByteArray ret = f.readAll();
    f.close();
    return ret;
}

//write some file to the nand
bool NandDump::SaveData( const QByteArray ba, const QString& path )
{
    if( basePath.isEmpty() )
	return false;
    qDebug() << "NandDump::SaveData" << path << hex << ba.size();
    QFile f( basePath + path );
    if( !f.open( QIODevice::WriteOnly ) )
    {
	qWarning() << "NandDump::SaveData -> cant open file for writing" << path;
	return false;
    }
    f.write( ba );
    f.close();
    return true;
}

//delete a file from the nand
void NandDump::DeleteData( const QString & path )
{
    qDebug() << "NandDump::DeleteData" << path;
    if( basePath.isEmpty() )
	return;
    QFile::remove( basePath + path );
}

bool NandDump::InstallTicket( const QByteArray ba, quint64 tid )
{
    Ticket t( ba );
    if( t.Tid() != tid )
    {
	qWarning() << "NandDump::InstallTicket -> bad tid" << hex << tid << t.Tid();
	return false;
    }
    //only write the first chunk of the ticket to the nand
    QByteArray start = ba.left( t.SignedSize() );
    if( start.size() != 0x2a4 )
    {
	qWarning() << "NandDump::InstallTicket -> ticket size" << hex << start.size();
    }
    QString p = QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
    p.insert( 8 ,"/" );
    p.prepend( "/ticket/" );
    QString folder = p;
    folder.resize( 17 );
    folder.prepend( basePath );
    if( !QFileInfo( folder ).exists() && !QDir().mkpath( folder ) )
	return false;

    p.append( ".tik" );
    return SaveData( start, p );
}

bool NandDump::InstallTmd( const QByteArray ba, quint64 tid )
{
    Tmd t( ba );
    if( t.Tid() != tid )
    {
	qWarning() << "NandDump::InstallTmd -> bad tid" << hex << tid << t.Tid();
	return false;
    }
    //only write the first chunk of the ticket to the nand
    QByteArray start = ba.left( t.SignedSize() );
    QString p = QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
    p.insert( 8 ,"/" );
    p.prepend( "/title/" );
    p.append( "/content/title.tmd" );
    return SaveData( start, p );
}

bool NandDump::InstallSharedContent( const QByteArray ba, const QByteArray hash )
{
    QByteArray h = hash;
    if( h.isEmpty() )
	h = GetSha1( ba );

    if( !cMap.GetAppFromHash( h ).isEmpty() )//already have this one
	return true;

    //qDebug() << "adding shared content";
    QString appName = cMap.GetNextEmptyCid();
    QString p = "/shared1/" + appName + ".app";
    if( !SaveData( ba, p ) )
	return false;

    cMap.AddEntry( appName, hash );
    cmDirty = true;
    return true;
}

bool NandDump::InstallPrivateContent( const QByteArray ba, quint64 tid, const QString &cid )
{
    QString p = QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
    p.insert( 8 ,"/" );
    p.prepend( "/title/" );
    p.append( "/content/" + cid + ".app" );
    return SaveData( ba, p );
}

//delete all .app and .tmd files in the content folder for this tid
void NandDump::AbortInstalling( quint64 tid )
{
    QString p = QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
    p.insert( 8 ,"/" );
    p.prepend( "/title" );
    p.append( "/content" );
    QDir d( p );
    if( !d.exists() )
	return;

    QFileInfoList fiL = d.entryInfoList( QStringList() << "*.app" << ".tmd", QDir::Files );
    foreach( QFileInfo fi, fiL )
	QFile::remove( fi.absoluteFilePath() );
}

bool NandDump::DeleteTitle( quint64 tid, bool deleteData )
{
    QString tidStr = QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
    tidStr.insert( 8 ,"/" );
    QString tikPath = tidStr;
    tikPath.prepend( "/ticket/" );
    tikPath.append( ".tik" );
    DeleteData( tikPath );

    if( !deleteData )
    {
	AbortInstalling( tid );
	return true;
    }

    QString tPath = basePath + "/title/" + tidStr;
    return RecurseDeleteFolder( tPath );
}

//go through and delete all the stuff in a given folder and then delete the folder itself
//this function expects an absolute path, not a relitive one inside the nand dump
bool NandDump::RecurseDeleteFolder( const QString &path )
{
    QDir d( path );
    QFileInfoList fiL = d.entryInfoList( QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot );
    foreach( QFileInfo fi, fiL )
    {
	if( fi.isFile() && !QFile::remove( fi.absoluteFilePath() ) )
	{
	    qWarning() << "NandDump::RecurseDeleteFolder -> error deleting" << fi.absoluteFilePath();
	    return false;
	}
	if( fi.isDir() && !RecurseDeleteFolder( fi.absoluteFilePath() ) )
	{
	    qWarning() << "NandDump::RecurseDeleteFolder -> error deleting" << fi.absoluteFilePath();
	    return false;
	}
    }
    return  QDir().rmdir( path );
}

bool NandDump::InstallNusItem( NusJob job )
{
    if( !job.tid || job.data.size() < 3 )
    {
	qWarning() << "NandDump::InstallNusItem -> invalid item";
	return false;
    }
    if( !uidDirty )
    {
	uidDirty = uidMap.GetUid( job.tid, false ) != 0;//only frag the uid as dirty if it has to be, this way it is only flushed if needed
    }
    uidMap.GetUid( job.tid );
    QString p = QString( "%1" ).arg( job.tid, 16, 16, QChar( '0' ) );
    p.insert( 8 ,"/" );
    p.prepend( "/title/" );
    QString path = basePath + p + "/content";
    if( !QFileInfo( path ).exists() && !QDir().mkpath( path ) )
	return false;

    path = basePath + p + "/data";
    if( !QFileInfo( path ).exists() && !QDir().mkpath( path ) )
	return false;

    QByteArray ba = job.data.takeFirst();
    if( !InstallTmd( ba, job.tid ) )
	return false;

    Tmd t( ba );

    ba = job.data.takeFirst();
    Ticket ti( ba );
    if( !InstallTicket( ba, job.tid ) )
    {
	AbortInstalling( job.tid );
	return false;
    }

    quint32 cnt = qFromBigEndian( t.payload()->num_contents );
    if( cnt != (quint32)job.data.size() )
    {
	AbortInstalling( job.tid );
	return false;
    }

    for( quint32 i = 0; i < cnt; i++ )
    {
	//make sure the content is not encrypted
	QByteArray decData;
	if( job.decrypt )
	{
	    decData = job.data.takeFirst();
	}
	else
	{
	    //seems like a waste to keep setting the key, but for now im doing it this way
	    //so multiple objects can be decrypting titles at the same time
	    AesSetKey( ti.DecryptedKey() );
	    QByteArray paddedEncrypted = PaddedByteArray( job.data.takeFirst(), 0x40 );
	    decData = AesDecrypt( i, paddedEncrypted );
	    decData.resize( t.Size( i ) );
	    QByteArray realHash = GetSha1( decData );
	    if( realHash != t.Hash( i ) )
	    {
		qWarning() << "NandDump::InstallNusItem -> hash doesnt match for content" << hex << i;
		hexdump( realHash );
		hexdump( t.Hash( i ) );
		AbortInstalling( job.tid );
		return false;
	    }
	}
	if( t.Type( i ) == 0x8001 )
	{
	    if( !InstallSharedContent( decData, t.Hash( i ) ) )
	    {
		AbortInstalling( job.tid );
		return false;
	    }
	}
	else if( t.Type( i ) == 1 )
	{
	    if( !InstallPrivateContent( decData, job.tid, t.Cid( i ) ) )
	    {
		AbortInstalling( job.tid );
		return false;
	    }
	}
	else//unknown content type
	{
	    qWarning() << "NandDump::InstallNusItem -> unknown content type";
	    AbortInstalling( job.tid );
	    return false;
	}
    }
    return true;
}
