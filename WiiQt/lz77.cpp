#include "lz77.h"

//most functions here are taken from wii.cs by leathl
//they have just been tweeked slightly to work with Qt C++

LZ77::LZ77()
{
    match_position = 0;
    match_length = 0;
    textsize = 0;
    codesize = 0;
}

int LZ77::GetLz77Offset( const QByteArray &data )
{
    QByteArray start = data.left( 5000 );

    return start.indexOf( "LZ77" );
}

LZ77::CompressionType LZ77::GetCompressedType( const QByteArray &data, int *outOffset )
{
	if( data.startsWith( 0x10 ) )
	{
		if( outOffset )
		{
			*outOffset = 0;
		}
		return v10;
	}
	if( data.startsWith( 0x11 ) )
	{
		if( outOffset )
		{
			*outOffset = 0;
		}
		return v11;
	}
	int t = GetLz77Offset( data );
	if( t >= 0 )
	{
		if( outOffset )
		{
			*outOffset = t;
		}
		return v10_w_magic;
	}
	return None;
}

QByteArray LZ77::Decompress( const QByteArray &stuff, LZ77::CompressionType *outType )
{
	int off;
	CompressionType t = GetCompressedType( stuff, &off );
	if( outType )
	{
		*outType = t;
	}
	if( t == v10 )
	{
		return Decompress_v10( stuff, 0 );
	}
	if( t == v10_w_magic )
	{
		return Decompress_v10( stuff, off + 4 );
	}
	if( t == v11 )
	{
		return LZ77_11::Decompress( stuff );
	}
	return stuff;
}

void LZ77::InitTree()
{
    int i;
    int N = 4096;
    for( i = N + 1; i <= N + 256; i++ )
		rson[ i ] = N;

    for( i = 0; i < N; i++ )
		dad[ i ] = N;
}

void LZ77::DeleteNode( int p )
{
    int N = 4096;
    int q;

    if( dad[ p ] == N )
		return;  /* not in tree */

    if( rson[ p ] == N )
		q = lson[ p ];

    else if( lson[ p ] == N )
		q = rson[ p ];

    else
    {
		q = lson[ p ];

		if( rson[ q ] != N )
		{
			do
			{
				q = rson[ q ];
			}
			while( rson[ q ] != N );

			rson[ dad[ q ] ] = lson[ q ];
			dad[ lson[ q ] ] = dad[ q ];

			lson[ q ] = lson[ p ];
			dad[ lson[ p ] ] = q;
		}
		rson[ q ] = rson[ p ];
		dad[ rson[ p ] ] = q;
    }
    dad[ q ] = dad[ p ];

    if( rson[ dad[ p ] ] == p )
		rson[ dad[ p ] ] = q;
    else
		lson[ dad[ p ] ] = q;

    dad[ p ] = N;
}

void LZ77::InsertNode( int r )
{
    int i, p, cmp;
    int N = 4096;
    int F = 18;
    cmp = 1;
    p = N + 1 + ( text_buf[ r ] == ( quint16 )0xffff ? ( quint16 )0 : text_buf[ r ] );
    rson[ r ] = lson[ r ] = N;
    match_length = 0;
    for( ; ; )
    {
		if( cmp >= 0 )
		{
			if( rson[ p ] != N )
				p = rson[ p ];
			else
			{
				rson[ p ] = r;
				dad[ r ] = p;
				return;
			}
		}
		else
		{
			if ( lson[ p ] != N )
				p = lson[ p ];

			else
			{
				lson[ p ] = r;
				dad[ r ] = p;
				return;
			}
		}
		for( i = 1; i < F; i++ )
			if( ( cmp = text_buf[ r + i ] - text_buf[ p + i ] ) != 0 )
				break;
		if( i > match_length )
		{
			match_position = p;
			if( ( match_length = i ) >= F )
				break;
		}
    }

    dad[ r ] = dad[ p ];
    lson[ r ] = lson[ p ];
    rson[ r ] = rson[ p ];
    dad[ lson[ p ] ] = r;
    dad[ rson[ p ] ] = r;

    if( rson[ dad[ p ] ] == p )
		rson[ dad[ p ] ] = r;

    else
		lson[ dad[ p ] ] = r;

    dad[ p ] = N;
}

