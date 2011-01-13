#include "wad.h"
#include "tools.h"
#include "tiktmd.h"

static QByteArray globalCert;

Wad::Wad( const QByteArray &stuff )
{
	ok = false;
	if( !stuff.size() )//prevent error text when it isnt required
		return;
	if( stuff.size() < 0x80 )//less than this and there is definitely nothing there
	{
		Err( "Size is < 0x80" );
		return;
	}

	QByteArray copy = stuff;
	QBuffer b( &copy );
	b.open( QIODevice::ReadOnly );

	quint32 tmp;
	if(b.read( (char*)&tmp, 4 ) != 4)
	{
		b.close();
		Err( "Can't read header size" );
		return;
	}
	if( qFromBigEndian( tmp ) != 0x20 )
	{
		b.close();
		hexdump(stuff, 0, 0x10);
		Err( "Bad header size" );
		return;
	}
	b.read( (char*)&tmp, 4 );
	tmp = qFromBigEndian( tmp );
	if( tmp != 0x49730000 &&
		tmp != 0x69620000 &&
		tmp != 0x426b0000 )
	{
		b.close();
		hexdump( stuff, 0, 0x40 );
		Err( "Bad file magic word" );
		return;
	}

	quint32 certSize;
	quint32 tikSize;
	quint32 tmdSize;
	quint32 appSize;
	quint32 footerSize;

	b.read( (char*)&certSize, 4 );
	certSize = qFromBigEndian( certSize );

	b.seek( 0x10 );
	b.read( (char*)&tikSize, 4 );
	tikSize = qFromBigEndian( tikSize );
	b.read( (char*)&tmdSize, 4 );
	tmdSize = qFromBigEndian( tmdSize );
	b.read( (char*)&appSize, 4 );
	appSize = qFromBigEndian( appSize );
	b.read( (char*)&footerSize, 4 );
	footerSize = qFromBigEndian( footerSize );

	b.close();//close the buffer, the rest of the data can be checked without it

	//sanity check this thing
	quint32 s = stuff.size();
	if( s < ( RU( certSize, 0x40 ) + RU( tikSize, 0x40 ) + RU( tmdSize, 0x40 ) + RU( appSize, 0x40 ) + RU( footerSize, 0x40 ) ) )
	{
		Err( "Total size is less than the combined sizes of all the parts that it is supposed to contain" );
		return;
	}

	quint32 pos = 0x40;
	certData = stuff.mid( pos, certSize );
	pos += RU( certSize, 0x4 );
	tikData = stuff.mid( pos, tikSize );
	pos += RU( tikSize, 0x40 );
	tmdData = stuff.mid( pos, tmdSize );
	pos += RU( tmdSize, 0x40 );

	Ticket ticket( tikData );
	Tmd t( tmdData );

	//the constructor for Ticket may have fixed a bad key index.  replace the data just incase it did
	tikData = ticket.Data();

	//hexdump( tikData );
	//hexdump( tmdData );

	if( ticket.Tid() != t.Tid() )
		qWarning() << "wad contains 2 different TIDs";

	quint32 cnt = t.Count();
	//qDebug() << "Wad contains" << hex << cnt << "contents";

	//another quick sanity check
	quint32 totalSize = 0;
	for( quint32 i = 0; i < cnt; i++ )
		totalSize += t.Size( i );

	if( totalSize > appSize )
	{
		Err( "Size of all the apps in the tmd is greater than the size in the wad header" );
		return;
	}
	//read all the contents, check the hash, and remember the data ( still encrypted )
	for( quint32 i = 0; i < cnt; i++ )
	{

		quint32 s = RU( t.Size( i ), 0x40 );
		//qDebug() << "content" << i << "is at" << hex << pos
		//        << "with size" << s;
		QByteArray encData = stuff.mid( pos, s );
		pos += s;

		//doing this here in case there is some other object that
		//is using the AES that would change the key on us
		AesSetKey( ticket.DecryptedKey() );

		QByteArray decData = AesDecrypt( i, encData );
		decData.resize( t.Size( i ) );
		QByteArray realHash = GetSha1( decData );
		if( realHash != t.Hash( i ) )
		{
			Err( QString( "hash doesnt match for content %1" ).arg( i ) );
		}
		partsEnc << encData;
	}
	//wtf?  some VC titles have a full banner as the footer?  maybe somebody's wad packer is busted
	//QByteArray footer = stuff.mid( pos, stuff.size() - pos );
	//qDebug() << "footer";
	//hexdump( footer );
	ok = true;
}

