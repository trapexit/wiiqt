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
    int off = GetLz77Offset( data );
    if( off == -1 )
    {
	qWarning() << "LZ77::GetDecompressedSize -> no lz77 magic";
	return 0;
    }
    QByteArray ba = data;
    QBuffer buf( &ba );
    if( !buf.open( QBuffer::ReadOnly ) )
    {
	qWarning() << "LZ77::GetDecompressedSize -> Can't create buffer";
	return 0;
    }
    buf.seek( off + 4 );

    quint32 gbaheader;
    buf.seek( off + 4 );
    buf.read( (char*)&gbaheader, 4 );

    return ( gbaheader >> 8 );
}

QByteArray LZ77::Decompress( const QByteArray &compressed, int offset )
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
    infile.seek( offset + 4 );
    infile.read( (char*)&gbaheader, 4 );

    decomp_size = gbaheader >> 8;
    quint8 text_buf[ N + 17 ];

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

QByteArray LZ77::Decompress( const QByteArray &compressed )
{
    int off = GetLz77Offset( compressed );
    if( off < 0 )
    {
	qWarning() << "LZ77::Decompress -> data is not compressed";
	return compressed;
    }
    return Decompress( compressed, off );
}

QByteArray LZ77::Compress( const QByteArray &ba )
{
    //testing 1, 2
    //return ba;
    LZ77 lz;
    QByteArray ret = lz.Compr( ba );
    return ret;
}

QByteArray LZ77::Compr( const QByteArray &ba )
{
    int i, len, r, s, last_match_length, code_buf_ptr;
    char ch;
    int code_buf[ 17 ];
    int mask;
    int N = 4096;
    int F = 18;
    int threshold = 2;
    quint32 filesize = (ba.size() << 8) + 0x10;

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
    output.putChar( 'L' );
    output.putChar( 'Z' );
    output.putChar( '7' );
    output.putChar( '7' );

    output.write( (const char*)&filesize, 4 );

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
