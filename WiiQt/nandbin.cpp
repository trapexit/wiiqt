#include "nandbin.h"
#include "tools.h"

NandBin::NandBin( QObject * parent, const QString &path ) : QObject( parent )
{
    type = -1;
    fatNames = false;
    fstInited = false;
    root = NULL;

    if( !path.isEmpty() )
        SetPath( path );
}

NandBin::~NandBin()
{
    if( f.isOpen() )
    {
#ifdef NAND_BIN_CAN_WRITE
        f.flush();
#endif
        f.close();
    }

    if( root )
        delete root;
}

bool NandBin::SetPath( const QString &path )
{
    fstInited = false;
    nandPath = path;
    bootBlocks = Blocks0to7();
    if( f.isOpen() )
        f.close();

    f.setFileName( path );
    bool ret = ( f.exists() &&
#ifdef NAND_BIN_CAN_WRITE
                 f.open( QIODevice::ReadWrite ) );
#else
    f.open( QIODevice::ReadOnly ) );
#endif
    if( !ret )
    {
        emit SendError( tr( "Cant open %1" ).arg( path ) );
    }

    return ret;
}
const QString NandBin::FilePath()
{
	if( !f.isOpen() )
		return QString();
	return QFileInfo( f ).absoluteFilePath();
}

//#if 0 // apparently you dont need any extra reserved blocks for the thing to boot?
bool NandBin::CreateNew( const QString &path, const QByteArray &keys, const QByteArray &first8, const QList<quint16> &badBlocks )
{
#ifndef NAND_BIN_CAN_WRITE
	qWarning() << __FILE__ << "was built without write support";
	return false;
#endif
	if( keys.size() != 0x400 || first8.size() != 0x108000 )
	{
		qWarning() << "NandBin::CreateNew -> bad sizes" << hex << keys.size() << first8.size();
		return false;
	}

	if( f.isOpen() )
		f.close();

	//create the new file, write the first 8 blocks, fill it with 0xff, and write the keys.bin at the end
	f.setFileName( path );
	if( !f.open( QIODevice::ReadWrite | QIODevice::Truncate ) )
	{
		qWarning() << "NandBin::CreateNew -> can't create file" << path;
		return false;
	}

	f.write( first8 );
	QByteArray block( 0x4200, 0xff );//generic empty cluster
	for( quint16 i = 0; i < 0x7fc0; i++ )
		f.write( block );

	f.write( keys );
	if( f.pos() != 0x21000400 )//no room left on the drive?
	{
		qWarning() << "NandBin::CreateNew -> dump size is wrong" << (quint32)f.pos();
		return false;
	}

	//setup variables
	nandPath = path;
	currentSuperCluster = 0x7f00;
	superClusterVersion = 1;
	type = 2;
	fats.clear();
	memset( &fsts, 0, sizeof( fst_t ) * 0x17ff );

	for( quint16 i = 0; i < 0x17ff; i++ )
		fsts[ i ].fst_pos = i;

	//reserve blocks 0 - 7
	for( quint16 i = 0; i < 0x40; i++ )
	{
		fats << 0xfffc;
	}
	//mark all the "normal" blocks - free, or bad
	for( quint16 i = 0x40; i < 0x7f00; i++ )
	{
		if( badBlocks.contains( i / 8 ) )
			fats << 0xfffd;
		else
			fats << 0xfffe;

	}
	//reserve the superclusters
	for( quint16 i = 0x7f00; i < 0x8000; i++ )
	{
		fats << 0xfffc;
	}

	//make the root item
	fsts[ 0 ].filename[ 0 ] = '/';
	fsts[ 0 ].attr = 0x16;
	fsts[ 0 ].sib = 0xffff;
	fsts[ 0 ].sub = 0xffff;

	fstInited = true;

	//set keys
	QByteArray hmacKey = keys.mid( 0x144, 0x14 );
	spare.SetHMacKey( hmacKey );//set the hmac key for calculating spare data
	key = keys.mid( 0x158, 0x10 );

	//write the metada to each of the superblocks
	for( quint8 i = 0; i < 0x10; i++ )
	{
		if( !WriteMetaData() )
		{
			qWarning() << "NandBin::CreateNew -> error writing superblock" << i;
			return false;
		}
	}

	//build the tree
	if( root )
		delete root;
	root = new QTreeWidgetItem( QStringList() << nandPath );
	AddChildren( root, 0 );

	return true;
}
//#endif

bool NandBin::Format( bool secure )
{
#ifndef NAND_BIN_CAN_WRITE
	qWarning() << __FILE__ << "was built without write support";
	return false;
#endif
	if( !f.isOpen() || fats.size() != 0x8000 )
	{
		qWarning() << "NandBin::Format -> error" << hex << fats.size() << f.isOpen();
		return false;
	}

	//mark any currently used clusters free
	QByteArray cluster( 0x4200, 0xff );//generic empty cluster
	for( quint16 i = 0x40; i < 0x7f00; i++ )
	{
		if( fats.at( i ) >= 0xf000 && fats.at( i ) != 0xfffe )	//preserve special marked ones
			continue;

		fats[ i ] = 0xfffe;										//free the cluster
		if( !secure )
			continue;

		f.seek( 0x4200 * i );									//overwrite anything there with the unused cluster
		f.write( cluster );
	}

	//reset fsts
	memset( &fsts, 0, sizeof( fst_t ) * 0x17ff );
	for( quint16 i = 0; i < 0x17ff; i++ )
		fsts[ i ].fst_pos = i;

	//make the root item
	fsts[ 0 ].filename[ 0 ] = '/';
	fsts[ 0 ].attr = 0x16;
	fsts[ 0 ].sib = 0xffff;
	fsts[ 0 ].sub = 0xffff;

	//write the metada to each of the superblocks
	for( quint8 i = 0; i < 0x10; i++ )
	{
		if( !WriteMetaData() )
		{
			qWarning() << "NandBin::Format -> error writing superblock" << i;
			return false;
		}
	}

	//build the tree
	if( root )
		delete root;
	root = new QTreeWidgetItem( QStringList() << nandPath );
	AddChildren( root, 0 );

	return true;

}

