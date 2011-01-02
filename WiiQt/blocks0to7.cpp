#include "blocks0to7.h"
#include "tools.h"
#include "tiktmd.h"

Blocks0to7::Blocks0to7( QList<QByteArray>blocks )
{
    _ok = false;
    if( !blocks.isEmpty() )
        SetBlocks( blocks );
}

bool Blocks0to7::SetBlocks( QList<QByteArray>blocks )
{
    //qDebug() << "Blocks0to7::SetBlocks" << blocks.size();
    _ok = false;
    this->blocks.clear();
    boot2Infos.clear();
    quint16 cnt = blocks.size();
    if( cnt != 8 )
        return false;
    for( quint16 i = 0; i < cnt; i++ )
    {
        if( blocks.at( i ).size() != 0x20000 )
        {
            qWarning() << "Blocks0to7::SetBlocks -> block" << i << "is" << hex << blocks.at( i ).size() << "bytes";
            return false;
        }
    }
    this->blocks = blocks;
    _ok = true;
    return true;
}

quint8 Blocks0to7::Boot1Version()
{
    if( blocks.size() != 8 )
    {
        qWarning() << "Blocks0to7::Boot1Version -> not enough blocks" << blocks.size();
        return BOOT_1_UNK;
    }
    QByteArray hash = GetSha1( blocks.at( 0 ) );

    if( hash == QByteArray( "\x4a\x7c\x6f\x30\x38\xde\xea\x7a\x07\xd3\x32\x32\x02\x4b\xe9\x5a\xfb\x56\xbf\x65" ) )
        return BOOT_1_A;
    if( hash == QByteArray( "\x2c\xdd\x5a\xff\xd2\xe7\x8c\x53\x76\x16\xa1\x19\xa7\xa2\xe1\xc5\x68\xe9\x1f\x22" ) )
        return BOOT_1_B;
    if( hash == QByteArray( "\xf0\x1e\x8a\xca\x02\x9e\xe0\xcb\x52\x87\xf5\x05\x5d\xa1\xa0\xbe\xd2\xa5\x33\xfa" ) )
        return BOOT_1_C;
    if( hash == QByteArray( "\x8d\x9e\xcf\x2f\x8f\x98\xa3\xc1\x07\xf1\xe5\xe3\x6f\xf2\x4d\x57\x7e\xac\x36\x08" ) )
        return BOOT_1_D; //displayed as "boot1?" in ceilingcat

    qWarning() << "Blocks0to7::Boot1Version -> unknown boot1 hash:" << hash.toHex();

    return BOOT_1_UNK;
}

