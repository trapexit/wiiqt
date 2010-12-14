#include "blocks0to7.h"
#include "tools.h"
#include "tiktmd.h"

int check_cert_chain( const QByteArray data );

enum
{
  ERROR_SUCCESS = 0,
  ERROR_SIG_TYPE,
  ERROR_SUB_TYPE,
  ERROR_RSA_FAKESIGNED,
  ERROR_RSA_HASH,
  ERROR_RSA_TYPE_UNKNOWN,
  ERROR_RSA_TYPE_MISMATCH,
  ERROR_CERT_NOT_FOUND,
  ERROR_COUNT
};

Blocks0to7::Blocks0to7( QList<QByteArray>blocks )
{
    _ok = false;
    if( !blocks.isEmpty() )
	SetBlocks( blocks );
}

bool Blocks0to7::SetBlocks( QList<QByteArray>blocks )
{
    _ok = false;
    this->blocks.clear();
    boot2Infos.clear();
    quint16 cnt = blocks.size();
    if( cnt != 8 )
	return false;
    for( quint16 i = 0; i < cnt; i++ )
    {
	if( blocks.at( i ).size() != 0x20000 )
	{
	    qWarning() << "Blocks0to7::SetBlocks -> block" << i << "is" << hex << blocks.at( i ).size() << "bytes";
	    return false;
	}
    }
    this->blocks = blocks;
    _ok = true;
    return true;
}

quint8 Blocks0to7::Boot1Version()
{
    if( blocks.size() != 8 )
	return BOOT_1_UNK;
    QByteArray hash = GetSha1( blocks.at( 0 ) );

    if( hash == QByteArray( "\x4a\x7c\x6f\x30\x38\xde\xea\x7a\x07\xd3\x32\x32\x02\x4b\xe9\x5a\xfb\x56\xbf\x65" ) )
	return BOOT_1_A;
    if( hash == QByteArray( "\x2c\xdd\x5a\xff\xd2\xe7\x8c\x53\x76\x16\xa1\x19\xa7\xa2\xe1\xc5\x68\xe9\x1f\x22" ) )
	return BOOT_1_B;
    if( hash == QByteArray( "\xf0\x1e\x8a\xca\x02\x9e\xe0\xcb\x52\x87\xf5\x05\x5d\xa1\xa0\xbe\xd2\xa5\x33\xfa" ) )
	return BOOT_1_C;
    if( hash == QByteArray( "\x8d\x9e\xcf\x2f\x8f\x98\xa3\xc1\x07\xf1\xe5\xe3\x6f\xf2\x4d\x57\x7e\xac\x36\x08" ) )
	return BOOT_1_D; //displayed as "boot1?" in ceilingcat

    return BOOT_1_UNK;
}