quint32 LZ77::GetDecompressedSize( const QByteArray &data )
{
	int off;
	LZ77::CompressionType ct = LZ77::GetCompressedType( data, &off );
	if( ct == None )
	{
		return data.size();
	}
	if( ct == v10_w_magic )
	{
		off += 4;
	}
    QByteArray ba = data;
    QBuffer buf( &ba );
    if( !buf.open( QBuffer::ReadOnly ) )
    {
		qWarning() << "LZ77::GetDecompressedSize -> Can't create buffer";
		return 0;
    }
	buf.seek( off );

    quint32 gbaheader;
	buf.seek( off );
    buf.read( (char*)&gbaheader, 4 );
	quint32 ret = ( gbaheader >> 8 );
	if( !ret )
	{
		buf.read( (char*)&ret, 4 );
	}

	return ret;
}

QByteArray LZ77::Decompress_v10( const QByteArray &compressed, int offset )
{
    int N = 4096;
    int F = 18;
    int threshold = 2;
    int k, r, z, flags;
    quint16 i, j;
    char ch;
    quint32 decomp_size;
    quint32 cur_size = 0;

    QByteArray crap = compressed;
    QBuffer infile( &crap );
    if( !infile.open( QBuffer::ReadOnly ) )
    {
		qWarning() << "Can't create buffer 1";
		return QByteArray();
    }
    QByteArray ret;
    QBuffer outfile( &ret );
    if( !outfile.open( QBuffer::ReadWrite ) )
    {
		qWarning() << "Can't create buffer 2";
		return QByteArray();
    }

    quint32 gbaheader;
	infile.seek( offset );
    infile.read( (char*)&gbaheader, 4 );

    decomp_size = gbaheader >> 8;
    if( !decomp_size )
    {
        infile.read( (char*)&decomp_size, 4 );
    }
    quint8 text_buf[ N + 17 ];
    //qDebug() << "decomp_size:" << decomp_size;

    for( i = 0; i < N - F; i++ )
		text_buf[ i ] = 0xdf;

    r = N - F;
    flags = 7;
    z = 7;

    while( true )
    {
		flags <<= 1;
		z++;
		if( z == 8 )
		{
			if( !infile.getChar( &ch ) )
				break;
			flags = (quint8) ch;
			z = 0;
		}
		if( ( flags & 0x80 ) == 0 )
		{
			if( !infile.getChar( &ch ) )
				break;

			if( cur_size < decomp_size )
				outfile.putChar( ch );

			text_buf[ r++ ] = ch;
			r &= ( N - 1 );
			cur_size++;
		}
		else
		{
			if( !infile.getChar( &ch ) )
				break;
			i = (quint8)ch;

			if( !infile.getChar( &ch ) )
				break;
			j = (quint8)ch;

			j = j | ( (i << 8) & 0xf00 );
			i = ( ( i >> 4 ) & 0x0f ) + threshold;

			for( k = 0; k <= i; k++ )
			{
				ch = text_buf[ ( r - j - 1 ) & ( N - 1 ) ];
				if( cur_size < decomp_size )
					outfile.putChar( ch );

				text_buf[ r++ ] = ch;
				r &= ( N - 1 );
				cur_size++;
			}
		}
    }

    return ret;
}

/*QByteArray LZ77::Decompress( const QByteArray &compressed )
{
    int off = GetLz77Offset( compressed );
    if( off < 0 )
    {
		qWarning() << "LZ77::Decompress -> data is not compressed";
		return compressed;
    }
    return Decompress( compressed, off );
}*/

QByteArray LZ77::Compress( const QByteArray &ba, LZ77::CompressionType type )
{
	if( type == v10 )
	{
		return Compress_v10( ba, false );
	}
	if( type == v10_w_magic )
	{
		return Compress_v10( ba, true );
	}
	if( type == v11 )
	{
		return LZ77_11::Compress( ba );
	}
	return ba;
}

