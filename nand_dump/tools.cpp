#include "tools.h"
#include "includes.h"
#include "aes.h"
#include "sha1.h"

QString currentDir;
QString cachePath = "./NUS_cache";
QString nandPath = "./dump";

char ascii( char s ) {
    if ( s < 0x20 ) return '.';
    if ( s > 0x7E ) return '.';
    return s;
}

//using stderr just because qtcreator stows it correctly when mixed with qDebug(), qWarning(), etc
//otherwise it may not be shown in the correct spot in the output due to stdout/stderr caches
void hexdump( const void *d, int len ) {
    unsigned char *data;
    int i, off;
    data = (unsigned char*)d;
    fprintf( stderr, "\n");
    for ( off = 0; off < len; off += 16 ) {
	fprintf( stderr, "%08x  ", off );
	for ( i=0; i<16; i++ )
	{
	    if( ( i + 1 ) % 4 )
	    {
		if ( ( i + off ) >= len ) fprintf( stderr,"  ");
		else fprintf( stderr,"%02x",data[ off + i ]);
	    }
	    else
	    {
		if ( ( i + off ) >= len ) fprintf( stderr,"   ");
		else fprintf( stderr,"%02x ",data[ off + i ]);
	    }
	}

	fprintf( stderr, " " );
	for ( i = 0; i < 16; i++ )
	    if ( ( i + off) >= len ) fprintf( stderr," ");
	    else fprintf( stderr,"%c", ascii( data[ off + i ]));
	fprintf( stderr,"\n");
    }
    fflush( stderr );
}

void hexdump( const QByteArray &d, int from, int len )
{
    hexdump( d.data() + from, len == -1 ? d.size() : len );
}

void hexdump12( const void *d, int len ) {
    unsigned char *data;
    int i, off;
    data = (unsigned char*)d;
    fprintf( stderr, "\n");
    for ( off = 0; off < len; off += 12 ) {
	fprintf( stderr, "%08x  ", off );
	for ( i=0; i<12; i++ )
	{
	    if( ( i + 1 ) % 4 )
	    {
		if ( ( i + off ) >= len ) fprintf( stderr,"  ");
		else fprintf( stderr,"%02x",data[ off + i ]);
	    }
	    else
	    {
		if ( ( i + off ) >= len ) fprintf( stderr,"   ");
		else fprintf( stderr,"%02x ",data[ off + i ]);
	    }
	}

	fprintf( stderr, " " );
	for ( i = 0; i < 12; i++ )
	    if ( ( i + off) >= len ) fprintf( stderr," ");
	    else fprintf( stderr,"%c", ascii( data[ off + i ]));
	fprintf( stderr,"\n");
    }
    fflush( stderr );
}

void hexdump12( const QByteArray &d, int from, int len )
{
    hexdump12( d.data() + from, len == -1 ? d.size() : len );
}

QByteArray PaddedByteArray( const QByteArray &orig, quint32 padTo )
{
    QByteArray padding( RU( padTo, orig.size() ) - orig.size(), '\0' );
    //qDebug() << "padding with" << hex << RU( padTo, orig.size() ) << "bytes" <<
    return orig + padding;
}

QByteArray AesDecrypt( quint16 index, const QByteArray source )
{
	static quint8 iv[ 16 ];

	quint16 beidx = qFromBigEndian( index );
	memset( iv, 0, 16 );
	memcpy( iv, &beidx, 2 );
	QByteArray ret( source.size(), '\0' );
	aes_decrypt( iv, (const quint8*)source.data(), (quint8*)ret.data(), source.size() );
	return ret;
}

void AesSetKey( const QByteArray key )
{
    aes_set_key( (const quint8*)key.data() );
}

QByteArray GetSha1( QByteArray stuff )
{
    SHA1Context sha;
    SHA1Reset( &sha );
    SHA1Input( &sha, (const unsigned char*)stuff.data(), stuff.size() );
    if( !SHA1Result( &sha ) )
    {
	qWarning() << "GetSha1 -> sha error";
	return QByteArray();
    }
    QByteArray ret( 20, '\0' );
    quint8 *p = (quint8 *)ret.data();
    for( int i = 0; i < 5 ; i++ )
    {
	quint32 part = qFromBigEndian( sha.Message_Digest[ i ] );
	memcpy( p + ( i * 4 ), &part, 4 );
    }
    //hexdump( ret );
    return ret;
}