//really ugly thing to get the different versions of boot2.
//this doesnt take into account the possibility that boot2 is bigger and takes up more than 2 blocks
//there are 0x40 blocks in the blockmap, but only 8 are used.  maybe IOS has the authority to hijack the others if
//it runs out of room here.  if that ever happns, this code will become quite wrong
QList<Boot2Info> Blocks0to7::Boot2Infos()
{
    if( !boot2Infos.isEmpty() )
    {
	//qDebug() << "Blocks0to7::Boot2Infos() returning data from last time";
	return boot2Infos;
    }

    QList< Boot2Info > ret;
    if( blocks.size() != 8 )
	return ret;

    quint16 cnt = blocks.size();
    if( cnt != 8 )
	return ret;

    //get all the blockmaps
    quint16 newest = 0;
    quint8 lbm[ 8 ];
    for( quint8 i = 1; i < 8; i++ )
    {
	Boot2Info info = GetBlockMap( blocks.at( i ) );
	if( info.state == BOOT_2_ERROR )//this block doesnt contain a decent blockmap
	    continue;

	info.secondBlock = i;	    //this one is easy enough
	//find the first block that belongs to this second one
	if( i > 4 )//blocks are backwards
	{
	    bool found = false;
	    for( quint8 j = 7; j > i; j++ )
	    {
		if( info.blockMap[ j ] )//marked bad
		    continue;

		if( ( ( j > i + 1 ) && !info.blockMap[ i + 1 ] )//probably a much cleaner way to write this
		    || ( j == 6 && !info.blockMap[ 7 ] ) )	//but basically check for a couple stupid shit in the layout that really shouldnt ever happen
		    break;

		info.firstBlock = j;
		found = true;
		break;
	    }
	    if( !found )
		continue;
	}
	else//blocks are forwards
	{
	    bool found = false;
	    for( quint8 j = 0; j < i; j++ )
	    {
		if( info.blockMap[ j ] )//marked bad
		    continue;

		info.firstBlock = j;
		found = true;
		break;
	    }
	    if( !found )
		continue;
	    found = false;
	    //probably a much cleaner way to write this
	    //but basically check for a couple stupid shit in the layout that really shouldnt ever happen
	    for( quint8 j = info.firstBlock + 1; j < info.secondBlock && !found; j++ )
	    {
		if( info.blockMap[ j ] )//marked bad
		    continue;
		found = true;
	    }
	    if( found )//this means there is some other good block between the first block and this one that has the block map
		continue;
	}
	//we made it this far, it means that so far we have a correct looking blockmap that points to this copy of boot2
	if( info.generation > newest )
	{
	    newest = info.generation;
	    for( quint8 j = 0; j < 8; j++ )
		lbm[ j ] = info.blockMap[ j ];
	}

	ret << info;
    }
    //qDebug() << "newest blockmap" << QByteArray( (const char*)&lbm, 8 ).toHex();

    cnt = ret.size();
    bool foundBoot = false;
    bool foundBackup = false;
    for( quint8 i = 0; i < cnt; i++ )
    {
	ret[ i ] = CheckHashes( ret[ i ] );//check all the hashes and stuff
	if( !foundBoot && !lbm[ ret.at( i ).firstBlock ] && !lbm[ ret.at( i ).secondBlock ] )
	{
	    //qDebug() << "copy" << i << "is used when booting";
	    ret[ i ].state |= BOOT_2_USED_TO_BOOT;
	    //ret[ i ].usedToBoot = true;
	    foundBoot = true;
	}
	else if( lbm[ ret.at( i ).firstBlock ] || lbm[ ret.at( i ).secondBlock ] )
	{
	    ret[ i ].state |= BOOT_2_MARKED_BAD;
	}
    }
    for( quint8 i = ret.size(); !foundBackup && i > 0; i-- )
    {
	if( !lbm[ ret.at( i - 1 ).firstBlock ] && !lbm[ ret.at( i - 1 ).secondBlock ] && ret.at( i - 1 ).firstBlock > ret.at( i - 1 ).secondBlock )
	{
	    //qDebug() << "copy" << i << "is used when booting from backup";
	    ret[ i - 1 ].state |= BOOT_2_BACKUP_COPY;
	    foundBackup = true;
	    if( !foundBoot )
		ret[ i - 1 ].state |= BOOT_2_USED_TO_BOOT;
	}
    }
    boot2Infos = ret;
    return ret;
}

Boot2Info Blocks0to7::GetBlockMap( QByteArray block )
{
    Boot2Info ret;
    ret.state = BOOT_2_ERROR;
    if( block.size() != 0x20000 )
	return ret;

    QByteArray first = block.mid( 0x1f800, 0x4c );
    QByteArray second = block.mid( 0x1f84c, 0x4c );
    QByteArray third = block.mid( 0x1f898, 0x4c );
    QByteArray goodOne = QByteArray( "\x26\xF2\x9A\x40\x1E\xE6\x84\xCF" );
    if( first.startsWith( goodOne ) && ( first == second || first == third ) )
	goodOne = first;
    else if( second.startsWith( goodOne ) && ( second == third ) )
	goodOne = second;
    else
	return ret;

    ret.generation = goodOne.mid( 8, 4 ).toHex().toInt( NULL, 16 );
    for( quint8 i = 0; i < 8; i++ )
	ret.blockMap[ i ] = goodOne.at( i + 12 );

    ret.state = BOOT_2_TIK_SIG_OK;	//just assign this for now.  it will be corrected before this data leaves the class
    return ret;
}

