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
/*
block ff1 cluster 7 page: 6 ( 3fc7e )
NandBin::GetPage(  3fc7e ,  true  )

00000000  ff1646bd cef67127 e662a4dc 5154ec52  ..F...q'.b..QT.R
00000010  c844ebb9 fb1646bd cef67127 e662a4dc  .D....F...q'.b..
00000020  51000000 00000000 00000000 00000000  Q...............
00000030  00000000 00000000 00000000 00000000  ................
block ff1 cluster 7 page: 7 ( 3fc7f )
NandBin::GetPage(  3fc7f ,  true  )

00000000  ff54ec52 c844ebb9 fb000000 0020049e  .T.R.D....... ..
00000010  00000000 40000000 002004f1 a8000000  ....@.... ......
00000020  00000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................

block ff3 cluster 7 page: 6 ( 3fcfe )
NandBin::GetPage(  3fcfe ,  true  )

00000000  ff199405 907b607c 4cd691d2 825f2de9  .....{`|L...._-.
00000010  18185217 38199405 907b607c 4cd691d2  ..R.8....{`|L...
00000020  82000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................
block ff3 cluster 7 page: 7 ( 3fcff )
NandBin::GetPage(  3fcff ,  true  )

00000000  ff5f2de9 18185217 38600000 000043dd  ._-...R.8`....C.
00000010  05cf0bd7 38c882b2 24b67f57 6087c41f  ....8...$..W`...
00000020  4d000000 00000000 00000000 00000000  M...............
00000030  00000000 00000000 00000000 00000000  ................

block ff5 cluster 7 page: 6 ( 3fd7e )
NandBin::GetPage(  3fd7e ,  true  )

00000000  ff1c1964 3e97b995 b864f566 b9fa025d  ...d>....d.f...]
00000010  4f708e95 cc1c1964 3e97b995 b864f566  Op.....d>....d.f
00000020  b9000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................
block ff5 cluster 7 page: 7 ( 3fd7f )
NandBin::GetPage(  3fd7f ,  true  )

00000000  fffa025d 4f708e95 cc2004f8 80b8f3d4  ...]Op... ......
00000010  b94820bc f631c4c2 40c8e1da 12a29ba9  .H ..1..@.......
00000020  df000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................
*//*
block ff7 cluster 7 page: 6 ( 3fdfe )
NandBin::GetPage(  3fdfe ,  true  )

00000000  ffff8ca6 8f936ad2 d4fb8424 16cd48ef  ......j....$..H.
00000010  1ee1003f edff8ca6 8f936ad2 d4fb8424  ...?......j....$
00000020  16000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................
block ff7 cluster 7 page: 7 ( 3fdff )
NandBin::GetPage(  3fdff ,  true  )

00000000  ffcd48ef 1ee1003f ed000000 1020000d  ..H....?..... ..
00000010  eb000000 01000000 03000000 022004f7  ............. ..
00000020  f4000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................

block ff9 cluster 7 page: 6 ( 3fe7e )
NandBin::GetPage(  3fe7e ,  true  )

00000000  ffada309 ba23b5e5 fa37a52d 10d13f10  .....#...7.-..?.
00000010  72265162 43ada309 ba23b5e5 fa37a52d  r&QbC....#...7.-
00000020  10000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................
block ff9 cluster 7 page: 7 ( 3fe7f )
NandBin::GetPage(  3fe7f ,  true  )

00000000  ffd13f10 72265162 432000ae 00000000  ..?.r&QbC ......
00000010  002004ae 00000000 102004f1 e02000ad  . ....... ... ..
00000020  c4000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................

block ffb cluster 7 page: 6 ( 3fefe )
NandBin::GetPage(  3fefe ,  true  )

00000000  ff40a82f f3e32161 5f9e91e7 841daf5e  .@./..!a_......^
00000010  c74678b2 b540a82f f3e32161 5f9e91e7  .Fx..@./..!a_...
00000020  84000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................
block ffb cluster 7 page: 7 ( 3feff )
NandBin::GetPage(  3feff ,  true  )

00000000  ff1daf5e c74678b2 b52000ae 0000ed15  ...^.Fx.. ......
00000010  e892928e 766d0000 00000000 00022001  ....vm........ .
00000020  da000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................

block ffd cluster 7 page: 6 ( 3ff7e )
NandBin::GetPage(  3ff7e ,  true  )

00000000  ff80ded9 67d7c195 ffd65a8b 907ea776  ....g.....Z..~.v
00000010  8c56dc33 8280ded9 67d7c195 ffd65a8b  .V.3....g.....Z.
00000020  90000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................
block ffd cluster 7 page: 7 ( 3ff7f )
NandBin::GetPage(  3ff7f ,  true  )

00000000  ff7ea776 8c56dc33 82000000 00200091  .~.v.V.3..... ..
00000010  c0000000 40000000 002004f1 80000000  ....@.... ......
00000020  00000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................

block fff cluster 7 page: 6 ( 3fffe )
NandBin::GetPage(  3fffe ,  true  )

00000000  ff9570f7 915460fe 3f32d363 9a1ebfa9  ..p..T`.?2.c....
00000010  74d3e0a4 969570f7 915460fe 3f32d363  t.....p..T`.?2.c
00000020  9a000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................
block fff cluster 7 page: 7 ( 3ffff )
NandBin::GetPage(  3ffff ,  true  )

00000000  ff1ebfa9 74d3e0a4 962004f8 80b8f3d4  ....t.... ......
00000010  b94820bc f631c4c2 40c8e1da 12a29ba9  .H ..1..@.......
00000020  df000000 00000000 00000000 00000000  ................
00000030  00000000 00000000 00000000 00000000  ................

 */
