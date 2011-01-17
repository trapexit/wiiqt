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
    ret = FlushReplacementStrings() && ret;
    return FlushContentMap() && ret;
}

bool NandDump::SetPath( const QString &path )
{
    qDebug() << "NandDump::SetPath(" << path << ")";
	uidDirty = true;
	cmDirty = true;
    //check what is already in this path and create stuff that is missing
    QFileInfo fi( path );
    basePath = fi.absoluteFilePath();
    if( fi.exists() && fi.isFile() )
    {
        qWarning() << "NandDump::SetPath ->" << path << "is a file";
        basePath.clear();
        return false;
    }
    if( !fi.exists() && !QDir().mkpath( path ) )
    {
        qWarning() << "NandDump::SetPath -> cant create" << path;
        basePath.clear();
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
        basePath.clear();
        return false;
    }

    //make sure there is a valid uid.sys
    QString uidPath = fi.absoluteFilePath() + "/sys/uid.sys";
    fi.setFile( uidPath );
    if( !fi.exists() && !FlushUID() )
    {
        qWarning() << "NandDump::SetPath -> can\'t create new uid.sys";
        basePath.clear();
        return false;
    }
    else
    {
        QFile f( uidPath );
        if( !f.open( QIODevice::ReadOnly ) )
        {
            qWarning() << "NandDump::SetPath -> error opening existing uid.sys" << uidPath;
            basePath.clear();
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
        basePath.clear();
        return false;
    }
    else
    {
        QFile f( cmPath );
        if( !f.open( QIODevice::ReadOnly ) )
        {
            qWarning() << "NandDump::SetPath -> error opening existing content.map" << cmPath;
            basePath.clear();
            return false;
        }
        QByteArray u = f.readAll();
        f.close();
        cMap = SharedContentMap( u );//checked automatically by the constructor
        //cMap.Check( basePath + "/shared1" );//just checking to make sure everything is ok.
        cmDirty = false;
    }

    //read the list of strings used to fix the nand for certain filesystems
    ReadReplacementStrings();

    //TODO - need a setting.txt up in here



    return true;
}

const QString NandDump::BasePath()
{
    return basePath;
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

//write the file of replacement strings to HDD
bool NandDump::FlushReplacementStrings()
{
    QString st;
    QMap< QString, QString >::iterator i = replaceStrings.begin();
    while( i != replaceStrings.end() )
    {
        st += i.key() + " " + i.value() + "\n";
        i++;
    }
    if( st.isEmpty() )
        return true;

    return SaveData( st.toLatin1(), "/sys/replace" );
}

QByteArray NandDump::GetSettingTxt()
{
    return GetFile( "/title/00000001/00000002/data/setting.txt" );
}

bool NandDump::SetSettingTxt( const QByteArray &ba )
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
    if( basePath.isEmpty() || !path.startsWith( "/" ) )
        return QByteArray();

    return ReadFile( basePath + path );
}

//write some file to the nand
bool NandDump::SaveData( const QByteArray &ba, const QString& path )
{
    if( basePath.isEmpty() || !path.startsWith( "/" ) )
        return false;
    qDebug() << "NandDump::SaveData" << path << hex << ba.size();
    return WriteFile( basePath + path, ba );
}

//delete a file from the nand
void NandDump::DeleteData( const QString & path )
{
    qDebug() << "NandDump::DeleteData" << path;
    if( basePath.isEmpty() || !path.startsWith( "/" ) )
        return;

    QFile::remove( basePath + path );
}

bool NandDump::InstallTicket( const QByteArray &ba, quint64 tid )
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

bool NandDump::InstallTmd( const QByteArray &ba, quint64 tid )
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

bool NandDump::InstallSharedContent( const QByteArray &ba, const QByteArray &hash )
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

bool NandDump::InstallPrivateContent( const QByteArray &ba, quint64 tid, const QString &cid )
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
    if( basePath.isEmpty() )
        return false;

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
    if( basePath.isEmpty() || !path.startsWith( QFileInfo( basePath ).absoluteFilePath() ) )//make sure we arent deleting something outside the virtual nand
    {
        qWarning() << "NandDump::RecurseDeleteFolder -> something is amiss; tried to delete" << path;
        return false;
    }
    QDir d( path );
    if( !d.exists() )
        return true;

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

bool NandDump::InstallNusItem( const NusJob &job )
{
    if( !job.tid || job.data.size() < 3 )
    {
        qWarning() << "NandDump::InstallNusItem -> invalid item";
        return false;
    }
    if( !uidDirty )
    {
        uidDirty = uidMap.GetUid( job.tid, false ) == 0;//only flag the uid as dirty if it has to be, this way it is only flushed if needed
    }
    uidMap.GetUid( job.tid );
    QString p = QString( "%1" ).arg( job.tid, 16, 16, QChar( '0' ) );
    p.insert( 8 ,"/" );
    p.prepend( "/title/" );
    QString path = basePath + p + "/content";

    //remove old title if it exists
    AbortInstalling( job.tid );

    if( !QFileInfo( path ).exists() && !QDir().mkpath( path ) )
        return false;

    path = basePath + p + "/data";
    if( !QFileInfo( path ).exists() && !QDir().mkpath( path ) )
        return false;

    QByteArray ba = job.data.at( 0 );
    if( !InstallTmd( ba, job.tid ) )
        return false;

    Tmd t( ba );

    ba = job.data.at( 1 );
    Ticket ti( ba );
    if( !InstallTicket( ba, job.tid ) )
    {
        AbortInstalling( job.tid );
        return false;
    }

    quint32 cnt = qFromBigEndian( t.payload()->num_contents );
	if( cnt != (quint32)job.data.size() - 2 )
    {
		qWarning() << "cnt != (quint32)job.data.size()";
        AbortInstalling( job.tid );
        return false;
    }

    for( quint32 i = 0; i < cnt; i++ )
    {
        //make sure the content is not encrypted
        QByteArray decData;
        if( job.decrypt )
        {
            decData = job.data.at( i + 2 );
        }
        else
        {
            //seems like a waste to keep setting the key, but for now im doing it this way
            //so multiple objects can be decrypting titles at the same time
            AesSetKey( ti.DecryptedKey() );
            QByteArray paddedEncrypted = PaddedByteArray( job.data.at( i + 2 ), 0x40 );
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

bool NandDump::InstallWad( Wad wad )
{
	if( !wad.Tid() || wad.content_count() < 3 )
	{
		qWarning() << "NandDump::InstallNusItem -> invalid item";
		return false;
	}
	if( !uidDirty )
	{
		uidDirty = uidMap.GetUid( wad.Tid(), false ) == 0;//only flag the uid as dirty if it has to be, this way it is only flushed if needed
	}
	uidMap.GetUid( wad.Tid() );
	QString p = QString( "%1" ).arg( wad.Tid(), 16, 16, QChar( '0' ) );
	p.insert( 8 ,"/" );
	p.prepend( "/title/" );
	QString path = basePath + p + "/content";

    //remove old title if it exists
    AbortInstalling( wad.Tid() );

	if( !QFileInfo( path ).exists() && !QDir().mkpath( path ) )
        return false;

	path = basePath + p + "/data";
	if( !QFileInfo( path ).exists() && !QDir().mkpath( path ) )
        return false;

	QByteArray ba = wad.getTmd();
	if( !InstallTmd( ba, wad.Tid() ) )
        return false;

	Tmd t( ba );

	ba = wad.getTik();
    //Ticket ti( ba );
	if( !InstallTicket( ba, wad.Tid() ) )
	{
		AbortInstalling( wad.Tid() );
		return false;
	}

	quint32 cnt = qFromBigEndian( t.payload()->num_contents );
	if( cnt != wad.content_count() )
	{
		AbortInstalling( wad.Tid() );
		return false;
	}

	for( quint32 i = 0; i < cnt; i++ )
	{
		QByteArray decData = wad.Content(i);

		if( t.Type( i ) == 0x8001 )
		{
			if( !InstallSharedContent( decData, t.Hash( i ) ) )
			{
				AbortInstalling( wad.Tid() );
				return false;
			}
		}
		else if( t.Type( i ) == 1 )
		{
			if( !InstallPrivateContent( decData, wad.Tid(), t.Cid( i ) ) )
			{
				AbortInstalling( wad.Tid() );
				return false;
			}
		}
		else//unknown content type
		{
			qWarning() << "NandDump::InstallWad -> unknown content type";
			AbortInstalling( wad.Tid() );
			return false;
		}
	}
	return true;
}

QMap< quint64, quint16 > NandDump::GetInstalledTitles()
{
    QMap< quint64, quint16 >ret;
    if( basePath.isEmpty() )
        return ret;

    //get all the tickets
    QDir d( basePath + "/ticket" );
    QFileInfoList fiL = d.entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot );//get all folders in "/ticket"
    foreach( QFileInfo fi, fiL )
    {
        if( fi.fileName().size() != 8 )
            continue;

        bool ok = false;
        quint32 upper = fi.fileName().toInt( &ok, 16 );
        if( !ok )
            continue;

        QDir sd( fi.absoluteFilePath() );
        QFileInfoList sfiL = sd.entryInfoList( QStringList() << "*.tik", QDir::Files );//get all "*.tik" files in this subfolder
        foreach( QFileInfo sfi, sfiL )
        {
            QString lowerStr = sfi.fileName();//drop the ".tik" extension and convert to u32
            lowerStr.resize( 8 );

            quint32 lower = lowerStr.toInt( &ok, 16 );
            if( !ok )
                continue;

            //load the TMD
            QByteArray tmdData = GetFile( "/title/" + fi.fileName() + "/" + lowerStr + "/content/title.tmd" );
            if( tmdData.isEmpty() )
                continue;

            //get version of tmd
            Tmd t( tmdData );
            quint16 version = t.Version();
            quint64 tid = (quint64)( ((quint64)upper << 32 ) | lower );

            //add this title to the return list
            ret.insert( tid, version );
        }
    }
    return ret;
}

QMap< quint64, quint32 > NandDump::GetSaveList()
{
    qDebug() << "NandDump::GetSaveList()";
    QMap< quint64, quint32 >ret;
    if( basePath.isEmpty() )
        return ret;

    //get all the tickets
    QDir d( basePath + "/title" );
    QFileInfoList fiL = d.entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot );//get all folders in "/title"
    foreach( QFileInfo fi, fiL )
    {
        //qDebug() << "fi:" << fi.absoluteFilePath();
        if( fi.fileName().size() != 8 )
            continue;

        bool ok = false;
        quint32 upper = fi.fileName().toInt( &ok, 16 );
        if( !ok )
            continue;
        //qDebug() << " upper" << hex << upper;

        QDir sd( fi.absoluteFilePath() );//subDir
        QFileInfoList sfiL = sd.entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot );//get all subfolders in this subfolder
        foreach( QFileInfo sfi, sfiL )
        {
            QString lowerStr = sfi.fileName();
            if( lowerStr.size() != 8 )
                continue;

            quint32 lower = lowerStr.toInt( &ok, 16 );
            if( !ok )
                continue;

            if( !QFileInfo( basePath + "/title/" + fi.fileName() + "/" + lowerStr + "/data/banner.bin" ).exists() )
                continue;

            quint64 tid = ( ( (quint64)upper << 32 ) | lower );
            quint32 size = 0;
            //go through the data directory and get all the sizes
            QDirIterator it( QDir( basePath + "/title/" + fi.fileName() + "/" + lowerStr + "/data" ), QDirIterator::Subdirectories );
            while( it.hasNext() )
            {
                it.next();
                QFileInfo saveStuff = it.fileInfo();
                if( saveStuff.isFile() )//its a file, get the data and add it to the return idem
                {
                    size += saveStuff.size();
                }
            }
            //add this title to the return list
            ret.insert( tid, size );
        }
    }
    return ret;
}

bool NandDump::DeleteSave( quint64 tid )
{
    if( basePath.isEmpty() )
        return false;
    QString tidStr = QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
    QString dataPath = tidStr;
    dataPath.insert( 8, "/" );
    dataPath.prepend( basePath + "/title/" );
    dataPath.append( "/data" );

    //make a list of everything in the data folder and then go through the list in reverse order and delete everything
    QFileInfoList list;
    QDir dir( dataPath );
    dir.setFilter( QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot );
    QDirIterator it( dir, QDirIterator::Subdirectories );
    while( it.hasNext() )
    {
        it.next();
        list << it.fileInfo();
    }
    while( !list.isEmpty() )
    {
        QFileInfo fi = list.takeLast();
        //qDebug() << "would delete" << fi.absoluteFilePath();
        if( ( fi.isFile() && !QFile::remove( fi.absoluteFilePath() ) )
            || ( fi.isDir() && !QDir().rmdir( fi.absoluteFilePath() ) ) )
        {
            qDebug() << "failed to delete" << fi.absoluteFilePath();
            return false;
        }
    }
    return true;
}

void NandDump::ReadReplacementStrings()
{
    replaceStrings.clear();
    QByteArray ba = GetFile( "/sys/replace" );
    if( ba.isEmpty() )
        return;

    QRegExp re( "[^/?*:;{}\\]+" );

    QString all( ba );
    all.replace( "\r\n", "\n" );
    QStringList lines = QString( ba ).split( "\n", QString::SkipEmptyParts );
    foreach( QString line, lines )
    {
        //skip lines that are less than 3 characters on dont have a space as their second character or have characters not allowed on FAT32
        if( line.size() < 3 || line.at( 1 ) != ' ' || line.contains( re ) )
            continue;

        QString ch = line.left( 1 );
        QString rp = line.right( line.size() - 2 );

        replaceStrings.insert( ch, rp );
    }
}

bool NandDump::SetReplaceString( const QString &ch, const QString &replaceWith )
{
    qWarning() << "NandDump::SetReplaceString(" << ch << "," << replaceWith << ")";
    QRegExp re( "[^/?*:;{}\\]+" );
    if( replaceWith.contains( re ) )
    {
        qWarning() << "NandDump::SetReplaceString -> replacement string contains illegal characters";
        return false;
    }

    QString from;
    QString to;
    QMap< QString, QString >::iterator i = replaceStrings.find( ch );

    if( i == replaceStrings.end() )	//currently not replacing this character
    {
        if( replaceWith.isEmpty() )//nothing to do
            return true;
        from = ch;
        to = replaceWith;
    }
    else				//this character is already being replaced by something
    {
        if( i.value() == replaceWith )//nothing to do
            return true;

        from = i.value();
        if( replaceWith.isEmpty() ) //set all names back to their correct ones
        {
            to = ch;
        }
        else			    //change the names from one replacement character to another
        {
            to = replaceWith;
        }
    }

    //now go through and try to apply the new naming to all existing files/folders
    //if something goes wrong, try to rename all files back to their original name
    if( !RecurseRename( QFileInfo( basePath ).absoluteFilePath() + "/title", from, to ) )
    {
        qWarning() << "NandDump::SetReplaceString -> error renaming something; trying to undo whatever i did";
        if( !RecurseRename( QFileInfo( basePath ).absoluteFilePath() + "/title", from, to ) )
        {
            qWarning() << "NandDump::SetReplaceString -> something went wrong and i couldnt fix it.";
        }
        return false;
    }
    if( to.isEmpty() )
        replaceStrings.remove( ch );
    else
        replaceStrings.insert( ch, to );
    return true;
}

bool NandDump::RecurseRename( const QString &path, const QString &from, const QString &to )
{
    //qDebug() << "NandDump::RecurseRename(" << path << "," << from << "," << to << ")";
    //make sure we arent messing with something outside the virtual nand
    if( basePath.isEmpty() || !path.startsWith( QFileInfo( basePath ).absoluteFilePath() ) )
    {
        qWarning() << "NandDump::RecurseRename -> something is amiss; tried to rename" << path;
        return false;
    }
    QDir d( path );
    if( !d.exists() )
        return true;

    QFileInfoList fiL = d.entryInfoList( QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot );
    foreach( QFileInfo fi, fiL )
    {
        QString name = fi.fileName();
        name.replace( from, to );

        if( fi.isFile() )
        {
            if( fi.fileName() != name && !d.rename( fi.fileName(), name ) )
            {
                qWarning() << "NandDump::RecurseRename -> error renaming" << fi.absoluteFilePath() << "to" << name;
                return false;
            }
        }
        if( fi.isDir() )
        {
            if( fi.fileName() != name && !d.rename( fi.fileName(), name ) )
            {
                qWarning() << "NandDump::RecurseRename -> error renaming" << fi.absoluteFilePath() << "to" << name;
                return false;
            }
            if( !RecurseRename( fi.absoluteFilePath(), from, to ) )
                return false;
        }
    }
    return true;
}

const QString NandDump::ToNandName( const QString &name )
{
    QString ret = name;
    QMap< QString, QString >::iterator i = replaceStrings.begin();
    while( i != replaceStrings.end() )
    {
        ret.replace( i.key(), i.value() );
        i++;
    }
    return ret;
}

const QString NandDump::FromNandName( const QString &name )
{
    QString ret = name;
    QMap< QString, QString >::iterator i = replaceStrings.begin();
    while( i != replaceStrings.end() )
    {
        ret.replace( i.value(), i.key() );
        i++;
    }
    return ret;
}

const QString NandDump::ToNandPath( const QString &path )
{
    QString ret;
    QStringList parts = path.split( "/", QString::SkipEmptyParts );
    foreach( QString part, parts )
        ret += "/" + ToNandName( part );

    return ret;
}

const QString NandDump::FromNandPath( const QString &path )
{
    QString ret;
    QStringList parts = path.split( "/", QString::SkipEmptyParts );
    foreach( QString part, parts )
        ret += "/" + FromNandName( part );

    return ret;
}

QMap<QString, QString> NandDump::GetReplacementStrings()
{
    return replaceStrings;
}

SaveGame NandDump::GetSaveData( quint64 tid )
{
    SaveGame ret;
    ret.tid = 0;
    if( basePath.isEmpty() )
        return ret;

    //build the path to the data folder
    QString p = QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
    p.insert( 8 ,"/" );
    p.prepend( "/title/" );
    p += "/data";
    QString path = basePath + p;

    QDir d( path );
    if( !d.exists() )//folder doesnt exist, theres nothing to get
        return ret;

    d.setFilter( QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot );

    //go through this directory and get the goods
    QDirIterator it( d, QDirIterator::Subdirectories );
    while( it.hasNext() )
    {
        QString str = it.next();
        if( !str.startsWith( path ) )
        {
            qWarning() << "NandDump::GetSaveData -> bad path" << path << str;
            return ret;
        }
        str.remove( 0, path.size() );
        ret.entries << FromNandPath( str );//convert from the paths used in the local filesystem to ones expected by wii games

        QFileInfo fi = it.fileInfo();
        if( fi.isFile() )//its a file, get the data and add it to the return idem
        {
            ret.data << ReadFile( fi.absoluteFilePath() );
            ret.attr << DEFAULT_SAVE_ATTR_FILE;
        }
        else//its a folder
        {
            ret.attr << DEFAULT_SAVE_ATTR_DIR;
        }
    }
    ret.tid = tid;
    return ret;
}

bool NandDump::InstallSave( const SaveGame &save )
{
    if( basePath.isEmpty() || !IsValidSave( save ) )
        return false;

    //build the path to the data folder
    QString p = QString( "%1" ).arg( save.tid, 16, 16, QChar( '0' ) );
    p.insert( 8 ,"/" );
    p.prepend( "/title/" );
    //p += "/data";
    QString path = basePath + p + "/data";

    //make sure the path exists
	if( !QFileInfo( path ).exists() && !QDir().mkpath( path ) )
    {
        qWarning() << "NandDump::InstallSave -> error creating" << path;
        return false;
    }
    //try to make the content folder, but it doesnt matter if it isnt created for whatever reason
    if( !QFileInfo( basePath + p + "/content" ).exists() )
        QDir().mkpath( basePath + p + "/content" );

	path = p + "/data";

    quint16 dataIdx = 0;
    quint16 entryIdx = 0;
    foreach( QString entry, save.entries )
	{
		QString cp = ToNandPath( entry );
        quint8 attr = save.attr.at( entryIdx );
        if( NAND_ATTR_TYPE( attr ) == NAND_FILE )//this is a file
        {
            if( !SaveData( save.data.at( dataIdx++ ), path + cp ) )
                return false;
        }
        else				//this is a directory
        {
            if( !QDir().mkpath( path + cp ) )
                return false;
        }
        entryIdx++;
    }
    return true;
}

