#include "savedatabin.h"
#include "aes.h"
#include "md5.h"
#include "ec.h"

SaveDataBin::SaveDataBin( QByteArray stuff )
{
//    qDebug() << "SaveDataBin::SaveDataBin" << hex << stuff.size();
    _ok = false;
    ngID = 0;
    ngKeyID = 0;
    sg.tid = 0;
    if( stuff.size() < 0xf140 )//header + backup header
    {
        qWarning() << "SaveDataBin::SaveDataBin -> size is too small";
        return;
    }
    quint32 size = 0xf0c0;
    QByteArray header( size, '\0' );

    //decrypt the header
    quint8 iv[ 16 ] = SD_IV;
    quint8 sdkey[ 16 ] = SD_KEY;
    aes_set_key( sdkey );
    aes_decrypt( iv, (quint8*)stuff.data(), (quint8*)header.data(), size );
    //check MD5
    quint8 md5blanker[ 16 ] = MD5_BLANKER;
    QByteArray expected = header.mid( 0xe, 16 );
    QByteArray headerWithBlanker = header.left( 0xe ) + QByteArray( (const char*)&md5blanker, 16 ) + header.right( 0xf0a2 );
    //hexdump( headerWithBlanker.left( 0x50 ) );
    MD5 hash;
    hash.update( headerWithBlanker.data(), size );
    hash.finalize();

    QByteArray actual = QByteArray( (const char*)hash.hexdigestChar(), 16 );
    if( actual != expected )
    {
        qWarning() << "SaveDataBin::SaveDataBin -> md5 mismatch";
        hexdump( expected );
        hexdump( actual );
    }
    //read the tid & banner.bin size
    quint32 bnrSize;
    quint8 bnrPerm;
    QDataStream ds( header );
    ds.setByteOrder( QDataStream::BigEndian );
    ds >> sg.tid;
    ds >> bnrSize;
    ds >> bnrPerm;
    if( bnrSize < 0x72a0 || bnrSize > 0xf0a0 || ( bnrSize - 0x60a0 ) % 0x1200 )
    {
        qWarning() << "SaveDataBin::SaveDataBin -> bad size" << hex << bnrSize;
        return;
    }
    //add the entry for banner.bin in the save struct
    quint8 attr = ( bnrPerm << 2 ) | NAND_FILE;

    sg.attr << attr;
    sg.entries << "/banner.bin";
    sg.data << header.mid( 0x20, bnrSize );

    QBuffer b( &stuff );
    b.open( QIODevice::ReadOnly );
    b.seek( size );

    //read the Bk header
    quint32 tmp;
    quint32 cnt;
    quint32 fSize;
    quint32 tSize;

    b.read( (char*)&tmp, 4 );
    tmp = qFromBigEndian( tmp );
    if( tmp != 0x70 )
    {
        qWarning() << "SaveDataBin::SaveDataBin -> bad hdr size" << hex << tmp;
        b.close();
        return;
    }
    b.read( (char*)&tmp, 4 );
    tmp = qFromBigEndian( tmp );
    if( tmp != 0x426b0001 )
    {
        qWarning() << "SaveDataBin::SaveDataBin -> bad magic" << hex << tmp;
        b.close();
        return;
    }
    b.read( (char*)&tmp, 4 );
    ngID = qFromBigEndian( tmp );
    //qDebug() << "NG id:" << hex << ngID;
    b.read( (char*)&tmp, 4 );
    cnt = qFromBigEndian( tmp );
    b.read( (char*)&tmp, 4 );
    fSize = qFromBigEndian( tmp );
    b.seek( b.pos() + 8 );
    b.read( (char*)&tmp, 4 );
    tSize = qFromBigEndian( tmp );
    //qDebug() << "cnt  :" << hex << cnt;
    //qDebug() << "fSize:" << hex << fSize;
    //qDebug() << "tSize:" << hex << tSize << stuff.size();

    if( (quint32)stuff.size() < fSize + 0xf140 )
    {
        qWarning() << "SaveDataBin::SaveDataBin -> buffer size is less than expected" << hex << fSize;
        b.close();
        return;
    }

    ngMac = stuff.mid( 0xf128, 6 );
    //qDebug() << "ngMac:" << ngMac.toHex();
    //read all the files and folders
    b.seek( 0xf140 );
    for( quint32 i = 0; i < cnt; i++ )
    {
        //QByteArray peek = b.peek( 0x80 );
        //hexdump( peek );
        quint32 size;
        quint8 perm;
        quint8 attr;
        quint8 type;
        quint32 start = b.pos();
        quint8 iv[ 0x10 ];
        QByteArray name;
        b.read( (char*)&tmp, 4 );
        tmp = qFromBigEndian( tmp );
        if( tmp != 0x03adf17e )
        {
            qWarning() << "SaveDataBin::SaveDataBin -> bad file magic" << hex << i << tmp;
            b.close();
            return;
        }
        b.read( (char*)&tmp, 4 );
        size = qFromBigEndian( tmp );
        b.read( (char*)&perm, 1 );
        b.read( (char*)&attr, 1 );//?
        b.read( (char*)&type, 1 );

        name = b.read( 0x45 );
        b.read( (char*)&iv, 0x10 );

        /*qDebug() << "size:" << hex << size
                << "perm:" << perm
                << "attr:" << attr
                << "type:" << type
                << "name:" << QString( name )
                << "iv:" << QByteArray( (const char*)iv, 0x10 ).toHex();*/

        perm = ( perm << 2 ) | type;
        //qDebug() << "perm2:" << hex << perm;

        sg.entries << "/" + name;
        sg.attr << perm;

        switch( type )
        {
        case NAND_FILE:
            {
                QByteArray encData = stuff.mid( start + 0x80, RU( size, 0x40 ) );
                QByteArray decData( RU( size, 0x40 ), '\0' );
                aes_set_key( sdkey );
                aes_decrypt( iv, (quint8*)encData.data(), (quint8*)decData.data(), RU( size, 0x40 ) );
                decData.resize( size );
                sg.data << decData;

                /*qDebug() << QString( name );
                qDebug() << "size:" << hex << size;
                hexdump( decData, 0, 0x30 );*/
            }
            break;
        case NAND_DIR:
            break;
        default:
            qWarning() << "SaveDataBin::SaveDataBin -> unknown type" << hex << i << type;
            return;
            break;
        }

        //seek to beginning of next file
        b.seek( start + 0x80 + RU( size, 0x40 ) );
    }

    //get a couple keys useful for repacking
    quint32 cStart = b.pos();
    //qDebug() << "pos:" << hex << (quint32)b.pos();
    b.seek( b.pos() + 0x144 );
    b.read( (char*)&tmp, 4 );
    ngKeyID = qFromBigEndian( tmp );
    ngSig = stuff.mid( cStart + 0x44, 0x3c );
    //qDebug() << "ngKeyID:" << hex << ngKeyID;
    //qDebug() << "ngSig  :" << hex << ngSig.toHex();

    //check the cert mumbojombo
    b.close();
    quint32 data_size = tSize - 0x340;
    //qDebug() << hex << data_size << tSize;
    QByteArray sha1H = GetSha1( stuff.mid( 0xf0c0, data_size ) );
    sha1H = GetSha1( sha1H );

    _ok = check_ec( (quint8*)stuff.mid( cStart + 0x40, 0x180 ).data(),
                    (quint8*)stuff.mid( cStart + 0x1c0, 0x180 ).data(),
                    (quint8*)stuff.mid( cStart, 0x40 ).data(),
                    (quint8*)sha1H.data() );
    //qDebug() << "_ok:" << _ok;
}
SaveDataBin::SaveDataBin( const SaveGame &old )
{
    ngID = 0;
    ngKeyID = 0;
    sg = old;
    _ok = IsValidSave( sg );
}

