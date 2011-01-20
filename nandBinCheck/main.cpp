#include "../WiiQt/includes.h"
#include "../WiiQt/nandbin.h"
#include "../WiiQt/sharedcontentmap.h"
#include "../WiiQt/uidmap.h"
#include "../WiiQt/tools.h"
#include "../WiiQt/tiktmd.h"
#include "../WiiQt/settingtxtdialog.h"
#include "../WiiQt/u8.h"


//yippie for global variables
QStringList args;
NandBin nand;
SharedContentMap sharedM;
UIDmap uidM;
QList< quint64 > tids;
QList< quint64 > validIoses;//dont put stubs in this list.
QTreeWidgetItem *root;
QList<quint16> fats;
quint32 verbose = 0;
bool tryToKeepGoing = false;
bool color = true;
QByteArray sysMenuResource;

#ifndef Q_WS_WIN
#define LEN_STR_PAIR(s) sizeof (s) - 1, s
enum indicator_no
{
	C_LEFT, C_RIGHT, C_END, C_RESET, C_NORM, C_FILE, C_DIR, C_LINK,
	C_FIFO, C_SOCK,
	C_BLK, C_CHR, C_MISSING, C_ORPHAN, C_EXEC, C_DOOR, C_SETUID, C_SETGID,
	C_STICKY, C_OTHER_WRITABLE, C_STICKY_OTHER_WRITABLE, C_CAP, C_MULTIHARDLINK,
	C_CLR_TO_EOL
};

struct bin_str
{
	size_t len;			/* Number of bytes */
	const char *string;		/* Pointer to the same */
};

static struct bin_str color_indicator[] =
{
	{ LEN_STR_PAIR ("\033[") },		/* lc: Left of color sequence */
	{ LEN_STR_PAIR ("m") },		/* rc: Right of color sequence */
	{ 0, NULL },			/* ec: End color (replaces lc+no+rc) */
	{ LEN_STR_PAIR ("0") },		/* rs: Reset to ordinary colors */
	{ 0, NULL },			/* no: Normal */
	{ 0, NULL },			/* fi: File: default */
	{ LEN_STR_PAIR ("01;34") },		/* di: Directory: bright blue */
	{ LEN_STR_PAIR ("01;36") },		/* ln: Symlink: bright cyan */
	{ LEN_STR_PAIR ("33") },		/* pi: Pipe: yellow/brown */
	{ LEN_STR_PAIR ("01;35") },		/* so: Socket: bright magenta */
	{ LEN_STR_PAIR ("01;33") },		/* bd: Block device: bright yellow */
	{ LEN_STR_PAIR ("01;33") },		/* cd: Char device: bright yellow */
	{ 0, NULL },			/* mi: Missing file: undefined */
	{ 0, NULL },			/* or: Orphaned symlink: undefined */
	{ LEN_STR_PAIR ("01;32") },		/* ex: Executable: bright green */
	{ LEN_STR_PAIR ("01;35") },		/* do: Door: bright magenta */
	{ LEN_STR_PAIR ("37;41") },		/* su: setuid: white on red */
	{ LEN_STR_PAIR ("30;43") },		/* sg: setgid: black on yellow */
	{ LEN_STR_PAIR ("37;44") },		/* st: sticky: black on blue */
	{ LEN_STR_PAIR ("34;42") },		/* ow: other-writable: blue on green */
	{ LEN_STR_PAIR ("30;42") },		/* tw: ow w/ sticky: black on green */
	{ LEN_STR_PAIR ("30;41") },		/* ca: black on red */
	{ 0, NULL },			/* mh: disabled by default */
	{ LEN_STR_PAIR ("\033[K") },	/* cl: clear to end of line */
};

static void put_indicator( const struct bin_str *ind )
{
	fwrite( ind->string, ind->len, 1, stdout );
}

void PrintColoredString( const char *msg, int highlite )
{
	if( !color )
	{
		printf( "%s\n", msg );
	}
	else
	{
		QString m( msg );
		QString m2 = m.trimmed();
		m.resize( m.indexOf( m2 ) );
		printf( "%s", m.toLatin1().data() );				//print all leading whitespace
		put_indicator( &color_indicator[ C_LEFT ] );
		put_indicator( &color_indicator[ highlite ] );		//change color
		put_indicator( &color_indicator[ C_RIGHT ] );
		printf( "%s", m2.toLatin1().data() );				//print text
		put_indicator( &color_indicator[ C_LEFT ] );
		put_indicator( &color_indicator[ C_NORM ] );		//reset color
		put_indicator( &color_indicator[ C_RIGHT ] );
		printf( "\n" );
	}
	fflush( stdout );
}

//redirect text output.  by default, qDebug() goes to stderr
void DebugHandler( QtMsgType type, const char *msg )
{
	switch( type )
	{
	case QtDebugMsg:
		printf( "%s\n", msg );
		fflush( stdout );
		break;
	case QtWarningMsg:
		PrintColoredString( msg, C_STICKY );
		break;
	case QtCriticalMsg:
		PrintColoredString( msg, C_CAP );
		break;
	case QtFatalMsg:
		fprintf(stderr, "Fatal Error: %s\n", msg);
		abort();
		break;
	}
}
#endif

bool CheckTitleIntegrity( quint64 tid );