#if 0    // this boots ok on real HW.  trap15 thinks these reserved blocks are IOS's way of marking ones it thinks are bad
bool NandBin::CreateNew( const QString &path, const QByteArray &keys, const QByteArray &first8, const QList<quint16> &badBlocks )
{
#ifndef NAND_BIN_CAN_WRITE
	qWarning() << __FILE__ << "was built without write support";
	return false;
#endif
	if( keys.size() != 0x400 || first8.size() != 0x108000 )
	{
		qWarning() << "NandBin::CreateNew -> bad sizes" << hex << keys.size() << first8.size();
		return false;
	}
	for( quint16 i = 0; i < 0x40; i++ )
	{
		if( badBlocks.contains( i / 8 ) )
		{
			qWarning() << "NandBin::CreateNew -> creating a nand with bad blocks in the first 8 is not supported";
			return false;
		}
	}

	for( quint16 i = 0x7f00; i < 0x8000; i++ )
	{
		if( badBlocks.contains( i / 8 ) )
		{
			qWarning() << "NandBin::CreateNew -> creating a nand with bad blocks in the superclusters not supported";
			return false;
		}
	}

	if( f.isOpen() )
		f.close();

	//create the new file, write the first 8 blocks, fill it with 0xff, and write the keys.bin at the end
	f.setFileName( path );
	if( !f.open( QIODevice::ReadWrite | QIODevice::Truncate ) )
	{
		qWarning() << "NandBin::CreateNew -> can't create file" << path;
		return false;
	}

	f.write( first8 );
	QByteArray block( 0x4200, 0xff );//generic empty block
	for( quint16 i = 0; i < 0x7fc0; i++ )
		f.write( block );

	f.write( keys );
	if( f.pos() != 0x21000400 )//no room left on the drive?
	{
		qWarning() << "NandBin::CreateNew -> dump size is wrong" << (quint32)f.pos();
		return false;
	}

	//setup variables
	nandPath = path;
	currentSuperCluster = 0x7f00;
	superClusterVersion = 1;
	type = 2;
	fats.clear();
	memset( &fsts, 0, sizeof( fst_t ) * 0x17ff );

	for( quint16 i = 0; i < 0x17ff; i++ )
		fsts[ i ].fst_pos = i;

	//reserve blocks 0 - 7
	for( quint16 i = 0; i < 0x40; i++ )
	{
		fats << 0xfffc;
	}
	//find 90 blocks to reserve.  they always appear to be close to the end of the nand
	//TODO - this isnt always 90, all my nands have a different number, and 90 is right in the middle
	//sometimes IOS adds more
	quint16 bCnt = badBlocks.size();
	quint16 offset = 0;
	for( quint16 i = 0; i < bCnt; i++ )
	{
		if( i >= 3998 )
			offset++;
	}

	//mark all the "normal" blocks - free, or bad
	for( quint16 i = 0x40; i < 0x7cf0 - offset; i++ )
	{
		if( badBlocks.contains( i / 8 ) )
			fats << 0xfffd;
		else
			fats << 0xfffe;

	}
	//mark the 90 reserved ones from above and reserve the superclusters
	for( quint16 i = 0x7cf0 - offset; i < 0x8000; i++ )
	{
		fats << 0xfffc;
	}

	//make the root item
	fsts[ 0 ].filename[ 0 ] = '/';
	fsts[ 0 ].attr = 0x16;
	fsts[ 0 ].sib = 0xffff;
	fsts[ 0 ].sub = 0xffff;

	fstInited = true;

	//set keys
	QByteArray hmacKey = keys.mid( 0x144, 0x14 );
	spare.SetHMacKey( hmacKey );//set the hmac key for calculating spare data
	key = keys.mid( 0x158, 0x10 );

	//write the metada to each of the superblocks
	for( quint8 i = 0; i < 0x10; i++ )
	{
		if( !WriteMetaData() )
		{
			qWarning() << "NandBin::CreateNew -> error writing superblock" << i;
			return false;
		}
	}

	//build the tree
	if( root )
		delete root;
	root = new QTreeWidgetItem( QStringList() << nandPath );
	AddChildren( root, 0 );

	return true;
}
#endif
QTreeWidgetItem *NandBin::GetTree()
{
    //qDebug() << "NandBin::GetTree()";
    return root->clone();
}

bool NandBin::ExtractToDir( QTreeWidgetItem *item, const QString &path )
{
    if( !item )
        return false;
    bool ok = false;
    quint16 entry = item->text( 1 ).toInt( &ok );
    if( !ok )
    {
        emit SendError( tr( "Error converting entry(%1) to a number" ).arg( item->text( 1 ) ) );
        return false;
    }
    return ExtractFST( entry, path, true );//dont bother extracting this item's siblings

    return true;
}

QTreeWidgetItem *NandBin::CreateItem( QTreeWidgetItem *parent, const QString &name, quint32 size, quint16 entry, quint32 uid, quint32 gid, quint32 x3, quint8 attr, quint8 wtf)
{
    if( !parent )
        return NULL;

    QStringList text;

    QString enStr = QString( "%1" ).arg( entry );
    QString sizeStr = QString( "%1" ).arg( size, 0, 16 );
    QString uidStr = QString( "%1" ).arg( uid, 8, 16, QChar( '0' ) );
    QString gidStr = QString( "%1 (\"%2%3\")" ).arg( gid, 4, 16, QChar( '0' ) )
                     .arg( QChar( ascii( (char)( (gid >> 8) & 0xff ) ) ) )
                     .arg( QChar( ascii( (char)( (gid) & 0xff ) ) ) );
    QString x3Str = QString( "%1" ).arg( x3, 8, 16, QChar( '0' ) );
    QString wtfStr = QString( "%1" ).arg( wtf, 2, 16, QChar( '0' ) );
    QString attrStr = QString( "%1 " ).arg( ( attr & 3 ), 2, 16, QChar( '0' ) );

    quint8 m = attr;
    const char perm[ 3 ] = {'-','r','w'};
    for( quint8 i = 0; i < 3; i++ )
    {
        attrStr += perm[ ( m >> 6 ) & 1 ];
        attrStr += perm[ ( m >> 6 ) & 2 ];
        attrStr += ' ';
        m <<= 2;
    }
    attrStr += QString( "[%1]" ).arg( attr, 2, 16, QChar( '0' ) );

    text << name << enStr << sizeStr << uidStr << gidStr << x3Str << wtfStr << attrStr;
    //qDebug() << "adding" << name << en << size << uid << gid << x3 << mode << attr;
    QTreeWidgetItem *child = new QTreeWidgetItem( parent, text );
    child->setTextAlignment( 1, Qt::AlignRight | Qt::AlignVCenter );//align to the right
    child->setTextAlignment( 2, Qt::AlignRight | Qt::AlignVCenter );
    child->setTextAlignment( 3, Qt::AlignRight | Qt::AlignVCenter );
    child->setTextAlignment( 4, Qt::AlignRight | Qt::AlignVCenter );
    child->setTextAlignment( 5, Qt::AlignRight | Qt::AlignVCenter );
    child->setTextAlignment( 6, Qt::AlignRight | Qt::AlignVCenter );
    //child->setTextAlignment( 7, Qt::AlignRight | Qt::AlignVCenter );
    child->setFont( 7, QFont( "Courier New", 10, 5 ) );
    return child;
}

bool NandBin::AddChildren( QTreeWidgetItem *parent, quint16 entry )
{
    //qDebug() << "NandBin::AddChildren" << parent->text( 0 ) << hex << entry;
    if( entry >= 0x17ff )
    {
        qDebug() << "NandBin::AddChildren: entry >= 0x17ff";
        emit SendError( tr( "entry is above 0x17ff mmmmkay [ 0x%1 ]" ).arg( entry, 0, 16 ) );
        return false;
    }

    fst_t fst = GetFST( entry );

    if( !fst.filename[ 0 ] )//something is amiss, better quit now
    {
        qDebug() << "NandBin::AddChildren: !fst.filename[ 0 ]";
        return false;
    }

    if( fst.sib != 0xffff )
    {
        if( !AddChildren( parent, fst.sib ) )
            return false;
    }

    QTreeWidgetItem *child = CreateItem( parent, FstName( fst ), fst.size, entry, fst.uid, fst.gid, fst.x3, fst.attr, fst.wtf );

    //set some icons
    if( ( fst.attr & 3 ) == 1 )
    {
        //qDebug() << "is a file";
        child->setIcon( 0, keyIcon );
    }
    else
    {
        //qDebug() << "is a folder" << (fst.attr & 3);
        child->setIcon( 0, groupIcon );
        //try to add subfolder contents to the tree
        if( fst.sub != 0xffff && !AddChildren( child, fst.sub ) )
            return false;
    }

    return true;
}

const QString NandBin::FstName( fst_t fst )
{
    QByteArray ba( (char*)fst.filename, 0xc );
    QString ret = QString( ba );
    if( fatNames )
        ret.replace( ":", "-" );
    return ret;
}

bool NandBin::ExtractFST( quint16 entry, const QString &path, bool singleFile )
{
    //qDebug() << "NandBin::ExtractFST(" << hex << entry << "," << path << ")";
    fst_t fst = GetFST( entry );

    if( !fst.filename[ 0 ] )//something is amiss, better quit now
        return false;

    if( !singleFile && fst.sib != 0xffff && !ExtractFST( fst.sib, path ) )
        return false;

    switch( fst.attr & 3 )
    {
    case 2:
        if( !ExtractDir( fst, path ) )
            return false;
        break;
    case 1:
        if( !ExtractFile( fst, path ) )
            return false;
        break;
    default://wtf
        emit SendError( tr( "Unknown fst mode.  Bailing out" ) );
        return false;
        break;
    }
    return true;
}

