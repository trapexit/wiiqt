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
    //hexdump( stuff );
}

quint64 Tmd::Tid()
{
    if( !p_tmd )
	return 0;
    return qFromBigEndian( p_tmd->title_id );
}

QString Tmd::Cid( quint16 i )
{
    if( p_tmd && i > qFromBigEndian( p_tmd->num_contents ) )
	return QString();
    return QString( "%1" ).arg( qFromBigEndian( p_tmd->contents[ i ].cid ), 8, 16, QChar( '0' ) );
}

QByteArray Tmd::Hash( quint16 i )
{
    if( p_tmd && i > qFromBigEndian( p_tmd->num_contents ) )
	return QByteArray();

    return QByteArray( (const char*)&p_tmd->contents[ i ].hash, 20 );
}

quint64 Tmd::Size( quint16 i )
{
    if( p_tmd && i > qFromBigEndian( p_tmd->num_contents ) )
	return 0;
    return qFromBigEndian( p_tmd->contents[ i ].size );
}

quint16 Tmd::Type( quint16 i )
{
    if( p_tmd && i > qFromBigEndian( p_tmd->num_contents ) )
	return 0;
    return qFromBigEndian( p_tmd->contents[ i ].type );
}

quint32 Tmd::SignedSize()
{
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

Ticket::Ticket( QByteArray stuff )
{
    data = stuff;
    p_tik = NULL;
    if( data.isEmpty() || data.size() < 4 )//maybe 4 is still too low?
	return;

    SetPointer();
}

quint64 Ticket::Tid()
{
    if( !p_tik )
	return 0;
    return qFromBigEndian( p_tik->titleid );
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
