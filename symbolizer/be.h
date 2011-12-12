#ifndef BE_H
#define BE_H

#include "../WiiQt/includes.h"

class be64
{
	quint64 value; // in big endian;

public:
	explicit be64( quint64 v ): value( qFromBigEndian( v ) ) {}
	be64(): value( 0 ) {}

	quint64 operator*()
	{
		return to_quint64();
	}

	quint64 to_quint64() const
	{
		return qToBigEndian( value );
	}

	be64 &operator = ( const quint64 &v )
	{
		value = qFromBigEndian( v );
		return *this;
	}
};

class be32
{
	quint32 value; // in big endian;

public:
	explicit be32( quint32 v ): value( qFromBigEndian( v ) ) {}
	be32(): value( 0 ) {}

	quint32 operator*()
	{
		return to_quint32();
	}

	quint32 to_quint32() const
	{
		return qToBigEndian( value );
	}

	be32 &operator = ( const quint32 &v )
	{
		value = qFromBigEndian( v );
		return *this;
	}
};

class be16
{
	quint16 value; // in big endian;

public:
	explicit be16( quint16 v ): value( qFromBigEndian( v ) ) {}
	be16(): value( 0 ) {}

	quint16 operator*()
	{
		return to_quint16();
	}

	quint16 to_quint16() const
	{
		return qToBigEndian( value );
	}

	be16 &operator = ( const quint16 &v )
	{
		value = qFromBigEndian( v );
		return *this;
	}
};



#endif // BE_H
