#include "uidmap.h"
#include "tools.h"

UIDmap::UIDmap( const QByteArray old )
{
    data = old;
    if( data.isEmpty() )
	CreateNew();//no need to add all the factory uids if the nand in never going to be sent back to the big N
}
UIDmap::~UIDmap()
{

}

bool UIDmap::Check()
{
    if( !data.size() || data.size() % 12 )
    {
	qWarning() << "UIDmap::Check() bad size:" << hex << data.size();
	return false;
    }

    quint64 tid;
    quint32 uid;
    QBuffer buf( &data );
    buf.open( QIODevice::ReadOnly );

    buf.read( (char*)&tid, 8 );
    buf.read( (char*)&uid, 4 );

    tid = qFromBigEndian( tid );
    uid = qFromBigEndian( uid );
    if( tid != 0x100000002ull || uid != 0x1000 )//system menu should be the first entry
    {
	qWarning() << "UIDmap::Check() system menu entry is messed up:" << hex << tid << uid;
	buf.close();
	return false;
    }

    QList<quint64> tids;
    QList<quint32> uids;
    //check that there are no duplicate tid or uid and that all uid are as expected
    quint32 cnt = data.size() / 12;
    for( quint32 i = 1; i < cnt; i++ )
    {
	buf.read( (char*)&tid, 8 );
	buf.read( (char*)&uid, 4 );
	tid = qFromBigEndian( tid );
	uid = qFromBigEndian( uid );
	if( tids.contains( tid ) )
	{
	    qWarning() << "UIDmap::Check() tid is present more than once:" << QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
	    buf.close();
	    return false;
	}
	if( ( uid != 0x1000 + i ) || uids.contains( uid ) )
	{
	    qWarning() << "UIDmap::Check() uid error:" << QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) ) << hex << uid;
	    buf.close();
	    return false;
	}

	tids << tid;
	uids << uid;
    }

    return true;

}

quint32 UIDmap::GetUid( quint64 id, bool autoCreate )
{
    //qDebug() << "UIDmap::GetUid" << hex << id;
    quint64 tid;
    quint32 uid;
    QBuffer buf( &data );
    buf.open( QIODevice::ReadWrite );

    quint32 cnt = data.size() / 12;
    for( quint32 i = 0; i < cnt; i++ )
    {
	buf.read( (char*)&tid, 8 );
	buf.read( (char*)&uid, 4 );
	tid = qFromBigEndian( tid );
	if( tid == id )
	{
	    buf.close();
	    return qFromBigEndian( uid );
	}
    }

    //not found
    if( !autoCreate )
	return 0;

    //add the new entry
    tid = qFromBigEndian( id );
    buf.write( (const char*)&tid, 8 );
    uid = qFromBigEndian( qFromBigEndian( uid ) + 1 );
    buf.write( (const char*)&uid, 4 );
    buf.close();

    //qDebug() << "new uid:";
    //hexdump12( data, ( cnt - 3 ) * 12, 0x30 );

    return qFromBigEndian( uid );
}