void Usage()
{
	qWarning() << "usage:" << QCoreApplication::arguments().at( 0 ) << "nand.bin" << "<other options>";
    qDebug() << "\nOther options:";
	qDebug() << "   -boot           shows information about boot 1 and 2";
	qDebug() << "";
	qDebug() << "   -fs             verify the filesystem is in tact";
	qDebug() << "                   verifies presence of uid & content.map & checks the hashes in the content.map";
	qDebug() << "                   check all titles with a ticket for RSA & sha1 validity";
	qDebug() << "                   check all titles with a ticket titles for required IOS, proper uid & gid";
	qDebug() << "";
	qDebug() << "   -settingtxt     check setting.txt itself and against system menu resources.  this must be combined with \"-fs\"";
	qDebug() << "";
	qDebug() << "   -uid            Look any titles in the uid.sys, check signatures and whatnot.  this must be combined with \"-fs\"";
	qDebug() << "";
	qDebug() << "   -clInfo         shows free, used, and lost ( marked used, but dont belong to any file ) clusters";
	qDebug() << "";
	qDebug() << "   -spare          calculate & compare ecc for all pages in the nand";
	qDebug() << "                   calculate & compare hmac signatures for all files and superblocks";
	qDebug() << "";
	qDebug() << "   -all            does all of the above";
	qDebug() << "";
	qDebug() << "   -v              increase verbosity";
	qDebug() << "";
	qDebug() << "   -continue       try to keep going as fas as possible on errors that should be fatal";
#ifndef Q_WS_WIN
	qDebug() << "";
	qDebug() << "   -nocolor        don\'t use terminal color trickery";
#endif
    exit( 1 );
}

void Fail( const QString& str )
{
	qCritical() << str;
	if( !tryToKeepGoing )
		exit( 1 );
}

QString TidTxt( quint64 tid )
{
    return QString( "%1" ).arg( tid, 16, 16, QChar( '0' ) );
}

QString AsciiTxt( quint32 lower )
{
	QString ret;
	lower = qFromBigEndian( lower );
	for( int i = 0; i < 4; i++ )
		ret += ascii( (char)( lower >> ( 8 * i ) ) & 0xff );
	return ret;
}

void ShowBootInfo( quint8 boot1, QList<Boot2Info> boot2stuff )
{
    switch( boot1 )
    {
    case BOOT_1_A:
        qDebug() << "Boot1 A (vulnerable)";
        break;
    case BOOT_1_B:
        qDebug() << "Boot1 B (vulnerable)";
        break;
    case BOOT_1_C:
        qDebug() << "Boot1 C (fixed)";
        break;
    case BOOT_1_D:
        qDebug() << "Boot1 D (fixed)";
        break;
    default:
        qDebug() << "unrecognized boot1 version";
        break;
    }
    quint16 cnt = boot2stuff.size();
    if( !cnt )
        Fail( "didnt find any boot2.  this nand wont work" );

    qDebug() << "found" << cnt << "copies of boot2";
    for( quint16 i = 0; i < cnt; i++ )
    {
        Boot2Info bi = boot2stuff.at( i );
        QString str = QString( "blocks %1 & %2: " ).arg( bi.firstBlock ).arg( bi.secondBlock );
        if( bi.state == BOOT_2_ERROR_PARSING || bi.state == BOOT_2_ERROR )
            str += "parsing error";
	    else if( bi.state == BOOT_2_BAD_SIGNATURE )
            str +=  "Bad RSA Signature";
	    else if( bi.state == BOOT_2_BAD_CONTENT_HASH )
            str += "Content hash doesn't match TMD";
	    else if( bi.state == BOOT_2_ERROR_PARSING )
            str += "Error parsing boot2";
	    else
	    {

            if( bi.state & BOOT_2_MARKED_BAD )
                str += "Marked as bad blocks; ";
            else if( bi.state & BOOT_2_USED_TO_BOOT )
            {
                str += "Used for booting; ";
                if( boot1 != BOOT_1_A && boot1 != BOOT_1_B
                    && (( bi.state & BOOT_2_TIK_FAKESIGNED) ||
                        ( bi.state & BOOT_2_TMD_FAKESIGNED ) ) )
                    Fail( "The version of boot1 installed doesn't have the strncmp bug and the copy of boot2 used for booting is fakesigned" );
            }
            else if( bi.state & BOOT_2_BACKUP_COPY )
                str += "Backup copy; ";


            str += "Content Sha1 matches TMD; ";

            if( bi.state & BOOT_2_TMD_FAKESIGNED )
                str += "TMD is fakesigned; ";
            else if( bi.state & BOOT_2_TMD_SIG_OK )
                str += "TMD officially signed; ";
            else
                str += "Error checking TMD; " ;

            if( bi.state & BOOT_2_TIK_FAKESIGNED )
                str += "Ticket is fakesigned; ";
            else if( bi.state & BOOT_2_TIK_SIG_OK )
                str += "Ticket officially signed; ";
            else
                str += "Error checking ticket; ";

            QString ver;
            switch( bi.version )
            {
            case BOOTMII_11:
                ver = "BootMii 1.1";
                break;
            case BOOTMII_13:
                ver = "BootMii 1.3";
                break;
            case BOOTMII_UNK:
                ver = "BootMii (Unk)";
                break;
            default:
                ver = QString( "Version %1" ).arg( bi.version );
                break;
            }
            str += ver;
	    }
	    qDebug() << str;
    }
}

