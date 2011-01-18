#include "uidmap.h"
#include "tools.h"

UIDmap::UIDmap( const QByteArray &old )
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

void UIDmap::CreateNew( quint8 addFactorySetupDiscs )
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
	quint8 reg = addFactorySetupDiscs;

    //add some entries for the factory setup discs, as seen on my nand up until the first retail game
	for( quint32 i = 1; i < 0x14; i++ )
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
		case 0x8:tid = qFromBigEndian( 0x10000000full ); break;
		case 0x9:tid = qFromBigEndian( 0x0001000030303032ull ); break;
		case 0xa:tid = qFromBigEndian( 0x10000000bull ); break;
		case 0xb:tid = qFromBigEndian( 0x10000000cull ); break;
		case 0xc:tid = qFromBigEndian( 0x10000000dull ); break;
		case 0xd:tid = qFromBigEndian( 0x0001000248414341ull ); break;
		case 0xe:tid = qFromBigEndian( 0x0001000248414141ull ); break;
		case 0xf:tid = qFromBigEndian( 0x0001000248414641ull ); break;
		case 0x10:tid = qFromBigEndian( 0x0001000248414241ull ); break;
		case 0x11:tid = qFromBigEndian( 0x0001000248414741ull ); break;
		case 0x12:tid = qFromBigEndian( (quint64)( 0x0001000848414b00ull | reg ) ); break;
		case 0x13:tid = qFromBigEndian( 0x0001000031323200ull ); break;
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
