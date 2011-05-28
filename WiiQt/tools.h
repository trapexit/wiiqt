#ifndef TOOLS_H
#define TOOLS_H
#include "includes.h"
//#include "

//#define RU(x,n)	(-(-(x) & -(n)))    //round up
#define RU(N, S) ((((N) + (S) - 1) / (S)) * (S))

#define MIN( x, y ) ( ( x ) < ( y ) ? ( x ) : ( y ) )
#define MAX( x, y ) ( ( x ) > ( y ) ? ( x ) : ( y ) )

#define NAND_FILE   1
#define NAND_DIR    2
#define NAND_READ   1
#define NAND_WRITE  2
#define NAND_RW	    ( NAND_READ | NAND_WRITE )

#define NAND_ATTR( type, user, group, other ) ( ( quint8 )( ( type & 3 ) | ( ( user & 3 ) << 6 ) | ( ( group & 3 ) << 4 ) | ( ( other & 3 ) << 2 ) ) )
#define NAND_ATTR_TYPE( attr ) ( attr & 3 )
#define NAND_ATTR_USER( attr ) ( ( attr >> 6 ) & 3 )
#define NAND_ATTR_GROUP( attr ) ( ( attr >> 4 ) & 3 )
#define NAND_ATTR_OTHER( attr ) ( ( attr >> 2 ) & 3 )

#define COMMON_KEY		{ 0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7 }
#define KOREAN_KEY		{ 0x63, 0xb8, 0x2b, 0xb4, 0xf4, 0x61, 0x4e, 0x2e, 0x13, 0xf2, 0xfe, 0xfb, 0xba, 0x4c, 0x9b, 0x7e }


#define SD_KEY { 0xab, 0x01, 0xb9, 0xd8, 0xe1, 0x62, 0x2b, 0x08, 0xaf, 0xba, 0xd8, 0x4d, 0xbf, 0xc2, 0xa5, 0x5d };
#define SD_IV { 0x21, 0x67, 0x12, 0xe6, 0xaa, 0x1f, 0x68, 0x9f, 0x95, 0xc5, 0xa2, 0x23, 0x24, 0xdc, 0x6a, 0x98 };
#define MD5_BLANKER { 0x0e, 0x65, 0x37, 0x81, 0x99, 0xbe, 0x45, 0x17, 0xab, 0x06, 0xec, 0x22, 0x45, 0x1a, 0x57, 0x93 };


#define TITLE_LATEST_VERSION 0xffff
//struct used to keep all the data about a NUS request together
//when a finished job is returned, the data list will be the TMD, then the ticket, then all the contents
struct NusJob
{
    quint64 tid;
    quint16 version;
    bool decrypt;
    QList<QByteArray> data;
};

struct SaveGame//struct to hold save data
{
    quint64 tid;		//tid this data belongs to
    QStringList	entries;	//paths of all the files & folders
    QList<quint8>attr;		//attributes for each file & folder in the save
    QList<QByteArray> data;	//data for each file.  size of this list should equal the number of files in the above list
};

char ascii( char s );
void hexdump( const void *d, int len );
void hexdump( const QByteArray &d, int from = 0, int len = -1 );

void hexdump12( const void *d, int len );
void hexdump12( const QByteArray &d, int from = 0, int len = -1 );

//simplified functions for crypto shit
void AesSetKey( const QByteArray &key );
QByteArray AesDecrypt( quint16 index, const QByteArray &source );
QByteArray AesEncrypt( quint16 index, const QByteArray &source );

QByteArray GetSha1( const QByteArray &stuff );
QByteArray GetMd5( const QByteArray &stuff );

bool IsValidSave( const SaveGame &save );
const QByteArray DataFromSave( const SaveGame &save, const QString &name );
quint32 SaveItemSize( const SaveGame &save );
quint8 AttrFromSave( const SaveGame &save, const QString &name );

//get a padded version of the given buffer
QByteArray PaddedByteArray( const QByteArray &orig, quint32 padTo );

//read a file into a bytearray
QByteArray ReadFile( const QString &path );

//save a file to disc
bool WriteFile( const QString &path, const QByteArray &ba );

//cleanup an svn revision string
QString CleanSvnStr( const QString &orig );

#define CERTS_DAT_SIZE 2560
extern const quint8 certs_dat[ CERTS_DAT_SIZE ];
extern const quint8 root_dat[];

#endif // TOOLS_H