Wad::Wad( const QList< QByteArray > &stuff, bool encrypted )
{
	ok = false;
    if( stuff.size() < 3 )
    {
        Err( "Cant treate a wad with < 3 items" );
        return;
    }

    tmdData = stuff.at( 0 );
    tikData = stuff.at( 1 );

    Ticket ticket( tikData );
    Tmd t( tmdData );

    quint16 cnt = stuff.size() - 2;
    if( cnt != t.Count() )
    {
        Err( "The number of items given doesnt match the number in the tmd" );
        return;
    }
    for( quint16 i = 0; i < cnt; i++ )
    {
        QByteArray encData;

        if( encrypted )
        {
            encData = stuff.at( i + 2 );
        }
        else
        {
            QByteArray decDataPadded = PaddedByteArray( stuff.at( i + 2 ), 0x40 );
            //doing this here in case there is some other object that is using the AES that would change the key on us
            AesSetKey( ticket.DecryptedKey() );
            encData = AesEncrypt( i, decDataPadded );
        }
        partsEnc << encData;
    }
    ok = true;

}

Wad::Wad( QDir dir )
{
	ok = false;
	QFileInfoList tmds = dir.entryInfoList( QStringList() << "*.tmd" << "tmd.*", QDir::Files );
	if( tmds.isEmpty() )
	{
		Err( "TMD not found" );
		return;
	}
	tmdData = ReadFile( tmds.at( 0 ).absoluteFilePath() );
	if( tmdData.isEmpty() )
		return;
	QFileInfoList tiks = dir.entryInfoList( QStringList() << "*.tik" << "cetk", QDir::Files );
	if( tiks.isEmpty() )
	{
		Err( "Ticket not found" );
		return;
	}
	tikData = ReadFile( tiks.at( 0 ).absoluteFilePath() );
	if( tikData.isEmpty() )
		return;

	Tmd t( tmdData );
	Ticket ticket( tikData );

	//make sure to only add the tmd & ticket without all the cert mumbo jumbo
	tmdData = t.Data();
	tikData = ticket.Data();

	quint16 cnt = t.Count();

	bool tmdChanged = false;
	for( quint16 i = 0; i < cnt; i++ )
	{
		QByteArray appD = ReadFile( dir.absoluteFilePath( t.Cid( i ) + ".app" ) );
		if( appD.isEmpty() )
		{
			Err( t.Cid( i ) + ".app not found" );
			return;
		}

		if( (quint32)appD.size() != t.Size( i ) )
		{
			t.SetSize( i, appD.size() );
			tmdChanged = true;
		}
		QByteArray realHash = GetSha1( appD );
		if( t.Hash( i ) != realHash )
		{
			t.SetHash( i, realHash );
			tmdChanged = true;
		}
		AesSetKey( ticket.DecryptedKey() );
		appD = PaddedByteArray( appD, 0x40 );
		QByteArray encData = AesEncrypt( i, appD );
		partsEnc << encData;
	}
	//if something in the tmd changed, fakesign it
	if( tmdChanged )
	{
		if( !t.FakeSign() )
		{
			Err( "Error signing the wad" );
			return;
		}
		else
		{
			tmdData = t.Data();
		}
	}
	QFileInfoList certs = dir.entryInfoList( QStringList() << "*.cert", QDir::Files );
	if( !certs.isEmpty() )
	{
		certData = ReadFile( certs.at( 0 ).absoluteFilePath() );
	}
	ok = true;
}

