
#include "../WiiQt/tools.h"
#include "elfparser.h"

ElfParser::ElfParser( const QString &stuff ) : error( false )
{
	ParseText( stuff );
}

bool ElfParser::ParseText( const QString &str )
{
	QString fileName;
	QStringList fileLines;
	QMap< QString, QStringList >rawFiles;
	QMap< QString, QStringList >rawSections;
	QMap< QString, QStringList >rawSymbolTable;
	QStringList lines = str.split( '\n', QString::KeepEmptyParts );
	quint32 lineCnt = lines.size();
	for( quint32 i = 0; i < lineCnt; i++ )
	{
		const QString &line = lines.at( i );


		// start of a new file
		if( line.contains( ":     file format" ) )
		{
			// add existing file to the list
			if( !fileName.isEmpty() && fileLines.size() )
			{
				rawFiles[ fileName ] = fileLines;
			}
			fileLines.clear();
			fileName.clear();
			fileName = line.left( line.indexOf( ":     file format" ) );
			//qDebug() << "starting file" << fileName;


			// read symbol table
			for( ; i < lineCnt; i++ )
			{
				if( lines.at( i ).startsWith( "SYMBOL TABLE:" ) )
				{
					//qDebug() << "lines.at( i )" << lines.at( i );
					break;
				}
			}


			QStringList symbolListLines;
			for( ; i < lineCnt && !lines.at( i ).isEmpty(); i++ )
			{
				symbolListLines << lines.at( i );
			}
			rawSymbolTable[ fileName ] = symbolListLines;


			// read hex dump
			for( ; i < lineCnt; i++ )
			{
				if( lines.at( i ).startsWith( "Contents of section " ) )
				{
					//qDebug() << "lines.at( i )" << lines.at( i );
					break;
				}
			}

			QStringList secList;
			for( ; i < lineCnt && !lines.at( i ).isEmpty(); i++ )
			{
				secList << lines.at( i );
			}
			//qDebug() << "section" << fileName << secList.size();
			rawSections[ fileName ] = secList;

			for( ; i < lineCnt - 1; i++ )
			{
				if( lines.at( i + 1 ).startsWith( "Disassembly of section" ) )
				{
					break;
				}
				if( lines.at( i + 1 ).contains( ":     file format" ) )// happens if the .o doesnt contain any rode
				{
					break;
				}
			}
			continue;
		}
		if( line.startsWith( "Disassembly of section" ) )
		{
			continue;
		}
		fileLines << line;
	}
	// add the last file in there
	if( !fileName.isEmpty() && fileLines.size() )
	{
		rawFiles[ fileName ] = fileLines;
	}


	QMapIterator< QString, QStringList > it( rawFiles );
	while( it.hasNext() )
	{
		it.next();

		//qDebug() << "File:" << it.key() << it.value().size();
		File file( it.key() );
		if( !ParseFileText( it.value(), rawSections.find( it.key() ).value(), rawSymbolTable.find( it.key() ).value(), file ) )
		{
			error = true;
			return false;
		}
		files << file;
	}

	foreach( const File &f, files )
	{
		//qDebug() << f.Name();
		/*foreach( const Function &fun, f.Functions() )
		{
			//qDebug() << "   " << fun.Name();
			foreach( const SymRef &ref, fun.References() )
			{
				//qDebug() << "      " << hex << ref.off << ref.name;
			}
		}*/
		/*foreach( const SymAlias &alias, f.Aliases() )
		{
			qDebug() << " " << alias.name << alias.containerName;
		}*/
	}
	//exit( 0 );


	return true;
}