bool NandBin::ExtractDir( fst_t fst, const QString &parent )
{
    //qDebug() << "NandBin::ExtractDir(" << parent << ")";
    QByteArray ba( (char*)fst.filename, 0xc );
    QString filename( ba );

    QFileInfo fi( parent );
    if( filename != "/" )
    {
        fi.setFile( parent + "/" + filename );
        if( !fi.exists() && !QDir().mkpath( fi.absoluteFilePath() ) )
        {
            emit SendError( tr( "Can\'t create directory \"%1\"" ).arg( fi.absoluteFilePath() ) );
            return false;
        }
    }

    if( fst.sub != 0xffff && !ExtractFST( fst.sub, fi.absoluteFilePath() ) )
        return false;
    return true;
}

bool NandBin::ExtractFile( fst_t fst, const QString &parent )
{
    QByteArray ba( (char*)fst.filename, 0xc );
    QString filename( ba );
    QFileInfo fi( parent + "/" + filename );
    qDebug() << "extract" << fi.absoluteFilePath();
    emit SendText( tr( "Extracting \"%1\"" ).arg( fi.absoluteFilePath() ) );

    QByteArray data = GetFile( fst );
	if( fst.size && !data.size() )//dont worry if files dont have anything in them anyways
		//return true;
	return false;

    if( !WriteFile( fi.absoluteFilePath(), data ) )
    {
        emit SendError( tr( "Error writing \"%1\"" ).arg( fi.absoluteFilePath() ) );
        return false;
    }
    return true;
}

bool NandBin::InitNand( const QIcon &dirs, const QIcon &files )
{
    fstInited = false;
    memset( (void*)&fsts, 0, sizeof( fst_t ) * 0x17ff );
    fats.clear();
    type = GetDumpType( f.size() );
    if( type < 0 || type > 3 )
        return false;

    //qDebug() << "dump type:" << type;
    if( !GetKey( type ) )
        return false;

    loc_super = FindSuperblock();
    if( loc_super < 0 )
        return false;

    quint32 n_fatlen[] = { 0x010000, 0x010800, 0x010800 };
    loc_fat = loc_super;
    loc_fst = loc_fat + 0x0C + n_fatlen[ type ];

    //cache all the entries
    for( quint16 i = 0; i < 0x17ff; i++ )
        fsts[ i ] = GetFST( i );

    //cache all the fats
    for( quint16 i = 0; i < 0x8000; i++ )
        fats << GetFAT( i );

    fstInited = true;

    if( root )
        delete root;

    groupIcon = dirs;
    keyIcon = files;

    root = new QTreeWidgetItem( QStringList() << nandPath );
	AddChildren( root, 0 );

    //checkout the blocks for boot1&2
    QList<QByteArray>blocks;
    for( quint16 i = 0; i < 8; i++ )
    {
        QByteArray block;
        for( quint16 j = 0; j < 8; j++ )
        {
            block += GetCluster( ( i * 8 ) + j, false );
        }
        if( block.size() != 0x4000 * 8 )
        {
            qDebug() << "wrong block size" << i;
            return false;
        }
        blocks << block;
    }

    if( !bootBlocks.SetBlocks( blocks ) )
        return false;

    //ShowInfo();
    return true;
}

void NandBin::ShowLostClusters()
{
    QList<quint16> u = GetFatsForEntry( 0 );
    quint16 ss = fats.size();
    qDebug() << "total used clusters" << u.size() << "of" << ss << "total";
    quint16 lost = 0;
    QList<quint16> ffs;
    QList<quint16> frs;
    for( quint16 i = 0; i < ss; i++ )
    {
        if( u.contains( fats.at( i ) ) )//this cluster is really used
            continue;
        switch( fats.at( i ) )
        {
        case 0xFFFB:
        case 0xFFFC:
        case 0xFFFD:
            break;
        case 0xFFFE:
            frs << i;
            break;
        case 0xFFFF:
            ffs << i;
            break;
        default:
            lost++;
            qDebug() << hex << i << fats.at( i );
            break;
        }
    }
    qDebug() << "found" << lost << "lost clusters\nUNK ( 0xffff )" << hex << ffs.size() << ffs <<
            "\nfree           " << frs.size();
}

int NandBin::GetDumpType( quint64 fileSize )
{
    quint64 sizes[] = { 536870912,    // type 0 | 536870912 == no ecc
                        553648128,    // type 1 | 553648128 == ecc
                        553649152 };  // type 2 | 553649152 == old bootmii
    for( int i = 0; i < 3; i++ )
    {
        if( sizes[ i ] == fileSize )
            return i;
    }
    emit SendError( tr( "Can't tell what type of nand dump this is" ) );
    return -1;
}

const QList<Boot2Info> NandBin::Boot2Infos()
{
    if( !bootBlocks.IsOk() )
        return QList<Boot2Info>();

    return bootBlocks.Boot2Infos();
}

quint8 NandBin::Boot1Version()
{
    if( !bootBlocks.IsOk() )
        return 0;

    return bootBlocks.Boot1Version();
}

bool NandBin::GetKey( int type )
{
    QByteArray hmacKey;
    switch( type )
    {
    case 0:
    case 1:
        {
            QString keyPath = nandPath;
            int sl = keyPath.lastIndexOf( "/" );
            if( sl == -1 )
            {
                emit SendError( tr( "Error getting path of keys.bin" ) );
                return false;
            }
            keyPath.resize( sl + 1 );
            keyPath += "keys.bin";

            key = ReadKeyfile( keyPath, 0 );
            if( key.isEmpty() )
                return false;
            hmacKey = ReadKeyfile( keyPath, 1 );
            if( hmacKey.isEmpty() )
                return false;
        }
        break;
    case 2:
        {
            if( !f.isOpen() )
            {
                emit SendError( tr( "Tried to read keys from unopened file" ) );
                return false;
            }
            f.seek( 0x21000144 );
            hmacKey = f.read( 20 );

            f.seek( 0x21000158 );
            key = f.read( 16 );
        }
        break;
    default:
        emit SendError( tr( "Tried to read keys for unknown dump type" ) );
        return false;
        break;
    }
    spare.SetHMacKey( hmacKey );//set the hmac key for calculating spare data
    //hexdump( hmacKey );
    return true;
}

const QByteArray NandBin::ReadKeyfile( const QString &path, quint8 type )
{
    QByteArray retval;
    QFile f( path );
    if( !f.exists() || !f.open( QIODevice::ReadOnly ) )
    {
        emit SendError( tr( "Can't open %1!" ).arg( path ) );
        return retval;
    }
    if( f.size() < 0x400 )
    {
        f.close();
        emit SendError( tr( "keys.bin is too small!" ) );
        return retval;
    }
    if( type == 0 )
    {
        f.seek( 0x158 );
        retval = f.read( 16 );
    }
    else if( type == 1 )
    {
        f.seek( 0x144 );
        retval = f.read( 20 );
    }
    f.close();

    return retval;
}