QByteArray LZ77::Compress_v10( const QByteArray &ba, bool addMagic )
{
    LZ77 lz;
	QByteArray ret = lz.Compr_v10( ba, addMagic );
    return ret;
}

QByteArray LZ77::Compr_v10( const QByteArray &ba, bool addMagic )
{
    int i, len, r, s, last_match_length, code_buf_ptr;
    char ch;
    int code_buf[ 17 ];
    int mask;
    int N = 4096;
    int F = 18;
    int threshold = 2;
	quint32 filesize;

    QByteArray crap = ba;
    QBuffer infile( &crap );
    if( !infile.open( QBuffer::ReadOnly ) )
    {
		qWarning() << "Can't create buffer 1";
		return QByteArray();
    }
    QByteArray ret;
    QBuffer output( &ret );
    if( !output.open( QBuffer::ReadWrite ) )
    {
		qWarning() << "Can't create buffer 2";
		return QByteArray();
    }

	if( addMagic )
	{
		output.putChar( 'L' );
		output.putChar( 'Z' );
		output.putChar( '7' );
		output.putChar( '7' );
	}

	if( ba.size() <= 0xffffff )
	{
		filesize = ( ba.size() << 8 ) | 0x10;
		output.write( (const char*)&filesize, 4 );
		//output.putChar( '\0' );
		//output.putChar( '\0' );
		//output.putChar( '\0' );
		//output.putChar( '\0' );
	}
	else
	{
		filesize = 0x10;
		output.write( (const char*)&filesize, 4 );
		filesize = ba.size();
		output.write( (const char*)&filesize, 4 );
	}

    InitTree();
    code_buf[ 0 ] = 0;
    code_buf_ptr = 1;
    mask = 0x80;
    s = 0;
    r = N - F;

    for( i = s; i < r; i++ )
		text_buf[ i ] = 0xffff;

    for( len = 0; len < F && infile.getChar( &ch ); len++ )
		text_buf[ r + len ] = (quint8)ch;

    if( ( textsize = len ) == 0)
		return ba;

    for( i = 1; i <= F; i++ )
		InsertNode( r - i );

    InsertNode( r );

    do
    {
		if( match_length > len )
			match_length = len;

		if( match_length <= threshold )
		{
			match_length = 1;
			code_buf[ code_buf_ptr++ ] = text_buf[ r ];
		}
		else
		{
			code_buf[ 0 ] |= mask;

			code_buf[ code_buf_ptr++ ] = (quint8)
										 ( ( ( r - match_position - 1 ) >> 8) & 0x0f ) |
										 ( ( match_length - ( threshold + 1 ) ) << 4 );

			code_buf[ code_buf_ptr++ ] = (quint8)( ( r - match_position - 1 ) & 0xff );
		}
		if( ( mask >>= 1 ) == 0 )
		{
			for( i = 0; i < code_buf_ptr; i++ )
				output.putChar( (quint8)code_buf[ i ] );

			codesize += code_buf_ptr;
			code_buf[ 0 ] = 0; code_buf_ptr = 1;
			mask = 0x80;
		}

		last_match_length = match_length;
		for( i = 0; i < last_match_length && infile.getChar( &ch ); i++ )
		{
			DeleteNode( s );
			text_buf[ s ] = (quint8)ch;
			if( s < F - 1 )
				text_buf[ s + N ] = (quint8)ch;

			s = ( s + 1 ) & ( N - 1 );
			r = ( r + 1 ) & ( N - 1 );
			InsertNode( r );
		}

		while( i++ < last_match_length )
		{
			DeleteNode( s );
			s = ( s + 1 ) & ( N - 1 );
			r = ( r + 1 ) & ( N - 1 );
			if( --len != 0 )
				InsertNode( r );
		}
    }
    while( len > 0 );

    if( code_buf_ptr > 1 )
    {
		for( i = 0; i < code_buf_ptr; i++ )
			output.putChar( (quint8)code_buf[ i ] );

		codesize += code_buf_ptr;
    }
    int padding = codesize % 4;
    if( padding != 0 )
		output.write( QByteArray( 4 - padding, '\0' ) );

    infile.close();
    return ret;
}