Boot2Info Blocks0to7::CheckHashes( Boot2Info info )
{
    Boot2Info ret = info;
    ret.state = BOOT_2_ERROR_PARSING;

    QByteArray stuff = blocks.at( ret.firstBlock );
    QBuffer b( &stuff );
    b.open( QIODevice::ReadOnly );
    quint32 headerSize;
    quint32 dataOff;
    quint32 certSize;
    quint32 ticketSize;
    quint32 tmdSize;
    quint32 tmp;

    b.read( (char*)&tmp, 4 );
    headerSize = qFromBigEndian( tmp );
    if( headerSize != 0x20 )
	return ret;

    b.read( (char*)&tmp, 4 );
    dataOff = qFromBigEndian( tmp );
    b.read( (char*)&tmp, 4 );
    certSize = qFromBigEndian( tmp );
    b.read( (char*)&tmp, 4 );
    ticketSize = qFromBigEndian( tmp );
    b.read( (char*)&tmp, 4 );
    tmdSize = qFromBigEndian( tmp );
    b.close();

    QByteArray tikD = stuff.mid( headerSize + certSize, ticketSize );
    QByteArray tmdD = stuff.mid( headerSize + certSize + ticketSize, tmdSize );
    Tmd t( tmdD );
    Ticket ticket( tikD );
    if( t.Tid() != 0x100000001ull || ticket.Tid() != 0x100000001ull )
    {
	qWarning() << "Blocks0to7::CheckHashes -> bad TID";
	return ret;
    }
    ret.state = 0;
    int res = check_cert_chain( tikD );
    switch( res )
    {
    default:
    case ERROR_SIG_TYPE:
    case ERROR_SUB_TYPE:
    case ERROR_RSA_TYPE_UNKNOWN:
    case ERROR_CERT_NOT_FOUND:
	ret.state = BOOT_2_ERROR;
	qWarning() << "check_cert_chain( tikD ):" << res;
	return ret;
	break;
    case ERROR_RSA_TYPE_MISMATCH:
    case ERROR_RSA_HASH:
	ret.state = BOOT_2_BAD_SIGNATURE;
	//qWarning() << "check_cert_chain( tikD ):" << res;
	return ret;
	break;
    case ERROR_RSA_FAKESIGNED:
	ret.state |= BOOT_2_TIK_FAKESIGNED;
	break;
    case ERROR_SUCCESS:
	ret.state |= BOOT_2_TIK_SIG_OK;
	break;
    }
    res = check_cert_chain( tmdD );
    switch( res )
    {
    default:
    case ERROR_SIG_TYPE:
    case ERROR_SUB_TYPE:
    case ERROR_RSA_TYPE_UNKNOWN:
    case ERROR_CERT_NOT_FOUND:
	ret.state = BOOT_2_ERROR;
	//qWarning() << "check_cert_chain( tikD ):" << res;
	return ret;
	break;
    case ERROR_RSA_TYPE_MISMATCH:
    case ERROR_RSA_HASH:
	ret.state = BOOT_2_BAD_SIGNATURE;
	//qWarning() << "check_cert_chain( tikD ):" << res;
	return ret;
	break;
    case ERROR_RSA_FAKESIGNED:
	{
	    ret.state |= BOOT_2_TMD_FAKESIGNED;
	    if( tmdD.contains( "BM1.1" ) )
		ret.version = BOOTMII_11;
	    else if( tmdD.contains( "BM1.3" ) )
		    ret.version = BOOTMII_13;
	    else
		ret.version = BOOTMII_UNK;
	}
	break;
    case ERROR_SUCCESS:
	{
	    ret.state |= BOOT_2_TMD_SIG_OK;
	    ret.version = t.Version();
	}
	break;
    }

    //now decrypt boot2 and check the hash ( only checking 1 content because thats all there is )
    stuff += blocks.at( ret.secondBlock );

    AesSetKey( ticket.DecryptedKey() );
    QByteArray decD = AesDecrypt( 0, stuff.mid( dataOff, RU( 0x40, t.Size( 0 ) ) ) );
    decD.resize( t.Size( 0 ) );
    QByteArray realHash = GetSha1( decD );
    if( realHash != t.Hash( 0 ) )
	ret.state |= BOOT_2_BAD_CONTENT_HASH;
    return ret;
}

