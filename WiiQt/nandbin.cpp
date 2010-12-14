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
	f.close();

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
    bool ret = !f.exists() || !f.open( QIODevice::ReadOnly );
    if( ret )
    {
	emit SendError( tr( "Cant open %1" ).arg( path ) );
    }

    return !ret;
}

QTreeWidgetItem *NandBin::GetTree()
{
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

bool NandBin::AddChildren( QTreeWidgetItem *parent, quint16 entry )
{
    if( entry >= 0x17ff )
    {
	emit SendError( tr( "entry is above 0x17ff mmmmkay [ 0x%1 ]" ).arg( entry, 0, 16 ) );
	return false;
    }

    fst_t fst = GetFST( entry );

    if( !fst.filename[ 0 ] )//something is amiss, better quit now
	return false;

    if( fst.sib != 0xffff )
    {
	if( !AddChildren( parent, fst.sib ) )
	    return false;
    }
    QStringList text;
    QString name = FstName( fst );

    QString en = QString( "%1" ).arg( entry );
    QString size = QString( "%1" ).arg( fst.size, 0, 16 );
    QString uid = QString( "%1" ).arg( fst.uid, 8, 16, QChar( '0' ) );
    QString gid = QString( "%1 (\"%2%3\")" ).arg( fst.gid, 4, 16, QChar( '0' ) )
		  .arg( QChar( ascii( (char)( (fst.gid >> 8) & 0xff ) ) ) )
		  .arg( QChar( ascii( (char)( (fst.gid) & 0xff ) ) ) );
    QString x3 = QString( "%1" ).arg( fst.x3, 8, 16, QChar( '0' ) );
    QString mode = QString( "%1" ).arg( fst.mode, 2, 16, QChar( '0' ) );
    QString attr = QString( "%1" ).arg( fst.attr, 2, 16, QChar( '0' ) );

    text << name << en << size << uid << gid << x3 << mode << attr;
    QTreeWidgetItem *child = new QTreeWidgetItem( parent, text );
    child->setTextAlignment( 1, Qt::AlignRight | Qt::AlignVCenter );//align to the right
    child->setTextAlignment( 2, Qt::AlignRight | Qt::AlignVCenter );
    child->setTextAlignment( 3, Qt::AlignRight | Qt::AlignVCenter );
    child->setTextAlignment( 4, Qt::AlignRight | Qt::AlignVCenter );
    child->setTextAlignment( 5, Qt::AlignRight | Qt::AlignVCenter );
    child->setTextAlignment( 6, Qt::AlignRight | Qt::AlignVCenter );
    child->setTextAlignment( 7, Qt::AlignRight | Qt::AlignVCenter );

    //set some icons
    if( fst.mode )
    {
	child->setIcon( 0, keyIcon );
    }
    else
    {
	child->setIcon( 0, groupIcon );
	//try to add subfolder contents to the tree
	if( fst.sub != 0xffff && !AddChildren( child, fst.sub ) )
	    return false;
    }

    return true;
}

QString NandBin::FstName( fst_t fst )
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

    switch( fst.mode )
    {
    case 0:
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

bool NandBin::ExtractDir( fst_t fst, QString parent )
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

bool NandBin::ExtractFile( fst_t fst, QString parent )
{
    QByteArray ba( (char*)fst.filename, 0xc );
    QString filename( ba );
    QFileInfo fi( parent + "/" + filename );
    qDebug() << "extract" << fi.absoluteFilePath();
    emit SendText( tr( "Extracting \"%1\"" ).arg( fi.absoluteFilePath() ) );

    QByteArray data = GetFile( fst );
    if( fst.size && !data.size() )//dont worry if files dont have anything in them anyways
	return false;

    if( !WriteFile( fi.absoluteFilePath(), data ) )
    {
	emit SendError( tr( "Error writing \"%1\"" ).arg( fi.absoluteFilePath() ) );
	return false;
    }
    return true;
}

bool NandBin::InitNand( QIcon dirs, QIcon files )
{
    fstInited = false;
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

    return true;
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

	    key = ReadKeyfile( keyPath );
	    if( key.isEmpty() )
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
	    f.seek( 0x21000158 );
	    key = f.read( 16 );
	}
	break;
    default:
	emit SendError( tr( "Tried to read keys for unknown dump type" ) );
	return false;
	break;
    }
    return true;
}

