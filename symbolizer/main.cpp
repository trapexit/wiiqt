#include <QtCore/QCoreApplication>

#include "dol.h"
#include "elfparser.h"
#include "ppc_disasm.h"
#include "../WiiQt/includes.h"
#include "../WiiQt/tools.h"

// holds info about a data section that is assumed correct
struct KnownData
{
	quint32 addr;							// memory address
	quint32 len;							// size
	QString name;							// symbol name
	ElfParser::File *file;					// pointer to the file object that contains the data
	KnownData() : addr( 0 ), len( 0 ), file( NULL )
	{
	}
};

// holds info about a function that is assumed correct
struct KnownFunction
{
	QString debug;// just for debugging this program
	const ElfParser::Function *function;
	const ElfParser::File *file;
	const quint32 addr;
	const QString name;// name is only used for functinos that dont have symbols
	KnownFunction( const ElfParser::Function *f = NULL, const ElfParser::File *fi = NULL, quint32 a = 0, const QString &n = QString() )
		: function( f ), file( fi ), addr( a ), name( n )
	{
	}
};

#define GLOBALVAR_MASK( x )		( (quint32)( x & ( PPCAMASK | 0xffff ) ) )
// keep track of known rtoc & r13 references
struct KnownVariable
{
	quint32 sig;							// signature for opcodes dealing with this variable when GLOBALVAR_MASK()'d
											//! given lwz     %r0, -0x5460(%r13), it will store the -0x5460(%r13)
	QString name;							// symbol name
	ElfParser::File *file;					// pointer to the file object that contains the data
};

// holds the info about a dol to use while searching for crap in it
Dol dol;
QStringList dolTextHex;// store a copy of the dol as hex  for easier searching for patterns
QStringList dolDataHex;

// this is a list containing all the parsed libraries
QList< ElfParser::File > libFiles;

// keep track of stuff that matched exactly
QList< KnownData > knownData;
QList< KnownFunction > knownFunctions;

// keep track of variables accessed through r2 and r13
QList< KnownVariable > knownVariables;

// keep a list of the locations that each function's pattern matched to keep from looking them up over and over
QMap< const ElfParser::Function *, QList< quint32 > >patternMatches;

#define DU32( x ) qDebug().nospace() << #x << ": " << hex << (x)

QString NStr( quint32 num, quint8 width = 8 );
QString NStr( quint32 num, quint8 width )
{
	return QString( "%1" ).arg( num, width, 16, QChar( '0' ) );
}

bool IsVariableKnown( quint32 sig, const QList< KnownVariable > &list = knownVariables );
bool IsVariableKnown( quint32 sig, const QList< KnownVariable > &list )
{
	foreach( const KnownVariable &var, list )
	{
		if( var.sig == sig )
		{
			return true;
		}
	}
	return false;
}

// read dol into memory
bool LoadDol( const QString &path )
{
	QByteArray ba = ReadFile( path );
	if( ba.isEmpty() )
	{
		return false;
	}
	if( path.endsWith( ".dol", Qt::CaseInsensitive ) )
	{
		if( !dol.Parse( ba ) )
		{
			return false;
		}
	}
	else
	{
		ba.resize( 0x807B3E88 - 0x80004000 );
		dol = Dol::FakeDol( ba, 0x80004000 );
	}
	foreach( const DolSection &sec, dol.TextSections() )
	{
		QString hexS = sec.data.toHex().toUpper();
		dolTextHex << hexS;
	}
	foreach( const DolSection &sec, dol.DataSections() )
	{
		QString hexS = sec.data.toHex().toUpper();
		dolDataHex << hexS;
	}
	return true;
}

// use objdump on a file and parse its output for elfy goodness
bool ObjDumpLib( const QString &path )
{
	QProcess p;
	QStringList cmd = QStringList() << "-xds" << path;

	p.start( "./objdump", cmd );
	if( !p.waitForStarted() )
	{
		DBG << "!p.waitForStarted()";
		return false;
	}

	if( !p.waitForFinished() )
	{
		DBG << "!p.waitForStarted()";
		return false;
	}
	QByteArray output = p.readAll();
	if( p.exitCode() != QProcess::NormalExit )
	{
		DBG << "p.exitCode() != QProcess::NormalExit";
		return false;
	}

	QString s( output );
	ElfParser parser( s );
	if( parser.Error() )
	{
		WRN << "parser failed";
		return false;
	}

	libFiles << parser.Files();
	return true;
}

// decide if the path is a file or a folder coontaining a bunch of files
bool LoadLibs( const QString &path )
{
	QFileInfo fi( path );
	if( !fi.exists() )
	{
		DBG << "file";
		return false;
	}
	if( fi.isFile() )
	{
		if( !ObjDumpLib( path ) )
		{
			return false;
		}
	}
	else
	{
		QDir dir( path );
		QFileInfoList fil = dir.entryInfoList( QStringList() << "*.a", QDir::Files );
		foreach( const QFileInfo &f, fil )
		{
			// skip debug libs
			if( f.completeBaseName().endsWith( 'D' ) )
			{
				continue;
			}
			if( !ObjDumpLib( f.absoluteFilePath() ) )
			{
				return false;
			}
		}
	}
	return true;
}

// search for a bytearray in another one, but aligned to 4
int AlignedBASearch( const QByteArray &needle, const QByteArray &haystack, qint64 start = 0 );
int AlignedBASearch( const QByteArray &needle, const QByteArray &haystack, qint64 start )
{

	qint64 hSize = haystack.size();
	qint64 nSize = needle.size();

	if( start % 4 || start >= hSize )
	{
		return -1;
	}
	qint64 end = hSize - nSize;
	for( ; start < end; start += 4 )
	{
		qint64 j;
		for( j = 0; j < nSize; j++ )
		{
			if( needle.at( j ) != haystack.at( start + j ) )
			{
				break;
			}
		}
		if( j == nSize )
		{
			return start;
		}
	}
	return -1;
}

