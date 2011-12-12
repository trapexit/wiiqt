#include "dol.h"

Dol::Dol( const QByteArray &dol ) : dh( NULL )
{
	if( (quint32)dol.size() > sizeof( Dolheader ) )
	{
		Parse( dol );
	}
}

bool Dol::Parse(  const QByteArray &dol )
{
	dh = NULL;
	text.clear();
	data.clear();

	if( (quint32)dol.size() < sizeof( Dolheader ) )
	{
		WRN << "dol.size() < sizeof( Dolheader )";
		return false;
	}
	headerBuf = dol.left( sizeof( Dolheader ) );
	dh = reinterpret_cast< Dolheader * >( headerBuf.data() );
	if( !dh )
	{
		WRN << "!dh";
		return false;
	}
	//DBG << hex << (*(dh->entrypoint));

	for( quint32 i = 0; i < 7; i ++ )
	{
		quint32 fileOff = (*(dh->offsetText[ i ] ) );
		quint32 len = (*(dh->sizeText[ i ] ) );
		quint32 addr = (*(dh->addressText[ i ] ) );
		if( !fileOff || !len || !addr )
		{
			continue;
		}
		if( fileOff + len > (quint32) dol.size() )
		{
			WRN << "text section is out of range:" << i << hex << fileOff << len;
			continue;
		}
		DolSection sec;
		sec.addr = addr;
		sec.data = dol.mid( fileOff, len );

		text << sec;
	}

	for( quint32 i = 0; i < 11; i ++ )
	{
		quint32 fileOff = (*(dh->offsetData[ i ] ) );
		quint32 len = (*(dh->sizeData[ i ] ) );
		quint32 addr = (*(dh->addressData[ i ] ) );
		if( !fileOff || !len || !addr )
		{
			continue;
		}
		if( fileOff + len > (quint32) dol.size() )
		{
			WRN << "data section is out of range:" << i << hex << fileOff << len;
			continue;
		}
		DolSection sec;
		sec.addr = addr;
		sec.data = dol.mid( fileOff, len );

		data << sec;
	}

	return true;
}

Dol Dol::FakeDol( const QByteArray &mem, quint32 loadAddress )
{
	Dol ret;
	DolSection sec;
	sec.addr = loadAddress;
	sec.data = mem;
	ret.text << sec;
	return ret;
}
