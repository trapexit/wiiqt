#include "tiktmd.h"
#include "tools.h"
#include "aes.h"

#include <iostream>

Tmd::Tmd( const QByteArray &stuff )
{
    data = stuff;
    p_tmd = NULL;
    if( data.isEmpty() || data.size() < 4 )//maybe 4 is still too low?
        return;

    SetPointer();
    if( (quint32)data.size() != SignedSize() )
    {
        data.resize( SignedSize() );
        SetPointer();
    }
    //hexdump( stuff );
}

quint64 Tmd::Tid()
{
    if( !p_tmd )
        return 0;
    return qFromBigEndian( p_tmd->title_id );
}

bool Tmd::SetTid( quint64 tid )
{
    if( !p_tmd )
        return false;

    p_tmd->title_id = qFromBigEndian( tid );
    return true;
}

quint64 Tmd::IOS()
{
    if( !p_tmd )
        return 0;
    return qFromBigEndian( p_tmd->sys_version );
}

bool Tmd::SetIOS( quint64 ios )
{
    if( !p_tmd )
        return false;

    p_tmd->sys_version = qFromBigEndian( ios );
    return true;
}

bool Tmd::SetAhb( bool remove )
{
    if( !p_tmd )
        return false;

	quint32 access = qFromBigEndian( p_tmd->access_rights );
	if( remove )
		access |=  1;
	else
		access &= 0xfffffffe;
	p_tmd->access_rights = qToBigEndian( access );
    return true;
}

bool Tmd::SetDiskAccess( bool allow )
{
    if( !p_tmd )
        return false;

	quint32 access = qFromBigEndian( p_tmd->access_rights );
	if( allow )
		access |=  2;
	else
		access &= 0xfffffffd;
	p_tmd->access_rights = qToBigEndian( access );
    return true;
}

quint32 Tmd::AccessFlags()
{
	if( !p_tmd )
		return 0;

	return qFromBigEndian( p_tmd->access_rights );
}

quint16 Tmd::Gid()
{
    if( !p_tmd )
        return 0;
    return qFromBigEndian( p_tmd->group_id );
}

QString Tmd::Cid( quint16 i )
{
    if( !p_tmd || i > qFromBigEndian( p_tmd->num_contents ) )
        return QString();
    return QString( "%1" ).arg( qFromBigEndian( p_tmd->contents[ i ].cid ), 8, 16, QChar( '0' ) );
}

QByteArray Tmd::Hash( quint16 i )
{
    if( !p_tmd || i > qFromBigEndian( p_tmd->num_contents ) )
        return QByteArray();

    return QByteArray( (const char*)&p_tmd->contents[ i ].hash, 20 );
}

bool Tmd::SetHash( quint16 cid, const QByteArray &hash )
{
    if( !p_tmd || cid >= qFromBigEndian( p_tmd->num_contents ) || hash.size() != 20 )
        return false;

    const char* h = hash.data();
    for( quint8 i = 0; i < 20; i++ )
        p_tmd->contents[ cid ].hash[ i ] = h[ i ];
    return true;
}

quint16 Tmd::Count()
{
    if( !p_tmd )
        return 0;

    return qFromBigEndian( p_tmd->num_contents );
}

quint16 Tmd::Version()
{
    if( !p_tmd )
        return 0;

    return qFromBigEndian( p_tmd->title_version );
}

bool Tmd::SetVersion( quint16 v )
{
    if( !p_tmd )
        return false;

    p_tmd->title_version = qFromBigEndian( v );
    return true;
}

quint64 Tmd::Size( quint16 i )
{
    if( !p_tmd || i > qFromBigEndian( p_tmd->num_contents ) )
        return 0;
    return qFromBigEndian( p_tmd->contents[ i ].size );
}

bool Tmd::SetSize( quint16 cid, quint32 size )
{
    if( !p_tmd || cid >= qFromBigEndian( p_tmd->num_contents ) )
        return false;

    p_tmd->contents[ cid ].size = qFromBigEndian( (quint64)size );
    return true;
}

quint16 Tmd::BootIndex( quint16 i )
{
	if( !p_tmd || i > qFromBigEndian( p_tmd->num_contents ) )
		return 0;
	return qFromBigEndian( p_tmd->contents[ i ].index );
}

quint16 Tmd::Type( quint16 i )
{
    if( !p_tmd || i > qFromBigEndian( p_tmd->num_contents ) )
        return 0;
    return qFromBigEndian( p_tmd->contents[ i ].type );
}

bool Tmd::SetType( quint16 cid, quint16 type )
{
    if( !p_tmd || cid >= qFromBigEndian( p_tmd->num_contents ) )
        return false;

    p_tmd->contents[ cid ].type = qFromBigEndian( type );
    return true;
}

quint32 Tmd::SignedSize()
{
    if( !p_tmd )
        return 0;
    return payLoadOffset + sizeof( tmd ) + ( sizeof( tmd_content ) * qFromBigEndian( p_tmd->num_contents ) );
}

void Tmd::SetPointer()
{
    payLoadOffset = 0;
    if( data.startsWith( "\x0\x1\x0\x0" ) )
        payLoadOffset = sizeof( sig_rsa2048 );

    else if( data.startsWith( "\x0\x1\x0\x1" ) )
        payLoadOffset = sizeof( sig_rsa4096 );

    else if( data.startsWith( "\x0\x1\x0\x2" ) )
        payLoadOffset = sizeof( sig_ecdsa );

    p_tmd = (tmd*)((quint8*)data.data() + payLoadOffset);
}

