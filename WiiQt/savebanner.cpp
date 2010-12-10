#include "savebanner.h"
#include "tools.h"

static int ConvertRGB5A3ToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height);

SaveBanner::SaveBanner()
{
    ok = false;
}

SaveBanner::SaveBanner( QByteArray stuff )
{
    ok = false;
    QBuffer f( &stuff );
    f.open( QIODevice::ReadOnly );
    quint32 size = f.size();
    if( size < 0x72a0 || ( ( size - 0x60a0 ) % 0x1200 )  )//sanity check the size.  must have enough data for the header, names, banner, and 1 icon image
    {
	qDebug() << "SaveBanner::SaveBanner -> bad filesize" << hex << size;
	f.close();
	return;
    }
    quint32 magic;
    f.read( (char*)&magic, 4 );
    if( qFromBigEndian( magic ) != 0x5749424e )//WIBN
    {
	hexdump( stuff, 0, 0x30 );
	f.close();
	qWarning() << "SaveBanner::SaveBanner -> bad file magic" << hex << qFromBigEndian( magic );
	return;
    }
    //no clue what this stuff is, dont really need it though
    //i suspect instructions for animation ? ( VC icons display forwards and backwards in the system menu )
    //also speed is not always the same
    //quint32 tmp;
    //f.read( (char*)&tmp, 4 );
    //quint32 tmp2;
    //f.read( (char*)&tmp2, 4 );
    f.seek( 0x20 );

    quint16 name[ 0x20 ];
    quint16 name2[ 0x20 ];

    f.read( (char*)&name, 0x40 );
    f.read( (char*)&name2, 0x40 );

    //QString title;
    for( int i = 0; i < 0x20 && name[ i ] != 0; i++ )
	saveTitle += QChar( qFromBigEndian( name[ i ] ) );

    saveTitle = saveTitle.trimmed();

    //qDebug() << hex << qFromBigEndian( tmp ) << qFromBigEndian( tmp2 ) << saveTitle;

    //QString title2;
    for( int i = 0; i < 0x20 && name2[ i ] != 0; i++ )
	saveTitle2 += QChar( qFromBigEndian( name2[ i ] ) );

    saveTitle2 = saveTitle2.trimmed();

    //get the banner
    f.seek( 0xa0 );
    QByteArray ban = f.read( 0x6000 );

    //convert to an image we can display
    bannerImg = ConvertTextureToImage( ban, 0xc0, 0x40 );
    if( bannerImg.isNull() )
    {
	f.close();
	qWarning() << "SaveBanner::SaveBanner -> error converting banner image";
	return;
    }

    //get the images that make up the icon
    while( f.pos() != size )
    {
	QByteArray icn = f.read( 0x1200 );
	//check that there is actually data.  some banners use all 0x00 for some images
	bool null = true;
	for( int i = 0; i < 0x1200; i++ )
	{
	    if( icn.at( i ) )//this buffer contains at least 1 byte of data, try to turn it into an image
	    {
		null = false;
		break;
	    }
	}
	if( null )
	{
	    //qDebug() << "skipping empty image";
	    break;
	}

	//make this texture int an image
	QImage iconImg = ConvertTextureToImage( icn, 0x30, 0x30 );
	if( iconImg.isNull() )
	    break;

	//add the image to the list
	iconImgs << iconImg;
    }
    f.close();
    ok = true;

    //qDebug() << hex << QString( "%1 %2").arg( qFromBigEndian( tmp ), 9, 16).arg( qFromBigEndian( tmp2 ), 9, 16)
	    //<< saveTitle.leftJustified( 0x20 ) << QString( "icons: %1").arg( iconImgs.size(), 1, 16 ) << QString( "banner size: %1" ).arg( size, 4, 16 );

}