void RemoveOverlaps()
{
	QList< const KnownData * > removeData;
	QList< const KnownFunction * > removeFunctions;

	foreach( const KnownFunction &kf1, knownFunctions )
	{
		if( !kf1.function )
		{
			continue;
		}
		quint32 start = kf1.addr;
		quint32 end = start + ( kf1.function->Pattern().size() / 2 );
		foreach( const KnownFunction &kf2, knownFunctions )
		{
			if( !kf2.function )
			{
				continue;
			}
			if( kf1.function == kf2.function )
			{
				continue;
			}
			quint32 start2 = kf2.addr;
			if( start2 >= start && start2 < end )
			{
				removeFunctions << &kf1 << &kf2;
			}
		}
		foreach( const KnownData & kd, knownData )
		{
			quint32 start2 = kd.addr;
			if( start2 >= start && start2 < end )
			{
				removeFunctions << &kf1;
				removeData << &kd;
			}
		}
	}
	// this check works, but when allowing tiny data patterns into the list, it allows some false positives and throws errors
	foreach( const KnownData & kd, knownData )
	{
		quint32 start = kd.addr;
		quint32 end = start + kd.len;
		foreach( const KnownFunction &kf2, knownFunctions )
		{
			if( !kf2.function )
			{
				continue;
			}
			quint32 start2 = kf2.addr;
			if( start2 >= start && start2 < end )
			{
				removeFunctions << &kf2;
				removeData << &kd;
			}
		}
		foreach( const KnownData & kd2, knownData )
		{
			if( kd.name == kd2.name )
			{
				continue;
			}
			quint32 start2 = kd2.addr;
			if( start2 >= start && start2 < end )
			{
				removeData << &kd << &kd2;
			}
		}
	}

	// build new lists and exclude the overlapped stuff
	QList< KnownData > knownData2;
	QList< KnownFunction > knownFunctions2;

	foreach( const KnownFunction &kf, knownFunctions )
	{
		if( !removeFunctions.contains( &kf ) )
		{
			knownFunctions2 << kf;
		}
	}
	foreach( const KnownData & kd, knownData )
	{
		if( !removeData.contains( &kd ) )
		{
			knownData2 << kd;
		}
	}

	knownData = knownData2;
	knownFunctions = knownFunctions2;
}

void AddFunctionToKnownList( const ElfParser::Function *function, const ElfParser::File *file, quint32 addr, const QString &debug = QString() );
void AddFunctionToKnownList( const ElfParser::Function *function, const ElfParser::File *file, quint32 addr, const QString &debug )
{
	foreach( const KnownFunction &kf, knownFunctions )
	{
		if( kf.function == function )
		{
			if( kf.addr != addr )
			{
				DBG << "tried to add" << function->Name() << "to known functions at" << hex << addr << "but it already exists at" << kf.addr;
				// TODO, probably need to remove the existing function from the list
			}
			return;
		}
	}
	KnownFunction kf( function, file, addr );
	kf.debug = debug;
	knownFunctions << kf;
}

void AddFunctionToKnownList( const QString &name, quint32 addr, const QString &debug = QString() );
void AddFunctionToKnownList( const QString &name, quint32 addr, const QString &debug )
{
	foreach( const KnownFunction &kf, knownFunctions )
	{
		if( kf.name == name )
		{
			if( kf.addr != addr )
			{
				DBG << "tried to add" << name << "to known functions at" << hex << addr << "but it already exists at" << kf.addr;
				// TODO, probably need to remove the existing function from the list
			}
			return;
		}
	}
	KnownFunction kf( NULL, NULL, addr, name );
	kf.debug = debug;
	knownFunctions << kf;
}

int PatternSearch( const QString &needle, const QString &haystack, qint64 start = 0 );
int PatternSearch( const QString &needle, const QString &haystack, qint64 start )
{
	qint64 hSize = haystack.size();
	qint64 nSize = needle.size();

	if( start % 8 || start >= hSize )
	{
		return -1;
	}
	qint64 end = hSize - nSize;
	for( ; start < end; start += 8 )
	{
		qint64 j;
		for( j = 0; j < nSize; j++ )
		{
			QChar c = needle.at( j );
			if( c == '.' )
			{
				continue;
			}
			if( c != haystack.at( start + j ) )
			{
				//DBG << "index" << hex << (quint32)j;
				break;
			}
		}
		if( j == nSize )
		{
			return start;
		}
	}
	return -1;
}

const QList< quint32 > &PatternMatches( const ElfParser::Function *fun )
{
	QMap< const ElfParser::Function *, QList< quint32 > >::iterator it = patternMatches.find( fun );
	if( it != patternMatches.end() )
	{
		return it.value();
	}

	QStringList wholeDolHex = dolTextHex + dolDataHex;
	QList< DolSection > wholeDol = dol.TextSections() + dol.DataSections();
	QList< quint32 > addrs;

	// find all pattern matches
	for( int i = 0; i < wholeDol.size(); i++ )
	{
		const QString &line = wholeDolHex.at( i );
		qint64 off2 = PatternSearch( fun->Pattern(), line );
		while( off2 >= 0 )
		{
			// convert to actual address
			quint32 maybeAddr = ( (quint32)off2 / 2 ) + wholeDol.at( i ).addr;

			// add it to the list
			addrs << maybeAddr;
			off2 = PatternSearch( fun->Pattern(), line, off2 + 8 );
		}
	}
	patternMatches[ fun ] = addrs;

	return patternMatches.find( fun ).value();
}

bool PattenMatches( const QString &needle, const QString &haystack, qint64 offset )
{
	qint64 hSize = haystack.size();
	qint64 nSize = needle.size();

	if( nSize > hSize || offset >= hSize )
	{
		return false;
	}
	qint64 j;
	for( j = 0; j < nSize; j++ )
	{
		QChar c = needle.at( j );
		if( c == '.' )
		{
			continue;
		}
		if( c != haystack.at( j + offset ) )
		{
			return false;
		}
	}
	return true;
}

quint32 GetOpcodeFromAddr( quint32 addr )
{
	QList< DolSection > wholeDol = dol.TextSections() + dol.DataSections();
	foreach( const DolSection &sec, wholeDol )
	{
		quint32 end = sec.addr + sec.data.size();
		if( addr >= sec.addr && addr < end )
		{
			quint32 offset = addr - sec.addr;
			return qToBigEndian( *(quint32*)( sec.data.data() + offset ) );
		}
	}
	return 0xdeadbeef;
}

quint32 ResolveBranch( quint32 addr, quint32 opcode )
{
	quint32 ret = opcode & 0x3fffffc;
	if( ret >= 0x2000000 )
	{
		return addr - ( 0x4000000 - ret );
	}
	return addr + ret;
}

bool AddressIsInDol( quint32 addr, quint32 *section = NULL );
bool AddressIsInDol( quint32 addr, quint32 *section )
{
	QList< DolSection > wholeDol = dol.DataSections() + dol.TextSections();
	for( quint32 i = 0; i < (quint32)wholeDol.size(); i++ )
	{
		const DolSection &sec = wholeDol.at( i );
		quint32 end = sec.addr + sec.data.size();
		if( addr >= sec.addr && addr < end )
		{
			if( section )
			{
				*section = i;
			}
			return true;
		}
	}
	return false;
}

bool ListContains( const QList< QPair< const ElfParser::Function *, quint32 > > &list, const ElfParser::Function *fun, quint32 addr )
{
	int s = list.size();
	for( int i = 0; i < s; i++ )
	{
		const QPair< const ElfParser::Function *, quint32 > &p = list.at( i );
		if( p.first == fun && p.second == addr )
		{
			return true;
		}
	}
	return false;
}