void Wad::SetCert( const QByteArray &stuff )
{
    certData = stuff;
}

quint64 Wad::Tid()
{
    if( !tmdData.size() )
    {
        Err( "There is no data in the TMD" );
        return 0;
    }
    Tmd t( tmdData );
    return t.Tid();
}

void Wad::SetGlobalCert( const QByteArray &stuff )
{
    globalCert = stuff;
}

const QByteArray Wad::getTmd()
{
	return tmdData;
}

const QByteArray Wad::getTik()
{
	return tikData;
}

const QByteArray Wad::GetCert()
{
	return certData.isEmpty() ? globalCert : certData;
}

const QByteArray Wad::Content( quint16 i )
{
    if( tmdData.isEmpty() || tikData.isEmpty() )
    {
        Err( "Can't decryte data without a TMD and ticket" );
        return QByteArray();
    }
    Ticket ticket( tikData );
    Tmd t( tmdData );
    if( partsEnc.size() != t.Count() || i >= partsEnc.size() )
    {
        Err( "I dont know whats going on some number is out of range and i dont like it" );
        return QByteArray();
    }
    QByteArray encData = partsEnc.at( i );

    AesSetKey( ticket.DecryptedKey() );

    QByteArray decData = AesDecrypt( i, encData );
    decData.resize( t.Size( i ) );
    QByteArray realHash = GetSha1( decData );
    if( realHash != t.Hash( i ) )
    {
        Err( QString( "hash doesnt match for content %1" ).arg( i ) );
        return QByteArray();
    }
    return decData;
}

quint32 Wad::content_count()
{
	return partsEnc.size();
}

void Wad::Err( const QString &str )
{
    ok = false;
    errStr = str;
    qWarning() << "Wad::Error" << str;
}

const QByteArray Wad::Data( quint32 magicWord, const QByteArray &footer )
{
	//qDebug() << "Wad::Data" << hex << magicWord << footer.size();
	if( !partsEnc.size() || tmdData.isEmpty() || tikData.isEmpty() || ( certData.isEmpty() && globalCert.isEmpty() ) )
	{
		Err( "Dont have all the parts to make a wad" );
		return QByteArray();
	}

	//do some brief checks to make sure that wad is good
	Ticket ticket( tikData );
	Tmd t( tmdData );
	if( t.Tid() != ticket.Tid() )
	{
		Err( "Ticket and TMD have different TID" );
		return QByteArray();
	}
	if( partsEnc.size() != t.Count() )
	{
		Err( "Dont have enough contents according to the TMD" );
		return QByteArray();
	}

	//everything seems in order, try to make the thing
	QByteArray cert = certData.isEmpty() ? globalCert : certData;
	quint32 certSize = cert.size();
	quint32 tikSize = ticket.SignedSize();
	quint32 tmdSize = t.SignedSize();
	quint32 appSize = 0;
	quint32 footerSize = footer.size();

	//add all the app sizes together and check that they match the TMD
	quint16 cnt = t.Count();
	for( quint16 i = 0; i < cnt; i++ )
	{
		quint32 s = RU( partsEnc.at( i ).size(), 0x40 );
		if( RU( t.Size( i ), 0x40 ) != s )
		{
			Err( QString( "Size of content %1 is bad ( %2, %3, %4 )" )
                 .arg( i )
                 .arg( t.Size( i ), 0, 16 )
				 .arg( RU( t.Size( i ), 0x40 ), 0, 16 )
                 .arg( s, 0, 16 ) );
			return QByteArray();
		}
		appSize += s;
	}

	QByteArray header( 0x20, '\0' );
	QBuffer buf( &header );
	buf.open( QIODevice::WriteOnly );

	quint32 tmp = qFromBigEndian( 0x20 );//header size
	buf.write( (const char*)&tmp, 4 );
	tmp = qFromBigEndian( magicWord );//magic word
	buf.write( (const char*)&tmp, 4 );
	tmp = qFromBigEndian( certSize );
	buf.write( (const char*)&tmp, 4 );
	buf.seek( 0x10 );
	tmp = qFromBigEndian( tikSize );
	buf.write( (const char*)&tmp, 4 );
	tmp = qFromBigEndian( tmdSize );
	buf.write( (const char*)&tmp, 4 );
	tmp = qFromBigEndian( appSize );
	buf.write( (const char*)&tmp, 4 );
	tmp = qFromBigEndian( footerSize );
	buf.write( (const char*)&tmp, 4 );
	buf.close();
	//hexdump( header, 0, 0x20 );

	QByteArray ret = PaddedByteArray( header, 0x40 ) + PaddedByteArray( cert, 0x40 );
	//make sure we dont have the huge ticket and TMD that come when downloading from NUS
	QByteArray tik = tikData;
	tik.resize( tikSize );
	tik = PaddedByteArray( tik, 0x40 );

	QByteArray tm = tmdData;
	tm.resize( tmdSize );
	tm = PaddedByteArray( tm, 0x40 );

	//hexdump( tik );
	//hexdump( tm );

	ret += tik + tm;
	for( quint16 i = 0; i < cnt; i++ )
	{
		ret += PaddedByteArray( partsEnc.at( i ), 0x40 );
	}
	ret += footer;
	return ret;
}