LZ77_11::LZ77_11()
{
}

QByteArray LZ77_11::Compress( const QByteArray &stuff )
{
	// Test if the file is too large to be compressed
	if( (quint64)stuff.size() > 0xFFFFFFFF )
	{
		qDebug() << "LZ77_11::Compress -> Input file is too large to compress.";
		return QByteArray();
	}

	quint32 decompressedSize = stuff.size();

	quint32 SourcePointer = 0x0;
	quint32 DestPointer   = 0x4;

	quint32 tmp;
	QByteArray ret( decompressedSize, '\0' );//will reduce the size later
	QBuffer buf( &ret );
	buf.open( QIODevice::WriteOnly );


	// Set up the Lz Compression Dictionary
	LzWindowDictionary LzDictionary;
	LzDictionary.SetWindowSize( 0x1000 );
	LzDictionary.SetMaxMatchAmount( 0xFFFF + 273 );

	// Figure out where we are going to write the decompressed file size
	if( stuff.size() <= 0xFFFFFF )
	{
		tmp = ( decompressedSize << 8 ) | 0x11;									//dont switch endian?
		buf.write( (const char*)&tmp, 4 );
	}
	else
	{
		tmp = 0x11;
		buf.write( (const char*)&tmp, 4 );										//dont switch endian?
		tmp = decompressedSize;
		buf.write( (const char*)&tmp, 4 );										//dont switch endian?
		DestPointer += 0x4;
	}

	// Start compression
	while( SourcePointer < decompressedSize )
	{
		quint8 Flag = 0x0;
		quint32 FlagPosition = DestPointer;
		// It will be filled in later
		buf.putChar( Flag );
		DestPointer++;

		for( int i = 7; i >= 0; i-- )
		{
			QList<int>LzSearchMatch = LzDictionary.Search( stuff, SourcePointer, decompressedSize );
			if( LzSearchMatch[ 1 ] > 0 ) // There is a compression match
			{
				Flag |= (quint8)( 1 << i );

				// Write the distance/length pair
				if( LzSearchMatch[ 1 ] <= 0xF + 1 ) // 2 bytes
				{
					buf.putChar( (quint8)( (  ( ( LzSearchMatch[ 1 ] - 1) & 0xF ) << 4 ) | ( ( ( LzSearchMatch[ 0 ] - 1 ) & 0xFFF ) >> 8 ) ) );
					buf.putChar( (quint8)( ( LzSearchMatch[ 0 ] - 1 ) & 0xFF ) );
					DestPointer += 2;
				}
				else if (LzSearchMatch[1] <= 0xFF + 17) // 3 bytes
				{
					buf.putChar( (quint8)(((LzSearchMatch[1] - 17) & 0xFF) >> 4) );
					buf.putChar( (quint8)((((LzSearchMatch[1] - 17) & 0xF) << 4) | (((LzSearchMatch[0] - 1) & 0xFFF) >> 8)) );
					buf.putChar( (quint8)((LzSearchMatch[0] - 1) & 0xFF) );
					DestPointer += 3;
				}
				else // 4 bytes
				{
					buf.putChar( (quint8)((1 << 4) | (((LzSearchMatch[1] - 273) & 0xFFFF) >> 12)) );
					buf.putChar( (quint8)(((LzSearchMatch[1] - 273) & 0xFFF) >> 4) );
					buf.putChar( (quint8)((((LzSearchMatch[1] - 273) & 0xF) << 4) | (((LzSearchMatch[0] - 1) & 0xFFF) >> 8)) );
					buf.putChar( (quint8)((LzSearchMatch[0] - 1) & 0xFF) );
					DestPointer += 4;
				}

				LzDictionary.AddEntryRange( stuff, (int)SourcePointer, LzSearchMatch[ 1 ] );
				LzDictionary.SlideWindow( LzSearchMatch[ 1 ] );

				SourcePointer += (quint32)LzSearchMatch[ 1 ];
			}
			else // There wasn't a match
			{
				Flag |= (quint8)(0 << i);

				buf.putChar( stuff.at( SourcePointer ) );

				LzDictionary.AddEntry( stuff, (int)SourcePointer );
				LzDictionary.SlideWindow( 1 );

				SourcePointer++;
				DestPointer++;
			}

			// Check for out of bounds
			if( SourcePointer >= decompressedSize )
				break;
		}

		// Write the flag.
		// Note that the original position gets reset after writing.
		buf.seek( FlagPosition );
		buf.putChar( Flag );
		buf.seek( DestPointer );
	}

	buf.close();
	ret.resize( DestPointer );
	return ret;
}