qint32 NandBin::FindSuperblock()
{
    if( type < 0 || type > 3 )
    {
        emit SendError( tr( "Tried to get superblock of unknown dump type" ) );
        return -1;
    }
    if( !f.isOpen() )
    {
        emit SendError( tr( "Tried to get superblock of unopened dump" ) );
        return -1;
    }
    quint32 loc = 0, current = 0, magic = 0;
    superClusterVersion = 0;
    quint32 n_start[] = { 0x1FC00000, 0x20BE0000, 0x20BE0000 },
    n_end[] = { 0x20000000, 0x21000000, 0x21000000 },
    n_len[] = { 0x40000, 0x42000, 0x42000 };

    quint8 rewind = 1;
    for( loc = n_start[ type ], currentSuperCluster = 0x7f00; loc < n_end[ type ]; loc += n_len[ type ], currentSuperCluster += 0x10 )
    {
        f.seek( loc );
        //QByteArray sss = f.peek( 0x50 );
        f.read( (char*)&magic, 4 );//no need to switch endian here
        if( magic != 0x53464653 )
        {
            qWarning() << "oops, this isnt a supercluster" << hex << loc << magic << currentSuperCluster;
            rewind++;
            //hexdump( sss );
            continue;
        }

        f.read( (char*)&current, 4 );
        current = qFromBigEndian( current );

		//qDebug() << "superblock" << hex << current << currentSuperCluster << loc;

        if( current > superClusterVersion )
            superClusterVersion = current;
        else
        {
            //qDebug() << "using superblock" << hex << superClusterVersion << currentSuperCluster - 0x10 << f.pos() - n_len[ type ];
			//currentSuperCluster -= ( 0x10 * rewind );
			//loc -= ( n_len[ type ] * rewind );
			rewind = 1;
            break;
        }
		if( loc == n_end[ type ] )
		{
			rewind = 1;
		}
    }
    if( !superClusterVersion )
        return -1;

	currentSuperCluster -= ( 0x10 * rewind );
	loc -= ( n_len[ type ] * rewind );

	//qDebug() << "using superblock" << hex << superClusterVersion << currentSuperCluster << "page:" << ( loc / 0x840 );
    return loc;
}

fst_t NandBin::GetFST( quint16 entry )
{
	//qDebug() << "NandBin::GetFST(" << hex << entry << ")";
    fst_t fst;
    if( entry >= 0x17FF )
    {
        emit SendError( tr( "Tried to get entry above 0x17fe [ 0x%1 ]" ).arg( entry, 0, 16 ) );
        fst.filename[ 0 ] = '\0';
        return fst;
    }
    if( fstInited )//we've already read this once, just give back the one we already know
    {
        //qDebug() << "reading from cache" << hex << entry;
        return fsts[ entry ];
    }
    // compensate for 64 bytes of ecc data every 64 fst entries
    quint32 n_fst[] = { 0, 2, 2 };
    int loc_entry = ( ( ( entry / 0x40 ) * n_fst[ type ] ) + entry ) * 0x20;
    if( (quint32)f.size() < loc_fst + loc_entry + sizeof( fst_t ) )
    {
		qDebug() << hex << (quint32)f.size() << loc_fst << loc_entry << type << n_fst[ type ];
        emit SendError( tr( "Tried to read fst_t beyond size of nand.bin" ) );
        fst.filename[ 0 ] = '\0';
        return fst;
    }
    f.seek( loc_fst + loc_entry );

    f.read( (char*)&fst.filename, 0xc );
    f.read( (char*)&fst.attr, 1 );
    f.read( (char*)&fst.wtf, 1 );
    f.read( (char*)&fst.sub, 2 );
    f.read( (char*)&fst.sib, 2 );
    if( type && ( entry + 1 ) % 64 == 0 )//bug in other nand.bin extracterizers.  the entry for every 64th fst item is inturrupeted by some spare shit
    {
        f.read( (char*)&fst.size, 2 );
        f.seek( f.pos() + 0x40 );
        f.read( (char*)(&fst.size) + 2, 2 );
    }
    else
        f.read( (char*)&fst.size, 4 );
    f.read( (char*)&fst.uid, 4 );
    f.read( (char*)&fst.gid, 2 );
    f.read( (char*)&fst.x3, 4 );

    fst.sub = qFromBigEndian( fst.sub );
    fst.sib = qFromBigEndian( fst.sib );
    fst.size = qFromBigEndian( fst.size );
    fst.uid = qFromBigEndian( fst.uid );
    fst.gid = qFromBigEndian( fst.gid );
    fst.x3 = qFromBigEndian( fst.x3 );
    fst.fst_pos = entry;

    //fst.mode &= 1;
    return fst;
}

quint16 NandBin::GetFAT( quint16 fat_entry )
{
    if( fstInited )
        return fats.at( fat_entry );

    fat_entry += 6;

    // location in fat of cluster chain
    quint32 n_fat[] = { 0, 0x20, 0x20 };
    int loc = loc_fat + ((((fat_entry / 0x400) * n_fat[type]) + fat_entry) * 2);

    if( (quint32)f.size() < loc + sizeof( quint16 ) )
    {
        emit SendError( tr( "Tried to read FAT entry beyond size of nand.bin" ) );
        return 0;
    }
    f.seek( loc );

    quint16 ret;
    f.read( (char*)&ret, 2 );
    ret = qFromBigEndian( ret );
    return ret;
}

const QByteArray NandBin::GetPage( quint32 pageNo, bool withEcc )
{
    //qDebug() << "NandBin::GetPage( " << hex << pageNo << ", " << withEcc << " )";
    quint32 n_pagelen[] = { 0x800, 0x840, 0x840 };

    if( f.size() < ( pageNo + 1 ) * n_pagelen[ type ] )
    {
        emit SendError( tr( "Tried to read page past size of nand.bin" ) );
        return QByteArray();
    }
    f.seek( pageNo * n_pagelen[ type ] );	//seek to the beginning of the page to read
    //qDebug() << "reading page from" << hex << (quint32)f.pos();
    QByteArray page = f.read( ( type && withEcc ) ? n_pagelen[ type ] : 0x800 );
    return page;
}

const QByteArray NandBin::GetCluster( quint16 cluster_entry, bool decrypt )
{
    //qDebug() << "NandBin::GetCluster" << hex << cluster_entry;
    quint32 n_clusterlen[] = { 0x4000, 0x4200, 0x4200 };
    quint32 n_pagelen[] = { 0x800, 0x840, 0x840 };

    if( f.size() < ( cluster_entry * n_clusterlen[ type ] ) + ( 8 * n_pagelen[ type ] ) )
    {
        emit SendError( tr( "Tried to read cluster past size of nand.bin" ) );
        return QByteArray();
    }

    QByteArray cluster;

    for( int i = 0; i < 8; i++ )
    {
        f.seek( ( cluster_entry * n_clusterlen[ type ] ) + ( i * n_pagelen[ type ] ) );	//seek to the beginning of the page to read
        //qDebug() << "reading page from" << hex << (quint32)f.pos();
        //QByteArray page = f.read( n_pagelen[ type ] );					//read the page, with ecc
        QByteArray page = f.read( 0x800 );						//read the page, skip the ecc
        //hexdump( page.mid( 0x800, 0x40 ) );//just here for debugging purposes

        //cluster += page.left( 0x800 );
        cluster += page;
    }
    if( cluster.size() != 0x4000 )
    {
        qDebug() << "actual cluster size" << hex << cluster.size();
        emit SendError( tr( "Error reading cluster" ) );
        return QByteArray();
    }
    if( !decrypt )
        return cluster;

    //really redundant to do this for ever AES decryption, but the AES code only lets
    //1 key set at a time and it may be changed if some other object is decrypting something else
    AesSetKey( key );

    QByteArray ret = AesDecrypt( 0, cluster );
    return ret;
}

const QByteArray NandBin::GetFile( quint16 entry )
{
    fst_t fst = GetFST( entry );
    if( !fst.filename[ 0 ] )//something is amiss, better quit now
        return QByteArray();
    return GetFile( fst );
}