QByteArray NandBin::ReadKeyfile( QString path )
{
    QByteArray retval;
    QFile f( path );
    if( !f.exists() || !f.open( QIODevice::ReadOnly ) )
    {
	emit SendError( tr( "Can't open %1!" ).arg( path ) );
	return retval;
    }
    if( f.size() < 0x16e )
    {
	f.close();
	emit SendError( tr( "keys.bin is too small!" ) );
	return retval;
    }
    f.seek( 0x158 );
    retval = f.read( 16 );
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
    quint32 loc = 0, current = 0, last = 0;
    quint32 n_start[] = { 0x1FC00000, 0x20BE0000, 0x20BE0000 },
		n_end[] = { 0x20000000, 0x21000000, 0x21000000 },
		n_len[] = { 0x40000, 0x42000, 0x42000 };


    for( loc = n_start[ type ]; loc < n_end[ type ]; loc += n_len[ type ] )
    {
	f.read( (char*)&current, 4 );
	current = qFromBigEndian( current );

	//qDebug() << "superblock" << hex << current;

	if( current > last )
	    last = current;
	else
	{
	    //qDebug() << "superblock loc" << hex << loc - n_len[ type ];
	    return loc - n_len[ type ];
	}

	f.seek( n_len[ type ] - 4 );
    }
    return -1;
}

fst_t NandBin::GetFST( quint16 entry )
{
    //qDebug() << "NandBin::GetFST(" << hex << entry << ")";
    fst_t fst;
    if( entry >= 0x17FF )
    {
	emit SendError( tr( "Tried to get entry above 0x17ff [ 0x%1 ]" ).arg( entry, 0, 16 ) );
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
	emit SendError( tr( "Tried to read fst_t beyond size of nand.bin" ) );
	fst.filename[ 0 ] = '\0';
	return fst;
    }
    f.seek( loc_fst + loc_entry );

    f.read( (char*)&fst.filename, 0xc );
    f.read( (char*)&fst.mode, 1 );
    f.read( (char*)&fst.attr, 1 );
    f.read( (char*)&fst.sub, 2 );
    f.read( (char*)&fst.sib, 2 );
    if( type && ( entry + 1 ) % 64 == 0 )//bug in other nand.bin extracterizers.  the entry for every 64th fst item is inturrupeted by some ecc shit
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

    fst.mode &= 1;
    return fst;
}

