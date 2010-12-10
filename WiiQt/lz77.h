#ifndef LZ77_H
#define LZ77_H

#include "includes.h"

//class for daling with LZ77 compression
//! in most cases, you just want to use the static functions
//  QByteArray stuff = LZ77::Decompress( someCompressedData );
//  QByteArray compressedData = LZ77::Compress( someData );
class LZ77
{
public:
    LZ77();
    void InsertNode( int r );
    void DeleteNode( int p );
    void InitTree();

    //gets the offset in a bytearray if the lz77 magic word
    static int GetLz77Offset( const QByteArray &data );

    //gets the decompressed size of a lz77 compressed buffer
    static quint32 GetDecompressedSize( const QByteArray &data );

    //used internally by the static compression function
    QByteArray Compr( const QByteArray &ba );

    static QByteArray Decompress( const QByteArray &compressed, int offset );

    //finds the lz77 magic word and decompressed the data after it
    //  returns only the decompressed data.  anything before the lz77 magic word is not included
    static QByteArray Decompress( const QByteArray &compressed );

    //compressed a qbytearray with the lz77 argorythm
    //returns a qbytearray ncluding the lz77 header
    static QByteArray Compress( const QByteArray &ba );

private:
    int lson[ 4097 ];
    int rson[ 4353 ];
    int dad[ 4097 ];
    quint16 text_buf[ 4113 ];
    int match_position;
    int match_length;
    int textsize;
    int codesize;
};

#endif // LZ77_H