void CleanupList( QList< QPair< const ElfParser::Function *, quint32 > > &list )
{
	QList< const ElfParser::Function * >dupFunctions;
	QList< quint32 >dupAddrs;

	QList< QPair< const ElfParser::Function *, quint32 > > ret;

	int s = list.size();
	for( int i = 0; i < s; i++ )
	{
		const QPair< const ElfParser::Function *, quint32 > &p1 = list.at( i );
		for( int j = 0; j < s; j++ )
		{
			const QPair< const ElfParser::Function *, quint32 > &p2 = list.at( j );
			if( p1.second == p2.second )// is address the same
			{
				if( p1.first != p2.first )// is function the same
				{
					dupAddrs << p1.second;// same address but 2 different functions
				}
			}
			else if( p1.first == p2.first )
			{
				dupFunctions << p1.first;// different address but same function
			}
		}
	}

	// now build a new list
	for( int i = 0; i < s; i++ )
	{
		const QPair< const ElfParser::Function *, quint32 > &p1 = list.at( i );

		// this one clashed with another function
		if( dupAddrs.contains( p1.second ) || dupFunctions.contains( p1.first ) )
		{
			continue;
		}

		// make sure this function isnt already in the list
		int t = ret.size();
		bool alreadyHaveIt = false;
		for( int j = 0; j < t; j++ )
		{
			const QPair< const ElfParser::Function *, quint32 > &p2 = ret.at( j );
			if( p1.second == p2.second )
			{
				alreadyHaveIt = true;
				break;
			}
		}
		if( !alreadyHaveIt )
		{
			ret << p1;
		}
	}
	list = ret;
}

void CleanupList( QList< QPair< QString, quint32 > > &list )
{
	QStringList dupNames;
	QList< quint32 >dupAddrs;

	QList< QPair< QString, quint32 > > ret;

	int s = list.size();
	for( int i = 0; i < s; i++ )
	{
		const QPair< QString, quint32 > &p1 = list.at( i );
		for( int j = 0; j < s; j++ )
		{
			const QPair< QString, quint32 > &p2 = list.at( j );
			if( p1.second == p2.second )// is address the same
			{
				if( p1.first != p2.first )// is name the same
				{
					dupAddrs << p1.second;// same address but 2 different names
				}
			}
			else if( p1.first == p2.first )
			{
				dupNames << p1.first;// different address but same names
			}
		}
	}

	// now build a new list
	for( int i = 0; i < s; i++ )
	{
		const QPair< QString, quint32 > &p1 = list.at( i );

		// this one clashed with another function
		if( dupAddrs.contains( p1.second ) || dupNames.contains( p1.first ) )
		{
			continue;
		}

		// make sure this function isnt already in the list
		int t = ret.size();
		bool alreadyHaveIt = false;
		for( int j = 0; j < t; j++ )
		{
			const QPair< QString, quint32 > &p2 = ret.at( j );
			if( p1.second == p2.second )
			{
				alreadyHaveIt = true;
				break;
			}
		}
		if( !alreadyHaveIt )
		{
			ret << p1;
		}
	}
	list = ret;
}

bool FunctionIsKnown( const ElfParser::Function *f )
{
	foreach( const KnownFunction &kf1, knownFunctions )
	{
		if( kf1.function == f )
		{
			return true;
		}
	}
	return false;
}

bool FunctionIsKnown( const QString &str, quint32 *addrOut = NULL );
bool FunctionIsKnown( const QString &str, quint32 *addrOut )
{
	foreach( const KnownFunction &kf1, knownFunctions )
	{
		if( !kf1.function )
		{
			if( kf1.name == str )
			{
				if( addrOut )
				{
					*addrOut = kf1.addr;
				}
				return true;
			}
		}
		else if( kf1.function->Name() == str )
		{
			if( addrOut )
			{
				*addrOut = kf1.addr;
			}
			return true;
		}
	}
	return false;
}


// look for non-function data and try to match it
void TryToMatchData()
{
	QList< quint32 > matchedAddrs;
	QList< quint32 > reusedAddrs;
	QList< KnownData > maybeMatches;

	QList< DolSection > wholeDol = dol.DataSections() + dol.TextSections();

	foreach( const ElfParser::File &f, libFiles )// search each file
	{
		//qDebug() << "f.Name()" << f.Name() << f.Sections().size();
		QMapIterator< QString, QByteArray >it( f.Sections() );// search each data section in each file
		while( it.hasNext() )
		{
			it.next();

			qint64 off = -1;
			quint32 addr = 0;
			bool fail = false;
			foreach( const DolSection &sec, wholeDol )// search in each section of the dol
			{
				//qDebug() << "expected" << sec.data.indexOf( it.value() );
				qint64 off2 = AlignedBASearch( it.value(), sec.data );
				if( off2 < 0 )
				{
					continue;
				}
				//qDebug() << "matched" << it.key();
				qint64 off3 = AlignedBASearch( it.value(), sec.data, off2 + 4 );
				if( off3 > 0 )
				{
					//qDebug() << "matched more than once in 1 section" << it.key();
					continue;
				}
				if( off >= 0 )
				{
					//qDebug() << "matched more than once in 2 section" << it.key();
					fail = true;
					break;
				}
				off = off2;
				addr = sec.addr + off;
			}
			if( fail || off < 0 )// the data section was matched more than 1 time or wasnt matched at all
			{
				//qDebug() << "didnt match" << fail << off;
				continue;
			}
			if( matchedAddrs.contains( addr ) )// something else already matched this location
			{
				//qDebug() << "matched but reused addr";
				reusedAddrs << addr;
				continue;
			}

			// add this to the list of posibilities
			matchedAddrs << addr;
			KnownData kd;
			kd.addr = addr;
			kd.name = it.key();
			kd.len = it.value().size();
			kd.file = (ElfParser::File*)&f;
			maybeMatches << kd;
		}
	}

	// now go back and pick out the ones that are really matches
	//qDebug() << "Matched data sections:";
	foreach( const KnownData &kd, maybeMatches )
	{
		if( reusedAddrs.contains( kd.addr ) )
		{
			continue;
		}
		knownData << kd;
		/*qDebug() << hex << kd.addr << kd.len << kd.name << "from" << kd.file->Name();
  // print aliases
  foreach( const SymAlias &alias, kd.file->Aliases() )
  {
   if( alias.containerName == kd.name )
   {
	qDebug() << hex << "  " << ( kd.addr + alias.offset ) << alias.size << alias.name;
   }
  }*/
	}
	RemoveOverlaps();
}

