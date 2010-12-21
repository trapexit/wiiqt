#include "nandspare.h"
#include "tools.h"

NandSpare::NandSpare()
{
}
void NandSpare::SetHMacKey( const QByteArray key )
{
    hmacKey = key;
}

quint8 NandSpare::Parity( quint8 x )
{
    quint8 y = 0;
    while( x )
    {
	y ^= ( x & 1 );
	x >>= 1;
    }
    return y;
}

QByteArray NandSpare::CalcEcc( QByteArray in )
{
    if( in.size() != 0x800 )
	return QByteArray();

    quint8 a[ 12 ][ 2 ];
    quint32 a0, a1;
    quint8 x;

    QByteArray ret( 16, '\0' );
    char* ecc = ret.data();
    char* data = in.data();

    for( int k = 0; k < 4; k++ )
    {
	memset( a, 0, sizeof a );
	for( int i = 0; i < 512; i++ )
	{
	    x = data[ i ];
	    for( int j = 0; j < 9; j++ )
		a[ 3 + j ][ ( i >> j ) & 1 ] ^= x;
	}

	x = a[ 3 ][ 0 ] ^ a[ 3 ][ 1 ];
	a[ 0 ][ 0 ] = x & 0x55;
	a[ 0 ][ 1 ] = x & 0xaa;
	a[ 1 ][ 0 ] = x & 0x33;
	a[ 1 ][ 1 ] = x & 0xcc;
	a[ 2 ][ 0 ] = x & 0x0f;
	a[ 2 ][ 1 ] = x & 0xf0;

	for( int j = 0; j < 12; j++ )
	{
	    a[ j ][ 0 ] = Parity( a[ j ][ 0 ]);
	    a[ j ][ 1 ] = Parity( a[ j ][ 1 ]);
	}
	a0 = a1 = 0;

	for( int j = 0; j < 12; j++ )
	{
	    a0 |= a[ j ][ 0 ] << j;
	    a1 |= a[ j ][ 1 ] << j;
	}
	ecc[ 0 ] = a0;
	ecc[ 1 ] = a0 >> 8;
	ecc[ 2 ] = a1;
	ecc[ 3 ] = a1 >> 8;

	data += 512;
	ecc += 4;
    }
    return ret;
}

typedef struct{
	unsigned char key[ 0x40 ];
	SHA1Context hash_ctx;
} hmac_ctx;

void wbe32(void *ptr, quint32 val) { *(quint32*)ptr = qFromBigEndian( (quint32)val ); }
void wbe16(void *ptr, quint16 val) { *(quint16*)ptr = qFromBigEndian( val ); }

// reversing done by gray
static unsigned char hmac_key[ 20 ];

void hmac_init(hmac_ctx *ctx, const char *key, int key_size)
{
    key_size = key_size<0x40 ? key_size: 0x40;

    memset( ctx->key,0,0x40 );
    memcpy( ctx->key,key,key_size );

    for( int i = 0; i < 0x40; ++i )
	ctx->key[ i ] ^= 0x36; // ipad

    SHA1Reset( &ctx->hash_ctx );
    SHA1Input( &ctx->hash_ctx, ctx->key, 0x40 );
}

void hmac_update( hmac_ctx *ctx, const quint8 *data, int size )
{
	SHA1Input( &ctx->hash_ctx,data,size );
}

void hmac_final( hmac_ctx *ctx, unsigned char *hmac )
{
    //int i;
    unsigned char hash[ 0x14 ];
    memset( hash, 0, 0x14 );

    SHA1Result(&ctx->hash_ctx);

    // this sha1 implementation is buggy, needs to switch endian
    for( int i = 0;i < 5;++i )
    {
	wbe32( hash + 4 * i, ctx->hash_ctx.Message_Digest[ i ] );
    }
    for( int i = 0; i < 0x40; ++i )
	ctx->key[i] ^= 0x36^0x5c; // opad

    SHA1Reset(&ctx->hash_ctx);
    SHA1Input(&ctx->hash_ctx,ctx->key,0x40);
    SHA1Input(&ctx->hash_ctx,hash,0x14);
    SHA1Result(&ctx->hash_ctx);

    //hexdump(ctx->hash_ctx.Message_Digest, 0x14);
    for( int i = 0; i < 5; ++i )
    {
	wbe32( hash + 4 * i, ctx->hash_ctx.Message_Digest[ i ] );
    }
    memcpy( hmac, hash, 0x14 );
}

void fs_hmac_set_key(const char *key, int key_size)
{
    memset( hmac_key, 0, 0x14 );
    memcpy( hmac_key, key, key_size < 0x14 ? key_size : 0x14 );
}

void fs_hmac_generic( const unsigned char *data, int size, const unsigned char *extra, int extra_size, unsigned char *hmac )
{
    hmac_ctx ctx;

    hmac_init( &ctx,(const char *)hmac_key,0x14 );

    hmac_update( &ctx,extra, extra_size );
    hmac_update( &ctx,data, size );

    hmac_final( &ctx, hmac );
}

void fs_hmac_meta( const unsigned char *super_data, short super_blk, unsigned char *hmac )
{
    unsigned char extra[ 0x40 ];

    memset( extra,0,0x40 );
    wbe16( extra + 0x12, super_blk );

    fs_hmac_generic( super_data, 0x40000, extra, 0x40, hmac );
}

void fs_hmac_data( const unsigned char *data, quint32 uid, const unsigned char *name, quint32 entry_n, quint32 x3, quint16 blk, unsigned char *hmac )
{
    //int i,j;
    unsigned char extra[ 0x40 ];
    memset( extra, 0, 0x40 );

    wbe32( extra, uid );

    memcpy( extra + 4, name, 12 );

    wbe16( extra + 0x12, blk );
    wbe32( extra + 0x14, entry_n );
    wbe32( extra + 0x18, x3 );

//	fprintf(stderr,"extra (%s): \nX ",name);
//	for(i=0;i<0x20;++i){
//	    fprintf(stderr,"%02X ",extra[i]);
//	    if(!((i+1)&0xf)) fprintf(stderr,"\nX ");
//	}
    fs_hmac_generic( data, 0x4000, extra, 0x40, hmac );
}

QByteArray NandSpare::Get_hmac_data( const QByteArray cluster, quint32 uid, const unsigned char *name, quint32 entry_n, quint32 x3, quint16 blk )
{
    //qDebug() << "NandSpare::Get_hmac_data" << hex << cluster.size() << uid << QString( QByteArray( (const char*)name, 12 ) ) << entry_n << x3 << blk;
    if( hmacKey.size() != 0x14 || cluster.size() != 0x4000 )
	return QByteArray();

    fs_hmac_set_key( hmacKey.data(), 0x14 );

    QByteArray ret( 0x14, '\xba' );
    fs_hmac_data( (const unsigned char *)cluster.data(), uid, name, entry_n, x3, blk, (unsigned char *)ret.data() );

    return ret;
}

QByteArray NandSpare::Get_hmac_meta( const QByteArray cluster, quint16 super_blk )
{
    //qDebug() << "NandSpare::Get_hmac_meta" << hex << super_blk;
    if( hmacKey.size() != 0x14 || cluster.size() != 0x40000 )
    {
	//qDebug() << "NandSpare::Get_hmac_meta" << hex << hmacKey.size() << cluster.size();
	return QByteArray();
    }

    fs_hmac_set_key( hmacKey.data(), 0x14 );

    QByteArray ret( 0x14, '\0' );
    fs_hmac_meta( (const unsigned char *)cluster.data(), super_blk, (unsigned char *)ret.data() );

    return ret;
}