QByteArray LZ77_11::Decompress( QByteArray stuff )
{
	if( !stuff.startsWith( 0x11 ) )
	{
		qWarning() << "LZ77_11::Decompress -> data doesnt start with 0x11";
		return QByteArray();
	}

	// Compressed & Decompressed Data Information
	QBuffer b( &stuff );
	b.open( QIODevice::ReadOnly );

	quint32 compressedSize   = (quint32)stuff.size();
	quint32 decompressedSize;
	b.read( (char*)&decompressedSize, 4 );					//is this really little endian?
	decompressedSize >>= 8;

	quint32 sourcePointer = 0x4;
	quint32 destPointer   = 0x0;
	unsigned char tempbuffer[ 10 ];
	quint32 backwards_offset;

	if( !decompressedSize ) // Next 4 bytes are the decompressed size
	{
		b.read( (char*)&decompressedSize, 4 );
		sourcePointer += 0x4;
	}

	b.close();

	QByteArray decompressedData( decompressedSize, '\0' );
	if( (quint32)decompressedData.size() != decompressedSize )
	{
		qWarning() << "LZ77_11::Decompress -> failed to allocate" << hex << decompressedSize << "bytes";
		return QByteArray();
	}

	// Start Decompression
	quint32 num_bytes_to_copy;
	quint32 copy_start_index;
	while( sourcePointer < compressedSize && destPointer < decompressedSize )
	{
		quint8 flag = stuff[ sourcePointer++ ];


		for( quint32 i = 0; i < 8; i++ )
		{
			if( flag & ( 0x80 >> i ) )
			{
				// Take encoded data
				tempbuffer[ 0 ] = stuff[ sourcePointer++ ];
				tempbuffer[ 1 ] = stuff[ sourcePointer++ ];

				switch( tempbuffer[ 0 ] & 0xF0 )
				{
				case 0:
					tempbuffer[ 2 ] = stuff[ sourcePointer++ ];
					num_bytes_to_copy = ( ( tempbuffer[ 0 ] << 4 ) + ( tempbuffer[ 1 ] >> 4 ) + 0x11 );
					backwards_offset = ( ( ( tempbuffer[ 1 ] & 0x0F ) << 8 ) + tempbuffer[ 2 ] + 1 );
					break;
				case 0x10:
					tempbuffer[ 2 ] = stuff[ sourcePointer++ ];
					tempbuffer[ 3 ] = stuff[ sourcePointer++ ];
					num_bytes_to_copy = ( ( tempbuffer[ 0 ] & 0x0F ) << 12 ) + ( tempbuffer[ 1 ] << 4 ) + ( tempbuffer[ 2 ] >> 4 ) + 0x111;
					backwards_offset = ( ( tempbuffer[ 2 ] & 0x0F ) << 8 ) + tempbuffer[ 3 ] + 1;
					break;
				default:
					num_bytes_to_copy = ( tempbuffer[ 0 ] >> 4 ) + 0x01;
					backwards_offset = ( ( tempbuffer[ 0 ] & 0x0F ) << 8 ) + tempbuffer[ 1 ] + 1;
					break;
				}
				copy_start_index = destPointer - backwards_offset;
				for( quint32 copy_counter = 0; copy_counter < num_bytes_to_copy; copy_counter++ )
				{
					if( ( copy_start_index + copy_counter ) >= destPointer )
					{
						qWarning() << "LZ77_11::Decompress -> Error occured while decompressing: The input seems to be telling us to copy uninitialized data.";
						return QByteArray();
					}
					else
					{
						decompressedData[ destPointer++ ] = decompressedData[ copy_start_index + copy_counter ];
					}
				}
			}
			else
			{
				decompressedData[ destPointer++ ] = stuff[ sourcePointer++ ];
			}
		}
	}
	return decompressedData;
}