QTreeWidgetItem *FindItem( const QString &s, QTreeWidgetItem *parent )
{
    int cnt = parent->childCount();
    for( int i = 0; i <cnt; i++ )
    {
        QTreeWidgetItem *r = parent->child( i );
        if( r->text( 0 ) == s )
        {
            return r;
        }
    }
    return NULL;
}

QTreeWidgetItem *ItemFromPath( const QString &path )
{
    QTreeWidgetItem *item = root;
    if( !path.startsWith( "/" ) || path.contains( "//" ))
    {
        return NULL;
    }
    int slash = 1;
    while( slash )
    {
        int nextSlash = path.indexOf( "/", slash + 1 );
        QString lookingFor = path.mid( slash, nextSlash - slash );
        item = FindItem( lookingFor, item );
        if( !item )
        {
//			if( verbose )
//				qWarning() << "ItemFromPath ->item not found" << path;
            return NULL;
        }
        slash = nextSlash + 1;
    }
    return item;
}

QString PathFromItem( QTreeWidgetItem *item )
{
    QString ret;
    while( item )
    {
        ret.prepend( "/" + item->text( 0 ) );
        item = item->parent();
        if( item->text( 0 ) == "/" )// dont add the root
            break;
    }
    return ret;

}

//get the installed titles in the nand - first check for tickets and then check for TMDs that match that ticket
QList< quint64 > InstalledTitles()
{
    QList< quint64 > ret;
    QTreeWidgetItem *tikFolder = ItemFromPath( "/ticket" );
    if( !tikFolder )
        Fail( "Couldnt find a ticket folder in this nand" );

    quint16 subfc = tikFolder->childCount();
    for( quint16 i = 0; i < subfc; i++ )//check all subfolders of "/ticket"
    {
        QTreeWidgetItem *subF = tikFolder->child( i );
		//qDebug() << "checking folder" << subF->text( 0 );
        bool ok = false;
		quint32 upper = subF->text( 0 ).toUInt( &ok, 16 );//make sure it can be converted to int
        if ( !ok )
            continue;

        quint16 subfc2 = subF->childCount();//get all entries in this subfolder
        for( quint16 j = 0; j < subfc2; j++ )
        {
            QTreeWidgetItem *tikI = subF->child( j );
            QString name = tikI->text( 0 );
			//qDebug() << "checking item" << subF->text( 0 ) + "/" + name;
            if( !name.endsWith( ".tik" ) )
            {
				//qDebug() << "!tik";
                continue;
            }

            name.resize( 8 );
			quint32 lower = name.toUInt( &ok, 16 );
            if( !ok )
            {
				//qDebug() << "!ok";
                continue;
            }

            //now see if there is a tmd
            QTreeWidgetItem *tmdI = ItemFromPath( "/title/" + subF->text( 0 ) + "/" + name + "/content/title.tmd" );
            if( !tmdI )
            {
				//qDebug() << "no tmd";
                continue;
            }

            quint64 tid = (( (quint64)upper << 32) | lower );
			//qDebug() << "adding item to list" << TidTxt( tid );
            ret << tid;
        }
    }
	qSort( ret.begin(), ret.end() );
    return ret;
}

void CheckShared()
{
    QByteArray ba = nand.GetData( "/shared1/content.map" );
    if( ba.isEmpty() )
        Fail( "No content map found in the nand" );

    sharedM = SharedContentMap( ba );
    quint16 cnt = sharedM.Count();
    for( quint16 i = 0; i < cnt; i++ )
    {
        QString path = "/shared1/" + sharedM.Cid( i ) + ".app";
        qDebug() << "checking" << path << "...";
        QByteArray stuff = nand.GetData( path );
        if( stuff.isEmpty() )
            Fail( "One of the shared contents in this nand is missing" );

        QByteArray realHash = GetSha1( stuff );
        if( realHash != sharedM.Hash( i ) )
            Fail( "The hash for at least 1 content is bad" );
    }
}

void BuildGoodIosList()
{
    foreach( quint64 tid, tids )
    {
        quint32 upper = ( tid >> 32 ) & 0xffffffff;
        if( upper != 1 )
            continue;

        quint32 lower = tid & 0xffffffff;

        if( lower < 3 || lower > 255 )
            continue;

        QString tmdp = TidTxt( tid );
        tmdp.insert( 8, "/" );
        tmdp.prepend( "/title/" );
        tmdp += "/content/title.tmd";
        QByteArray ba = nand.GetData( tmdp );
        if( ba.isEmpty() )
            continue;

        Tmd t( ba );			//skip stubbzzzzz
        if( !( t.Version() % 0x100 ) && //version is a nice pretty round number
            t.Count() == 3  &&		//3 contents, 1 private and 2 shared
            t.Type( 0 ) == 1 &&
            t.Type( 1 ) == 0x8001 &&
            t.Type( 2 ) == 0x8001 )
        {
            continue;
        }
        if( !CheckTitleIntegrity( tid ) )
            continue;

        validIoses << tid;//seems good enough.  add it to the list
    }
}

bool RecurseCheckGidUid( QTreeWidgetItem *item, const QString &uidS, const QString &gidS, const QString &path )
{
	bool ret = true;
	quint16 cnt = item->childCount();
	for( quint16 i = 0; i < cnt; i++ )
	{
		QTreeWidgetItem *child = item->child( i );
		if( child->text( 3 ) != uidS || !child->text( 4 ).startsWith( gidS ) )
		{
			ret = false;
			qWarning() << "\tincorrect uid/gid for" << QString( path + child->text( 0 ) );
		}
		if( !RecurseCheckGidUid( child, uidS, gidS, path + child->text( 0 ) + "/" ) )
			ret = false;
	}
	return ret;
}

