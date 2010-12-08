#include "tools.h"
#include "includes.h"

QString currentDir;
QString pcPath = "./saveBackups";
QString sneekPath = "/media/SDHC_4GB";

char ascii( char s ) {
    if ( s < 0x20 ) return '.';
    if ( s > 0x7E ) return '.';
    return s;
}
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
