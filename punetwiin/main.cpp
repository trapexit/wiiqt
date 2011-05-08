#include "../WiiQt/includes.h"
#include "../WiiQt/nandbin.h"
#include "../WiiQt/tools.h"

//yippie for global variables
QStringList args;
NandBin nandS;//source
NandBin nandD;//dest
QTreeWidgetItem *root = NULL;
quint32 verbose = 0;
bool color = true;
bool testMode = true;
bool CheckTitleIntegrity( quint64 tid );
quint32 fileCnt = 0;
quint32 dirCnt = 0;

#ifdef Q_WS_WIN
#include <windows.h>
#define C_STICKY 31
#define C_CAP    192

int origColor;
HANDLE hConsole;

int GetColor()
{
    WORD wColor = 0;

    HANDLE hStdOut = GetStdHandle( STD_OUTPUT_HANDLE );
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    //We use csbi for the wAttributes word.
    if( GetConsoleScreenBufferInfo( hStdOut, &csbi ) )
    {
        wColor = csbi.wAttributes;
    }
    return wColor;
}

#else
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
    { LEN_STR_PAIR ("m") },			/* rc: Right of color sequence */
    { 0, NULL },					/* ec: End color (replaces lc+no+rc) */
    { LEN_STR_PAIR ("0") },			/* rs: Reset to ordinary colors */
    { 0, NULL },					/* no: Normal */
    { 0, NULL },					/* fi: File: default */
    { LEN_STR_PAIR ("01;34") },		/* di: Directory: bright blue */
    { LEN_STR_PAIR ("01;36") },		/* ln: Symlink: bright cyan */
    { LEN_STR_PAIR ("33") },		/* pi: Pipe: yellow/brown */
    { LEN_STR_PAIR ("01;35") },		/* so: Socket: bright magenta */
    { LEN_STR_PAIR ("01;33") },		/* bd: Block device: bright yellow */
    { LEN_STR_PAIR ("01;33") },		/* cd: Char device: bright yellow */
    { 0, NULL },					/* mi: Missing file: undefined */
    { 0, NULL },					/* or: Orphaned symlink: undefined */
    { LEN_STR_PAIR ("01;32") },		/* ex: Executable: bright green */
    { LEN_STR_PAIR ("01;35") },		/* do: Door: bright magenta */
    { LEN_STR_PAIR ("37;41") },		/* su: setuid: white on red */
    { LEN_STR_PAIR ("30;43") },		/* sg: setgid: black on yellow */
    { LEN_STR_PAIR ("37;44") },		/* st: sticky: black on blue */
    { LEN_STR_PAIR ("34;42") },		/* ow: other-writable: blue on green */
    { LEN_STR_PAIR ("30;42") },		/* tw: ow w/ sticky: black on green */
    { LEN_STR_PAIR ("30;41") },		/* ca: black on red */
    { 0, NULL },					/* mh: disabled by default */
    { LEN_STR_PAIR ("\033[K") },	/* cl: clear to end of line */
};

static void put_indicator( const struct bin_str *ind )
{
    fwrite( ind->string, ind->len, 1, stdout );
}
#endif
void PrintColoredString( const char *msg, int highlite )
{
    if( !color )
    {
        printf( "%s\n", msg );
    }
    else
    {
        QString str( msg );
        QStringList list = str.split( "\n", QString::SkipEmptyParts );
        foreach( QString s, list )
        {
            QString m = s;
            QString m2 = s.trimmed();
            m.resize( m.indexOf( m2 ) );
            printf( "%s", m.toLatin1().data() );				//print all leading whitespace
#ifdef Q_WS_WIN
            SetConsoleTextAttribute( hConsole, highlite );
#else
            put_indicator( &color_indicator[ C_LEFT ] );
            put_indicator( &color_indicator[ highlite ] );		//change color
            put_indicator( &color_indicator[ C_RIGHT ] );
#endif
            printf( "%s", m2.toLatin1().data() );				//print text
#ifdef Q_WS_WIN
            SetConsoleTextAttribute( hConsole, origColor );
#else
            put_indicator( &color_indicator[ C_LEFT ] );
            put_indicator( &color_indicator[ C_NORM ] );		//reset color
            put_indicator( &color_indicator[ C_RIGHT ] );
#endif
            printf( "\n" );
        }
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
        fprintf( stderr, "Fatal Error: %s\n", msg );
        abort();
        break;
    }
}