void Tmd::Dbg()
{
    if( !p_tmd )
        return;

    QString contents;
    quint16 cnt = Count();
    for( quint16 i = 0; i < cnt; i++ )
    {
        contents += QString( "%1 %2 %3 %4 " ).arg( i, 2, 16 ).arg( Type( i ), 4, 16 ).arg( Cid( i ) ).arg( Size( i ), 8, 16 )
                    + Hash( i ).toHex() + "\n";
    }

    QString s = QString( "TMD Dbg:\ntid:: %1\ncnt:: %2\n" )
                .arg( Tid(), 16, 16, QChar( '0' ) )
                .arg( cnt ) + contents;
    qDebug() << s;
}

bool Tmd::FakeSign()
{
    if( !p_tmd || payLoadOffset < 5 )
        return false;

    quint32 size = SignedSize();
    memset( (void*)( data.data() + 4 ), 0, payLoadOffset - 4 );//zero the RSA

    quint16 i = 0;
    bool ret = false;//brute force the sha1
    do
    {
        p_tmd->zero = i;//no need to worry about endian here
        if( GetSha1( data.mid( payLoadOffset, size ) ).startsWith( '\0' ) )
        {
            ret = true;
            break;
        }
    }
    while( ++i );
    return ret;
}

Ticket::Ticket( const QByteArray &stuff, bool fixKeyIndex )
{
    data = stuff;
    p_tik = NULL;
    if( data.isEmpty() || data.size() < 4 )//maybe 4 is still too low?
        return;

    SetPointer();

    quint8 iv[ 16 ];
    quint8 keyin[ 16 ];
    quint8 keyout[ 16 ];
    quint8 commonkey[ 16 ] = COMMON_KEY;

    quint8 *enc_key = (quint8 *)&p_tik->cipher_title_key;
    memcpy( keyin, enc_key, sizeof keyin );
    memset( keyout, 0, sizeof keyout );
    memset( iv, 0, sizeof iv);
    memcpy( iv, &p_tik->titleid, sizeof p_tik->titleid );

    aes_set_key( commonkey );
    aes_decrypt( iv, keyin, keyout, sizeof( keyin ) );

    decKey = QByteArray( (const char*)&keyout, 16 );

    if( (quint32)data.size() != SignedSize() )
    {
        data.resize( SignedSize() );
        SetPointer();
    }
	quint8 *keyindex = ((quint8*)p_tik) + 0xb1;
	if( fixKeyIndex && ( *keyindex > 0 ) )
	{
		if( *keyindex == 1 )//for now, just bail out for korean key
		{
			qWarning() << "Ticket::Ticket -> ticket uses the korean key. Only titles encrypted with the common key are supported";
			return;
		}
		qWarning() << "Ticket::Ticket -> key index is" << hex << *keyindex << ". Setting it to 0 and fakesigning";

		*keyindex = 0;//anything other than 0 or 1 is probably an error.  fix it
		FakeSign();
		//hexdump( data );
	}
}

quint64 Ticket::Tid()
{
    if( !p_tik )
        return 0;
    return qFromBigEndian( p_tik->titleid );
}

bool Ticket::SetTid( quint64 tid )
{
    if( !p_tik )
        return false;

	p_tik->titleid = qFromBigEndian( tid );

	//create new title key
	quint8 iv[ 16 ];
	quint8 keyin[ 16 ];
	quint8 commonkey[ 16 ] = COMMON_KEY;

	memcpy( &keyin, (quint8 *)decKey.data(), 16 );
	memset( &p_tik->cipher_title_key, 0, 16 );
	memset( &iv, 0, 16 );
	memcpy( &iv, &p_tik->titleid, sizeof(quint64) );

	aes_set_key( (quint8 *)&commonkey );
	aes_encrypt( (quint8 *)&iv, (quint8 *)&keyin,
                 (quint8 *)&p_tik->cipher_title_key, 16 );

    return true;
}

QByteArray Ticket::DecryptedKey()
{
    return decKey;
}

quint32 Ticket::SignedSize()
{
    return payLoadOffset + sizeof( tik );
}

void Ticket::SetPointer()
{
    payLoadOffset = 0;
    if( data.startsWith( "\x0\x1\x0\x0" ) )
        payLoadOffset = sizeof( sig_rsa2048 );

    else if( data.startsWith( "\x0\x1\x0\x1" ) )
        payLoadOffset = sizeof( sig_rsa4096 );

    else if( data.startsWith( "\x0\x1\x0\x2" ) )
        payLoadOffset = sizeof( sig_ecdsa );

    p_tik = (tik*)((quint8*)(data.data()) + payLoadOffset);
}

bool Ticket::FakeSign()
{
    if( !p_tik || payLoadOffset < 5 )
        return false;

    quint32 size = SignedSize();
    memset( (void*)( data.data() + 4 ), 0, payLoadOffset - 4 );//zero the RSA

    quint16 i = 0;
    bool ret = false;//brute force the sha1
    do
    {
        p_tik->padding = i;//no need to worry about endian here
        if( GetSha1( data.mid( payLoadOffset, size ) ).startsWith( '\0' ) )
        {
            ret = true;
            break;
        }
    }
    while( ++i );
    return ret;
}


//theres probably a better place to put all this stuff.  but until then, just put it here
//and this BE() probably isnt too good for cross-platformyitutiveness
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

static int check_rsa( const QByteArray &h, const quint8 *sig, const quint8 *key, const quint32 n )
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

	qDebug() << "Decrypted signature hash:" << QByteArray( (const char*)&x[ n-20 ], 20 ).toHex();
	qDebug() << "               SHA1 hash:" << h.toHex();
    return ERROR_RSA_HASH;
}

static int check_hash( const QByteArray &h, const quint8 *sig, const quint8 *key )
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

int check_cert_chain( const QByteArray &data )
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