const QByteArray SaveDataBin::Data( const QByteArray &ngPriv, const QByteArray &ngSig, const QByteArray &ngMac, quint32 ngId, quint32 ngKeyId )
{
    if( !IsValidSave( sg ) )
    {
        qWarning() << "SaveDataBin::Data -> invalid save data";
        return QByteArray();
    }
    quint8 tmp8;
    QByteArray bnr = DataFromSave( sg, "/banner.bin" );
    if( bnr.isEmpty() )
    {
        qWarning() << "SaveDataBin::Data -> no banner.bin found in the save";
        return QByteArray();
    }

    quint32 ng_ID = ngId ? ngId : ngID;
    quint32 ng_Key_ID = ngKeyId ? ngKeyId : this->ngKeyID;
    QByteArray ng_Sig = ngSig.size() ? ngSig : this->ngSig;
    QByteArray ng_Mac = ngMac.size() ? ngMac : this->ngMac;
    quint32 tmp32;
    quint8 md5blanker[ 16 ] = MD5_BLANKER;

    //quick sanity checks  TODO.. probably need more up in here
    if( ng_Mac.size() != 6 || !ng_ID || !ng_Key_ID || ng_Sig.size() != 0x3c || ng_Mac.size() != 6 || ngPriv.size() != 30 )
    {
        qWarning() << "SaveDataBin::Data -> something isnt right:\n" << ng_Mac.toHex()
                << "\n" << ng_Sig.toHex()
                << "\n" << ng_Mac.toHex()
                << "\n" << ngPriv.toHex()
                << "\n" << hex << ng_ID
                << "\n" << ng_Key_ID;
        return QByteArray();
    }

    //make the different byteArrays that make up this beast and assemble them all at the end
    QByteArray header( 0xd, '\0' );
    QBuffer b( &header );
    b.open( QIODevice::WriteOnly );

    quint64 tmp64 = qFromBigEndian( sg.tid );                                               //tid
    b.write( (const char*)&tmp64, 8 );

    tmp32 = (quint32)qFromBigEndian( (quint32)bnr.size() );                                 //size of banner.bin
    b.write( (const char*)&tmp32, 4 );

    tmp8 = AttrFromSave( sg, "/banner.bin" );                                               //attributes of banner.bin
    tmp8 >>= 2;
    b.write( (const char*)&tmp8, 1 );
    tmp8 = 0;                                                                               //nocopy or some shit like that?
    b.write( (const char*)&tmp8, 1 );
    b.close();
    QByteArray header2 = header +
                         QByteArray( (const char*)&md5blanker, 16 ) +
                         QByteArray( 2, '\0' );                                             //md5 blanker + padding to 0x20
    header2 += bnr;                                                                          //add the banner.bin
    header2 = PaddedByteArray( header2, 0xf0c0 );                                            //pad to 0xf0c0

    MD5 hash;
    hash.update( header2.data(), 0xf0c0 );
    hash.finalize();

    QByteArray actual = QByteArray( (const char*)hash.hexdigestChar(), 16 );
    header += actual + QByteArray( 2, '\0' ) + bnr;
    header = PaddedByteArray( header, 0xf0c0 );

    quint8 iv[ 16 ] = SD_IV;
    quint8 sdkey[ 16 ] = SD_KEY;
    aes_set_key( sdkey );
    QByteArray header3( 0xf0c0, '\0' );//wtf
    aes_encrypt( iv, (quint8*)header.data(), (quint8*)header3.data(), 0xf0c0 );              //encrypt the header

    quint32 fSize = 0;
    quint32 tSize = 0;
    quint32 cnt = sg.attr.size();
    //do the file stuff
    QByteArray files;
    quint16 idx = 0;
    for( quint16 i = 0; i < cnt; i++ )
    {
        if( sg.entries.at( i ) == "/banner.bin" )
        {
            idx++;
            continue;
        }
        //qDebug() << "adding file" << sg.entries.at( i );
        quint32 size = 0;
        quint8 attr = sg.attr.at( i );
        quint8 type = NAND_ATTR_TYPE( attr );
        attr >>= 2;
        QByteArray fHeader( 0x80, '\0' );
        QBuffer c( &fHeader );
        c.open( QIODevice::WriteOnly );

        tmp32 = qFromBigEndian( (quint32)0x3adf17e );                                       //file magic
        c.write( (const char*)&tmp32, 4 );
        c.seek( 8 );
        c.write( (const char*)&attr, 1 );                                                   //permissions
        tmp8 = 0;
        c.write( (const char*)&tmp8, 1 );                                                   //?
        c.write( (const char*)&type, 1 );                                                   //file or dir

        QString name = sg.entries.at( i );                                                  //path
        name.remove( 0, 1 );                                                                //remove leading "/"
        c.write( name.toLatin1() );

        if( type == NAND_FILE )
        {
            size = sg.data.at( idx ).size();                                                //write actual file size
            c.seek( 4 );
            tmp32 = qFromBigEndian( size );
            c.write( (const char*)&tmp32, 4 );
            c.close();
            files += fHeader;
            quint8 iv[ 0x10 ];//copy the iv to keep aes_encrypt from altering the original
            memcpy( &iv, ((quint8*)fHeader.data()) + 0x50, 0x10 );

            QByteArray encData( RU( size, 0x40 ), '\0' );
            QByteArray decPaddad = PaddedByteArray( sg.data.at( idx ), 0x40 );

            aes_set_key( sdkey );
            aes_encrypt( (quint8*)&iv,
                         (const quint8*)decPaddad.data(),
                         (quint8*)encData.data(),
                         encData.size() );

            quint32 fs = encData.size();
            fSize += 0x80 + fs;

            files += encData;
            idx++;
        }
        else
        {
            c.seek( 4 );
            c.write( (const char*)&size, 4 );
            c.close();
            fSize += 0x80;
            files += fHeader;
        }
    }

    QByteArray bkHeader( 0x80, '\0' );                                                      //backup header
    b.setBuffer( &bkHeader );
    b.open( QIODevice::WriteOnly );
    tmp32 = qFromBigEndian( (quint32)0x70 );                                                //bk header size
    b.write( (const char*)&tmp32, 4 );
    tmp32 = qFromBigEndian( (quint32)0x426b0001 );                                          //magic word ( & version )?
    b.write( (const char*)&tmp32, 4 );
    tmp32 = qFromBigEndian( ng_ID );                                                        //ngid
    b.write( (const char*)&tmp32, 4 );
    tmp32 = qFromBigEndian( cnt - 1 );                                                      //file & folder count ( -banner.bin )
    b.write( (const char*)&tmp32, 4 );
    tmp32 = qFromBigEndian( fSize );                                                        //padded data size
    b.write( (const char*)&tmp32, 4 );
    b.seek( 0x1c );
    tmp32 = qFromBigEndian( fSize + 0x3c0 );                                                //total size
    b.write( (const char*)&tmp32, 4 );
    b.seek( 0x60 );
    tmp64 = qFromBigEndian( sg.tid );                                                       //tid
    b.write( (const char*)&tmp64, 8 );
    b.write( ng_Mac );                                                                      //ngmac
    b.close();

    //make the magic cert stuff
    char signer[ 64 ];
    char name[ 64 ];
    quint8 sig[ 0x40 ];
    quint8 ng_cert[ 0x180 ];
    quint8 ap_cert[ 0x180 ];
    quint8 ap_priv[ 30 ];
    quint8 ap_sig[ 60 ];
    sprintf( signer, "Root-CA00000001-MS00000002" );
    sprintf( name, "NG%08x", ng_ID );
    make_ec_cert( (quint8*)&ng_cert, (quint8*)ng_Sig.data(), signer, name, (quint8*)ngPriv.data(), ng_Key_ID );
    memset( (char*)&ap_priv, 0, sizeof( ap_priv ) );
    ap_priv[ 10 ] = 1;
    memset( (char*)&ap_sig, 81, sizeof( ap_sig ) );
    sprintf( signer, "Root-CA00000001-MS00000002-NG%08x", ng_ID );
    sprintf( name, "AP%08x%08x", 1, 2 );
    make_ec_cert( (quint8*)&ap_cert, (quint8*)&ap_sig, signer, name, (quint8*)&ap_priv, 0 );
    QByteArray sha = GetSha1( QByteArray( (const char*)&ap_cert + 0x80, 0x100 ) );
    generate_ecdsa( (quint8*)&ap_sig, (quint8*)&ap_sig + 30, (quint8*)ngPriv.data(), (quint8*)sha.data() );
    make_ec_cert( (quint8*)&ap_cert, (quint8*)&ap_sig, signer, name, (quint8*)&ap_priv, 0 );
    tSize = fSize + 0x80;
    QByteArray stuffToHash = bkHeader + files;
    sha = GetSha1( stuffToHash );
    sha = GetSha1( sha );
    generate_ecdsa( (quint8*)&sig, (quint8*)&sig + 30, (quint8*)&ap_priv, (quint8*)sha.data() );
    tmp32 = qFromBigEndian( 0x2f536969 );
    memcpy( (char*)&sig + 60, (char*)&tmp32, 4 );
//    hexdump( QByteArray( (const char*)&sig, 0x40 ) );
//    hexdump( QByteArray( (const char*)&ng_cert, 0x180 ) );
//    hexdump( QByteArray( (const char*)&ap_cert, 0x180 ) );
    return header3
            + bkHeader
            + files
            + QByteArray( (const char*)&sig, 0x40 )
            + QByteArray( (const char*)&ng_cert, 0x180 )
            + QByteArray( (const char*)&ap_cert, 0x180 );
}

