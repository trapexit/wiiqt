#ifndef TOOLS_H
#define TOOLS_H
#include "includes.h"

#define RU(x,n)	(-(-(x) & -(n)))    //round up

#define MIN( x, y ) ( ( x ) < ( y ) ? ( x ) : ( y ) )
#define MAX( x, y ) ( ( x ) > ( y ) ? ( x ) : ( y ) )

char ascii( char s );

void hexdump( const void *d, int len );
void hexdump( const QByteArray &d, int from = 0, int len = -1 );

void hexdump12( const void *d, int len );
void hexdump12( const QByteArray &d, int from = 0, int len = -1 );

//simplified functions for crypto shit
void AesSetKey( const QByteArray key );
QByteArray AesDecrypt( quint16 index, const QByteArray source );

//QByteArray GetSha1( QByteArray stuff );

//get a padded version of the given buffer
QByteArray PaddedByteArray( const QByteArray &orig, quint32 padTo );

//keep track of the last folder browsed to when looking for files
extern QString currentDir;

//folder used to cache stuff downloaded from NUS so duplicate titles dont need to be downloaded
extern QString cachePath;

//folder to use as the base path for the nand
extern QString nandPath;

#endif // TOOLS_H