void PrintName( const QByteArray &app )
{
	QString desc;
	if( app.size() == 0x40 )//tag for IOS, region select, ...
	{
		desc = QString( app ).simplified();
		QString desc2  = QString( app.right( 0x10 ) );
		if( !desc2.isEmpty() )
			desc += " " + desc2;
	}
	else if( U8::GetU8Offset( app ) >= 0 )					//maybe this is an IMET header.  try to get a name from it
	{
		U8 u8( app );
		if( u8.IsOK() )
		{
			QStringList names = u8.IMETNames();
			quint8 cnt = names.size();
			if( cnt >= 2 )									//try to use english name first
				desc = names.at( 1 );
			for( quint8 i = 0; i < cnt && desc.isEmpty(); i++ )
			{
				desc = names.at( i );
			}
		}
	}
	if( desc.isEmpty() )
	{
		qDebug() << "\tUnable to get title";
		return;
	}
	qDebug() << "\tname:   " << desc;
}

bool CheckTitleIntegrity( quint64 tid )
{
    if( validIoses.contains( tid ) )//this one has already been checked
		return true;
	quint32 upper = ( ( tid >>32 ) & 0xffffffff );
	quint32 lower = ( tid & 0xffffffff );

	if( verbose )
		qDebug() << "";
	qDebug() << "Checking" << qPrintable( TidTxt( tid ).insert( 8, "-" ) + ( upper != 1 ? " (" + AsciiTxt( lower ) + ")" : "" ) ) << "...";
    QString p = TidTxt( tid );
    p.insert( 8 ,"/" );
    QString tikp = p;
    tikp.prepend( "/ticket/" );
    tikp += ".tik";

    QString tmdp = p;
    tmdp.prepend( "/title/" );
    tmdp += "/content/title.tmd";

    Tmd t;
    //check the presence of the tmd & ticket.  also check their signatures
    //remember the tmd for checking the actual contents
    for( quint8 i = 0; i < 2; i++ )
    {
        QString it = ( i ? "tmd" : "ticket" );
        QByteArray ba = nand.GetData( i ? tmdp : tikp );
        if( ba.isEmpty() )
        {
            qDebug() << "error getting" << it << "data";
            return false;
        }
		qint32 ch = check_cert_chain( ba );
		switch( ch )
        {
        case ERROR_SIG_TYPE:
        case ERROR_SUB_TYPE:
        case ERROR_RSA_HASH:
        case ERROR_RSA_TYPE_UNKNOWN:
        case ERROR_RSA_TYPE_MISMATCH:
        case ERROR_CERT_NOT_FOUND:
			qWarning().nospace() << "\t" << qPrintable( it ) << " RSA signature isn't even close ( " << ch << " )";
            //return false;					    //maye in the future this will be true, but for now, this doesnt mean it wont boot
            break;
        case ERROR_RSA_FAKESIGNED:
			qWarning().nospace() << "\t" << qPrintable( it ) << " fakesigned";
            break;
        default:
            break;
        }
        if( i )
        {
            t = Tmd( ba );
            if( t.Tid() != tid )
            {
				qWarning() << "\tthe TMD contains the wrong TID";
                return false;
			}
        }
        else
        {
			Ticket ticket( ba, false );
            if( ticket.Tid() != tid )
            {
				qWarning() << "\tthe ticket contains the wrong TID";
                return false;
			}
        }
    }

	if( upper == 0x10005 ||  upper == 0x10007 )							    //dont try to verify all the contents of DLC, it will just find a bunch of missing contents and bitch about them
        return true;

    quint16 cnt = t.Count();
    for( quint16 i = 0; i < cnt; i++ )
    {
        if( t.Type( i ) == 0x8001 )//shared
        {
            if( sharedM.GetAppFromHash( t.Hash( i ) ).isEmpty() )
            {
				qWarning() << "\tone of the shared contents is missing";
                return false;
            }
        }
        else//private
        {
            QString cid = t.Cid( i );
            QString pA = tmdp;
            pA.resize( 33 );
            pA += cid + ".app";
            QByteArray ba = nand.GetData( pA );
            if( ba.isEmpty() )
            {
				qWarning() << "\t error reading one of the private contents" << pA;
                return false;
            }
            QByteArray realH = GetSha1( ba );
            if( realH != t.Hash( i ) )
            {
				qWarning() << "\tone of the private contents' hash doesnt check out" << i << pA <<
						"\n\texpected" << qPrintable( QString( t.Hash( i ).toHex() ) ) <<
						"\n\tactual  " << qPrintable( QString( realH.toHex() ) );
                //return false;									    //dont return false, as this this title may still boot
			}

			//if we are going to check the setting.txt stuff, we need to get the system menu resource file to compare ( check for opera bricks )
			//so far, i think this file is always boot index 1, type 1
			if( tid == 0x100000002ull && t.BootIndex( i ) == 1 &&
				  ( args.contains( "-settingtxt", Qt::CaseInsensitive ) || args.contains( "-all", Qt::CaseInsensitive ) ) )
			{
				sysMenuResource = ba;
			}

			//print a description of this title
			if( verbose > 1 && t.BootIndex( i ) == 0 )
			{
				PrintName( ba );
			}
        }


    }

	//print version
	if( verbose )
	{
		quint16 vers = t.Version();
		qDebug() << "\tversion:" << qPrintable( QString( "%1.%2" ).arg( ( vers >> 8 ) & 0xff ).arg( vers & 0xff ).leftJustified( 10 ) )
				<< qPrintable( QString( "%1" ).arg( vers ).leftJustified( 10 ) )
				<< "hex:" << hex << t.Version();
		if( t.AccessFlags() )
			qDebug() << "\taccess :" << hex << t.AccessFlags();
	}

    quint64 ios = t.IOS();
    if( ios && !validIoses.contains( ios ) )
    {
		qWarning() << "\tthe IOS for this title is not bootable\n\t" << TidTxt( ios ).insert( 8, "-" );
        return false;
    }

	if( verbose > 1 && upper != 1 )
		qDebug() << "\trequires IOS" << ((quint32)( ios & 0xffffffff ));// << TidTxt( ios ).insert( 8, "-" );
    quint32 uid = uidM.GetUid( tid, false );
    if( !uid )
    {
        qDebug() << "\tthis title has no UID entry";
        return false;
    }

    //make sure all the stuff in this title's data directory belongs to it, and all the contents belong to nobody
    //( im looking at you priibricker & dop-mii )
    QString dataP = tmdp;
    dataP.resize( 25 );
    dataP += "data";
    QTreeWidgetItem *dataI = ItemFromPath( dataP );
    if( dataI )
    {
        quint16 gid = t.Gid();
        QString uidS = QString( "%1" ).arg( uid, 8, 16, QChar( '0' ) );
        QString gidS = QString( "%1" ).arg( gid, 4, 16, QChar( '0' ) );
        if( dataI->text( 3 ) != uidS || !dataI->text( 4 ).startsWith( gidS ) )//dont necessarily fail for this.  the title will still be bootable without its data
			qWarning() << "\tincorrect uid/gid for data folder";

		RecurseCheckGidUid( dataI, uidS, gidS, "data/" );
		/*quint16 cnt = dataI->childCount();
        for( quint16 i = 0; i < cnt; i++ )
        {
            QTreeWidgetItem *item = dataI->child( i );
            if( item->text( 3 ) != uidS || !item->text( 4 ).startsWith( gidS ) )
                qDebug() << "\tincorrect uid/gid for" << QString( "data/" + item->text( 0 ) );
		}*/
    }
    dataP.resize( 25 );
    dataP += "content";
    dataI = ItemFromPath( dataP );
    if( dataI )
    {
        quint16 cnt = dataI->childCount();
        for( quint16 i = 0; i < cnt; i++ )
        {
            QTreeWidgetItem *item = dataI->child( i );
            if( item->text( 3 ) != "00000000" || !item->text( 4 ).startsWith( "0000" ) )
				qWarning() << "\tincorrect uid/gid for" << QString( "content/" + item->text( 0 ) );
        }
    }
    return true;
}