QList< SymAlias > ElfParser::ParseSymbolTable( const QStringList &lines )
{
	QList< SymAlias >ret;
	foreach( const QString &line, lines )
	{
		if( line.size() < 19 )// too small
		{
			continue;
		}
		int tab = line.indexOf( '\t' );
		if( tab < 17 || line.size() < tab + 11 )
		{
			continue;
		}
		bool ok;
		SymAlias ref;
		ref.containerName = line.mid( 17, tab - 17 );

		// filter out certain sections
		if( ref.containerName.startsWith( '*' )
				|| ref.containerName.startsWith( ".text" )
				|| ref.containerName.startsWith( ".debug" )
				|| ref.containerName.startsWith( ".comment" )
				|| ref.containerName.startsWith( ".gnu" )
				|| ref.containerName.startsWith( ".init" ) )
		{
			continue;
		}
		ref.offset = line.left( 8 ).toUInt( &ok, 16 );
		if( !ok )
		{
			continue;
		}
		ref.size = line.mid( tab + 1, 8 ).toUInt( &ok, 16 );
		if( !ok )
		{
			continue;
		}
		//qDebug() << line.mid( tab + 1, 8 );
		if( !ref.offset && !ref.size )
		{
			continue;
		}
		ref.name = line.mid( tab + 10 );
		//qDebug() << hex << QString( "%1" ).arg( ref.offset, 8, 16, QChar( QChar( '0' ) ) )
		//		 << ref.containerName
		//		 << QString( "%1" ).arg( ref.size, 8, 16, QChar( QChar( '0' ) ) )
		//		 << ref.name;
		ret << ref;
	}
	return ret;
}

QMap< QString, QByteArray > ElfParser::ParseSectionText( const QStringList &list )
{
	QMap< QString, QByteArray >ret;
	QMap< QString, QByteArray >ret2;
	QByteArray ba;
	QString name;
	for( quint32 i = 0; i < (quint32)list.size(); i++ )
	{
		const QString &line = list.at( i );
		if( line.startsWith( "Contents of section " ) )
		{
			if( !name.isEmpty() && ba.size() )
			{
				ret[ name ] = ba;
			}
			ba.clear();
			name = line.mid( 20 );
			name.resize( name.size() - 1 );
			//DBG << name;
			continue;
		}
		QString hexS = line.mid( 6, 35 );
		QByteArray hexA = hexS.toLatin1();
		hexA = QByteArray::fromHex( hexA );
		ba += hexA;
	}
	if( !name.isEmpty() && ba.size() )
	{
		ret[ name ] = ba;
	}

	// remove unwanted sections
	QMapIterator< QString, QByteArray > it( ret );
	while( it.hasNext() )
	{
		it.next();
		if( !it.key().contains( ".text" )
				&& !it.key().startsWith( ".init" )
				&& !it.key().startsWith( ".ctors" )
				&& !it.key().startsWith( ".dtors" )
				&& !it.key().startsWith( ".debug" )
				&& !it.key().startsWith( ".comment" )
				&& !it.key().startsWith( "extab" ))
		{
			ret2[ it.key() ] = it.value();
		}
	}

	// debug
	/*QMapIterator< QString, QByteArray > it2( ret2 );
 while( it2.hasNext() )
 {
  it2.next();
  qDebug() << it2.key();
  hexdump( it2.value() );
 }*/

	return ret2;
}