void TryToMatchFunctions0()
{
	QList<quint32> usedAddrs;
	QList<quint32> dupAddrs;

	QList< QPair< const ElfParser::Function *, quint32> > maybeMatches;// keep track of functions to check

	QMap< const ElfParser::Function *, const ElfParser::File * >fileMap;

	foreach( const ElfParser::File &f, libFiles )// search each file
	{
		foreach( const ElfParser::Function &fun, f.Functions() )// search each function in each file
		{
			if( fun.References().size() )
			{
				continue;
			}
			quint32 addr;
			const QList< quint32 > &addrs = PatternMatches( &fun );
			if( addrs.size() != 1 )
			{
				continue;
			}
			addr = addrs.at( 0 );
			if( usedAddrs.contains( addr ) )
			{
				dupAddrs << addr;
				continue;
			}
			usedAddrs << addr;
			maybeMatches << QPair< const ElfParser::Function *, quint32>( &fun, addr );
			fileMap[ &fun ] = &f;
		}
	}

	//qDebug() << " -- Matched by searching for patterns with no wildcards --";
	int ss = maybeMatches.size();
	for( int i = 0; i < ss; i++ )
	{
		const QPair< const ElfParser::Function *, quint32>&p = maybeMatches.at( i );
		if( dupAddrs.contains( p.second ) )
		{
			//qDebug() << "tossing out" << p.first->Name() << "because addr" << hex << p.second << "is reused";
			continue;
		}
		//qDebug() << hex << p.second << NStr( p.first->Pattern().size() / 2, 4 ) << p.first->Name();
		AddFunctionToKnownList( p.first, fileMap.find( p.first ).value(), p.second, __FUNCTION__ );
	}
	RemoveOverlaps();
}

void TryToMatchFunctions1()
{
	QList< QPair< const ElfParser::Function *, const KnownData *> > maybeMatches;// keep track of functions to check
	QList< QPair< QPair< const ElfParser::Function *, const KnownData *> *, const SymAlias * > > aliasMatches;// keep track of functions to check via aliases

	QMap< const ElfParser::Function *, const ElfParser::File * >fileMap;

	// build a list of all the functions that reference the known data
	foreach( const ElfParser::File &f, libFiles )// search each file
	{
		//qDebug() << "file" << f.Name();
		foreach( const ElfParser::Function &fun, f.Functions() )// search each function in each file
		{
			bool doneWithFunction = false;
			QStringList doneRefs;// keep a list of refs we already tried to match up against

			foreach( const SymRef &ref, fun.References() )// look at each reference from each function
			{
				if( doneWithFunction )
				{
					break;
				}
				//bool aliased = false;
				if( doneRefs.contains( ref.name ) )
				{
					continue;
				}
				doneRefs << ref.name;
				switch( ref.type )
				{
				case SymRef::R_PPC_EMB_SDA21:// not sure how to handle these 2 types, so skip them for now
				case SymRef::R_PPC_SDAREL16:
				case SymRef::R_PPC_WTF:
				{
					continue;
				}
				break;
				default:
					break;
				}

				//qDebug() << "    " << ref.name;
				foreach( const KnownData &kd, knownData )// see if this function references a known symbol
				{
					if( kd.file != (ElfParser::File*)&f )
					{
						continue;
					}
					//qDebug() << "      " << kd.name;
					if( kd.name == ref.name )
					{
						//qDebug() << "function:" << fun.Name() << "references" << kd.name << kd.file->Name();
						//qDebug() << ref.name;
						maybeMatches << QPair< const ElfParser::Function *, const KnownData *>( &fun, &kd );
						doneWithFunction = true;
						break;
					}

					foreach( const SymAlias &alias, f.Aliases() )
					{
						bool ok = false;
						if( alias.containerName == kd.name && alias.name == ref.name )
						{
							//qDebug() << "function:" << fun.Name() << "references" << kd.name << "through alias" << alias.name << " in" << kd.file->Name();
							//qDebug() << "container str" << alias.containerName;
							QPair< const ElfParser::Function *, const KnownData *> np( &fun, &kd );
							maybeMatches << np;
							QPair< QPair< const ElfParser::Function *, const KnownData *> *, const SymAlias * > ap( &np, &alias );
							aliasMatches << ap;
							doneWithFunction = true;
							break;
						}
						if( ok )
						{
							break;
						}
					}
				}
				if( doneWithFunction )
				{
					fileMap[ &fun ] = &f;
				}
			}
		}
	}

	// now search the dol and see if the possible matches will fit
	int listSize = maybeMatches.size();
	QList< QPair< const ElfParser::Function *, quint32 > > probablyMatches;
	for( int j = 0; j < listSize; j++ )
	{
		const QPair< const ElfParser::Function *, const KnownData *> &it = maybeMatches.at( j );

		// look for a possible match for this function
		const QList< quint32 > &addrs = PatternMatches( it.first );
		foreach( quint32 addr, addrs )
		{
			//qDebug() << "using address" << hex << addr << "for" << it.first->Name();
			foreach( const SymRef &ref, it.first->References() )
			{
				switch( ref.type )
				{
				case SymRef::R_PPC_EMB_SDA21:// not sure how to handle these 2 types, so skip them for now
				case SymRef::R_PPC_SDAREL16:
				case SymRef::R_PPC_WTF:
				{
					//qDebug() << "skipped due to ref type" << ref.type;
					continue;
				}
				break;
				default:
					break;
				}

				bool fail = false;

				quint32 refOff = ref.off;
				quint32 aliasDiff = 0;

				if( ref.name != it.second->name )// wrong reference
				{
					//qDebug() << "  ref.name" << ref.name;
					//qDebug() << it.second->name << "wasn't found.  checking aliases";
					bool ok = false;
					int s = aliasMatches.size();
					for( int ss = 0; ss < s; ss++ )
					{
						const QPair< QPair< const ElfParser::Function *, const KnownData *> *, const SymAlias * > &aliasMatch = aliasMatches.at( ss );
						if( it.second->name == aliasMatch.second->containerName )
						{
							//qDebug() << "search using alias" << aliasMatch.second->name;
							aliasDiff = aliasMatch.second->offset;
							ok = true;
							break;
						}
					}
					if( !ok )
					{
						continue;
					}
				}

				quint32 codeOff = ( addr + refOff ) & ~3;
				quint32 opcode = GetOpcodeFromAddr( codeOff );
				//qDebug() << "possible match" << it.first->Name();

				switch( ref.type )
				{
				case SymRef::R_PPC_ADDR16_HI:// upper 16 bits
				{
					if( ( opcode & 0xffff ) != ( ( ( it.second->addr + ref.symOff + aliasDiff ) & 0xffff0000 ) >> 16 ) )
					{
						fail = true;
						//qDebug() << "bad high" << hex <<  opcode << refOff << ref.name << ref.symOff;
						//qDebug() << hex << "expected" << (quint32)( ( ( it.second->addr + ref.symOff + aliasDiff ) & 0xffff0000 ) >> 16 );
						//DumpRefs( *( it.first ) );
					}
				}
				break;
				case SymRef::R_PPC_ADDR16_LO:// lower 16 bits
				{
					if( ( opcode & 0xffff ) != ( (it.second->addr + ref.symOff + aliasDiff ) & 0xffff ) )
					{
						fail = true;
						//qDebug() << "bad low" << hex <<  opcode << refOff << ref.name << ref.symOff;
						//qDebug() << hex << "expected" << (quint32)( ( (it.second->addr + ref.symOff + aliasDiff ) & 0xffff ) );
						//DumpRefs( *( it.first ) );
					}
				}
				break;
				case SymRef::R_PPC_REL24:// branch
				{
					quint32 res = ResolveBranch( addr + ( refOff & ~3 ), opcode );
					if( !AddressIsInDol( res ) )// just make sure the branch is inside the dol for now.  no functions are actually known
					{
						fail = true;
						//qDebug() << "bad dranch" << hex << res << opcode << it.second->addr << ref.name;
					}
				}
				break;
				default:
					continue;
					break;
				}
				//qDebug() << "fakematch" << hex << addr << it.first->Name();

				// if we found a possible match and we dont already have this one
				if( !fail && !ListContains( probablyMatches, it.first, addr ) )
				{
					probablyMatches << QPair< const ElfParser::Function *, quint32 > ( it.first, addr );
				}
			}
		}
	}



	// cleanup the list
	CleanupList( probablyMatches );
	int s = probablyMatches.size();
	//qDebug() << " -- Functions matched by data references --";
	for( int i = 0; i < s; i++ )
	{
		const QPair< const ElfParser::Function *, quint32 > &p = probablyMatches.at( i );
		//qDebug() << hex << p.second << NStr( p.first->Pattern().size() / 2, 4 ) << p.first->Name();
		AddFunctionToKnownList( p.first, fileMap.find( p.first ).value(), p.second, __FUNCTION__ );
	}
	RemoveOverlaps();
}