void ListDeletedTitles()
{
	QByteArray uidSys = uidM.Data();
	if( uidSys.isEmpty() )
	{
		qDebug() << "Can\'t check deleted titles without a uid.sys";
		return;
	}
	qDebug() << "Comparing uid.sys against the filesystem...";

	QBuffer buf( &uidSys );
	buf.open( QIODevice::ReadWrite );

	quint64 tid;
	quint16 factory = 0;
	quint32 cnt = uidSys.size() / 12;
	//skip past items installed at the factory
	for( quint32 i = 0; i < cnt; i++ )
	{
		buf.seek( i * 12 );
		buf.read( (char*)&tid, 8 );
		tid = qFromBigEndian( tid );
		quint32 upper = ( ( tid >> 32 ) & 0xffffffff );
		quint32 lower = ( tid & 0xffffffff );
		if( ( upper == 0x10001 && ( ( lower >> 24 ) & 0xff ) != 0x48 ) ||		//a channel, not starting with 'H'
			lower == 0x48415858 ||												//original HBC
			( upper == 0x10000 && ( ( lower & 0xffffff00 ) == 0x555000 ) ) )	//a disc update partition
			break;
		if( ( verbose || upper != 0x10000 ) && !tids.contains( tid ) )
			qDebug().nospace() << "\t" << qPrintable( TidTxt( tid ).insert( 8, "-" ) + ( upper != 1 ? " (" + AsciiTxt( lower ) + ")" : "" ) ) << " was installed at the factory and is now missing";

		factory++;
	}
	if( verbose )
		qDebug() << factory << "titles were installed before any user intervention";
	for( quint32 i = factory; i < cnt; i++ )
	{
		buf.seek( i * 12 );
		buf.read( (char*)&tid, 8 );
		tid = qFromBigEndian( tid );
		if( tids.contains( tid ) )		//this one is already checked
			continue;
		quint32 upper = ( ( tid >> 32 ) & 0xffffffff );
		quint32 lower = ( tid & 0xffffffff );

		bool deleted = false;
		//load tmd
		QString path = QString( "/title/%1/%2/content/title.tmd" ).arg( upper, 8, 16, QChar( '0' ) ).arg( lower, 8, 16, QChar( '0' ) );
		QTreeWidgetItem *item = ItemFromPath( path );
		if( !item )
		{
			if( verbose > 1 )
				qWarning() << "\tCan\'t find TMD for" << qPrintable( TidTxt( tid ).insert( 8, "-" ) + ( upper != 1 ? " (" + AsciiTxt( lower ) + ")" : "" ) );
			deleted = true;
		}
		else
		{
			QByteArray ba = nand.GetData( path );
			if( ba.isEmpty() )
			{
				if( verbose > 1 )
					qWarning() << "\tError reading TMD for" << qPrintable( TidTxt( tid ).insert( 8, "-" ) + ( upper != 1 ? " (" + AsciiTxt( lower ) + ")" : "" ) );
				deleted = true;
			}
			else
			{
				qint32 ch = check_cert_chain( ba );
				switch( ch )
				{
				case ERROR_SIG_TYPE:
				case ERROR_SUB_TYPE:
				case ERROR_RSA_HASH:
				case ERROR_RSA_TYPE_UNKNOWN:
				case ERROR_RSA_TYPE_MISMATCH:
				case ERROR_CERT_NOT_FOUND:
					qWarning().nospace() << "\tTMD RSA signature isn't even close for " << qPrintable( TidTxt( tid ).insert( 8, "-" ) ) << " ( " << ch << " )";
					break;
				case ERROR_RSA_FAKESIGNED:
					qWarning().nospace() << "\tTMD for " << qPrintable( TidTxt( tid ).insert( 8, "-" ) + ( upper != 1 ? " (" + AsciiTxt( lower ) + ")" : "" ) ) << " is fakesigned";
					break;
				default:
					break;
				}
			}
		}
		if( upper != 0x10000 )//dont try to load ticket for disc games
		{
			//load ticket
			path = QString( "/ticket/%1/%2.tik" ).arg( upper, 8, 16, QChar( '0' ) ).arg( lower, 8, 16, QChar( '0' ) );
			item = ItemFromPath( path );
			if( !item )
			{
				if( verbose > 1 )
					qWarning() << "\tCan\'t find ticket for" << qPrintable( TidTxt( tid ).insert( 8, "-" ) + ( upper != 1 ? " (" + AsciiTxt( lower ) + ")" : "" ) );
				deleted = true;
			}
			else
			{
				QByteArray ba = nand.GetData( path );
				if( ba.isEmpty() )
				{
					if( verbose > 1 )
						qWarning() << "\tError reading ticket for" << qPrintable( TidTxt( tid ).insert( 8, "-" ) + ( upper != 1 ? " (" + AsciiTxt( lower ) + ")" : "" ) );
					deleted = true;
				}
				else
				{
					qint32 ch = check_cert_chain( ba );
					switch( ch )
					{
					case ERROR_SIG_TYPE:
					case ERROR_SUB_TYPE:
					case ERROR_RSA_HASH:
					case ERROR_RSA_TYPE_UNKNOWN:
					case ERROR_RSA_TYPE_MISMATCH:
					case ERROR_CERT_NOT_FOUND:
						qWarning().nospace() << "\tticket RSA signature isn't even close for " << qPrintable( TidTxt( tid ).insert( 8, "-" ) ) << " ( " << ch << " )";
						break;
					case ERROR_RSA_FAKESIGNED:
						qWarning().nospace() << "\tticket for " << qPrintable( TidTxt( tid ).insert( 8, "-" ) + ( upper != 1 ? " (" + AsciiTxt( lower ) + ")" : "" ) ) << " is fakesigned";
						break;
					default:
						break;
					}
				}
			}
		}
		if( deleted )
			qWarning().nospace() << "\t" << qPrintable( TidTxt( tid ).insert( 8, "-" ) + ( upper != 1 ? " (" + AsciiTxt( lower ) + ")" : "" ) ) << " has been deleted";
	}
	buf.close();
}