const QByteArray NandBin::GetFile( fst_t fst_ )
{
    //qDebug() << "NandBin::GetFile" << QByteArray( (const char*)fst_.filename, 12 );
    if( !fst_.size )
        return QByteArray();

    quint16 fat = fst_.sub;
    //int cluster_span = (int)( fst.size / 0x4000) + 1;

    QByteArray data;

    //QList<quint16> readFats;
    //int idx = 0;a
    for (int i = 0; fat < 0xFFF0; i++)
    {
        QByteArray cluster = GetCluster( fat );
        if( cluster.size() != 0x4000 )
            return QByteArray();

        //debug shit...  am i creating correct hmac data?
        //WriteDecryptedCluster( 0, cluster, fst_, idx++ );

        data += cluster;
        //readFats << fat;
        fat = GetFAT( fat );
    }
    //qDebug() << "actually read data from fats\n" << hex << readFats;
    //qDebug() << "stopped reading because of" << hex << fat;

    //this check doesnt really seem to matter, it always appears to be 1 extra cluster added to the end
    //of the file and that extra bit is dropped in this function before the data is returned.
    /*if( data.size() != cluster_span * 0x4000 )
    {
	qDebug() << "data.size() != cluster_span * 0x4000 :: "
		<< hex << data.size()
		<< cluster_span
		<< ( cluster_span * 0x4000 )
		<< "expected size:" << hex << fst.size;

	emit SendError( tr( "Error reading file [ block size is not a as expected ] %1" ).arg( FstName( fst ) ) );
    }*/
    if( (quint32)data.size() < fst_.size )
    {
        qDebug() << "(quint32)data.size() < fst.size :: "
                << hex << data.size()
                << "expected size:" << hex << fst_.size;

        emit SendError( tr( "Error reading file [ returned data size is less that the size in the fst ]" ) );
        return QByteArray();
    }

    if( (quint32)data.size() > fst_.size )
        data.resize( fst_.size );//dont need to give back all the data, only up to the expected size

    return data;
}

const QList<quint16> NandBin::GetFatsForFile( quint16 i )
{
    //qDebug() << "NandBin::GetFatsForFile" << i;
    QList<quint16> ret;
    fst_t fst = GetFST( i );

    if( fst.filename[ 0 ] == '\0' )
        return ret;

    quint16 fat = fst.sub;
    //qDebug() << hex << fat;
    quint16 j = 0;//just to make sure a broken nand doesnt lead to an endless loop
    while ( fat < 0x8000 && fat > 0 && ++j )
    {
        ret << fat;
        fat = GetFAT( fat );

        //qDebug() << hex << fat;
    }
    //qDebug() << hex << ret;

    return ret;
}

const QList<quint16> NandBin::GetFatsForEntry( quint16 i )
{
    //qDebug() << "NandBin::GetFatsForEntry" << i;
    fst_t fst = GetFST( i );

    QList<quint16> ret;
    if( fst.sib != 0xffff )
    {
        ret.append( GetFatsForEntry( fst.sib ) );
    }

    if( ( fst.attr & 3 ) == 1 )
    {
        ret.append( GetFatsForFile( i ) );
    }
    else
    {
        if( fst.sub != 0xffff )
            ret.append( GetFatsForEntry( fst.sub ) );
    }

    return ret;
}

void NandBin::SetFixNamesForFAT( bool fix )
{
    fatNames = fix;
}

const QByteArray NandBin::GetData( const QString &path )
{
    QTreeWidgetItem *item = ItemFromPath( path );
    if( !item )
        return QByteArray();

    if( !item->text( 7 ).startsWith( "01" ) )
    {
        qDebug() << "NandBin::GetData -> can't get data for a folder" << item->text( 0 );
        return QByteArray();
    }

    bool ok = false;
    quint16 entry = item->text( 1 ).toInt( &ok, 10 );
    if( !ok )
        return QByteArray();

    return GetFile( entry );
}

QTreeWidgetItem *NandBin::ItemFromPath( const QString &path )
{
    if( !root || !root->childCount() )
        return NULL;

    QTreeWidgetItem *item = root->child( 0 );
    if( item->text( 0 ) != "/" )
    {
        qWarning() << "NandBin::ItemFromPath -> root is not \"/\"" << item->text( 0 );
        return NULL;
    }
    if( !path.startsWith( "/" ) || path.contains( "//" ))
    {
        qWarning() << "NandBin::ItemFromPath -> invalid path";
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
            qWarning() << "NandBin::ItemFromPath ->item not found" << path;
            return NULL;
        }
        slash = nextSlash + 1;
    }
    return item;
}

QTreeWidgetItem *NandBin::GetParent( const QString &path )
{
    if( !path.startsWith( "/" ) || !root || !root->childCount() )//invalid entry
        return NULL;
    if( path.count( "/" ) < 2 )//this will be an entry in the root
        return root->child( 0 );

    QString parent = path;
    if( parent.endsWith( "/" ) )
        parent.resize( parent.size() - 1 );
    int sl = parent.lastIndexOf( "/" );
    parent.resize( sl );

    return ItemFromPath( parent );
}

QTreeWidgetItem *NandBin::FindItem( const QString &s, QTreeWidgetItem *parent )
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

void NandBin::ShowInfo()
{
    quint16 badBlocks = 0;
    quint16 reserved = 0;
    quint16 freeBlocks = 0;
    quint16 used = 0;
    QList<quint16>badOnes;
    for( quint16 i = 0; i < 0x8000; i++ )
    {
        quint16 fat = GetFAT( i );
        if( 0xfffc == fat )
            reserved++;
        else if( 0xfffd == fat )
        {
            badBlocks++;
            if( i % 8 == 0 )
            {
                badOnes << ( i / 8 );
            }
        }
        else if( 0xfffe == fat )
            freeBlocks++;
        else
            used ++;
    }
    if( badBlocks )
        badBlocks /= 8;

    if( reserved )
        reserved /= 8;

    if( freeBlocks )
        freeBlocks /= 8;

    qDebug() << "free blocks:" << freeBlocks
            << "\nbadBlocks:" << badBlocks << badOnes
            << "\nreserved :" << reserved
            << "\nused      :" << used;
}

QTreeWidgetItem *NandBin::ItemFromEntry( quint16 i, QTreeWidgetItem *parent )
{
    return ItemFromEntry( QString( "%1" ).arg( i ), parent );
}

QTreeWidgetItem *NandBin::ItemFromEntry( const QString &i, QTreeWidgetItem *parent )
{
    if( !parent )
        return NULL;
    //qDebug() << "NandBin::ItemFromEntry" << i << parent->text( 0 );


    quint32 cnt = parent->childCount();
    for( quint32 j = 0; j < cnt; j++ )
    {
        QTreeWidgetItem *child = parent->child( j );
        if( child->text( 1 ) == i )
            return child;

        //qDebug() << child->text( 2 ) << i;

        QTreeWidgetItem *r = ItemFromEntry( i, child );
        if( r )
            return r;
    }
    return NULL;
}

bool NandBin::WriteCluster( quint32 pageNo, const QByteArray &data, const QByteArray &hmac )
{
    if( data.size() != 0x4000 )
    {
        qWarning() << "NandBin::WriteCluster -> size:" << hex << data.size();
        return false;
    }

    for( int i = 0; i < 8; i++ )
    {
        QByteArray spareData( 0x40, '\0' );
        quint8* sp = (quint8*)spareData.data();
        QByteArray ecc = spare.CalcEcc( data.mid( i * 0x800, 0x800 ) );
        memcpy( sp + 0x30, ecc.data(), 0x10 );
        sp[ 0 ] = 0xff; // good block
        if( !hmac.isEmpty() )
        {
            if( i == 6 )
            {
                memcpy( (char*)sp + 1, hmac.data(), 20 );
                memcpy( (char*)sp + 21, hmac.data(), 12 );
            }
            else if( i == 7 )
            {
                memcpy( (char*)sp + 1, (char*)(hmac.data()) + 12, 8 );
            }
        }
        if( !WritePage( pageNo + i, data.mid( i * 0x800, 0x800 ) + spareData ) )
            return false;
    }
    return true;
}

