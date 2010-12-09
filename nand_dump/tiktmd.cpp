#include "tiktmd.h"
#include "tools.h"
#include "aes.h"

#include <iostream>

Tmd::Tmd( QByteArray stuff )
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

bool Tmd::SetHash( quint16 cid, const QByteArray hash )
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

    p_tmd->contents[ cid ].size = qFromBigEndian( size );
    return true;
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

Ticket::Ticket( QByteArray stuff )
{
    data = stuff;
    p_tik = NULL;
    if( data.isEmpty() || data.size() < 4 )//maybe 4 is still too low?
	return;

    SetPointer();
    if( (quint32)data.size() != SignedSize() )
    {
	data.resize( SignedSize() );
	SetPointer();
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
    return true;
}

QByteArray Ticket::DecryptedKey()
{
    quint8 iv[ 16 ];
    quint8 keyin[ 16 ];
    quint8 keyout[ 16 ];
    static quint8 commonkey[ 16 ] = COMMON_KEY;

    quint8 *enc_key = (quint8 *)&p_tik->cipher_title_key;
    memcpy( keyin, enc_key, sizeof keyin );
    memset( keyout, 0, sizeof keyout );
    memset( iv, 0, sizeof iv);
    memcpy( iv, &p_tik->titleid, sizeof p_tik->titleid );

    aes_set_key( commonkey );
    aes_decrypt( iv, keyin, keyout, sizeof( keyin ) );

    return QByteArray( (const char*)&keyout, 16 );

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

    p_tik = (tik*)((quint8*)data.data() + payLoadOffset);
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