bool ElfParser::ParseFileText( const QStringList &strs, const QStringList &sectionStrs, const QStringList &symbolStrs, ElfParser::File &file )
{
	quint32 cnt = strs.size();

	quint32 fOff = 0;
	quint32 fStart = 0;

	QString name;
	QString pattern;
	QList< SymRef > refs;

	//DBG << file.Name() << sectionStrs.size() << symbolStrs.size() << strs.size();
	QMap< QString, QByteArray >sections = ParseSectionText( sectionStrs );
	QList< SymAlias > aliases = ParseSymbolTable( symbolStrs );


	//DBG << file.Name() << sections.size() << aliases.size();

	for( quint32 i = 0; i < cnt; i++ )
	{
		const QString &str = strs.at( i );
		/*if( name == "WII_Initialize" )
  {
   qDebug() << str;
  }*/

		// start a new funciton
		if( IsFunctionStart( str, &fStart ) )
		{
			// add this function to the list
			if( !name.isEmpty() && fOff )
			{
				Function fun( name );
				fun.references = refs;
				fun.pattern = pattern;
				fun.file = &file;
				file.functions << fun;
				//qDebug() << "pattern:" << pattern;
			}
			//qDebug() << GetFunctionName( str );
			name = GetFunctionName( str );
			//DBG << name;
			if( fOff != (quint32)pattern.size() / 2 )
			{
				qDebug() << "size bad";
				exit( 0 );
			}
			fOff = 0;
			pattern.clear();
			refs.clear();

			sections.remove( name );// remove functions from the section list
			continue;
		}
		if( name.isEmpty() )
		{
			continue;
		}
		if( IsBlank( str ) )
		{
			//qDebug() << str << "is blank";
			continue;
		}
		if( IsSymbolLine( str ) )
		{
			//qDebug() << str << "IsSymbolLine";
			continue;
		}
		QString hex;
		QString oper;
		QString symbol;
		quint32 refOff = 0xdeadbeef;

		if( !ParseOpLine( str, hex, oper ) )
		{
			qDebug() << str << strs.at( i - 1 );
			return false;
		}
		/*if( name == "WII_Initialize" )
  {
   qDebug() << "hex" << hex;
  }*/

		if( ( i < cnt - 1 ) && IsSymbolLine( strs.at( i + 1 ) ) )
		{
			SymRef::Type refType;
			symbol = GetNonOperRef( strs.at( i + 1 ), &refOff, &refType );

			if( refOff < fStart )
			{
				WRN << "refOff < fStart" << str;
				return false;
			}
			SymRef ref;
			quint32 deRef;
			ref.name = DeReferenceSymbol( symbol, &deRef );
			ref.symOff = deRef;

			switch( refType )
			{
			case SymRef::R_PPC_ADDR16_HI:
			case SymRef::R_PPC_ADDR16_LO:
			{
				hex[ 4 ] = '.';
				hex[ 5 ] = '.';
				hex[ 6 ] = '.';
				hex[ 7 ] = '.';
			}
			break;
			case SymRef::R_PPC_REL24:
			case SymRef::R_PPC_EMB_SDA21:
			{
				hex[ 1 ] = '.';
				hex[ 2 ] = '.';
				hex[ 3 ] = '.';
				hex[ 4 ] = '.';
				hex[ 5 ] = '.';
				hex[ 6 ] = '.';
				hex[ 7 ] = '.';
			}
			break;
			case SymRef::R_PPC_SDAREL16:
			{
				hex = "........";
			}
			break;
			default:
				WRN << "unhandled reference type";
				return false;
				break;
			}

			ref.type = refType;
			ref.off = refOff - fStart;
			refs << ref;
			if( ref.off & 0xff000000 )
			{
				qDebug() << "ref.off is busted 1" << name << str;

				qDebug() << ::hex << refOff << fStart;
				exit( 0 );
			}
		}

		else if( OpNeedsWildCard( oper ) )
		{
			//DBG << "bl called without symbol reference\n" << str;
			hex = "........";
			if( symbol.isEmpty() )
			{
				symbol = GetOpersymRef( str );
			}
			SymRef ref;
			ref.name = symbol;
			ref.off = (quint32)(pattern.size());
			ref.type = SymRef::R_PPC_REL24;
			refs << ref;

			if( ref.off & 0xff000000 )
			{
				DBG << "ref.off is busted 2" << name << str;
				exit( 0 );
			}

		}
		pattern += hex.toUpper();
		/*if( name == "WII_Initialize" )
  {
   qDebug() << "hex" << pattern;
  }*/
		fOff += 4;
	}
	if( !name.isEmpty() )
	{
		Function fun( name );
		fun.references = refs;
		fun.pattern = pattern;
		fun.file = &file;
		file.functions << fun;
	}
	file.sections = sections;
	file.aliases = aliases;
	return true;
}

bool ElfParser::IsFunctionStart( const QString &str, quint32 *start )
{
	bool ok;
	quint32 s;
	if( str.size() < 12 )
	{
		return false;
	}
	if( str.startsWith( ' ' ) )
	{
		return false;
	}
	if( !str.endsWith( ">:" ) )
	{
		return false;
	}
	s = str.left( 8 ).toUInt( &ok, 16 );
	if( !ok )
	{
		return false;
	}
	int o = str.indexOf( '<' );
	if( o < 9 )
	{
		return false;
	}
	if( start )
	{
		*start = s;
	}
	return true;
}