bool NandBin::WriteDecryptedCluster( quint32 pageNo, const QByteArray &data, fst_t fst, quint16 idx )
{
    //qDebug() << "NandBin::WriteDecryptedCluster";
    QByteArray hmac = spare.Get_hmac_data( data, fst.uid, (const unsigned char *)&fst.filename, fst.fst_pos, fst.x3, idx );

    //hexdump( hmac );
    //return true;

    AesSetKey( key );
    QByteArray encData = AesEncrypt( 0, data );
    return WriteCluster( pageNo, encData, hmac );
}

bool NandBin::WritePage( quint32 pageNo, const QByteArray &data )
{
#ifndef NAND_BIN_CAN_WRITE
    qWarning() << __FILE__ << "was built without write support";
    return false;
#endif
	//qDebug() << "NandBin::WritePage(" << hex << pageNo << ")";
    quint32 n_pagelen[] = { 0x800, 0x840, 0x840 };
    if( (quint32)data.size() != n_pagelen[ type ] )
    {
        qWarning() << "data is wrong size" << hex << data.size();
        return false;
    }

    if( f.size() < ( pageNo + 1 ) * n_pagelen[ type ] )
    {
        emit SendError( tr( "Tried to write page past size of nand.bin" ) );
        return false;
    }
    f.seek( (quint64)pageNo * (quint64)n_pagelen[ type ] );	//seek to the beginning of the page to write
    //qDebug() << "writing page at:" << f.pos() << hex << (quint32)f.pos();
    //hexdump(  data, 0, 0x20 );
	return ( f.write( data ) == data.size() );
}

quint16 NandBin::CreateNode( const QString &name, quint32 uid, quint16 gid, quint8 attr, quint8 user_perm, quint8 group_perm, quint8 other_perm )
{
    attr = attr | ( ( user_perm & 3 ) << 6 ) | ( ( group_perm & 3 ) << 4 ) | ( ( other_perm & 3 ) << 2 );

    quint32 i;
    //qDebug() << "looking for first empty node";
    for( i = 1; i < 0x17ff; i++ )//cant be entry 0 because that is the root
    {
        //qDebug() << hex << i << FstName( fsts[ i ] );
        if( !fsts[ i ].filename[ 0 ] )//this one doesnt have a filename, it cant be used already
            break;
    }
    if( i == 0x17ff )
    {
        return 0;
    }

    QByteArray n = name.toLatin1().data();
    n.resize( 12 );
    //qDebug() << "will add entry for" << n << "at" << hex << i;
    memcpy( &fsts[ i ].filename, n, 12 );
    fsts[ i ].attr = attr;
    fsts[ i ].wtf = 0;
    if( ( attr & 3 ) == 2 )
    {
        fsts[ i ].sub = 0xffff;
    }
    else
    {
        fsts[ i ].sub = 0xfffb;
    }
    fsts[ i ].sib = 0xffff;
    fsts[ i ].size = 0;
    fsts[ i ].uid = uid;
    fsts[ i ].gid = gid;
    fsts[ i ].x3 = 0;
    //hexdump( (const void*)&fsts[ i ], sizeof( fst_t ) );

    return i;
}

quint16 NandBin::CreateEntry( const QString &path, quint32 uid, quint16 gid, quint8 attr, quint8 user_perm, quint8 group_perm, quint8 other_perm )
{
    //qDebug() << "NandBin::CreateEntry" << path;

    quint16 ret = 0;
    QTreeWidgetItem *par = GetParent( path );
    if( !par )
    {
        qDebug() << "NandBin::CreateEntry -> parent doesnt exist for" << path;
        return ret;
    }

    QString name = path;
    name.remove( 0, name.lastIndexOf( "/" ) + 1 );
    if( name.size() > 12 )
        return 0;
    //QTreeWidgetItem *cur = NULL;
    quint32 cnt = par->childCount();
    for( quint32 i = 0; i < cnt; i++ )
    {
        if( par->child( i )->text( 0 ) == name )
        {
            qDebug() << "NandBin::CreateEntry ->" << path << "already exists";
            return ret;
        }
    }
    bool ok = false;
    quint16 parIdx = par->text( 1 ).toInt( &ok );
    if( !ok || parIdx > 0x17fe )
        return 0;//wtf

    //fst_t parFst = fsts[ parIdx ];
    if( fsts[ parIdx ].sub == 0xffff )//directory has no entries yet
    {
        ret = CreateNode( name, uid, gid, attr, user_perm, group_perm, other_perm );
        if( !ret )
            return 0;
        fsts[ parIdx ].sub = ret;
    }
    else//find the last entry in this directory
    {
        quint16 entryNo = 0;
        for( quint32 i = cnt; i > 0; i-- )
        {
            entryNo = par->child( i - 1 )->text( 1 ).toInt( &ok );
            if( !ok || entryNo > 0x17fe )
                return 0;//wtf

            if( fsts[ entryNo ].sib == 0xffff )
                break;

            if( i == 1 )//wtf
                return 0;
        }
        if( !entryNo )//something is busted, none of the child entries is marked as the last one
            return 0;

        ret = CreateNode( name, uid, gid, attr, user_perm, group_perm, other_perm );
        if( !ret )
            return 0;

		//method 1: this works, and the nand is bootable. but doesnt mimic the IOS FS driver. ( my entries appear in reversed order when walking the FS )
		//fsts[ entryNo ].sib = ret;

		//method 2: trying to mimic the IOS FS driver ( insert new entries at the start of the chain, instead of the end )
		fsts[ ret ].sib = fsts[ parIdx ].sub;
		fsts[ parIdx ].sub = ret;
    }
    QTreeWidgetItem *child = CreateItem( par, name, 0, ret, uid, gid, 0, fsts[ ret ].attr, 0 );
    if( attr == NAND_FILE )
    {
        child->setIcon( 0, keyIcon );
    }
    else
    {
        child->setIcon( 0, groupIcon );
    }
    return ret;
}

bool NandBin::Delete( const QString &path )
{
    QTreeWidgetItem *i = ItemFromPath( path );
    return DeleteItem( i );
}