QString Wad::WadName( const QString &path )
{
    if( !tmdData.size() )
    {
        Err( "There is no data in the TMD" );
        return QString();
    }
    Tmd t( tmdData );
    return WadName( t.Tid(), t.Version(), path );
}

QString Wad::WadName( quint64 tid, quint16 version, const QString &path )
{
    quint32 type = (quint32)( ( tid >> 32 ) & 0xffffffff );
    quint32 base = (quint32)( tid & 0xffffffff );
    quint8 reg = (quint8)( tid & 0xff );
    quint32 regionFree = ( ( base >> 8 ) & 0xffffff ) << 8;
    QString region;
    switch( reg )
    {
    case 0x45:
        region = "US";
        break;
    case 0x50:
        region = "EU";
        break;
    case 0x4A:
        region = "JP";
        break;
    case 0x4B:
        region = "KO";//is this correct??  i have no korean games
        break;
    default:
        break;
    }
    QString name;
    switch( type )
    {
    case 1:
        switch( base )
        {
        case 1:
            name = QString( "BOOT2-v%1-64" ).arg( version );
            break;
        case 2:
            name = QString( "RVL-WiiSystemmenu-v%1" ).arg( version );
            break;
        case 0x100:
            name = QString( "RVL-bc-v%1" ).arg( version );
            break;
        case 0x101:
            name = QString( "RVL-mios-v%1" ).arg( version );
            break;
        default:
            if( base > 0xff )
                break;
            name = QString( "IOS%1-64-v%2" ).arg( base ).arg( version );
            break;
        }
        break;
    case 0x10002:
        switch( base )
        {
        case 0x48414141://HAAA
            name = QString( "RVL-photo-v%1" ).arg( version );
            break;
        case 0x48415941://HAYA
            name = QString( "RVL-photo2-v%1" ).arg( version );
            break;
        case 0x48414241://HABA
            name = QString( "RVL-Shopping-v%1" ).arg( version );
            break;
        case 0x48414341://HACA
            name = QString( "RVL-NigaoeNR-v%1" ).arg( version );//mii channel
            break;
        case 0x48414741://HAGA
            name = QString( "RVL-News-v%1" ).arg( version );
            break;
        case 0x48414641://HAFA
            name = QString( "RVL-Weather-v%1" ).arg( version );
            break;
        default:
            switch( regionFree )
            {
            case 0x48414600://HAF?
                name = QString( "RVL-Forecast_%1-v%2" ).arg( region ).arg( version );
                break;
            case 0x48414700://HAG?
                name = QString( "RVL-News_%1-v%2" ).arg( region ).arg( version );
                break;
            default:
                break;
            }
            break;
        }
        break;
    case 0x10008:
        switch( regionFree )
        {
        case 0x48414b00://HAK?
            name = QString( "RVL-Eulav_%1-v%2" ).arg( region ).arg( version );
            break;
        case 0x48414c00://HAL?
            name = QString( "RVL-Rgnsel_%1-v%2" ).arg( region ).arg( version );
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    if( name.isEmpty() )
        return QString();

    if( path.isEmpty() )
        return name + ".wad.out.wad";

    QString ret = name + ".wad.out.wad";
    int i = 1;
    while( QFile::exists( path + "/" + ret ) )
    {
        ret = name + QString( "(copy %1)" ).arg( i ) + ".wad.out.wad";
        i++;
    }
    return ret;
}

QByteArray Wad::FromDirectory( QDir dir )
{
    QFileInfoList tmds = dir.entryInfoList( QStringList() << "*.tmd" << "tmd.*", QDir::Files );
    if( tmds.isEmpty() )
    {
        qWarning() << "Wad::FromDirectory -> no tmd found in" << dir.absolutePath();
        return QByteArray();
    }
    QByteArray tmdD = ReadFile( tmds.at( 0 ).absoluteFilePath() );
    if( tmdD.isEmpty() )
        return QByteArray();
    QFileInfoList tiks = dir.entryInfoList( QStringList() << "*.tik" << "cetk", QDir::Files );
    if( tiks.isEmpty() )
    {
        qWarning() << "Wad::FromDirectory -> no tik found in" << dir.absolutePath();
        return QByteArray();
    }
    QByteArray tikD = ReadFile( tiks.at( 0 ).absoluteFilePath() );
    if( tikD.isEmpty() )
        return QByteArray();

    Tmd t( tmdD );
    Ticket ticket( tikD );

    //make sure to only add the tmd & ticket without all the cert mumbo jumbo
    QByteArray tmdP = tmdD;
    tmdP.resize( t.SignedSize() );
    QByteArray tikP = tikD;
    tikP.resize( ticket.SignedSize() );

    QList<QByteArray> datas = QList<QByteArray>()<< tmdP << tikP;

    quint16 cnt = t.Count();

    bool tmdChanged = false;
    for( quint16 i = 0; i < cnt; i++ )
    {
        QByteArray appD = ReadFile( dir.absoluteFilePath( t.Cid( i ) + ".app" ) );
        if( appD.isEmpty() )
            return QByteArray();

        if( (quint32)appD.size() != t.Size( i ) )
        {
            t.SetSize( i, appD.size() );
            tmdChanged = true;
        }
        QByteArray realHash = GetSha1( appD );
        if( t.Hash( i ) != realHash )
        {
            t.SetHash( i, realHash );
            tmdChanged = true;
        }
        datas << appD;
    }
    //if something in the tmd changed, fakesign it and replace the data in our list with the new data
    if( tmdChanged )
    {
        if( !t.FakeSign() )
        {
            qWarning() << "Error signing the wad";
        }
        else
        {
            datas.replace( 0, t.Data() );
        }
    }
    Wad wad( datas, false );
    if( !wad.IsOk() )
        return QByteArray();

    QFileInfoList certs = dir.entryInfoList( QStringList() << "*.cert", QDir::Files );
    if( !certs.isEmpty() )
    {
        QByteArray certD = ReadFile( certs.at( 0 ).absoluteFilePath() );
        if( !certD.isEmpty() )
            wad.SetCert( certD );
    }

    QByteArray ret = wad.Data();
    return ret;
}

bool Wad::SetTid( quint64 tid, bool fakeSign )
{
	if( !tmdData.size() || !tikData.size() )
	{
		Err( "Mising parts of the wad" );
		return false;
	}
	Tmd t( tmdData );
	Ticket ti( tikData );

	t.SetTid( tid );
	ti.SetTid( tid );

	if( fakeSign && !t.FakeSign() )
	{
		Err( "Error signing TMD" );
		return false;
	}
	if( fakeSign && !ti.FakeSign() )
	{
		Err( "Error signing ticket" );
		return false;
	}
	tmdData = t.Data();
	tikData = ti.Data();
	return true;
}

bool Wad::SetIOS( quint32 ios, bool fakeSign )
{
	if( !tmdData.size() || !tikData.size() )
	{
		Err( "Mising parts of the wad" );
		return false;
	}
	Tmd t( tmdData );

	t.SetIOS( ios );

	if( fakeSign && !t.FakeSign() )
	{
		Err( "Error signing TMD" );
		return false;
	}
	tmdData = t.Data();
	return true;
}

bool Wad::SetVersion( quint16 ver, bool fakeSign )
{
	if( !tmdData.size() || !tikData.size() )
	{
		Err( "Mising parts of the wad" );
		return false;
	}
	Tmd t( tmdData );

	t.SetVersion( ver );

	if( fakeSign && !t.FakeSign() )
	{
		Err( "Error signing TMD" );
		return false;
	}
	tmdData = t.Data();
	return true;
}

bool Wad::SetAhb( bool remove, bool fakeSign )
{
	if( !tmdData.size() || !tikData.size() )
	{
		Err( "Mising parts of the wad" );
		return false;
	}
	Tmd t( tmdData );

	t.SetAhb( remove );

	if( fakeSign && !t.FakeSign() )
	{
		Err( "Error signing TMD" );
		return false;
	}
	tmdData = t.Data();
	return true;
}

bool Wad::SetDiskAccess( bool allow, bool fakeSign )
{
	if( !tmdData.size() || !tikData.size() )
	{
		Err( "Mising parts of the wad" );
		return false;
	}
	Tmd t( tmdData );

	t.SetDiskAccess( allow );

	if( fakeSign && !t.FakeSign() )
	{
		Err( "Error signing TMD" );
		return false;
	}
	tmdData = t.Data();
	return true;
}

bool Wad::FakeSign( bool signTmd, bool signTicket )
{
	if( !tmdData.size() || !tikData.size() )
	{
		Err( "Mising parts of the wad" );
		return false;
	}

	if( signTicket )
	{
		Ticket ti( tikData );
		if( !ti.FakeSign() )
		{
			Err( "Error signing ticket" );
			return false;
		}
		tikData = ti.Data();
	}
	if( signTmd )
	{
		Tmd t( tmdData );
		if( !t.FakeSign() )
		{
			Err( "Error signing TMD" );
			return false;
		}
		tmdData = t.Data();
	}
	return true;
}

bool Wad::ReplaceContent( quint16 idx, const QByteArray &ba )
{
	if( idx >= partsEnc.size() || !tmdData.size() || !tikData.size() )
	{
		Err( "Mising parts of the wad" );
		return false;
	}
	QByteArray hash = GetSha1( ba );
	quint32 size = ba.size();

	Tmd t( tmdData );
	if( !t.SetHash( idx, hash ) )
	{
		Err( "Error setting content hash" );
		return false;
	}else{
		qDebug() << "Hash :" << hash.toHex();
	}
	if( !t.SetSize( idx, size ) )
	{
		Err( "Error setting content size" );
		return false;
	}
	if( !t.FakeSign() )
	{
		Err( "Error signing the tmd" );
		return false;
	}
	tmdData = t.Data();

	Ticket ti( tikData );
	AesSetKey( ti.DecryptedKey() );
	QByteArray decDataPadded = PaddedByteArray( ba, 0x40 );

	QByteArray encData = AesEncrypt( idx, decDataPadded );
	partsEnc.replace( idx, encData );

	return true;
}

QByteArray FromPartList( const QList< QByteArray > &stuff, bool isEncrypted = true )
{
    Wad wad( stuff, isEncrypted );
    if( !wad.IsOk() )
        return QByteArray();
    return wad.Data();
}