//theres probably a better place to put all this stuff.  but until then, just put it here
#define BE32(a) (((a)[0] << 24)|((a)[1] << 16)|((a)[2] << 8)|(a)[3])

#define bn_zero(a,b)      memset(a,0,b)
#define bn_copy(a,b,c)    memcpy(a,b,c)
#define bn_compare(a,b,c) memcmp(a,b,c)

// calc a = a mod N, given n = size of a,N in bytes
static void bn_sub_modulus( quint8 *a, const quint8 *N, const quint32 n )
{
    quint32 dig;
    quint8 c = 0;

    for( quint32 i = n - 1; i < n; i-- )
    {
	dig = N[ i ] + c;
	c = ( a [ i ] < dig );
	a[ i ] -= dig;
    }
}

// calc d = (a + b) mod N, given n = size of d,a,b,N in bytes
static void bn_add( quint8 *d, const quint8 *a, const quint8 *b, const quint8 *N, const quint32 n )
{
    quint32 i;
    quint32 dig;
    quint8 c = 0;

    for( i = n - 1; i < n; i--)
    {
	dig = a[ i ] + b[ i ] + c;
	c = ( dig >= 0x100 );
	d[ i ] = dig;
    }
    if( c )
	bn_sub_modulus( d, N, n );

    if( bn_compare( d, N, n ) >= 0 )
	bn_sub_modulus( d, N, n );
}

// calc d = (a * b) mod N, given n = size of d,a,b,N in bytes
static void bn_mul( quint8 *d, const quint8 *a, const quint8 *b, const quint8 *N, const quint32 n )
{
    quint8 mask;
    bn_zero( d, n );

    for( quint32 i = 0; i < n; i++ )
	for( mask = 0x80; mask != 0; mask >>= 1 )
	{
	    bn_add( d, d, d, N, n );
	    if( ( a[ i ] & mask ) != 0 )
		bn_add( d, d, b, N, n );
	}
}

// calc d = (a ^ e) mod N, given n = size of d,a,N and en = size of e in bytes
static void bn_exp( quint8 *d, const quint8 *a, const quint8 *N, const quint32 n, const quint8 *e, const quint32 en )
{
    quint8 t[ 512 ];
    quint8 mask;

    bn_zero( d, n );
    d[ n-1 ] = 1;
    for( quint32 i = 0; i < en; i++ )
	for( mask = 0x80; mask != 0; mask >>= 1 )
	{
	    bn_mul( t, d, d, N, n );
	    if( ( e [ i ] & mask ) != 0 )
		bn_mul( d, t, a, N, n );
	    else
		bn_copy( d, t, n );
	}
}

static int get_sig_len( const quint8 *sig )
{
    quint32 type;
    type = BE32( sig );
    switch( type - 0x10000 )
    {
    case 0:
	return 0x240;
    case 1:
	return 0x140;
    case 2:
	return 0x80;
    }
    return -ERROR_SIG_TYPE;
}

static int get_sub_len( const quint8 *sub )
{
    quint32 type;
    type = BE32( sub + 0x40 );
    switch( type )
    {
    case 0: return 0x2c0;
    case 1: return 0x1c0;
    case 2: return 0x100;
    }
    return -ERROR_SUB_TYPE;
}

