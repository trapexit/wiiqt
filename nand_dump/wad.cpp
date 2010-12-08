#include "wad.h"
#include "tools.h"
#include "tiktmd.h"

static QByteArray globalCert;

Wad::Wad( const QByteArray stuff )
{
    ok = false;
    if( stuff.size() < 0x80 )//less than this and there is definitely nothing there
	return;

    QByteArray copy = stuff;
    QBuffer b( &copy );
    b.open( QIODevice::ReadOnly );

    quint32 tmp;
    b.read( (char*)&tmp, 4 );
    if( qFromBigEndian( tmp ) != 0x20 )
    {
	b.close();
	Err( "Bad header size" );
	return;
    }
    b.read( (char*)&tmp, 4 );
    tmp = qFromBigEndian( tmp );
    if( tmp != 0x49730000 && tmp != 0x69620000 && tmp != 0x426b0000 )
    {
	b.close();
	Err( "Bad file magic word" );
	return;
    }

    quint32 certSize;
    quint32 tikSize;
    quint32 tmdSize;
    quint32 appSize;
    quint32 footerSize;

    b.read( (char*)&certSize, 4 );
    certSize = qFromBigEndian( certSize );

    b.seek( 0x10 );
    b.read( (char*)&tikSize, 4 );
    tikSize = qFromBigEndian( tikSize );
    b.read( (char*)&tmdSize, 4 );
    tmdSize = qFromBigEndian( tmdSize );
    b.read( (char*)&appSize, 4 );
    appSize = qFromBigEndian( appSize );
    b.read( (char*)&footerSize, 4 );
    footerSize = qFromBigEndian( footerSize );

    b.close();//close the buffer, the rest of the data can be checked without it

    //sanity check this thing
    quint32 s = stuff.size();
    if( s < ( RU( 0x40, certSize ) + RU( 0x40, tikSize ) + RU( 0x40, tmdSize ) + RU( 0x40, appSize ) + RU( 0x40, footerSize ) ) )
    {
	Err( "Total size is less than the combined sizes of all the parts that it is supposed to contain" );
	return;
    }

    quint32 pos = 0x40;
    certData = stuff.mid( pos, certSize );
    pos += RU( 0x40, certSize );
    tikData = stuff.mid( pos, tikSize );
    pos += RU( 0x40, tikSize );
    tmdData = stuff.mid( pos, tmdSize );
    pos += RU( 0x40, tmdSize );

    Ticket ticket( tikData );
    Tmd t( tmdData );

    if( ticket.Tid() != t.Tid() )
	qWarning() << "wad contains 2 different TIDs";

    quint32 cnt = qFromBigEndian( t.payload()->num_contents );
    qDebug() << "Wad contains" << hex << cnt << "contents";

    //another quick sanity check
    quint32 totalSize = 0;
    for( quint32 i = 0; i < cnt; i++ )
	totalSize += t.Size( i );

    if( totalSize > appSize )
    {
	Err( "Size of all the apps in the tmd is greater than the size in the wad header" );
	return;
    }
    /*
void AesSetKey( const QByteArray key );
QByteArray AesDecrypt( quint16 index, const QByteArray source );
*/
    //read all the contents, check the hash, and remember the data ( still encrypted )
    for( quint32 i = 0; i < cnt; i++ )
    {
	quint32 s = RU( 0x40, t.Size( i ) );
	QByteArray encData = stuff.mid( pos, s );
	pos += s;

	//doing this here in case there is some other object that is using the AES that would change the key on us
	AesSetKey( ticket.DecryptedKey() );


    }

    //Data = stuff.mid( pos, Size );
    //pos += RU( 0x40, Size );






    ok = true;

}

Wad::Wad( QList< QByteArray > stuff, bool encrypted )
{

}

void Wad::Err( QString str )
{
    errStr = str;
    qWarning() << "Wad::Error" << str;
}
