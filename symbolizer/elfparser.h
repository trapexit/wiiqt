#ifndef ELFPARSER_H
#define ELFPARSER_H

#include "../WiiQt/includes.h"

// class to parse the output of "objdump -xds someLib.a"
//! specifically wii PPC libs


struct SymRef
{
	enum Type
	{
		R_PPC_ADDR16_HA,	// reference to the upper 16 bits of the variable and treats stuff as signed.  ie "lis, addi"
		R_PPC_ADDR16_HI,	// reference to the upper 16 bits of the variable and treats the valuas as unsigned.  ie "lis, ori"
		R_PPC_ADDR16_LO,	// refers to the lower 16 bits.  ie "addi"
		R_PPC_REL24,		// refers by branching.  ie "bl"
		R_PPC_SDAREL16,		// no clue wtf this one does
		R_PPC_EMB_SDA21,	// referenced by lwz/stw r13 or r2.  these are converted at link time
		R_PPC_WTF			// something went wrong
	};

	quint32 off;			// offset within the function where the reference occurs
	quint32 symOff;			// offset from the referenced symbol.  ie stringTable + 0x10
	Type type;
	QString name;			// name of the symbol referenced
	SymRef() : off( 0 ), symOff( 0 ), type( R_PPC_WTF )
	{
	}
};

// ninty's compiler seems to lump a bunch of data together in 1 section and then offer names to it
// gcc seems to give each piece of data its own section
struct SymAlias
{
	QString name;
	QString containerName;
	quint32 offset;
	quint32 size;
	SymAlias() : offset( 0 ), size( 0 )
	{
	}
};

class ElfParser
{
public:
	class File;
	class Function
	{
	public:
		Function( const QString &n = QString() ) : name( n )
		{
		}
		const QString &Name() const { return name; }
		const QString &Pattern() const { return pattern; }
		const QList< SymRef > &References() const { return references; }
		const File *PFile() const { return file; }
	private:
		friend class ElfParser;
		QString name;
		QString pattern;
		File *file;
		QList< SymRef > references;
	};

	class File
	{
	public:
		File( const QString &n = QString() ) : name( n )
		{
		}
		const QString &Name() const { return name; }
		const QList< Function > &Functions() const { return functions; }

		// doesnt contain the ".text" section, since that is already parsed and turned into functions
		//! this is for data and rodata and stuff
		const QMap< QString, QByteArray > &Sections() const { return sections; }
		const QList< SymAlias > &Aliases() const { return aliases; }
	private:
		friend class ElfParser;
		QString name;
		QList< Function > functions;
		QMap< QString, QByteArray >sections;
		QList< SymAlias >aliases;
	};

	ElfParser( const QString &stuff );

	//const QList< File >Files() const { return files; }
	bool Error() const { return error; }

	const QList< File > Files() const { return files; }
private:
	bool error;
	QList< File >files;

	// takes the entire dump and separetes it into files
	bool ParseText( const QString &str );

	// takes each file and separetes it into functions
	bool ParseFileText( const QStringList &strs, const QStringList &sectionStrs, const QStringList &symbolStrs, File &file );

	QRegExp funcStart;
	bool IsFunctionStart( const QString &str, quint32 *start = NULL );

	// extract function name from the line that starts it
	QString GetFunctionName( const QString &str );

	// check if operation is one that should be wildcarded
	bool OpNeedsWildCard( const QString & str );

	// check if this is a symbol line or opcode
	bool IsSymbolLine( const QString & str );

	bool IsBlank( const QString & str );

	bool ParseOpLine( const QString &str, QString &hex, QString &oper );

	// returns stuff in <***> at the end of a line
	QString GetOpersymRef( const QString &str );

	// returns the string referenced by an extra line and the type of reference
	QString GetNonOperRef( const QString &str, quint32 *off = NULL, SymRef::Type *type = NULL );

	// parses the hex dump text of sections
	QMap< QString, QByteArray >ParseSectionText( const QStringList &list );

	QString DeReferenceSymbol( const QString &reference, quint32 *offset = NULL );

	// parse symbol table
	QList< SymAlias >ParseSymbolTable( const QStringList &lines );

};

#endif // ELFPARSER_H