static int check_rsa( QByteArray h, const quint8 *sig, const quint8 *key, const quint32 n )
{
    quint8 correct[ 0x200 ], x[ 0x200 ];
    static const quint8 ber[ 16 ] = { 0x00,0x30,0x21,0x30,0x09,0x06,0x05,0x2b,
			     0x0e,0x03,0x02,0x1a,0x05,0x00,0x04,0x14 };
    correct[ 0 ] = 0;
    correct[ 1 ] = 1;
    memset( correct + 2, 0xff, n - 38 );
    memcpy( correct + n - 36, ber, 16 );
    memcpy( correct + n - 20, h.constData(), 20 );

    bn_exp( x, sig, key, n, key + n, 4 );

    //qDebug() << "Decrypted signature hash:" << QByteArray( (const char*)&x[ n-20 ], 20 ).toHex();
    //qDebug() << "               SHA1 hash:" << h.toHex();

    if( memcmp( correct, x, n ) == 0 )
	return 0;

    if( strncmp( (char*)h.constData(), (char*) x + n - 20, 20 ) == 0 )
	return ERROR_RSA_FAKESIGNED;

    return ERROR_RSA_HASH;
}

static int check_hash( QByteArray h, const quint8 *sig, const quint8 *key )
{
    quint32 type;
    type = BE32( sig ) - 0x10000;
    if( (qint32)type != BE32( key + 0x40 ) )
	return ERROR_RSA_TYPE_MISMATCH;

    if( type == 1 )
	return check_rsa( h, sig + 4, key + 0x88, 0x100 );
    return ERROR_RSA_TYPE_UNKNOWN;
}

static const quint8* find_cert_in_chain( const quint8 *sub, const quint8 *cert, const quint32 cert_len, int *err )
{
    char parent[ 64 ], *child;
    int sig_len, sub_len;
    const quint8 *p, *issuer;

    strncpy( parent, (char*)sub, sizeof parent );
    parent[ sizeof parent - 1 ] = 0;
    child = strrchr( parent, '-' );
    if( child )
	*child++ = 0;
    else
    {
	*parent = 0;
	child = (char*)sub;
    }

    *err = -ERROR_CERT_NOT_FOUND;

    for( p = cert; p < cert + cert_len; p += sig_len + sub_len )
    {
	sig_len = get_sig_len( p );
	if( sig_len < 0 )
	{
	    *err = sig_len;
	    break;
	}
	issuer = p + sig_len;
	sub_len = get_sub_len( issuer );
	if( sub_len < 0 )
	{
	    *err = sub_len;
	    break;
	}
	if( strcmp( parent, (char*)issuer ) == 0 && strcmp( child, (char*)issuer + 0x44 ) == 0 )
	    return p;
    }
    return NULL;
}

int check_cert_chain( const QByteArray data )
{
    int cert_err;
    const quint8* key;
    const quint8 *sig, *sub, *key_cert;
    int sig_len, sub_len;
    QByteArray h;
    int ret;
    const quint8 *certificates = certs_dat;

    sig = (const quint8*)data.constData();
    sig_len = get_sig_len( sig );
    if( sig_len < 0 )
	return -sig_len;

    sub = (const quint8*)( data.data() + sig_len );
    sub_len = data.size() - sig_len;
    if( sub_len <= 0 )
	return ERROR_SUB_TYPE;

    for( ; ; )
    {
	//qDebug() << "Verifying using" << QString( (const char*) sub );
	if( strcmp((char*)sub, "Root" ) == 0 )
	{
	    key = root_dat;
	    h = GetSha1( QByteArray( (const char*)sub, sub_len ) );
	    if( BE32( sig ) != 0x10000 )
		return ERROR_SIG_TYPE;
	    return check_rsa( h, sig + 4, key, 0x200 );
	}

	key_cert = find_cert_in_chain( sub, certificates, CERTS_DAT_SIZE, &cert_err );
	if( key_cert )
	    cert_err = get_sig_len( key_cert );

	if( cert_err < 0 )
	    return -cert_err;
	key = key_cert + cert_err;

	h = GetSha1( QByteArray( (const char*)sub, sub_len ) );
	ret = check_hash( h, sig, key );
	// remove this if statement if you don't want to check the whole chain
	if( ret != ERROR_SUCCESS )
	    return ret;
	sig = key_cert;
	sig_len = get_sig_len( sig );
	if( sig_len < 0 )
	    return -sig_len;
	sub = sig + sig_len;
	sub_len = get_sub_len( sub );
	if( sub_len < 0 )
	    return -sub_len;
    }
}
