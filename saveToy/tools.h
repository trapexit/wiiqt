#ifndef TOOLS_H
#define TOOLS_H
#include "includes.h"

#define RU(x,n)	(-(-(x) & -(n)))    //round up

#define MIN( x, y ) ( ( x ) < ( y ) ? ( x ) : ( y ) )
#define MAX( x, y ) ( ( x ) > ( y ) ? ( x ) : ( y ) )

char ascii( char s );
void hexdump( const void *d, int len );
void hexdump( const QByteArray &d, int from = 0, int len = -1 );

//keep track of the last folder browsed to when looking for files
extern QString currentDir;

extern QString pcPath;
extern QString sneekPath;

#endif // TOOLS_H