void UIDmap::CreateNew( bool addFactorySetupDiscs )
{
    quint64 tid;
    quint32 uid;
    QByteArray stuff;
    QBuffer buf( &stuff );
    buf.open( QIODevice::WriteOnly );

    //add the new entry
    tid = qFromBigEndian( 0x100000002ull );
    buf.write( (const char*)&tid, 8 );
    uid = qFromBigEndian( 0x1000 );
    buf.write( (const char*)&uid, 4 );
    if( !addFactorySetupDiscs )
    {
	buf.close();
	data = stuff;
	return;
    }

    //add some entries for the factory setup discs, as seen on my nand up until the first retail game
    for( quint32 i = 1; i < 0x2f; i++ )
    {
	switch( i )
	{
	case 0x1:tid = qFromBigEndian( 0x100000004ull ); break;
	case 0x2:tid = qFromBigEndian( 0x100000009ull ); break;
	case 0x3:tid = qFromBigEndian( 0x100003132334aull ); break;
	case 0x4:tid = qFromBigEndian( 0x000100000000deadull ); break;
	case 0x5:tid = qFromBigEndian( 0x100000100ull ); break;
	case 0x6:tid = qFromBigEndian( 0x100000101ull ); break;
	case 0x7:tid = qFromBigEndian( 0x000100003132314aull ); break;
	case 0x8:tid = qFromBigEndian( 0x100000015ull ); break;
	case 0x9:tid = qFromBigEndian( 0x0001000030303032ull ); break;
	case 0xa:tid = qFromBigEndian( 0x100000003ull ); break;
	case 0xb:tid = qFromBigEndian( 0x10000000aull ); break;
	case 0xc:tid = qFromBigEndian( 0x10000000bull ); break;
	case 0xd:tid = qFromBigEndian( 0x10000000cull ); break;
	case 0xe:tid = qFromBigEndian( 0x10000000dull ); break;
	case 0xf:tid = qFromBigEndian( 0x10000000eull ); break;
	case 0x10:tid = qFromBigEndian( 0x10000000full ); break;
	case 0x11:tid = qFromBigEndian( 0x100000011ull ); break;
	case 0x12:tid = qFromBigEndian( 0x100000014ull ); break;
	case 0x13:tid = qFromBigEndian( 0x100000016ull ); break;
	case 0x14:tid = qFromBigEndian( 0x10000001cull ); break;
	case 0x15:tid = qFromBigEndian( 0x10000001eull ); break;
	case 0x16:tid = qFromBigEndian( 0x10000001full ); break;
	case 0x17:tid = qFromBigEndian( 0x100000021ull ); break;
	case 0x18:tid = qFromBigEndian( 0x100000022ull ); break;
	case 0x19:tid = qFromBigEndian( 0x100000023ull ); break;
	case 0x1a:tid = qFromBigEndian( 0x100000024ull ); break;
	case 0x1b:tid = qFromBigEndian( 0x100000025ull ); break;
	case 0x1c:tid = qFromBigEndian( 0x100000026ull ); break;
	case 0x1d:tid = qFromBigEndian( 0x100000032ull ); break;
	case 0x1e:tid = qFromBigEndian( 0x100000033ull ); break;
	case 0x1f:tid = qFromBigEndian( 0x100000035ull ); break;
	case 0x20:tid = qFromBigEndian( 0x100000037ull ); break;
	case 0x21:tid = qFromBigEndian( 0x1000000feull ); break;
	case 0x22:tid = qFromBigEndian( 0x0001000248414341ull ); break;
	case 0x23:tid = qFromBigEndian( 0x0001000248414141ull ); break;
	case 0x24:tid = qFromBigEndian( 0x0001000248415941ull ); break;
	case 0x25:tid = qFromBigEndian( 0x0001000248414641ull ); break;
	case 0x26:tid = qFromBigEndian( 0x0001000248414645ull ); break;
	case 0x27:tid = qFromBigEndian( 0x0001000248414241ull ); break;
	case 0x28:tid = qFromBigEndian( 0x0001000248414741ull ); break;
	case 0x29:tid = qFromBigEndian( 0x0001000248414745ull ); break;
	case 0x2a:tid = qFromBigEndian( 0x0001000848414b45ull ); break;
	case 0x2b:tid = qFromBigEndian( 0x0001000848414c45ull ); break;
	case 0x2c:tid = qFromBigEndian( 0x0001000148434745ull ); break;
	case 0x2d:tid = qFromBigEndian( 0x0001000031323245ull ); break;
	case 0x2e:tid = qFromBigEndian( 0x0001000030303033ull ); break;
	default:
	    qWarning() << "oops" << hex << i;
	    return;
	    break;
	}

	uid = qFromBigEndian( 0x1000 + i );
	buf.write( (const char*)&tid, 8 );
	buf.write( (const char*)&uid, 4 );

    }

    buf.close();
    data = stuff;
   // hexdump12( data );
}
