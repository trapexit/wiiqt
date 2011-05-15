#ifndef NANDSPARE_H
#define NANDSPARE_H

//some class to handle the ecc & hmac data in the nandBin

#include "includes.h"
#include "sha1.h"


class NandSpare
{
public:
    NandSpare();
    void SetHMacKey( const QByteArray key );
    QByteArray Get_hmac_data( const QByteArray cluster, quint32 uid, const unsigned char *name, quint32 entry_n, quint32 x3, quint16 blk );
    QByteArray Get_hmac_meta( const QByteArray cluster, quint16 super_blk );

    static QByteArray CalcEcc( QByteArray in );
    static quint8 Parity( quint8 x );
private:
    QByteArray hmacKey;

};

#endif // NANDSPARE_H

/*
spare data...

0x40 bytes
0x30 = hmac
0x10 = ecc

block 0 ( boot1 )
all hmac is 0xffs
all ecc is calc'd. after the first 9 pages, it works out to all 0s

block 1 ( boot2v2 )
all hmac is 0xff and then 0s

block 2
same as above until page 2
starting at page 2, no ecc or hmac is written.  its all 0xff
spare data is only written for pages used, not on a cluster basis

block 3 ( bootmii )
same as block 1

block 4
same as above( hmac starts with 0xff then all 0s.  after page2 cluster 2 all ecc works at to 0s as well )
spare data is written for clusters used, not on a page basis

block 5 ( never been used )
all spare data is 0xffs, nothing is calculated

block 6 ( boot2 copy )
same as block 2

block 7
same as block 1



super blocks

hmac data is only on
block ff1 cluster 7 page: 6 ( 3fc7e )
block ff1 cluster 7 page: 7 ( 3fc7f )
block ff3 cluster 7 page: 6 ( 3fcfe )
block ff3 cluster 7 page: 7 ( 3fcff )
block ff5 cluster 7 page: 6 ( 3fd7e )
block ff5 cluster 7 page: 7 ( 3fd7f )
block ff7 cluster 7 page: 6 ( 3fdfe )
block ff7 cluster 7 page: 7 ( 3fdff )
block ff9 cluster 7 page: 6 ( 3fe7e )
block ff9 cluster 7 page: 7 ( 3fe7f )
block ffb cluster 7 page: 6 ( 3fefe )
block ffb cluster 7 page: 7 ( 3feff )
block ffd cluster 7 page: 6 ( 3ff7e )
block ffd cluster 7 page: 7 ( 3ff7f )
block fff cluster 7 page: 6 ( 3fffe )
block fff cluster 7 page: 7 ( 3ffff )

*/