void CheckLostClusters()
{
    QList<quint16> u = nand.GetFatsForEntry( 0 );//all clusters actually used for a file
    qDebug() << "total used clusters" << hex << u.size() << "of 0x8000";
    quint16 lost = 0;
    QList<quint16> ffs;
    QList<quint16> frs;
    fats = nand.GetFats();
    for( quint16 i = 0; i < 0x8000; i++ )
    {
        if( u.contains( fats.at( i ) ) )//this cluster is really used
            continue;
        switch( fats.at( i ) )
        {
        case 0xFFFB:
        case 0xFFFC:
        case 0xFFFD:
            break;
        case 0xFFFE:
            frs << i;
            break;
        case 0xFFFF:
            ffs << i;
            break;
        default:
            lost++;
            //qDebug() << hex << i << fats.at( i );
            break;
        }
    }
    qDebug() << "found" << lost << "lost clusters\nUNK ( 0xffff )" << hex << ffs.size() << ffs <<
            "\nfree           " << frs.size();
}

void CheckEcc()
{
    QList< quint32 > bad;
    QList< quint16 > clusters;
    fats = nand.GetFats();
    quint32 checked = 0;
    quint16 badClustersNotSpecial = 0;
    for( quint16 i = 0; i < 0x8000; i++ )
    {
        if( fats.at( i ) == 0xfffd || fats.at( i ) == 0xfffe )
            continue;
        //qDebug() << hex << i;

        for( quint8 j = 0; j < 8; j++, checked += 8 )
        {
            quint32 page = ( i * 8 ) + j;
            if( !nand.CheckEcc( page ) )
            {
                bad << page;
                if( !clusters.contains( i ) )
                    clusters << i;
            }
        }
    }
    QList< quint16 > blocks;
    QList< quint16 > clustersCpy = clusters;

    while( clustersCpy.size() )
    {
        quint16 p = clustersCpy.takeFirst();
        if( fats.at( p ) < 0xfff0 )
            badClustersNotSpecial ++;
        //qDebug() << p << hex << fats.at( p );
        quint16 block = p/8;
        if( !blocks.contains( block ) )
            blocks << block;
    }
    /*QList< quint32 > badCpy = bad;
    while( badCpy.size() )
    {
	quint16 p = badCpy.takeFirst();
	quint16 block = p/64;
	if( !blocks.contains( block ) )
	    blocks << block;
    }*/
    qDebug() << bad.size() << "out of" << checked << "pages had incorrect ecc.\nthey were spread through"
            << clusters.size() << "clusters in" << blocks.size() << "blocks:\n" << blocks;
    qDebug() << badClustersNotSpecial << "of those clusters are non-special (they belong to the fs)";
    //qDebug() << bad;
}