SaveBanner::SaveBanner( const QString &bannerPath )
{
    ok = false;
    QFile f( bannerPath );
    if( !f.exists() || !f.open( QIODevice::ReadOnly ) )
    {
	qWarning() << "SaveBanner::SaveBanner -> error opening" << bannerPath;
	return;
    }
    quint32 size = f.size();
    if( size < 0x72a0 || ( ( size - 0x60a0 ) % 0x1200 )  )//sanity check the size.  must have enough data for the header, names, banner, and 1 icon image
    {
	qDebug() << "SaveBanner::SaveBanner -> bad filesize" << hex << size;
	f.close();
	return;
    }
    quint32 magic;
    f.read( (char*)&magic, 4 );
    if( qFromBigEndian( magic ) != 0x5749424e )//WIBN
    {
	f.close();
	qWarning() << "SaveBanner::SaveBanner -> bad file magic" << qFromBigEndian( magic );
	return;
    }
    //get the title of the save
    f.seek( 0x20 );

    quint16 name[ 0x20 ];
    quint16 name2[ 0x20 ];

    f.read( (char*)&name, 0x40 );
    f.read( (char*)&name2, 0x40 );

    //QString title;
    for( int i = 0; i < 0x20 && name[ i ] != 0; i++ )
	saveTitle += QChar( qFromBigEndian( name[ i ] ) );

    saveTitle = saveTitle.trimmed();

    //QString title2;
    for( int i = 0; i < 0x20 && name2[ i ] != 0; i++ )
	saveTitle2 += QChar( qFromBigEndian( name2[ i ] ) );

    saveTitle2 = saveTitle2.trimmed();

    //get the banner
    f.seek( 0xa0 );
    QByteArray ban = f.read( 0x6000 );

    //convert to an image we can display
    bannerImg = ConvertTextureToImage( ban, 0xc0, 0x40 );
    if( bannerImg.isNull() )
    {
	f.close();
	qWarning() << "SaveBanner::SaveBanner -> error converting banner image";
	return;
    }

    //get the images that make up the icon
    while( f.pos() != size )
    {
	QByteArray icn = f.read( 0x1200 );
	//check that there is actually data.  some banners use all 0x00 for some images
	bool null = true;
	for( int i = 0; i < 0x1200; i++ )
	{
	    if( icn.at( i ) )//this buffer contains at least 1 byte of data, try to turn it into an image
	    {
		null = false;
		break;
	    }
	}
	if( null )
	{
	    //qDebug() << "skipping empty image";
	    break;
	}

	//make this texture int an image
	QImage iconImg = ConvertTextureToImage( icn, 0x30, 0x30 );
	if( iconImg.isNull() )
	    break;

	//add the image to the list
	iconImgs << iconImg;
    }
    f.close();
    ok = true;
}

QImage SaveBanner::ConvertTextureToImage( const QByteArray &ba, quint32 w, quint32 h )
{
    //qDebug() << "SaveBanner::ConvertTextureToImage" << ba.size() << hex << w << h;
    quint8* bitmapdata = NULL;//this will hold the converted image
    int ret = ConvertRGB5A3ToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h );
    if( !ret )
    {
	qWarning() << "SaveBanner::ConvertTextureToImage -> error converting image";
	return QImage();
    }
    QImage im = QImage( (const uchar*)bitmapdata, w, h, QImage::Format_ARGB32 );
    QImage im2 = im.copy( im.rect() );//make a copy of the image so the "free" wont delete any data we still want
    free( bitmapdata );
    return im2;
}

static int ConvertRGB5A3ToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height)
{
	quint32 x, y;
	quint32 x1, y1;
	quint32 iv;
	//tplpoint -= width;
	*bitmapdata = (quint8*)calloc(width * height, 4);
	if(*bitmapdata == NULL)
		return -1;
	quint32 outsz = width * height * 4;
	for(iv = 0, y1 = 0; y1 < height; y1 += 4) {
		for(x1 = 0; x1 < width; x1 += 4) {
			for(y = y1; y < (y1 + 4); y++) {
				for(x = x1; x < (x1 + 4); x++) {
					quint16 oldpixel = *(quint16*)(tplbuf + ((iv++) * 2));
					if((x >= width) || (y >= height))
						continue;
					oldpixel = qFromBigEndian(oldpixel);
					if(oldpixel & (1 << 15)) {
						// RGB5
						quint8 b = (((oldpixel >> 10) & 0x1F) * 255) / 31;
						quint8 g = (((oldpixel >> 5)  & 0x1F) * 255) / 31;
						quint8 r = (((oldpixel >> 0)  & 0x1F) * 255) / 31;
						quint8 a = 255;
						quint32 rgba = (r << 0) | (g << 8) | (b << 16) | (a << 24);
						(*(quint32**)bitmapdata)[x + (y * width)] = rgba;
					}else{
						// RGB4A3

					    quint8 a = (((oldpixel >> 12) & 0x7) * 255) / 7;
					    quint8 b = (((oldpixel >> 8)  & 0xF) * 255) / 15;
					    quint8 g = (((oldpixel >> 4)  & 0xF) * 255) / 15;
					    quint8 r = (((oldpixel >> 0)  & 0xF) * 255) / 15;
					    quint32 rgba = (r << 0) | (g << 8) | (b << 16) | (a << 24);
					    (*(quint32**)bitmapdata)[x + (y * width)] = rgba;
					}
				}
			}
		}
	}
	return outsz;
}