void Usage()
{
    qWarning() << "usage:" << QCoreApplication::arguments().at( 0 ) << "nand1.bin" << "nand2.bin" << "<other options>";
    qWarning() << "THIS WILL FORMAT \"nand2.bin\"!  It cannot be undone.";
    qDebug() << "By default, this program only runs in \"test mode\".  It only tells you what it would have done.";
    qDebug() << "In order to REALLY run the program, use the option \"-force\".";
    qDebug() << "";
    qDebug() << "";
    qDebug() << "\nOther options:";
    qDebug() << "   -force          run in \"live mode\".  this option is needed to actually do something";
    qDebug() << "";
    qDebug() << "   -nocolor        don\'t use terminal color trickery";
    qDebug() << "";
    qDebug() << "   -about          info about this program";
    exit( 1 );
}

void About()
{
    qCritical()   << "   (c) giantpune 2010, 2011";
    qCritical()   << "   http://code.google.com/p/wiiqt/";
    qCritical()   << "   built:" << __DATE__ << __TIME__;
    qWarning()    << "This software is licensed under GPLv2.  It comes with no guarentee that it will work,";
    qWarning()    << "or that it will work well.";
    qDebug()      << "";
    qDebug()      << "This program is designed to read files from one nand dump and write those files to another nand dump.";
    exit( 1 );
}

void Fail( const QString& str )
{
    qCritical() << str;
        exit( 1 );
}

