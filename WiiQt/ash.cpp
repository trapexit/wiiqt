#include "ash.h"
bool IsAshCompressed( const QByteArray ba )
{
    return ba.startsWith( "ASH" );
}

QByteArray DecryptAsh( const QByteArray ba )
{
    if( !IsAshCompressed( ba ) )
    {
	qWarning() << "DecryptAsh -> wrong magic";
	return QByteArray();
    }
    quint32 r[ 32 ];
    quint32 count=0;
    quint32 t;

    quint64 memAddr = (quint64)( ba.data() );//in
    r[4] = 0x80000000;
    qint64 inDiff = memAddr - r[ 4 ];//difference in r[ 4 ] and the real address.  hack to support higher memory addresses than crediar's version

    r[5] = 0x415348;
    r[6] = 0x415348;

//Rvl_decode_ash:

    r[5] = qFromBigEndian(*(quint32 *)( r[4] + inDiff + 4 ) );
    r[5] = r[5] & 0x00FFFFFF;

    quint32 size = r[5];
    //qDebug() << "Decompressed size:" << hex << size;

    char crap2[ size ];
    quint64 memAddr2 = (quint64)( crap2 );//outbuf
    r[3] = 0x90000000;
    qint64 outDiff = memAddr2 - r[ 3 ];//difference in r[ 3 ] and the real address

    quint32 o = r[ 3 ];
    memset( (void*)( r[ 3 ] + outDiff ), 0, size );

    r[24] = 0x10;
    r[28] = qFromBigEndian(*(quint32 *)(r[4]+8 + inDiff));
    r[25] = 0;
    r[29] = 0;
    r[26] = qFromBigEndian(*(quint32 *)(r[4]+0xC + inDiff));
    r[30] = qFromBigEndian(*(quint32 *)(r[4]+r[28] + inDiff));
    r[28] = r[28] + 4;
    //r[8] = 0x8108<<16;
    //HACK, pointer to RAM

    char crap3[ 0x100000 ];
    quint64 memAddr3 = (quint64)( crap3 );//outbuf
    r[8] = 0x84000000;
    qint64 outDiff2 = memAddr3 - r[ 8 ];//difference in r[ 3 ] and the real address
    memset( (void*)( r[8] + outDiff2 ), 0, 0x100000 );

    r[8] = r[8];
    r[9] = r[8] + 0x07FE;
    r[10] = r[9] + 0x07FE;
    r[11] = r[10] + 0x1FFE;
    r[31] = r[11] + 0x1FFE;
    r[23] = 0x200;
    r[22] = 0x200;
    r[27] = 0;

loc_81332124:

    if( r[25] != 0x1F )
    goto loc_81332140;

    r[0] = r[26] >> 31;
    r[26]= qFromBigEndian(*(quint32 *)(r[4] + r[24] + inDiff));
    r[25]= 0;
    r[24]= r[24] + 4;
    goto loc_8133214C;

loc_81332140:

    r[0] = r[26] >> 31;
    r[25]= r[25] + 1;
    r[26]= r[26] << 1;

loc_8133214C:

    if( r[0] == 0 )
    goto loc_81332174;

    r[0] = r[23] | 0x8000;
    *(quint16 *)(r[31] + outDiff2) = (quint16)qFromBigEndian((quint16)r[0]);
    r[0] = r[23] | 0x4000;
    *(quint16 *)(r[31]+2 + outDiff2) = (quint16)qFromBigEndian((quint16)r[0]);

    r[31] = r[31] + 4;
    r[27] = r[27] + 2;
    r[23] = r[23] + 1;
    r[22] = r[22] + 1;

    goto loc_81332124;

loc_81332174:

    r[12] = 9;
    r[21] = r[25] + r[12];
    t = r[21];
    if( r[21] > 0x20 )
    goto loc_813321AC;

    r[21] = (~(r[12] - 0x20))+1;
    r[6] = r[26] >> r[21];
    if( t == 0x20 )
    goto loc_8133219C;

    r[26] = r[26] << r[12];
    r[25] = r[25] + r[12];
    goto loc_813321D0;

loc_8133219C:

    r[26]= qFromBigEndian(*(quint32 *)(r[4] + r[24] + inDiff));
    r[25]= 0;
    r[24]= r[24] + 4;
    goto loc_813321D0;

loc_813321AC:

    r[0] = (~(r[12] - 0x20))+1;
    r[6] = r[26] >> r[0];
    r[26]= qFromBigEndian(*(quint32 *)(r[4] + r[24] + inDiff));
    r[0] = (~(r[21] - 0x40))+1;
    r[24]= r[24] + 4;
    r[0] = r[26] >> r[0];
    r[6] = r[6] | r[0];
    r[25] = r[21] - 0x20;
    r[26] = r[26] << r[25];

loc_813321D0:

    r[12]= (quint16)qFromBigEndian((quint16)(*(quint16 *)(( r[31]  + outDiff2 ) - 2)));
    r[31] -= 2;
    r[27]= r[27] - 1;
    r[0] = r[12] & 0x8000;
    r[12]= (r[12] & 0x1FFF) << 1;
    if( r[0] == 0 )
    goto loc_813321F8;

    *(quint16 *)(r[9]+r[12] + outDiff2 ) = (quint16)qFromBigEndian((quint16)r[6]);//?????
    r[6] = (r[12] & 0x3FFF)>>1;//extrwi %r6, %r12, 14,17
    if( r[27] != 0 )
    goto loc_813321D0;

    goto loc_81332204;

loc_813321F8:

    *(quint16 *)(r[8]+r[12] + outDiff2) = (quint16)qFromBigEndian((quint16)r[6]);
    r[23] = r[22];
    goto loc_81332124;

loc_81332204:

    r[23] = 0x800;
    r[22] = 0x800;

loc_8133220C:

    if( r[29] != 0x1F )
    goto loc_81332228;

    r[0] = r[30] >> 31;
    r[30]= qFromBigEndian(*(quint32 *)(r[4] + r[28] + inDiff));
    r[29]= 0;
    r[28]= r[28] + 4;
    goto loc_81332234;

loc_81332228:

    r[0] = r[30] >> 31;
    r[29]= r[29] + 1;
    r[30]= r[30] << 1;

loc_81332234:

    if( r[0] == 0 )
    goto loc_8133225C;

    r[0] = r[23] | 0x8000;
    *(quint16 *)(r[31] + outDiff2) = (quint16)qFromBigEndian((quint16)r[0]);
    r[0] = r[23] | 0x4000;
    *(quint16 *)(r[31]+2 + outDiff2) = (quint16)qFromBigEndian((quint16)r[0]);

    r[31] = r[31] + 4;
    r[27] = r[27] + 2;
    r[23] = r[23] + 1;
    r[22] = r[22] + 1;

    goto loc_8133220C;

loc_8133225C:

    r[12] = 0xB;
    r[21] = r[29] + r[12];
    t = r[21];
    if( r[21] > 0x20 )
    goto loc_81332294;

    r[21] = (~(r[12] - 0x20))+1;
    r[7] = r[30] >> r[21];
    if( t == 0x20 )
    goto loc_81332284;

    r[30] = r[30] << r[12];
    r[29] = r[29] + r[12];
    goto loc_813322B8;

loc_81332284:

    r[30]= qFromBigEndian(*(quint32 *)(r[4] + r[28] + inDiff));
    r[29]= 0;
    r[28]= r[28] + 4;
    goto loc_813322B8;

loc_81332294:

    r[0] = (~(r[12] - 0x20))+1;
    r[7] = r[30] >> r[0];
    r[30]= qFromBigEndian(*(quint32 *)(r[4] + r[28] + inDiff));
    r[0] = (~(r[21] - 0x40))+1;
    r[28]= r[28] + 4;
    r[0] = r[30] >> r[0];
    r[7] = r[7] | r[0];
    r[29]= r[21] - 0x20;
    r[30]= r[30] << r[29];

loc_813322B8:

    r[12]= (quint16)qFromBigEndian((quint16)(*(quint16 *)((r[31] + outDiff2 ) - 2)));
    r[31] -= 2;
    r[27]= r[27] - 1;
    r[0] = r[12] & 0x8000;
    r[12]= (r[12] & 0x1FFF) << 1;
    if( r[0] == 0 )
    goto loc_813322E0;

    *(quint16 *)(r[11]+r[12] + outDiff2 ) = (quint16)qFromBigEndian((quint16)r[7]);//????
    r[7] = (r[12] & 0x3FFF)>>1;// extrwi %r7, %r12, 14,17
    if( r[27] != 0 )
    goto loc_813322B8;

    goto loc_813322EC;

loc_813322E0:

    *(quint16 *)(r[10]+r[12] + outDiff2 ) = (quint16)qFromBigEndian((quint16)r[7]);
    r[23] = r[22];
    goto loc_8133220C;

loc_813322EC:

    r[0] = r[5];

loc_813322F0:

    r[12]= r[6];

loc_813322F4:

    if( r[12] < 0x200 )
    goto loc_8133233C;

    if( r[25] != 0x1F )
    goto loc_81332318;

    r[31] = r[26] >> 31;
    r[26] = qFromBigEndian(*(quint32 *)(r[4] + r[24] + inDiff));
    r[24] = r[24] + 4;
    r[25] = 0;
    goto loc_81332324;

loc_81332318:

    r[31] = r[26] >> 31;
    r[25] = r[25] + 1;
    r[26] = r[26] << 1;

loc_81332324:

    r[27] = r[12] << 1;
    if( r[31] != 0 )
    goto loc_81332334;

    r[12] = (quint16)qFromBigEndian((quint16)(*(quint16 *)(r[8] + r[27] + outDiff2 )));
    goto loc_813322F4;

loc_81332334:

    r[12] = (quint16)qFromBigEndian((quint16)(*(quint16 *)(r[9] + r[27] + outDiff2 )));
    goto loc_813322F4;

loc_8133233C:

    if( r[12] >= 0x100 )
    goto loc_8133235C;

    *(quint8 *)(r[3] + outDiff) = r[12];
    r[3] = r[3] + 1;
    r[5] = r[5] - 1;
    if( r[5] != 0 )
    goto loc_813322F0;

    goto loc_81332434;

loc_8133235C:

    r[23] = r[7];

loc_81332360:

    if( r[23] < 0x800 )
    goto loc_813323A8;

    if( r[29] != 0x1F )
    goto loc_81332384;

    r[31] = r[30] >> 31;
    r[30] = qFromBigEndian(*(quint32 *)(r[4] + r[28] + inDiff));
    r[28] = r[28] + 4;
    r[29] = 0;
    goto loc_81332390;

loc_81332384:

    r[31] = r[30] >> 31;
    r[29] = r[29] + 1;
    r[30] = r[30] << 1;

loc_81332390:

    r[27] = r[23] << 1;
    if( r[31] != 0 )
    goto loc_813323A0;

    r[23] = (quint16)qFromBigEndian((quint16)(*(quint16 *)(r[10] + r[27] + outDiff2 )));
    goto loc_81332360;

loc_813323A0:

    r[23] = (quint16)qFromBigEndian((quint16)(*(quint16 *)(r[11] + r[27] + outDiff2 )));
    goto loc_81332360;

loc_813323A8:

    r[12] = r[12] - 0xFD;
    r[23] = ~r[23] + r[3] + 1;
    r[5] = ~r[12] + r[5] + 1;
    r[31] = r[12] >> 3;

    if( r[31] == 0 )
    goto loc_81332414;

    count = r[31];

loc_813323C0:

    r[31] = *(quint8 *)(( r[23] + outDiff ) - 1);
    *(quint8 *)(r[3] + outDiff) = r[31];

    r[31] = *(quint8 *)( r[23] + outDiff );
    *(quint8 *)(r[3]+1 + outDiff) = r[31];

    r[31] = *(quint8 *)(( r[23] + outDiff ) + 1);
    *(quint8 *)(r[3]+2 + outDiff) = r[31];

    r[31] = *(quint8 *)(( r[23] + outDiff ) + 2);
    *(quint8 *)(r[3]+3 + outDiff) = r[31];

    r[31] = *(quint8 *)(( r[23] + outDiff ) + 3);
    *(quint8 *)(r[3]+4 + outDiff) = r[31];

    r[31] = *(quint8 *)(( r[23] + outDiff ) + 4);
    *(quint8 *)(r[3]+5 + outDiff) = r[31];

    r[31] = *(quint8 *)(( r[23] + outDiff ) + 5);
    *(quint8 *)(r[3]+6 + outDiff) = r[31];

    r[31] = *(quint8 *)(( r[23] + outDiff ) + 6);
    *(quint8 *)(r[3]+7 + outDiff) = r[31];

    r[23] = r[23] + 8;
    r[3] = r[3] + 8;

    if( --count )
    goto loc_813323C0;

    r[12] = r[12] & 7;
    if( r[12] == 0 )
    goto loc_8133242C;

loc_81332414:

    count = r[12];

loc_81332418:

    r[31] = *(quint8 *)(( r[23] + outDiff ) - 1);
    r[23] = r[23] + 1;
    *(quint8 *)(r[3] + outDiff ) = r[31];
    r[3] = r[3] + 1;

    if( --count )
    goto loc_81332418;

loc_8133242C:

    if( r[5] != 0 )
    goto loc_813322F0;

loc_81332434:

    r[3] = r[0];

    QByteArray ret( r[ 3 ], '\0' );
    QBuffer outBuf( &ret );
    outBuf.open( QIODevice::WriteOnly );
    outBuf.write( (const char*)( o + outDiff ), r[ 3 ] );
    outBuf.close();

    return ret;
}