void SetUpTree()
{
    if( root )
        return;
    QTreeWidgetItem *r = nand.GetTree();
    if( r->childCount() != 1 || r->child( 0 )->text( 0 ) != "/" )
	{
		tryToKeepGoing = false;
        Fail( "The nand FS is seriously broken.  I Couldn't even find a correct root" );
	}

    root = r->takeChild( 0 );
    delete r;
}

int RecurseCountFiles( QTreeWidgetItem *dir )
{
    int ret = 0;
    quint16 cnt = dir->childCount();
    for( quint16 i = 0; i < cnt; i++ )
    {
        QTreeWidgetItem *item = dir->child( i );
        if( item->text( 7 ).startsWith( "02" ) )
            ret += RecurseCountFiles( item );
        else
        {
            ret++;
        }
    }
    return ret;
}

int RecurseCheckHmac( QTreeWidgetItem *dir )
{
    int ret = 0;
    quint16 cnt = dir->childCount();
    for( quint16 i = 0; i < cnt; i++ )
    {
        QTreeWidgetItem *item = dir->child( i );
        if( item->text( 7 ).startsWith( "02" ) )
            ret += RecurseCheckHmac( item );
        else
        {
            bool ok = false;
            quint16 entry = item->text( 1 ).toInt( &ok );
            if( !ok )
                continue;

            if( !nand.CheckHmacData( entry ) )
            {
				qCritical() << "bad HMAC for" << PathFromItem( item );
                ret++;
            }
        }
    }
    return ret;
}

void CheckHmac()
{
    quint16 total = RecurseCountFiles( root );
    qDebug() << "verifying hmac for" << total << "files";
    quint16 bad = RecurseCheckHmac( root );
    qDebug() << bad << "files had bad HMAC data";
    qDebug() << "checking HMAC for superclusters...";
    QList<quint16> sclBad;
    for( quint16 i = 0x7f00; i < 0x8000; i += 0x10 )
    {
        if( !nand.CheckHmacMeta( i ) )
            sclBad << i;
    }
	qDebug() << sclBad.size() << "superClusters had bad HMAC data";
    if( sclBad.size() )
		qCritical() << sclBad;
}

void CheckSettingTxt()
{
	qDebug() << "Checking setting.txt stuff...";
	QByteArray settingTxt = nand.GetData( "/title/00000001/00000002/data/setting.txt" );
	if( settingTxt.isEmpty() )
	{
		Fail( "Error reading setting.txt" );
		return;
	}

	settingTxt = SettingTxtDialog::LolCrypt( settingTxt );
	QString area;
	bool hArea = false;
	bool hModel = false;
	bool hDvd = false;
	bool hMpch = false;
	bool hCode = false;
	bool hSer = false;
	bool hVideo = false;
	bool hGame = false;
	bool shownSetting = false;
	QString str( settingTxt );
	str.replace( "\r\n", "\n" );//maybe not needed to do this in 2 steps, but there may be some reason the file only uses "\n", so do it this way to be safe
	QStringList parts = str.split( "\n", QString::SkipEmptyParts );
	foreach( QString part, parts )
	{
		if( part.startsWith( "AREA=" ) )
		{
			if( hArea ) goto error;
			hArea = true;
			area = part;
			area.remove( 0, 5 );
		}
		else if( part.startsWith( "MODEL=" ) )
		{
			if( hModel ) goto error;
			hModel = true;
		}
		else if( part.startsWith( "DVD=" ) )
		{
			if( hDvd ) goto error;
			hDvd = true;
		}
		else if( part.startsWith( "MPCH=" ) )
		{
			if( hMpch ) goto error;
			hMpch = true;
		}
		else if( part.startsWith( "CODE=" ) )
		{
			if( hCode ) goto error;
			hCode = true;
		}
		else if( part.startsWith( "SERNO=" ) )
		{
			if( hSer ) goto error;
			hSer = true;
		}
		else if( part.startsWith( "VIDEO=" ) )
		{
			if( hVideo ) goto error;
			hVideo = true;
		}
		else if( part.startsWith( "GAME=" ) )
		{
			if( hGame ) goto error;
			hGame = true;
		}
		else
		{
			qDebug() << "Extra stuff in the setting.txt.";
			hexdump( settingTxt );
			qDebug() << QString( settingTxt );
			shownSetting = true;
		}
	}
	//something is missing
	if( !hArea || !hModel || !hDvd || !hMpch || !hCode || !hSer || !hVideo || !hGame )
		goto error;

	//check for opera brick,
	//or in certain cases ( such as KOR area setting on the wrong system menu, a full brick presenting as green & purple garbage instead of the "press A" screen )
	if( sysMenuResource.isEmpty() )
	{
		qCritical() << "Error getting the resource file for the system menu.\nCan\'t check it against setting.txt";
	}
	else
	{
		U8 u8( sysMenuResource );
		QStringList entries = u8.Entries();
		if( !u8.IsOK() || !entries.size() )
		{
			qCritical() << "Error parsing the resource file for the system menu.\nCan\'t check it against setting.txt";
		}
		else
		{
			QString sysMenuPath;
			//these are all the possibilities i saw for AREA in libogc
			if( area == "AUS" || area == "EUR" )																			//supported by 4.3e
				sysMenuPath = "html/EU2/iplsetting.ash/EU/EU/ENG/index01.html";
			else if( area == "USA" || area == "BRA" || area == "HKG" || area == "ASI" || area == "LTN" || area == "SAF" )	//supported by 4.3u
				sysMenuPath = "html/US2/iplsetting.ash/FIX/US/ENG/index01.html";
			else if( area == "JPN" || area == "TWN" || area == "ROC" )														//supported by 4.3j
				sysMenuPath = "html/JP2/iplsetting.ash/JP/JP/JPN/index01.html";
			else if( area == "KOR" )																						//supported by 4.3k
				sysMenuPath = "html/KR2/iplsetting.ash/KR/KR/KOR/index01.html";
			else
				qDebug() << "unknown AREA setting";
			if( !entries.contains( sysMenuPath ) )
			{
				qCritical() << sysMenuPath << "Was not found in the system menu resources, and is needed by the AREA setting" << area;
				Fail( "This will likely result in a full/opera brick" );
			}
			else
			{
				if( verbose )
					qDebug() << "system menu resource matches setting.txt AREA setting.";
			}
		}
	}
	if( verbose )
	{
		shownSetting = true;
		hexdump( settingTxt );
		qDebug() << qPrintable( str );
	}
	return;
error:
	qCritical() << "Something is wrong with this setting.txt";
	if( !shownSetting )
	{
		hexdump( settingTxt );
		qDebug() << qPrintable( str );
	}
}

