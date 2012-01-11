#ifndef LZ77_H
#define LZ77_H

#include "includes.h"

//class for dealing with LZ77 compression (version 0x10)
//! in most cases, you just want to use the static functions
//  QByteArray stuff = LZ77::Decompress( someCompressedData );
//  QByteArray compressedData = LZ77::Compress( someData );
class LZ77
{
public:
	enum CompressionType
	{
		None,		// not compressed
		v10,		// version 0x10
		v11,		// version 0x11
		v10_w_magic	// version 0x10 with "LZ77" magic bytes
	};

    LZ77();
    void InsertNode( int r );
    void DeleteNode( int p );
    void InitTree();

	//gets the offset in a bytearray of the lz77 magic word
    static int GetLz77Offset( const QByteArray &data );

    //gets the decompressed size of a lz77 compressed buffer
    static quint32 GetDecompressedSize( const QByteArray &data );

	//decompress a buffer that is compressed with the 0x10 variant
	static QByteArray Decompress_v10( const QByteArray &compressed, int offset );



	static QByteArray Compress( const QByteArray &ba, CompressionType type );
    //compressed a qbytearray with the lz77 argorythm
    //returns a qbytearray ncluding the lz77 header
	static QByteArray Compress_v10( const QByteArray &ba, bool addMagic = true );

	//check the type of archive, and get theoffset of the "LZ77" magic word in the case of v10_w_magic
	static CompressionType GetCompressedType( const QByteArray &data, int *outOffset = NULL );

	// decompress data and get whatever type of compression was used on the data
	static QByteArray Decompress( const QByteArray &stuff, CompressionType *outType = NULL );

private:
    int lson[ 4097 ];
    int rson[ 4353 ];
    int dad[ 4097 ];
    quint16 text_buf[ 4113 ];
    int match_position;
    int match_length;
    int textsize;
    int codesize;


	//used internally by the static compression function
	QByteArray Compr_v10( const QByteArray &ba, bool addMagic = true );
};

class LZ77_11
{
public:
	LZ77_11();
	static QByteArray Compress( const QByteArray &stuff );
	static QByteArray Decompress( const QByteArray stuff );
};

class LzWindowDictionary
{
public:
	LzWindowDictionary();

	QList<int> Search( const QByteArray &DecompressedData, quint32 offset, quint32 length );
	void SlideWindow( int Amount );
	void SlideBlock();
	void RemoveOldEntries( quint8 index );
	void SetWindowSize( int size );
	void SetMinMatchAmount( int amount );
	void SetMaxMatchAmount( int amount );
	void SetBlockSize( int size );
	void AddEntry( const QByteArray &DecompressedData, int offset );
	void AddEntryRange( const QByteArray &DecompressedData, int offset, int length );

private:
	int WindowSize;
	int WindowStart;
	int WindowLength;
	int MinMatchAmount;
	int MaxMatchAmount;
	int BlockSize;
	QList<int> OffsetList[ 0x100 ];
};

#endif // LZ77_H