LzWindowDictionary::LzWindowDictionary()
{
	WindowSize     = 0x1000;
	WindowStart    = 0;
	WindowLength   = 0;
	MinMatchAmount = 3;
	MaxMatchAmount = 18;
	BlockSize      = 0;
}

QList<int> LzWindowDictionary::Search( const QByteArray &DecompressedData, quint32 offset, quint32 length )
{
	RemoveOldEntries( DecompressedData[ offset ] ); // Remove old entries for this index

	if( offset < (quint32)MinMatchAmount || length - offset < (quint32)MinMatchAmount ) // Can't find matches if there isn't enough data
		return QList<int>() << 0 << 0;

	QList<int>Match = QList<int>() << 0 << 0;
	int MatchStart;
	int MatchSize;

	for( int i = OffsetList[ (quint8)( DecompressedData[ offset ] ) ].size() - 1; i >= 0; i-- )
	{
		MatchStart = OffsetList[ (quint8)( DecompressedData[ offset ] ) ][ i ];
		MatchSize  = 1;

		while( MatchSize < MaxMatchAmount
			   && MatchSize < WindowLength
			   && (quint32)(MatchStart + MatchSize) < offset
			   && offset + MatchSize < length
			   && DecompressedData[ offset + MatchSize ] == DecompressedData[ MatchStart + MatchSize ] )
			MatchSize++;

		if( MatchSize >= MinMatchAmount && MatchSize > Match[ 1 ] ) // This is a good match
		{
			Match = QList<int>() << (int)(offset - MatchStart) << MatchSize;

			if( MatchSize == MaxMatchAmount ) // Don't look for more matches
				break;
		}
	}

	// Return the match.
	// If no match was made, the distance & length pair will be zero
	return Match;
}

// Slide the window
void LzWindowDictionary::SlideWindow( int Amount )
{
	if( WindowLength == WindowSize )
		WindowStart += Amount;
	else
	{
		if( WindowLength + Amount <= WindowSize )
			WindowLength += Amount;
		else
		{
			Amount -= ( WindowSize - WindowLength );
			WindowLength = WindowSize;
			WindowStart += Amount;
		}
	}
}

// Slide the window to the next block
void LzWindowDictionary::SlideBlock()
{
	WindowStart += BlockSize;
}

// Remove old entries
void LzWindowDictionary::RemoveOldEntries( quint8 index )
{
	for( int i = 0; i < OffsetList[ index ].size(); ) // Don't increment i
	{
		if( OffsetList[ index ][ i ] >= WindowStart )
			break;
		else
			OffsetList[ index ].removeAt( 0 );
	}
}

// Set variables
void LzWindowDictionary::SetWindowSize( int size )
{
	WindowSize = size;
}
void LzWindowDictionary::SetMinMatchAmount( int amount )
{
	MinMatchAmount = amount;
}
void LzWindowDictionary::SetMaxMatchAmount( int amount )
{
	MaxMatchAmount = amount;
}
void LzWindowDictionary::SetBlockSize( int size )
{
	BlockSize    = size;
	WindowLength = size; // The window will work in blocks now
}

// Add entries
void LzWindowDictionary::AddEntry( const QByteArray &DecompressedData, int offset )
{
	OffsetList[ (quint8)( DecompressedData[ offset ] ) ] << offset;
}
void LzWindowDictionary::AddEntryRange( const QByteArray &DecompressedData, int offset, int length )
{
	for( int i = 0; i < length; i++ )
		AddEntry( DecompressedData, offset + i );
}