quint16 NandBin::GetFAT( quint16 fat_entry )
{
    if( fstInited )
	return fats.at( fat_entry );
    /*
    * compensate for "off-16" storage at beginning of superblock
    * 53 46 46 53   XX XX XX XX   00 00 00 00
    * S  F  F  S     "version"     padding?
    *   1     2       3     4       5     6*/
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

QByteArray NandBin::GetCluster( quint16 cluster_entry, bool decrypt )
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
	//QByteArray page = f.read( n_pagelen[ type ] );					//read the page, with ecc
	QByteArray page = f.read( 0x800 );						//read the page, skip the ecc

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

QByteArray NandBin::GetFile( quint16 entry )
{
    fst_t fst = GetFST( entry );
    if( !fst.filename[ 0 ] )//something is amiss, better quit now
	return QByteArray();
    return GetFile( fst );
}

QByteArray NandBin::GetFile( fst_t fst )
{
    if( !fst.size )
	return QByteArray();

    quint16 fat = fst.sub;
    //int cluster_span = (int)( fst.size / 0x4000) + 1;

    QByteArray data;

    for (int i = 0; fat < 0xFFF0; i++)
    {
	QByteArray cluster = GetCluster( fat );
	if( cluster.size() != 0x4000 )
	    return QByteArray();

	data += cluster;
	fat = GetFAT( fat );
    }
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
    if( (quint32)data.size() < fst.size )
    {
	qDebug() << "(quint32)data.size() < fst.size :: "
		<< hex << data.size()
		<< "expected size:" << hex << fst.size;

	emit SendError( tr( "Error reading file [ returned data size is less that the size in the fst ]" ) );
	return QByteArray();
    }

    if( (quint32)data.size() > fst.size )
	data.resize( fst.size );//dont need to give back all the data, only up to the expected size

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
    quint16 j = 0;//just to make sure a broken nand doesnt lead to an endless loop
    while ( fat < 0x8000 && fat > 0 && ++j )
    {
	ret << fat;
	fat = GetFAT( fat );
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

    if( !item->text( 6 ).contains( "1" ) )
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
    }
    if( badBlocks )
	 badBlocks /= 8;

    if( reserved )
	 reserved /= 8;

    if( freeBlocks )
	 freeBlocks /= 8;

    qDebug() << "free blocks:" << hex << freeBlocks
	     << "\nbadBlocks:" << hex << badBlocks << badOnes
	     << "\nreserved:" << hex << reserved;
}


/*
 structure of blocks 0 - 7

 block 0 ( boot 1 )

 0 - 0x4320
 a bunch of encrypted gibberish.
the rest of the block as all 0s


sha1 of the whole block is

 2cdd5affd2e78c537616a119a7a2e1c568e91f22 with boot1b
 f01e8aca029ee0cb5287f5055da1a0bed2a533fa with boot1c

<sven>         {1, {0x4a, 0x7c, 0x6f, 0x30, 0x38, 0xde, 0xea, 0x7a, 0x07, 0xd3, 0x32, 0x32, 0x02, 0x4b, 0xe9, 0x5a, 0xfb, 0x56, 0xbf, 0x65}},
<sven>         {1, {0x2c, 0xdd, 0x5a, 0xff, 0xd2, 0xe7, 0x8c, 0x53, 0x76, 0x16, 0xa1, 0x19, 0xa7, 0xa2, 0xe1, 0xc5, 0x68, 0xe9, 0x1f, 0x22}},
<sven>         {0, {0xf0, 0x1e, 0x8a, 0xca, 0x02, 0x9e, 0xe0, 0xcb, 0x52, 0x87, 0xf5, 0x05, 0x5d, 0xa1, 0xa0, 0xbe, 0xd2, 0xa5, 0x33, 0xfa}},
<sven>         {0, {0x8d, 0x9e, 0xcf, 0x2f, 0x8f, 0x98, 0xa3, 0xc1, 0x07, 0xf1, 0xe5, 0xe3, 0x6f, 0xf2, 0x4d, 0x57, 0x7e, 0xac, 0x36, 0x08}},


 block 1 ( boot2 )	//just guessing at these for now

 u32	    0x20	//header size
 u32	    0xf00	//data offset
 u32	    0xa00	//cert size
 u32	    0x2a4	//ticket size
 u32	    0x208	//tmd size
 u32[ 3 ]   0s		//padding till 0x20

0x20 - 0x9f0
cert
root ca00000001
Root-CA00000001 CP00000004
Root-CA00000001 XS00000003

round up to 0xa20
ticket for boot2

0xcc4
tmd?

0xecc - 0xf00
gibberish padding

0xf00
start of boot2 contents? ( mine is 0x00023E91 bytes in the TMD for v2 )
there is room for 0x1F100 bytes of it left in this block with 0x4D91 leftover


block 2
bunch of gibberish till 0x4da0
probably the rest of the contents of boot2, padded with gibberish


0x4da0
0s till 0x5000

0x5000 - 0x1f800
bunch of 0xffff
maybe this is untouched nand.  never been written to



0x1f800 - 0x1f8e4  blockmap?

26F29A401EE684CF0000000201000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

26F29A401EE684CF0000000201000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

26F29A401EE684CF0000000201000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

0x1f8e4 - 0x20000 ( end of block )
more 0s


block 2 ( looks a lot like block 1 )
wad type header again with the same values

same cert looking doodad again
same ticket again

tmd is different ( zero'd RSA )
1 content, 0x00027B5D bytes





block 4 ( the rest of bootmii )
0 - 0x8a60
gibberish - rest of bootmii content still encrypted

0x8a60 - 0x1f800
0s

0x1f800
26F29A401EE684CF0000000501010100
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

26F29A401EE684CF0000000501010100
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

26F29A401EE684CF0000000501010100
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

then 0s till 0x20000 ( end of block )



block 5
all 0xff

block 6
identical to block 2 except this  only difference is the 0x02 is 0x03
26F29A401EE684CF0000000301000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

26F29A401EE684CF0000000301000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

26F29A401EE684CF0000000301000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101


block 7
identical to block 1






///////////
nand #2 ( no bootmii )
///////////

block 0 ( boot 1c )
gibberish till 0x43f0.  then 0s for the rest of the block

block 1
same wad header as before, with the same values
cert doodad and ticket are the same
tmd is v4 and the content is 0x00027BE8 bytes

block 2
0 - 0x8af0
the rest of the content from boot2 ( padded to 0x40 )

0s till 0x9000
0xff till 0x18f00

26F29A401EE684CF0000000201000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

26F29A401EE684CF0000000201000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

26F29A401EE684CF0000000201000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101



block 3 - all 0xff
block 4 - all 0xff
block 5 - all 0xff

block 6 - identical to block 2 except the blockmap ( generation 3 ? )

26F29A401EE684CF0000000301000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

26F29A401EE684CF0000000301000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

26F29A401EE684CF0000000301000000
00000000010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
010101010101010101010101

block 7 matches block 1




/////////////
nand #3
/////////////

block 2
26F29A401EE684CF
00000004
01000000000000000101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101

block 4
26F29A401EE684CF
0000000F
01010100000000000101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101

block 6
26F29A401EE684CF
00000005
01000000000000000101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101



























26F29A401EE684CF
00000002

01000000000000000101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101


26F29A401EE684CF
00000005
01010100000000000101010101010101
01010101010101010101010101010101
01010101010101010101010101010101
01010101010101010101010101010101


 */