void FindGlobalVariables()
{
	QList< QPair< const ElfParser::Function *, const KnownData *> > maybeMatches;// keep track of functions to check
	//QList< QPair< QPair< const ElfParser::Function *, const KnownData *> *, const SymAlias * > > aliasMatches;// keep track of functions to check via aliases

	QMap< const ElfParser::Function *, const ElfParser::File * >fileMap;
	QMap< const ElfParser::Function *, quint32 >winners;

	QList< KnownVariable > newVariables;

	foreach( const KnownFunction &kf1, knownFunctions )// look in all known functions for global variables
	{
		if( !kf1.file )
		{
			continue;
		}
		foreach( const SymRef &ref, kf1.function->References() )
		{
			if( ref.type != SymRef::R_PPC_EMB_SDA21 )
			{
				continue;
			}
			quint32 addr = kf1.addr + ( ref.off & ~3 );
			quint32 opcode = GetOpcodeFromAddr( addr );
			if( opcode == 0xdeadbeef )
			{
				DBG << "opcode" << hex << opcode;
				continue;
			}
			quint32 reg = (quint32)PPCGETA( opcode );
			if( reg != 2 && reg != 13 )
			{
				DBG << "reg:" << hex << reg << kf1.function->Name() << ref.name;
				continue;
			}
			quint32 sig = GLOBALVAR_MASK( opcode );
			if( IsVariableKnown( sig, newVariables ) || IsVariableKnown( sig ) )
			{
				continue;
			}
			KnownVariable nw;
			nw.file = (ElfParser::File *)kf1.file;
			nw.name = ref.name;
			nw.sig = sig;
			newVariables << nw;

			/*qDebug() << "opcode" << hex << opcode << "addr" << addr;
			qDebug() << kf1.function->Name() << ref.name;
			qDebug();
			quint32 z = GLOBALVAR_MASK( opcode );
			opcode = z;
			quint32 s = (quint32)PPCGETD( opcode );
			quint32 a = (quint32)PPCGETA( opcode );
			quint32 d = (quint32)( opcode & 0xffff );
			quint32 o = (quint32)( PPCGETIDX( opcode ) - 32 );
			DU32( o );
			DU32( d );
			DU32( s );
			DU32( a );*/
			//exit( 0 );
		}
	}

	// look at all the variables and see if there is an unknown function in that file that refers to the variable

	QList< const ElfParser::Function * > bitches;
	foreach( const KnownVariable &var, newVariables )
	{
		foreach( const ElfParser::Function &fun, var.file->Functions() )
		{
			if( FunctionIsKnown( &fun ) )
			{
				continue;
			}
			foreach( const SymRef &ref, fun.References() )
			{
				if( ref.type != SymRef::R_PPC_EMB_SDA21 )
				{
					continue;
				}
				if( ref.name != var.name )
				{
					continue;
				}
				if( !bitches.contains( &fun ) )
				{
					bitches << &fun;
					fileMap[ &fun ] = var.file;
				}
			}
		}
	}

	// now look at all the references to global variables in each found function and if it refers to a known one,
	// check the opcode and see that it is correct
	QMapIterator< const ElfParser::Function *, const ElfParser::File * > it( fileMap );
	while( it.hasNext() )
	{
		it.next();
		QList< quint32 > addrs = PatternMatches( it.key() );
		quint32 winner = 0;
		bool functionFailed = false;
		foreach( quint32 addr, addrs )
		{
			bool fail = false;
			foreach( const SymRef &ref, it.key()->References() )
			{
				if( ref.type != SymRef::R_PPC_EMB_SDA21 )
				{
					continue;
				}
				quint32 varSig = 0xb00b5;
				foreach( const KnownVariable &kv, newVariables )
				{
					if( kv.file == it.value() && kv.name == ref.name )
					{
						varSig = kv.sig;
						break;
					}
				}
				if( varSig == 0xb00b5 )// we dont know this variable
				{
					continue;
				}
				quint32 opAddr = addr + ( ref.off & ~3 );
				quint32 opcode = GetOpcodeFromAddr( opAddr );
				if( opcode == 0xdeadbeef )
				{
					DBG << "opcode" << hex << opcode;
					continue;
				}
				if( GLOBALVAR_MASK( opcode ) != varSig )
				{
					fail = true;
					break;
				}
			}
			if( fail )
			{
				continue;
			}
			if( winner )// found more than 1 match for this little guy
			{
				functionFailed = true;
				break;
			}
			winner = addr;
		}
		if( winner && !functionFailed )
		{
			winners[ it.key() ] = winner;
		}
	}

	knownVariables << newVariables;

	//DBG << "added these bad boys";
	QMapIterator< const ElfParser::Function *, quint32 > ret( winners );
	while( ret.hasNext() )
	{
		ret.next();
		//qDebug() << hex << ret.value()
		//		 << NStr( ret.key()->Pattern().size() / 2, 4 )
		//		 << ret.key()->Name()
		//		 << fileMap.find( ret.key() ).value()->Name();
		AddFunctionToKnownList( ret.key(), fileMap.find( ret.key() ).value(), ret.value(), __FUNCTION__ );
	}
	RemoveOverlaps();

}