quint32 SaveDataBin::NgID()
{
    return ngID;
}

quint32 SaveDataBin::NgKeyID()
{
    return ngKeyID;
}

const QByteArray SaveDataBin::NgSig()
{
    return ngSig;
}

const QByteArray SaveDataBin::NgMac()
{
    return ngMac;
}

const QByteArray SaveDataBin::DataBinFromSaveStruct( const SaveGame &old, const QByteArray &ngPriv, const QByteArray &ngSig,
                                                     const QByteArray &ngMac, quint32 ngId, quint32 ngKeyId )
{
    SaveDataBin sb( old );
    if( !sb.IsOk() )
        return QByteArray();
    return sb.Data( ngPriv, ngSig, ngMac, ngId, ngKeyId );
}

const SaveGame SaveDataBin::StructFromDataBin( const QByteArray &dataBin )
{
    SaveDataBin sb( dataBin );//no need to check if it is loaded ok, the returned item will have 0 for TID and nothing in it
    return sb.SaveStruct();
}

const QByteArray SaveDataBin::GetFile( const QByteArray &dataBin, const QString &path )
{
    SaveDataBin sb( dataBin );
    if( !sb.IsOk() )
        return QByteArray();

    return DataFromSave( sb.SaveStruct(), path );
}

const QByteArray SaveDataBin::GetBanner( const QByteArray &dataBin )
{
    if( dataBin.size() < 0xf140 )//header + backup header
    {
        qWarning() << "SaveDataBin::GetBanner -> size is too small";
        return QByteArray();
    }
    quint32 size = 0xf0c0;
    QByteArray header( size, '\0' );

    //decrypt the header
    quint8 iv[ 16 ] = SD_IV;
    quint8 sdkey[ 16 ] = SD_KEY;
    aes_set_key( sdkey );
    aes_decrypt( iv, (const quint8*)dataBin.constData(), (quint8*)header.data(), size );

    //read banner.bin size
    quint32 bnrSize;
    QBuffer b( &header );
    b.open( QIODevice::ReadOnly );
    b.seek( 8 );
    b.read( (char*)&bnrSize, 4 );
    b.close();
    bnrSize = qFromBigEndian( bnrSize );

    //checken der sizen
    if( bnrSize < 0x72a0 || bnrSize > 0xf0a0 || ( bnrSize - 0x60a0 ) % 0x1200 )
    {
        qWarning() << "SaveDataBin::GetBanner -> bad size" << hex << bnrSize;
        return QByteArray();
    }
    return header.mid( 0x20, bnrSize );
}