QString ElfParser::GetFunctionName( const QString &str )
{
	QString ret = str;
	ret.remove( 0, ret.indexOf( '<' ) + 1 );
	ret.resize( ret.size() - 2 );
	return ret;
}

bool ElfParser::IsSymbolLine( const QString & str )
{
	if( str.startsWith( "\t\t\t" ) && str.indexOf( '\t', 3 ) > 4 )
	{
		return true;
	}
	return false;
}

bool ElfParser::IsBlank( const QString & str )
{
	QString sim = str.simplified();
	return sim.isEmpty() || sim == "...";
}

bool ElfParser::ParseOpLine( const QString &str, QString &hex, QString &oper )
{
	//    1c74:	41 82 01 54 	beq-
	int tab = str.indexOf( '\t', 3 );
	if( tab < 0 || str.size() < tab + 15 || str.at( tab + 3 ) != ' '  || str.at( tab + 6 ) != ' '  || str.at( tab + 9 ) != ' '  || str.at( tab + 12 ) != ' ' )
	{
		qDebug() << str << "is not an opline";
		qDebug() << hex << oper;
		return false;
	}
	//  "   0:	94 21 ff f0 	stwu    r1,-16(r1)"
	hex = str.mid( tab + 1, 11 );
	hex.remove( ' ' );

	oper = str.mid( tab + 14 );
	int i = oper.indexOf( ' ' );
	if( i > 0 )
	{
		oper.resize( i );
	}
	//qDebug() << str << '\n' << hex << oper;
	//exit( 0 );

	return true;
}

bool ElfParser::OpNeedsWildCard( const QString &str )
{
	if( str == "bl" )
	{
		return true;
	}
	return false;
}

QString ElfParser::GetNonOperRef( const QString &str, quint32 *off, SymRef::Type *type )
{
	int i = str.lastIndexOf( '\t' );
	if( i < 0 )
	{
		return QString();
	}
	if( off )// get offset
	{
		bool ok;
		QString n = str.mid( 3 );
		{
			n.resize( n.indexOf( ':' ) );
		}
		*off = n.toUInt( &ok, 16 );
		if( !ok )
		{
			*off = 0xdeadbeef;
			DBG << "error converting\n" << str << '\n' << n;
			exit( 0 );
		}
	}
	if( type )
	{
		if( str.contains( "R_PPC_REL24" ) )
		{
			*type = SymRef::R_PPC_REL24;
		}
		else if( str.contains( "R_PPC_ADDR16_LO" ) )
		{
			*type = SymRef::R_PPC_ADDR16_LO;
		}
		else if( str.contains( "R_PPC_EMB_SDA21" ) )
		{
			*type = SymRef::R_PPC_EMB_SDA21;
		}
		else if( str.contains( "R_PPC_ADDR16_HA" ) || str.contains( "R_PPC_ADDR16_HI" ) )
		{
			*type = SymRef::R_PPC_ADDR16_HI;
		}
		else if( str.contains( "R_PPC_SDAREL16" ) )
		{
			*type = SymRef::R_PPC_SDAREL16;
		}
		else
		{
			*type = SymRef::R_PPC_WTF;
			DBG << "*type = SymRef::R_PPC_WTF" << str;
			exit( 0 );
		}
	}
	return str.mid( i + 1 );
}

QString ElfParser::GetOpersymRef( const QString &str )
{
	QString ret;
	int o = str.indexOf( '<' );
	if( o < 0 )
	{
		return QString();
	}
	ret = str.mid( o + 1 );
	if( !ret.endsWith( '>' ) )
	{
		return QString();
	}
	ret.resize( ret.size() - 1 );
	return ret;
}

QString ElfParser::DeReferenceSymbol( const QString &reference, quint32 *offset )
{
	// .rodata.str1.1+0x5
	QString ret;
	if( offset )
	{
		*offset = 0;
	}
	int o = reference.indexOf( "+0x" );
	if( o < 0 )
	{

		return reference;
	}
	ret = reference.left( o );
	bool ok;
	quint32 d = reference.mid( o + 3 ).toUInt( &ok, 16 );
	if( !ok )
	{
		return reference;
	}
	if( offset )
	{
		*offset = d;
	}
	return ret;

}