void TryToMatchFunctions2( QMap< const ElfParser::Function *, quint32 > &nonMatchingBranches )
{
	QStringList wholeDolHex = dolDataHex + dolTextHex;
	QList< DolSection > wholeDol = dol.DataSections() + dol.TextSections();
	QMap< const ElfParser::Function *, const ElfParser::File * >fileMap;
	QList< QPair< const ElfParser::Function *, quint32 > > probablyMatches;// these have symbols


	QList< QPair< QString, quint32 > > probablyMatches2;// these have no symbols

	foreach( const KnownFunction &kf, knownFunctions )
	{
		if( !kf.function )// wont have these for functions we dont have symbols for
		{
			continue;
		}
		const ElfParser::Function * fun = kf.function;

		QStringList doneRefs;

		foreach( const SymRef &ref, fun->References() )// look at each reference from each function
		{
			switch( ref.type )
			{
			case SymRef::R_PPC_REL24:// we only care about branches right now
				break;
			default:
				continue;
				break;
			}

			if( doneRefs.contains( ref.name ) )// dont check branches to the same function from within the same calling function
			{
				continue;
			}
			doneRefs << ref.name;

			bool alreadyKnown = false;
			foreach( const KnownFunction &kf, knownFunctions )// dont bother checking branches if we already know the function it is branching to
			{
				if( !kf.function )// wont have these for functions we dont have symbols for
				{
					if( ref.name == kf.name )
					{
						alreadyKnown = true;
						break;
					}
					continue;
				}
				if( kf.function->Name() == ref.name )
				{
					alreadyKnown = true;
					break;
				}
			}
			if( alreadyKnown )
			{
				continue;
			}


			quint32 addr = kf.addr + ref.off;
			quint32 opcode = GetOpcodeFromAddr( addr );
			if( opcode == 0xdeadbeef )
			{
				DBG << "error getting opcode from" << hex << addr << fun->Name();
				break;
			}
			quint32 res = ResolveBranch( addr , opcode );
			quint32 dolIdx;

			if( !AddressIsInDol( res, &dolIdx ) )// make sure the branch is inside the dol
			{
				break;
			}
			//qDebug() << hex << res << ref.name << "from" << kf.addr << fun->Name() << addr << opcode;

			bool branchHasSymbols = false;
			bool skipIt = false;

			foreach( const ElfParser::File &f, libFiles )
			{
				//qDebug() << "f.Name():" << f.Name();
				foreach( const ElfParser::Function &fun2, f.Functions() )
				{
					//qDebug() << "    fun.Name():" << fun.Name() << ref.name;

					if( fun2.Name() == ref.name )
					{
						branchHasSymbols = true;
						if( nonMatchingBranches.find( &fun2 ) != nonMatchingBranches.end() )// already looked for this one
						{
							skipIt = true;
							break;
						}
						qint64 textOffset = res - wholeDol.at( dolIdx ).addr;
						textOffset *= 2;
						if( PattenMatches( fun2.Pattern(), wholeDolHex.at( dolIdx ), textOffset ) )
						{
							probablyMatches << QPair< const ElfParser::Function *, quint32 >( &fun2, res );
							fileMap[ &fun2 ] = &f;
						}
						else
						{
							/*if( fun2.Name() == "NANDPrivateCreateAsync" )
							{
								qDebug() << "expected" << fun2.Name() << "at" << hex << res << "but pattern didnt match";
								qDebug() << "being called from" << fun->Name() << "at" << hex << addr;
								qDebug() << "offset" << NStr( textOffset ) << "in section" << dolIdx;
								qDebug() << fun2.Pattern();
								qDebug() << wholeDolHex.at( dolIdx ).mid( textOffset, fun2.Pattern().size() );
								exit( 0 );

							}*/
							//qDebug() << "expected" << fun2.Name() << "at" << hex << res << "but pattern didnt match";
							//qDebug() << "being called from" << fun->Name() << "at" << hex << addr;
							nonMatchingBranches[ &fun2 ] = res;
						}
						break;
					}
				}
				if( skipIt )
				{
					break;
					//continue;
				}
				if( branchHasSymbols )
				{
					break;
				}
			}
			if( !branchHasSymbols )// we don't have any symbols for this function.  but just assume it is right for now
			{
				probablyMatches2 << QPair< QString, quint32 >( ref.name, res );
			}
		}

	}
	// cleanup the lists
	CleanupList( probablyMatches );
	int s = probablyMatches.size();
	for( int i = 0; i < s; i++ )
	{
		const QPair< const ElfParser::Function *, quint32 > &p = probablyMatches.at( i );
		//qDebug() << hex << p.second << p.first->Name();
		AddFunctionToKnownList( p.first, fileMap.find( p.first ).value(), p.second, __FUNCTION__ );
	}

	CleanupList( probablyMatches2 );
	//qDebug() << " -- Functions matched by branches from known functions --";
	s = probablyMatches2.size();
	for( int i = 0; i < s; i++ )
	{
		const QPair< QString, quint32 > &p = probablyMatches2.at( i );
		//qDebug() << hex << p.second << p.first;
		AddFunctionToKnownList( p.first, p.second, __FUNCTION__ );
	}
	RemoveOverlaps();
}

QList< QPair< const ElfParser::Function *, quint32> > TryToMatchFunctions3( QList< const ElfParser::Function * > &ignoreFunctions )
{
	QMap< const ElfParser::Function *, const ElfParser::File * >fileMap;

	QList<quint32> usedAddrs;
	QList<quint32> dupAddrs;

	QList< QPair< const ElfParser::Function *, quint32> > maybeMatches;// keep track of functions to check

	foreach( const ElfParser::File &f, libFiles )// search each file
	{
		foreach( const ElfParser::Function &fun, f.Functions() )// search each function in each file
		{
			if( ignoreFunctions.contains( &fun ) ||  FunctionIsKnown( &fun ) )
			{
				continue;
			}

			// keep a list of addresses of the functions this one branches to
			QMap< QString, quint32 >refAddrs;
			QStringList doneRefs;// keep a list of refs we already tried to match up against

			foreach( const SymRef &ref, fun.References() )// look at each reference from each function
			{
				if( ref.type != SymRef::R_PPC_REL24 || doneRefs.contains( ref.name ) )
				{
					continue;
				}
				doneRefs << ref.name;

				quint32 expectedAddr;
				if( FunctionIsKnown( ref.name, &expectedAddr ) )
				{
					refAddrs[ ref.name ] = expectedAddr;
				}
			}

			// this function doesnt branch to any known ones
			if( !refAddrs.size() )
			{
				continue;
			}

			// find a pattern match
			const QList< quint32 > &addrs = PatternMatches( &fun );
			foreach( quint32 maybeAddr, addrs )
			{
				bool fail = false;
				doneRefs.clear();
				foreach( const SymRef &ref, fun.References() )// look at each reference from each function
				{
					if( ref.type != SymRef::R_PPC_REL24 || doneRefs.contains( ref.name ) )
					{
						continue;
					}
					doneRefs << ref.name;

					QMap< QString, quint32 >::iterator refAddr = refAddrs.find( ref.name );
					if( refAddr == refAddrs.end() )
					{
						continue;
					}
					quint32 branchFromAddr = maybeAddr + ref.off;
					quint32 opcode = GetOpcodeFromAddr( branchFromAddr );
					if( opcode == 0xdeadbeef )
					{
						DBG << "error getting opcode from" << hex << branchFromAddr << fun.Name() << "ref" << ref.name;
						fail = true;
						break;
					}
					quint32 res = ResolveBranch( branchFromAddr , opcode );
					if( res != refAddr.value() )
					{
						fail = true;
						break;
					}
				}
				if( !fail )// all the branches from this function land on the expected other function
				{
					if( usedAddrs.contains( maybeAddr ) )
					{
						dupAddrs << maybeAddr;
						ignoreFunctions << &fun;
					}
					else
					{
						maybeMatches << QPair< const ElfParser::Function *, quint32 >( &fun, maybeAddr );
						fileMap[ &fun ] = &f;
					}
					usedAddrs << maybeAddr;
				}
				else
				{
					ignoreFunctions << &fun;
				}
			}
		}
	}


	// cleanup the list
	CleanupList( maybeMatches );
	int s = maybeMatches.size();
	//qDebug() << "Functions matched by branch references:";
	for( int i = 0; i < s; i++ )
	{
		const QPair< const ElfParser::Function *, quint32 > &p = maybeMatches.at( i );
		//qDebug() << hex << p.second << NStr( p.first->Pattern().size() / 2, 4 ) << p.first->Name();

		AddFunctionToKnownList( p.first, fileMap.find( p.first ).value(), p.second, __FUNCTION__ );
	}
	RemoveOverlaps();
	return maybeMatches;
}