bool NandBin::DeleteItem( QTreeWidgetItem *item )
{
    if( !item )
    {
        qWarning() << "cant delete a null item";
        return false;
    }

    qDebug() << "NandBin::DeleteItem" << item->text( 0 );
    bool ok = false;
    quint16 idx = item->text( 1 ).toInt( &ok );//get the index of the entry to remove
    if( !ok || idx > 0x17fe )
    {
        qWarning() << "wtf1";
        return false;//wtf
    }

    //find the entry that is this one's previous sibling
    quint16 pId = 0xffff;//this is the index of the item in relation to its parent
    quint16 prevSib = 0;
    QTreeWidgetItem *par = item->parent();
    if( !par )
    {
        qWarning() << "wtf2";
        return false;//trying to delete the root item
    }
    quint16 parIdx = par->text( 1 ).toInt( &ok );
    if( !ok || parIdx > 0x17fe )
    {
        qWarning() << "wtf1a";
        return false;//wtf
    }
    if( fsts[ parIdx ].sub == idx )		//this is the first item in the folder, point the parent to this items first sibling
    {
        fsts[ parIdx ].sub = fsts[ idx ].sib;
        quint16 cnt = par->childCount();
        for( quint16 i = 0; i < cnt; i++ )
        {
            if( par->child( i )->text( 0 ) == item->text( 0 ) )
            {
                pId = i;
                //qDebug() << "found the item";
                break;
            }
            if( i == cnt - 1 )//not found
            {
                qWarning() << "wtf 15" << pId << i << cnt;
                return false;
            }
        }
    }

    else					//point the previous entry to the next one
    {

        quint16 cnt = par->childCount();
        for( quint16 i = 0; i < cnt; i++ )
        {
            //qDebug() << i << par->child( i )->text( 0 ) << pId << prevSib;
            quint16 r = par->child( i )->text( 1 ).toInt( &ok );
            if( !ok || r > 0x17fe )
            {
                qWarning() << "wtf3";
                return false;//wtf
            }

            if( fsts[ r ].sib == idx )//this is the one we want
                prevSib = r;

            if( par->child( i )->text( 0 ) == item->text( 0 ) )
            {
                pId = i;
                //qDebug() << "found the item";
            }

            if( pId != 0xffff && prevSib )
                break;

            if( i == cnt - 1 )//not found
            {
                qWarning() << "wtf4" << pId << prevSib;
                return false;
            }
        }
        fsts[ prevSib ].sib = fsts[ idx ].sib;
    }

    switch( fsts[ idx ].attr & 3 )
    {
    case 1:
        {
            //int q = 0;
            qDebug() << "deleting clusters of" << item->text( 0 ) << idx;
            QList<quint16> toFree = GetFatsForFile( idx );
            foreach( quint16 cl, toFree )
            {
                fats.replace( cl, 0xfffe );
            }
            qDebug() << "delete loop done.  freed" << toFree.size() << "clusters";
        }
        break;
    case 2:
        {
            qDebug() << "deleting children of" << item->text( 0 );
            quint32 cnt = item->childCount();//delete all the children of this item
            qDebug() << cnt << "childern";
            for( quint32 i = cnt; i > 0; i-- )
            {
                if( !DeleteItem( item->child( i - 1 ) ) )
                {
                    qWarning() << "wtf6";
                    return false;
                }
            }
        }
        break;

    }
    memset( &fsts[ idx ], 0, sizeof( fst_t ) );	    //clear this entry
    fsts[ idx ].fst_pos = idx;				    //reset this
    QTreeWidgetItem *d = par->takeChild( pId );
    qDebug() << "deleting tree item" << d->text( 0 );
    delete d;
    return true;
}

bool NandBin::SetData( const QString &path, const QByteArray &data )
{
    QTreeWidgetItem *i = ItemFromPath( path );
    if( !i )
    {
        qDebug() << "!item" << path;
        return false;
    }

    bool ok = false;
    quint16 idx = i->text( 1 ).toInt( &ok );//find the entry
    if( !ok || idx > 0x17fe )
    {
        qDebug() << "out of range" << path;
        return false;
    }

    return SetData( idx, data );
}

bool NandBin::SetData( quint16 idx, const QByteArray &data )
{
    fst_t fst = fsts[ idx ];
    qDebug() << "NandBin::SetData" << FstName( fst );
    if( ( fst.attr & 3 ) != 1 )
    {
        qDebug() << idx << "is a folder";
        return false;
    }

    QList<quint16> fts = GetFatsForFile( idx );	//get the currently used fats and overwrite them.  this doesnt serve much purpose, but it seems cleaner
    QByteArray pData = PaddedByteArray( data, 0x4000 );//actual data that must be written to the nand
    quint32 size = pData.size();		//the actual space used in the nand for this file
    quint16 clCnt = size / 0x4000;		//the number of clusters needed to hold this file

    //if this new data will take more clusters than the old data, create the new ones
    if( clCnt > fts.size() )
    {
        //list all the free clusters
        QList<quint16>freeClusters;
        for( quint16 i = 0; i < 0x8000; i++ )
        {
            if( fats.at( i ) == 0xfffe )
                freeClusters << i;
        }
        if( freeClusters.size() < clCnt - fts.size() )
        {
            qWarning() << "not enough free clusters to write the file";
            return false;
        }

        //setup random number stuff to emulate wear leveling
        QTime midnight( 0, 0, 0 );
        qsrand( midnight.secsTo( QTime::currentTime() ) );

        //now grab the clusters that will be used from the list
        //qDebug() << "trying to find" << ( clCnt - fts.size() ) << "free clusters";
        while( fts.size() < clCnt )
        {
            if( !freeClusters.size() )//avoid endless loop in case there are some clusters that should be free, but the spare data says theyre bad
                return false;

            //grab a random cluster from the list
            quint16 idx = qrand() % freeClusters.size();
			quint16 cl = freeClusters.takeAt( idx );	//remove this number from the list

            fts << cl;					//add this one to the clusters that will be used to hold the data
			quint16 block = cl / 8;			//try to find other clusters in the same block
			//for( quint16 i = block * 8; i < ( ( block + 1 ) * 8 ) && fts.size() < clCnt; i++ )// <- this one scatters files all over the place
			quint16 max = freeClusters.at( freeClusters.size() - 1 );							// <- this one keeps files together; appears to closer mimic IOS's behavior
			for( quint16 i = block * 8; i < max && fts.size() < clCnt; i++ )
            {
                if( cl == i )				//this one is already added to the list
                    continue;

                if( fats.at( i ) == 0xfffe )		//theres more free clusters in this same block, grab them
                {
                    fts << i;
					freeClusters.removeAt( freeClusters.indexOf( i, 0 ) );
                }
            }
            //read the spare data to see that the cluster is good - removed for now.  but its probably not a bad idea to do this
            /*if( type )//if the dump doesnt have spare data, dont try to read it, just assume the cluster is good
	    {
		QByteArray page = GetPage( cl * 8, true );
		if( page.isEmpty() )
		    continue;

		QByteArray spr = page.right( 0x40 );
		if( !spr.startsWith( 0xff ) )
		{
		    qWarning() << "page" << hex << ( cl * 8 ) << "is bad??";
		    continue;
		}
	    }*/

        }
    }
	//sort clusters so file is written in order ( not like it matters on flash memory, though )
	qSort( fts.begin(), fts.end() );
    //qDebug() << "about to writing shit" << clCnt << fts.size();
    //qDebug() << "file will be on clusters\n" << hex << fts;
    for( quint32 i = 0; i < clCnt; i++ )
    {
        QByteArray cluster = pData.mid( i * 0x4000, 0x4000 );
        if( !WriteDecryptedCluster( ( fts.at( i ) * 8 ), cluster, fst, i ) )
            return false;
    }
    //qDebug() << "done writing shit, fix the fats now" << clCnt << fts.size();
    //all the data has been written, now make sure the fats are correct
	fsts[ idx ].sub = fts.at( 0 );

    for( quint16 i = 0; i < clCnt - 1; i++ )
    {
		fats.replace( fts.at( 0 ), fts.at( 1 ) );
		fts.takeFirst();
	}

	//qDebug() << "1 followed the chain to" << num << "items.  expected" << clCnt;
    fats.replace( fts.at( 0 ), 0xfffb );//last cluster in chain
    fts.takeFirst();
    //qDebug() << "fixed the last one" << hex << fts;
	// if the new data uses less clusters than the previous data, mark the extra ones as free
    while( !fts.isEmpty() )
    {
        fats.replace( fts.at( 0 ), 0xfffe );
        fts.takeFirst();
	}

    fsts[ idx ].size = data.size();

    QTreeWidgetItem *i = ItemFromEntry( idx, root );
    if( !i )
    {
        qDebug() << "!ItemFromEntry";
        return false;
    }

	i->setText( 2, QString( "%1" ).arg( data.size(), 0, 16 ) );
    return true;
}