int main( int argc, char *argv[] )
{
    QCoreApplication a( argc, argv );
#ifndef Q_WS_WIN
	qInstallMsgHandler( DebugHandler );
#endif
	args = QCoreApplication::arguments();

	if( args.contains( "-nocolor", Qt::CaseInsensitive ) )
		color = false;

	if( args.size() < 3 )
        Usage();

    if( !QFile( args.at( 1 ) ).exists() )
        Usage();

    if( !nand.SetPath( args.at( 1 ) ) || !nand.InitNand() )
        Fail( "Error setting path to nand object" );

    root = NULL;

	verbose = args.count( "-v" );

	if( args.contains( "-continue", Qt::CaseInsensitive ) )
		tryToKeepGoing = true;

    //these only serve to show info.  no action is taken
    if( args.contains( "-boot", Qt::CaseInsensitive ) || args.contains( "-all", Qt::CaseInsensitive ) )
    {
        qDebug() << "checking boot1 & 2...";
        ShowBootInfo( nand.Boot1Version(), nand.Boot2Infos() );
    }

    if( args.contains( "-fs", Qt::CaseInsensitive ) || args.contains( "-all", Qt::CaseInsensitive ) )
    {
        qDebug() << "checking uid.sys...";
        QByteArray ba = nand.GetData( "/sys/uid.sys" );
        if( ba.isEmpty() )
            Fail( "No uid map found in the nand" );
        uidM = UIDmap( ba );
        uidM.Check();					//dont really take action, the check should spit out info if there is an error.  not all errors are fatal

        qDebug() << "checking content.map...";
        CheckShared();					//check for the presence of the content.map as well as verify all the hashes

        SetUpTree();

        tids = InstalledTitles();

        qDebug() << "found" << tids.size() << "titles installed";
        BuildGoodIosList();
        qDebug() << "found" << validIoses.size() << "bootable IOS";
        foreach( quint64 tid, tids )
        {
            CheckTitleIntegrity( tid );
            //if( !CheckTitleIntegrity( tid ) && tid == 0x100000002ull )	//well, this SHOULD be the case.  but nintendo doesnt care so much about
            //Fail( "The System menu isnt valid" );			//checking signatures & hashes as the rest of us.
		}
		if( args.contains( "-settingtxt", Qt::CaseInsensitive ) || args.contains( "-all", Qt::CaseInsensitive ) )
		{
			CheckSettingTxt();
		}
		if( args.contains( "-uid", Qt::CaseInsensitive ) || args.contains( "-all", Qt::CaseInsensitive ) )
		{
			ListDeletedTitles();
		}
    }

    if( args.contains( "-clInfo", Qt::CaseInsensitive ) || args.contains( "-all", Qt::CaseInsensitive ) )
    {
        qDebug() << "checking for lost clusters...";
        CheckLostClusters();
    }

    if( args.contains( "-spare", Qt::CaseInsensitive ) || args.contains( "-all", Qt::CaseInsensitive ) )
    {
        qDebug() << "verifying ecc...";
        CheckEcc();

        SetUpTree();
        qDebug() << "verifying hmac...";
        CheckHmac();
    }
    return 0;
}