// looks for functions that DO contain a wildcard and only have 1 pattern match
QList< QPair< const ElfParser::Function *, quint32> > TryToMatchFunctions4( QList< const ElfParser::Function * > &ignoreFunctions, quint32 minLen )
{
	QMap< const ElfParser::Function *, const ElfParser::File * >fileMap;

	QList< QPair< const ElfParser::Function *, quint32> > maybeMatches;// keep track of functions to check

	foreach( const ElfParser::File &f, libFiles )// search each file
	{
		foreach( const ElfParser::Function &fun, f.Functions() )// search each function in each file
		{
			if( ignoreFunctions.contains( &fun ) ||  FunctionIsKnown( &fun ) )
			{
				continue;
			}
			quint32 len = fun.Pattern().length() / 2;
			if( len < minLen )
			{
				continue;
			}

			const QList< quint32 > &addrs = PatternMatches( &fun );
			if( addrs.size() != 1 )
			{
				continue;
			}
			quint32 addr = addrs.at( 0 );

			quint32 fEnd = addr + len;


			// see if inserting this function at this address will colide with any existing known data
			bool fail = false;
			foreach( const KnownFunction &kf1, knownFunctions )
			{
				if( !kf1.function )
				{
					continue;
				}
				quint32 start = kf1.addr;
				quint32 end = start + ( kf1.function->Pattern().size() / 2 );
				if( ( start >= addr && end < fEnd )
						|| ( addr >= start && fEnd < end ) )
				{
					fail = true;
					break;
				}
			}
			if( fail )
			{
				continue;
			}
			maybeMatches << QPair< const ElfParser::Function *, quint32 >( &fun, addr );
			fileMap[ &fun ] = &f;
		}
	}


	// cleanup the list
	CleanupList( maybeMatches );
	int s = maybeMatches.size();
	//qDebug() << "Functions that only have 1 pattern match, contain wildcards, and are larger than 0x" << hex << minLen << "bytes:";
	for( int i = 0; i < s; i++ )
	{
		const QPair< const ElfParser::Function *, quint32 > &p = maybeMatches.at( i );
		//qDebug() << hex << p.second << NStr( p.first->Pattern().size() / 2, 4 ) << p.first->Name();

		AddFunctionToKnownList( p.first, fileMap.find( p.first ).value(), p.second, __FUNCTION__ );
	}
	RemoveOverlaps();
	return maybeMatches;
}

#define INDENT_TXT	QString( "    " )

QString CleanupNameString( const QString &name )// gcc puts the section name at the front of the user-given name
{
	if( name.startsWith( ".sbss." ) )
	{
		return name.mid( 6 );
	}
	if( name.startsWith( ".rodata." ) )
	{
		return name.mid( 8 );
	}
	if( name.startsWith( ".sdata." ) )
	{
		return name.mid( 7 );
	}
	if( name.startsWith( ".data." ) )
	{
		return name.mid( 6 );
	}
	if( name.startsWith( ".text." ) )
	{
		return name.mid( 6 );
	}
	return name;
}