void SetUpTree()
{
    if( root )
        return;
    QTreeWidgetItem *r = nandS.GetTree();
    if( r->childCount() != 1 || r->child( 0 )->text( 0 ) != "/" )
    {
        Fail( "The nand FS is seriously broken.  I Couldn't even find a correct root" );
    }

    root = r->takeChild( 0 );
    delete r;
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
            //if( verbose )
            //	qWarning() << "ItemFromPath ->item not found" << path;
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

/*
quint16 MainWindow::CreateIfNeeded( const QString &path, quint32 uid, quint16 gid, quint8 attr, quint8 user_perm, quint8 group_perm, quint8 other_perm )
{
    QTreeWidgetItem *item = ItemFromPath( path );
    if( !item )
    {
        quint16 ret = nand.CreateEntry( path, uid, gid, attr, user_perm, group_perm, other_perm );
        if( ret && UpdateTree() )
            return ret;
        return 0;
    }
    return item->text( 1 ).toInt();
}
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
*/
void DumpItem( QTreeWidgetItem *item )
{
    if( !item )
        return;
    qDebug() << item->text( 0 ) << item->text( 1 ) << item->text( 2 ) << item->text( 3 )
            << item->text( 4 ) << item->text( 5 ) << item->text( 6 ) << item->text( 7 );
}

quint8 Attr( QTreeWidgetItem *item )
{
    static bool ok;
    QString attrS = item->text( 7 ).right( 3 );
    attrS.resize( 2 );
    quint8 attr = attrS.toInt( &ok, 16 );
    if( !ok )
    {
        DumpItem( item );
        Fail( "Attr(): error converting " + attrS + " to int" );
    }
    return attr;
}

void CopyItemChildren( QTreeWidgetItem *item )
{
    static bool ok = false;
    quint32 uid;
    quint16 gid;
    quint8 attr;
    quint32 cnt;
    if( !item )
    {
        Fail( "CopyItemChildren() received a NULL item" );
    }
    cnt = item->childCount();

    attr = Attr( item );
    //qDebug() << "CopyItemChildren()" << item->text( 0 ) << hex << attr;
    for( quint32 i = 0; i < cnt; i++ )
    {
        QTreeWidgetItem *ch = item->child( i );
        attr = Attr( ch );
        quint8 type = NAND_ATTR_TYPE( attr );
        quint8 perm1 = NAND_ATTR_USER( attr );
        quint8 perm2 = NAND_ATTR_GROUP( attr );
        quint8 perm3 = NAND_ATTR_OTHER( attr );
        quint16 handle = 0;
        QString path = PathFromItem( ch );

        uid = ch->text( 3 ).toUInt( &ok, 16 );
        if( !ok )
        {
            DumpItem( ch );
            Fail( "error converting UID to u32" );
        }
        gid = ch->text( 4 ).left( 4 ).toUInt( &ok, 16 );
        if( !ok )
        {
            DumpItem( ch );
            Fail( "error converting gid to u16" );
        }

        //qDebug() << ch->text( 0 ) << hex << type << perm1 << perm2 << perm3 << uid << gid;

        if( !testMode )
        {
            handle = nandD.CreateEntry( path, uid, gid, type, perm1, perm2, perm3 );
            if( !handle )
            {
                Fail( "error creating entry in destination nand" );
            }
            printf("\rcreated: %s", path.leftJustified( 60, ' ' ).toLocal8Bit().data() );
            fflush( stdout );
        }
        else
        {
            printf("\rwould create: %s", path.leftJustified( 60, ' ' ).toLocal8Bit().data() );
            fflush( stdout );
        }
        switch( type )
        {
        case NAND_DIR:
            dirCnt++;
            CopyItemChildren( ch );
            break;
        case NAND_FILE:
            {
                fileCnt++;
                quint32 size = ch->text( 2 ).toInt( &ok, 16 );
                if( !ok )
                {
                    DumpItem( ch );
                    Fail( "Error converting size to u32" );
                }
                QByteArray stuff;

                if( size )
                {
                    stuff = nandS.GetData( path );
                    if( (quint32)stuff.size() != size )
                    {
                        DumpItem( ch );
                        qCritical() << hex << (quint32)stuff.size() << "!=" << size;
                        Fail( "Error reading data for " + path );
                    }
                }
                if( !testMode )
                {
                    if( size )
                    {
                        if( !nandD.SetData( handle, stuff ) )
                        {
                            Fail( "Error writing data for " + path );
                        }
                    }
                }
            }
            break;
        default:
            Fail( "unhandled entry type" );
            break;
        }
    }
}

int main( int argc, char *argv[] )
{
    QCoreApplication a( argc, argv );
#ifdef Q_WS_WIN
    origColor = GetColor();
    hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
#endif
    qInstallMsgHandler( DebugHandler );
    args = QCoreApplication::arguments();

    if( args.contains( "-nocolor", Qt::CaseInsensitive ) )
        color = false;

    qCritical() << "** punetwiin: wii nand copierizer **";
    qCritical() << "   from giantpune";
    qCritical() << "   built:" << __DATE__ << __TIME__;

    if( args.contains( "-about", Qt::CaseInsensitive ) )
        About();

    if( args.size() < 3 )
        Usage();

    testMode = !args.contains( "-force", Qt::CaseInsensitive );

    if( !QFile( args.at( 1 ) ).exists() )
    {
        qDebug() << "source nand missing";
        Usage();
    }

    if( !nandS.SetPath( args.at( 1 ) ) || !nandS.InitNand() )
        Fail( "Error setting path to nand object 1" );

    qDebug() << "set source nand path to" << args.at( 1 );

    if( !QFile( args.at( 2 ) ).exists() )
    {
        qDebug() << "dest nand missing";
        Usage();
    }

    if( !nandD.SetPath( args.at( 2 ) ) || !nandD.InitNand() )
        Fail( "Error setting path to nand object 2" );

    qDebug() << "set destination nand path to" << args.at( 2 );
    if( testMode )
    {
        qDebug() << "would format destination nand now...";
    }
    else if( !nandD.Format() )
    {
        Fail( "error formatting destination nand." );
    }
    else
    {
        qDebug() << "destination nand formatted ok.";
    }


    //get the list of contents from the source nand
    SetUpTree();
    printf("\n");

    //start copying these to the destination nand
    CopyItemChildren( root );
    printf("\n");
    qDebug() << "processed" << dirCnt << "directories and" << fileCnt << "files";
    if( !testMode )
    {
        qDebug() << "finalizing...";
        //write metadata
        if( !nandD.WriteMetaData() )
        {
            Fail( "error writing metadata" );
        }
    }





    return 0;
}