//really ugly thing to get the different versions of boot2.
//this doesnt take into account the possibility that boot2 is bigger and takes up more than 2 blocks
//there are 0x40 blocks in the blockmap, but only 8 are used.  maybe IOS has the authority to hijack the others if
//it runs out of room here.  if that ever happns, this code will become quite wrong
QList<Boot2Info> Blocks0to7::Boot2Infos()
{
    if( !boot2Infos.isEmpty() )
    {
        //qDebug() << "Blocks0to7::Boot2Infos() returning data from last time";
        return boot2Infos;
    }

    QList< Boot2Info > ret;
    if( blocks.size() != 8 )
        return ret;

    quint16 cnt = blocks.size();
    if( cnt != 8 )
        return ret;

    //get all the blockmaps
    quint16 newest = 0;
    quint8 lbm[ 8 ];
    for( quint8 i = 1; i < 8; i++ )
    {
        Boot2Info info = GetBlockMap( blocks.at( i ) );
        if( info.state == BOOT_2_ERROR )//this block doesnt contain a decent blockmap
            continue;

        info.secondBlock = i;	    //this one is easy enough
        //find the first block that belongs to this second one
        if( i > 4 )//blocks are backwards
        {
            bool found = false;
            for( quint8 j = 7; j > i; j++ )
            {
                if( info.blockMap[ j ] )//marked bad
                    continue;

                if( ( ( j > i + 1 ) && !info.blockMap[ i + 1 ] )//probably a much cleaner way to write this
                    || ( j == 6 && !info.blockMap[ 7 ] ) )	//but basically check for a couple stupid shit in the layout that really shouldnt ever happen
                    break;

                info.firstBlock = j;
                found = true;
                break;
            }
            if( !found )
                continue;
        }
        else//blocks are forwards
        {
            bool found = false;
            for( quint8 j = 0; j < i; j++ )
            {
                if( info.blockMap[ j ] )//marked bad
                    continue;

                info.firstBlock = j;
                found = true;
                break;
            }
            if( !found )
                continue;
            found = false;
            //probably a much cleaner way to write this
            //but basically check for a couple stupid shit in the layout that really shouldnt ever happen
            for( quint8 j = info.firstBlock + 1; j < info.secondBlock && !found; j++ )
            {
                if( info.blockMap[ j ] )//marked bad
                    continue;
                found = true;
            }
            if( found )//this means there is some other good block between the first block and this one that has the block map
                continue;
        }
        //we made it this far, it means that so far we have a correct looking blockmap that points to this copy of boot2
        if( info.generation > newest )
        {
            newest = info.generation;
            for( quint8 j = 0; j < 8; j++ )
                lbm[ j ] = info.blockMap[ j ];
        }

        ret << info;
    }
    //qDebug() << "newest blockmap" << QByteArray( (const char*)&lbm, 8 ).toHex();

    cnt = ret.size();
    bool foundBoot = false;
    bool foundBackup = false;
    for( quint8 i = 0; i < cnt; i++ )
    {
        ret[ i ] = CheckHashes( ret[ i ] );//check all the hashes and stuff
        if( !foundBoot && !lbm[ ret.at( i ).firstBlock ] && !lbm[ ret.at( i ).secondBlock ] )
        {
            //qDebug() << "copy" << i << "is used when booting";
            ret[ i ].state |= BOOT_2_USED_TO_BOOT;
            //ret[ i ].usedToBoot = true;
            foundBoot = true;
        }
        else if( lbm[ ret.at( i ).firstBlock ] || lbm[ ret.at( i ).secondBlock ] )
        {
            ret[ i ].state |= BOOT_2_MARKED_BAD;
        }
    }
    for( quint8 i = ret.size(); !foundBackup && i > 0; i-- )
    {
        if( !lbm[ ret.at( i - 1 ).firstBlock ] && !lbm[ ret.at( i - 1 ).secondBlock ] && ret.at( i - 1 ).firstBlock > ret.at( i - 1 ).secondBlock )
        {
            //qDebug() << "copy" << i << "is used when booting from backup";
            ret[ i - 1 ].state |= BOOT_2_BACKUP_COPY;
            foundBackup = true;
            if( !foundBoot )
                ret[ i - 1 ].state |= BOOT_2_USED_TO_BOOT;
        }
    }
    boot2Infos = ret;
    return ret;
}

Boot2Info Blocks0to7::GetBlockMap( QByteArray block )
{
    Boot2Info ret;
    ret.state = BOOT_2_ERROR;
    if( block.size() != 0x20000 )
        return ret;

    QByteArray first = block.mid( 0x1f800, 0x4c );
    QByteArray second = block.mid( 0x1f84c, 0x4c );
    QByteArray third = block.mid( 0x1f898, 0x4c );
    QByteArray goodOne = QByteArray( "\x26\xF2\x9A\x40\x1E\xE6\x84\xCF" );
    if( first.startsWith( goodOne ) && ( first == second || first == third ) )
        goodOne = first;
    else if( second.startsWith( goodOne ) && ( second == third ) )
        goodOne = second;
    else
        return ret;

    ret.generation = goodOne.mid( 8, 4 ).toHex().toInt( NULL, 16 );
    for( quint8 i = 0; i < 8; i++ )
        ret.blockMap[ i ] = goodOne.at( i + 12 );

    ret.state = BOOT_2_TIK_SIG_OK;	//just assign this for now.  it will be corrected before this data leaves the class
    return ret;
}