QString MakeIDC( const QString &dolPath, const QString &libPath, const QMap< const ElfParser::Function *, quint32 > &nonMatchingBranches )
{
	QString ret = QString(
				"/***********************************************************\n"
				"* This file was created automatically with\n"
				"* DolPath: \"%1\"\n"
				"* LibPath: \"%2\"\n"
				"***********************************************************/\n"
				"\n"
				"#include <idc.idc>\n"
				"\n" )
			.arg( QFileInfo( dolPath ).absoluteFilePath() )
			.arg( QFileInfo( libPath ).absoluteFilePath() );

	QString makeCode =
			"static CreateFunction( addr, len, name )\n"
			"{\n"
			+ INDENT_TXT + "MakeCode( addr );\n"
			+ INDENT_TXT + "MakeFunction( addr, len );\n"
			+ INDENT_TXT + "MakeName( addr, name );\n"
			+"}\n\n";
	bool insertedMakeCode = false;
	bool haveData = knownData.size() != 0;
	if( haveData )
	{
		ret += "static DoData()\n{\n";
		foreach( const KnownData &kd, knownData )
		{
			QString line;
			bool havealias = false;
			foreach( const SymAlias &alias, kd.file->Aliases() )
			{
				if( alias.containerName == kd.name )
				{
					havealias = true;
					break;
				}
			}
			//TODO - maybe create data types like strings and words and stuff?


			line += INDENT_TXT + QString( "MakeComm( 0x%1, \"File   : %2\\nSection: %3\\nLen    : 0x%4\" );\n" )
					.arg( kd.addr, 8, 16, QChar( '0' ) ).arg( kd.file->Name() )
					.arg( kd.name ).arg( kd.len, 0, 16, QChar( '0' ) );
			if( havealias )
			{
				foreach( const SymAlias &alias, kd.file->Aliases() )
				{
					if( alias.containerName == kd.name )
					{
						line += INDENT_TXT + QString( "MakeName( 0x%1, \"%2\" );\n" )
								.arg( kd.addr + alias.offset, 8, 16, QChar( '0' ) ).arg( alias.name );
					}
				}
			}
			else
			{
				line += INDENT_TXT + QString( "MakeName( 0x%1, \"%2\" );\n" )
						.arg( kd.addr, 8, 16, QChar( '0' ) ).arg( kd.name );
			}

			ret += line;
		}

		ret += "\n}\n\n";
	}
	bool haveKnownFunctions = knownFunctions.size() != 0;
	if( haveKnownFunctions )
	{
		insertedMakeCode = true;
		ret += makeCode;
		ret += "static DoKnownFunctions()\n{\n";
		foreach( const KnownFunction &kf, knownFunctions )
		{
			QString line;
			if( kf.function )
			{
				line += INDENT_TXT + QString( "CreateFunction( 0x%1, 0x%2, \"%3\" );  " )
						.arg( kf.addr, 8, 16, QChar( '0' ) ).arg( kf.function->Pattern().size() / 2, 4, 16, QChar( '0' ) )
						.arg( CleanupNameString( kf.function->Name() ) );
				if( kf.file->Name() != libPath )
				{
					line += QString( "MakeComm( 0x%1, \"File: %2\" );" )
							.arg( kf.addr, 8, 16, QChar( '0' ) ).arg( kf.file->Name() );
				}

			//	line += "  // " + kf.debug;

				line += '\n';

				// do something cool here with the r13/rtoc references
				foreach( const SymRef &ref, kf.function->References() )
				{
					if( ref.type == SymRef::R_PPC_EMB_SDA21 )
					{
						line += INDENT_TXT + INDENT_TXT + QString( "MakeComm( 0x%1, \"%2\" );\n" )
								.arg( kf.addr + ( ref.off & ~3 ), 8, 16, QChar( '0' ) )
								.arg( CleanupNameString( ref.name ) );
					}
				}
			}
			else
			{
				line += INDENT_TXT + QString( "CreateFunction( 0x%1, BADADDR, \"%2\" );" )
						.arg( kf.addr, 8, 16, QChar( '0' ) )
						.arg( CleanupNameString( kf.name ) );

			//	line += "  // " + kf.debug;

				line += '\n';
			}
			ret += line;
		}

		ret += "\n}\n";
	}
	bool haveMaybeFunctions = nonMatchingBranches.size() != 0;
	if( haveMaybeFunctions )
	{
		if( !insertedMakeCode )
		{
			ret += makeCode;
			insertedMakeCode = true;
		}
		ret += "\nstatic DoMaybeFunctions()\n{\n";
		QMapIterator< const ElfParser::Function *, quint32 > it( nonMatchingBranches );
		while( it.hasNext() )
		{
			it.next();
			ret += INDENT_TXT + QString( "CreateFunction( 0x%1, BADADDR, \"%2\" );\n" )	// BADADDR because we cant trust the pattern size when it doesnt match the expected
					.arg( it.value(), 8, 16, QChar( '0' ) )
					.arg( CleanupNameString( it.key()->Name() ) );
			// do something cool here with the r13/rtoc references
			// these cant be trusted since the functions here dont match expected patterns
			/*foreach( const SymRef &ref, it.key()->References() )
			{
				if( ref.type == SymRef::R_PPC_EMB_SDA21 )
				{
					ret += INDENT_TXT + INDENT_TXT + QString( "MakeComm( 0x%1, \"%2\" );\n" )
							.arg( it.value() + ( ref.off & ~3 ), 8, 16, QChar( '0' ) )
							.arg( CleanupNameString( ref.name ) );
				}
			}*/
		}
		ret += "\n}\n";
	}

	// create the main()
	ret += "\nstatic main()\n{\n";
	if( haveData )
	{
		ret += INDENT_TXT + "DoData();\n";
	}
	if( haveKnownFunctions )
	{
		ret += INDENT_TXT + "DoKnownFunctions();\n";
	}
	if( haveMaybeFunctions )
	{
		ret += INDENT_TXT + "DoMaybeFunctions();\n";
	}
	ret += "}\n\n";

	return ret;
}

void Usage()
{
	qDebug() << "Usage:";
	qDebug() << "";
	qDebug() << "symbolize <dol file> <lib path> <output name>";
	qDebug() << "";
	qDebug() << "  this program requires objdump built for ppc in the same folder";

	exit( 1 );
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

	if( argc < 4 )
	{
		Usage();
	}

	QString dolPath( argv[ 1 ] );
	QString libPath( argv[ 2 ] );
	QString outName( argv[ 3 ] );

	qDebug() << "Loading dol...";
	if( !LoadDol( dolPath ) )
	{
		exit( 0 );
	}

	qDebug() << "Loading libs...";
	if( !LoadLibs( libPath ) )
	{
		exit( 0 );
	}

	// this is a list of functions that are branched to, but dont match the patterns read from the libs
	QMap< const ElfParser::Function *, quint32 > nonMatchingBranches;

	// find unique data symbols

	qDebug() << "matching data...";
	TryToMatchData();

	// find first round of functions
	TryToMatchFunctions1();

	// add functions by looking at rtoc and r13
	FindGlobalVariables();

	// find branches from the first round of functions
	TryToMatchFunctions2( nonMatchingBranches );

	FindGlobalVariables();

	// looking for functions that dont branch anywhere or use global variables or anything
	TryToMatchFunctions0();

	int maxRetries = 10;
	QList< const ElfParser::Function * > ignoreFunctions;
	for( int i = 0; i < maxRetries; i++ )
	{
		qDebug() << " -- Round" << i << '/' << maxRetries << "--";
		// look for functions that branch to known functions
		QList< QPair< const ElfParser::Function *, quint32> > newFunctions = TryToMatchFunctions3( ignoreFunctions );
		if( !newFunctions.size() )
		{
			qDebug() << "no new functions found" << i;
			break;
		}
		qDebug() << " - added" << newFunctions.size() << "new functions -";
	}

	// add functions by looking at rtoc and r13
	FindGlobalVariables();



	qDebug() << "Total functions found:" << knownFunctions.size();

	// find branches from the known functions
	int num = knownFunctions.size();
	for( int i = 0; i < maxRetries; i++ )
	{
		qDebug() << " -- Round" << i << '/' << maxRetries << " following branches --";
		TryToMatchFunctions2( nonMatchingBranches );

		int num2 = knownFunctions.size();
		if( num2 == num )
		{
			break;
		}
		qDebug() << " - added" << ( num2 - num ) << "new functions -";
		num = num2;
	}

	// look for functions that only match 1 place, have a certain minimum length, and dont overlap any known data.
	TryToMatchFunctions4( ignoreFunctions, 0x40 );
	TryToMatchFunctions4( ignoreFunctions, 0x30 );

	// try to follow branches from these functions until no new ones are discovered
	num = knownFunctions.size();
	for( int i = 0; i < maxRetries; i++ )
	{
		qDebug() << " -- Round" << i << '/' << maxRetries << " following branches and global variables --";
		TryToMatchFunctions2( nonMatchingBranches );

		// add functions by looking at rtoc and r13
		FindGlobalVariables();

		int num2 = knownFunctions.size();
		if( num2 == num )
		{
			break;
		}
		qDebug() << " - added" << ( num2 - num ) << "new functions -";
		num = num2;
	}

	qDebug() << "Total global variables: " << knownVariables.size();
	qDebug() << "Total data matches:     " << knownData.size();
	qDebug() << "Total functions found:  " << knownFunctions.size();

	qDebug() << "Generating idc file...";
	QString idc = MakeIDC( dolPath, libPath, nonMatchingBranches );
	//qDebug() << idc;

	WriteFile( outName, idc.toLatin1() );


	return 0;
}
