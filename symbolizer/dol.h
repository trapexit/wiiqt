#ifndef DOL_H
#define DOL_H

#include "../WiiQt/includes.h"
#include "../WiiQt/tools.h"
#include "be.h"

struct Dolheader
{
	be32 offsetText[ 7 ];		// 0	// 0000
	be32 offsetData[ 11 ];		// 28	// 0012
	be32 addressText[ 7 ];		// 72	// 0048
	be32 addressData[ 11 ];		// 100	// 0064
	be32 sizeText[ 7 ];			// 144	// 0090
	be32 sizeData[ 11 ];		// 172	// 00ac
	be32 addressBSS;			// 216	// 00d8
	be32 sizeBSS;				// 220	// 00dc
	be32 entrypoint;			// 224	// 00e0
};

struct DolSection
{
	quint32 addr;
	QByteArray data;
	DolSection() : addr( 0 )
	{
	}
};

class Dol
{
public:
	Dol( const QByteArray &dol = QByteArray() );

	const Dolheader *Header() const { return dh; }

	const QList< DolSection > &TextSections() const { return text; }
	const QList< DolSection > &DataSections() const { return data; }

	bool Parse(  const QByteArray &dol );

	// create a fake dol from a memory buffer
	//! Header() will return NULL, and it will create only 1 text section with the load address and no data sections
	//! added just for convenience
	static Dol FakeDol( const QByteArray &mem, quint32 loadAddress = 0x80000000 );
private:
	Dolheader *dh;
	QList< DolSection >text;
	QList< DolSection >data;

	QByteArray headerBuf;




};

#endif // DOL_H