Boot2Info Blocks0to7::CheckHashes( Boot2Info info )
{
    Boot2Info ret = info;
    ret.state = BOOT_2_ERROR_PARSING;

    QByteArray stuff = blocks.at( ret.firstBlock );
    QBuffer b( &stuff );
    b.open( QIODevice::ReadOnly );
    quint32 headerSize;
    quint32 dataOff;
    quint32 certSize;
    quint32 ticketSize;
    quint32 tmdSize;
    quint32 tmp;

    b.read( (char*)&tmp, 4 );
    headerSize = qFromBigEndian( tmp );
    if( headerSize != 0x20 )
        return ret;

    b.read( (char*)&tmp, 4 );
    dataOff = qFromBigEndian( tmp );
    b.read( (char*)&tmp, 4 );
    certSize = qFromBigEndian( tmp );
    b.read( (char*)&tmp, 4 );
    ticketSize = qFromBigEndian( tmp );
    b.read( (char*)&tmp, 4 );
    tmdSize = qFromBigEndian( tmp );
    b.close();

    QByteArray tikD = stuff.mid( headerSize + certSize, ticketSize );
    QByteArray tmdD = stuff.mid( headerSize + certSize + ticketSize, tmdSize );
    Tmd t( tmdD );
    Ticket ticket( tikD );
    if( t.Tid() != 0x100000001ull || ticket.Tid() != 0x100000001ull )
    {
        qWarning() << "Blocks0to7::CheckHashes -> bad TID";
        return ret;
    }
    ret.state = 0;
    int res = check_cert_chain( tikD );
    switch( res )
    {
    default:
    case ERROR_SIG_TYPE:
    case ERROR_SUB_TYPE:
    case ERROR_RSA_TYPE_UNKNOWN:
    case ERROR_CERT_NOT_FOUND:
        ret.state = BOOT_2_ERROR;
        qWarning() << "check_cert_chain( tikD ):" << res;
        return ret;
        break;
    case ERROR_RSA_TYPE_MISMATCH:
    case ERROR_RSA_HASH:
        ret.state = BOOT_2_BAD_SIGNATURE;
        //qWarning() << "check_cert_chain( tikD ):" << res;
        return ret;
        break;
    case ERROR_RSA_FAKESIGNED:
        ret.state |= BOOT_2_TIK_FAKESIGNED;
        break;
    case ERROR_SUCCESS:
        ret.state |= BOOT_2_TIK_SIG_OK;
        break;
    }
    res = check_cert_chain( tmdD );
    switch( res )
    {
    default:
    case ERROR_SIG_TYPE:
    case ERROR_SUB_TYPE:
    case ERROR_RSA_TYPE_UNKNOWN:
    case ERROR_CERT_NOT_FOUND:
        ret.state = BOOT_2_ERROR;
        //qWarning() << "check_cert_chain( tikD ):" << res;
        return ret;
        break;
    case ERROR_RSA_TYPE_MISMATCH:
    case ERROR_RSA_HASH:
        ret.state = BOOT_2_BAD_SIGNATURE;
        //qWarning() << "check_cert_chain( tikD ):" << res;
        return ret;
        break;
    case ERROR_RSA_FAKESIGNED:
        {
            ret.state |= BOOT_2_TMD_FAKESIGNED;
            if( tmdD.contains( "BM1.1" ) )
                ret.version = BOOTMII_11;
            else if( tmdD.contains( "BM1.3" ) )
                ret.version = BOOTMII_13;
            else
                ret.version = BOOTMII_UNK;
        }
        break;
    case ERROR_SUCCESS:
        {
            ret.state |= BOOT_2_TMD_SIG_OK;
            ret.version = t.Version();
        }
        break;
    }

    //now decrypt boot2 and check the hash ( only checking 1 content because thats all there is )
    stuff += blocks.at( ret.secondBlock );

    AesSetKey( ticket.DecryptedKey() );
    QByteArray decD = AesDecrypt( 0, stuff.mid( dataOff, RU( 0x40, t.Size( 0 ) ) ) );
    decD.resize( t.Size( 0 ) );
    QByteArray realHash = GetSha1( decD );
    if( realHash != t.Hash( 0 ) )
        ret.state |= BOOT_2_BAD_CONTENT_HASH;
    return ret;
}