quint32 SaveDataBin::GetSize( QByteArray dataBin )
{
	if( dataBin.size() < 0xf140 )//header + backup header
	{
		qWarning() << "SaveDataBin::GetSize -> size is too small";
		return 0;
	}
	quint32 ret = 0;
	quint32 size = 0xf0c0;
	QByteArray header( size, '\0' );

	//decrypt the header
	quint8 iv[ 16 ] = SD_IV;
	quint8 sdkey[ 16 ] = SD_KEY;
	aes_set_key( sdkey );
	aes_decrypt( iv, (quint8*)dataBin.data(), (quint8*)header.data(), size );
	//hexdump( header, 0, 0x20 );
	//check MD5
	quint8 md5blanker[ 16 ] = MD5_BLANKER;
	QByteArray expected = header.mid( 0xe, 16 );
	QByteArray headerWithBlanker = header.left( 0xe ) + QByteArray( (const char*)&md5blanker, 16 ) + header.right( 0xf0a2 );
	MD5 hash;
	hash.update( headerWithBlanker.data(), size );
	hash.finalize();

	QByteArray actual = QByteArray( (const char*)hash.hexdigestChar(), 16 );
	if( actual != expected )
	{
		qWarning() << "SaveDataBin::GetSize -> md5 mismatch";
		hexdump( expected );
		hexdump( actual );
	}
	//read banner.bin size
	quint32 bnrSize;
	QBuffer buf ( &header );
	buf.open( QIODevice::ReadOnly );
	buf.seek( 8 );
	buf.read( (char*)&bnrSize, 4 );
	bnrSize = qFromBigEndian( bnrSize );

	if( bnrSize < 0x72a0 || bnrSize > 0xf0a0 || ( bnrSize - 0x60a0 ) % 0x1200 )
	{
		qWarning() << "SaveDataBin::GetSize -> bad size" << hex << bnrSize;
		return 0;
	}
	buf.close();
	buf.setBuffer( &dataBin );
	buf.open( QIODevice::ReadOnly );
	buf.seek( 0xf0c0 );
	//read the Bk header
	quint32 tmp;
	quint32 cnt;

	buf.read( (char*)&tmp, 4 );
	tmp = qFromBigEndian( tmp );
	if( tmp != 0x70 )
	{
		qWarning() << "SaveDataBin::GetSize -> bad hdr size" << hex << tmp;
		buf.close();
		return 0;
	}
	buf.read( (char*)&tmp, 4 );
	tmp = qFromBigEndian( tmp );
	if( tmp != 0x426b0001 )
	{
		qWarning() << "SaveDataBin::GetSize -> bad magic" << hex << tmp;
		buf.close();
		return 0;
	}
	buf.seek( 0xf0cc );
	buf.read( (char*)&tmp, 4 );
	cnt = qFromBigEndian( tmp );
	//qDebug() << "cnt  :" << hex << cnt;
	buf.seek( 0xf140 );
	ret += bnrSize;
	for( quint32 i = 0; i < cnt; i++ )
	{
		//QByteArray peek = buf.peek( 0x80 );
		//hexdump( peek );
		quint32 size;
		quint32 start = buf.pos();
		buf.read( (char*)&tmp, 4 );
		tmp = qFromBigEndian( tmp );
		if( tmp != 0x03adf17e )
		{
			qWarning() << "SaveDataBin::GetSize -> bad file magic" << hex << i << tmp;
			break;
		}
		buf.read( (char*)&tmp, 4 );
		size = qFromBigEndian( tmp );
		ret += size;

		//seek to beginning of next file
		buf.seek( start + 0x80 + RU( size, 0x40 ) );
	}
	buf.close();
	return ret;
}