bool NandBin::WriteMetaData()
{
    //make sure the currect cluster is sane
    if( currentSuperCluster < 0x7f00 || currentSuperCluster > 0x7ff0 || currentSuperCluster % 16 || fats.size() != 0x8000 )
        return false;
    quint16 nextSuperCluster = currentSuperCluster + 0x10;
    if( nextSuperCluster > 0x7ff0 )
        nextSuperCluster = 0x7f00;

    quint32 nextClusterVersion = superClusterVersion + 1;
    QByteArray scl( 0x4000 * 16, '\0' );		    //this will hold all the data
    //qDebug() << "created the meta block buffer" << hex << scl.size();
    QBuffer b( &scl );
    b.open( QIODevice::WriteOnly );
    quint32 tmp;
    quint16 t;

    b.write( "SFFS" );				    //magic word
    tmp = qFromBigEndian( nextClusterVersion );
    b.write( (const char*)&tmp, 4 );		    //version
    tmp = qFromBigEndian( (quint32)0x10 );
    b.write( (const char*)&tmp, 4 );		    //wiibrew says its always 0x10.  but mine is 0
    //qDebug() << "writing the fats at" << hex << (quint32)b.pos();

    //write all the fats
    for( quint16 i = 0; i < 0x8000; i++ )
    {
        t = qFromBigEndian( fats.at( i ) );
        b.write( (const char*)&t, 2 );
    }

    //qDebug() << "writing the fsts at" << hex << (quint32)b.pos();
    //write all the fst entries
    for( quint16 i = 0; i < 0x17ff; i++ )
    {
        fst_t fst = fsts[ i ];

        b.write( (const char*)&fst.filename, 0xc );
        b.write( (const char*)&fst.attr, 1 );
        b.write( (const char*)&fst.wtf, 1 );

        t = qFromBigEndian( fst.sub );
        b.write( (const char*)&t, 2 );

        t = qFromBigEndian( fst.sib );
        b.write( (const char*)&t, 2 );

        tmp = qFromBigEndian( fst.size );
        b.write( (const char*)&tmp, 4 );

        tmp = qFromBigEndian( fst.uid );
        b.write( (const char*)&tmp, 4 );

        t = qFromBigEndian( fst.gid );
        b.write( (const char*)&t, 2 );

        tmp = qFromBigEndian( fst.x3 );
        b.write( (const char*)&tmp, 4 );
    }


    //qDebug() << "done adding shit" << hex << (quint32)b.pos();
    b.close();
    QByteArray hmR = spare.Get_hmac_meta( scl, nextSuperCluster );

    qDebug() << "about to write the meta block" << hex << nextSuperCluster << nextClusterVersion << "to page" << (quint32)( nextSuperCluster * 8 );

    for( quint8 i = 0; i < 0x10; i++ )
    {
        bool ret = WriteCluster( (quint32)( ( nextSuperCluster + i ) * 8 ), scl.mid( 0x4000 * i, 0x4000 ), ( i == 15 ? hmR : QByteArray() ) );
        if( !ret )
        {
            qWarning() << "failed to write the metadata.  this nand may be broken now :(" << i;
            return false;
        }
    }
    currentSuperCluster = nextSuperCluster;
    superClusterVersion = nextClusterVersion; //probably need to put some magic here in case the version wraps around back to 0

    //make sure all the data is really written
    f.flush();

    return true;
}

bool NandBin::CheckEcc( quint32 pageNo )
{
    if( !type )
        return false;

    QByteArray whole = GetPage( pageNo, true );
    if( whole.size() != 0x840 )
        return false;

    QByteArray data = whole.left( 0x800 );
    bool used = false;			    //dont calculate ecc for unused pages
    for( quint16 i = 0; i < 0x800; i++ )
    {
        if( data.at( i ) != '\xff' )
        {
            used = true;
            break;
        }
    }
    if( !used )
        return true;
    QByteArray ecc = whole.right( 0x10 );
    QByteArray calc = spare.CalcEcc( data );
    return ( ecc == calc );
}

bool NandBin::CheckHmacData( quint16 entry )
{
    if( entry > 0x17fe )
    {
        qDebug() << "bad entry #" << hex << entry;
        return false;
    }

    fst_t fst = fsts[ entry ];
    if( ( fst.attr & 3 ) != 1 )
    {
        qDebug() << "bad attributes" << ( fst.attr & 3 );
        return false;
    }

    if( !fst.size )
        return true;

	quint16 clCnt = ( RU( fst.size, 0x4000 ) / 0x4000 );
    //qDebug() << FstName( fst ) << "is" << hex << fst.size << "bytes (" << clCnt << ") clusters";

    quint16 fat = fst.sub;
    QByteArray sp1;
    QByteArray sp2;
    QByteArray hmac;
    //qDebug() << "fat" << hex << fat;
    for( quint32 i = 0; i < clCnt; i++ )
    {
        if( fat > 0x7fff )
        {
            qDebug() << "fat is out of range" << hex << fat;
            return false;
        }
        QByteArray cluster = GetCluster( fat );			//hmac is calculated with the decrypted cluster data
        if( cluster.size() != 0x4000 )
        {
            qDebug() << "error reading cluster";
            return false;
        }
        sp1 = GetPage( ( fat * 8 ) + 6, true );	//the spare data of these 2 pages hold the hmac data for the cluster
        sp2 = GetPage( ( fat * 8 ) + 7, true );
        if( sp1.isEmpty() || sp2.isEmpty() )
        {
            qDebug() << "error getting spare data";
            return false;
        }

        sp1 = sp1.right( 0x40 );				//only keep the spare data and drop the data
        sp2 = sp2.right( 0x40 );

        hmac = spare.Get_hmac_data( cluster, fst.uid, (const unsigned char*)&fst.filename, entry, fst.x3, i );

        //this part is kinda ugly, but this is how it is layed out by big N
        //really it allows 1 copy of hmac to be bad, but im being strict about it
        if( sp1.mid( 1, 0x14 ) != hmac )
        {
            qDebug() << "hmac bad (1)";
            goto error;
        }
        if( sp1.mid( 0x15, 0xc ) != hmac.left( 0xc ) )
        {
            qDebug() << "hmac bad (2)";
            goto error;
        }
        if( sp2.mid( 1, 8 ) != hmac.right( 8 ) )
        {
            qDebug() << "hmac bad (3)";
            goto error;
        }
        //qDebug() << "hmac ok for cluster" << i;
        //data += cluster;
        fat = GetFAT( fat );
    }
    return true;

    error:
    qDebug() << FstName( fst ) << "is" << hex << fst.size << "bytes (" << clCnt << ") clusters";
    hexdump( sp1 );
    hexdump( sp2 );
    hexdump( hmac );
    return false;

}

bool NandBin::CheckHmacMeta( quint16 clNo )
{
    if( clNo < 0x7f00 || clNo > 0x7ff0 || clNo % 0x10 )
        return false;

    QByteArray data;
    for( quint8 i = 0; i < 0x10; i++ )
    {
        data += GetCluster( ( clNo + i ), false );
    }
    QByteArray hmac = spare.Get_hmac_meta( data, clNo );
    quint32 baseP = ( clNo + 15 ) * 8;
    //qDebug() << "baseP" << hex << baseP << ( baseP + 6 ) << ( baseP + 7 );
    QByteArray sp1 = GetPage( baseP + 6, true );		    //the spare data of these 2 pages hold the hmac data for the supercluster
    QByteArray sp2 = GetPage( baseP + 7, true );
    if( sp1.isEmpty() || sp2.isEmpty() )
    {
        qDebug() << "error getting spare data";
        return false;
    }

    sp1 = sp1.right( 0x40 );					    //only keep the spare data and drop the data
	sp2 = sp2.right( 0x40 );

    //this part is kinda ugly, but this is how it is layed out by big N
    //really it allows 1 copy of hmac to be bad, but im being strict about it
    if( sp1.mid( 1, 0x14 ) != hmac )
    {
        qDebug() << "hmac bad (1)";
        goto error;
    }
    if( sp1.mid( 0x15, 0xc ) != hmac.left( 0xc ) )
    {
        qDebug() << "hmac bad (2)";
        goto error;
    }
    if( sp2.mid( 1, 8 ) != hmac.right( 8 ) )
    {
        qDebug() << "hmac bad (3)";
        goto error;
    }
    return true;

    error:
    qDebug() << "supercluster" << hex << clNo;
    hexdump( sp1 );
    hexdump( sp2 );
    hexdump( hmac );
    return false;

}


