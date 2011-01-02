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
    quint32 tmp;
    f.read( (char*)&tmp, 4 );
    quint32 tmp2;
    f.read( (char*)&tmp2, 4 );
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

/*
"        0     20000" "Data East Arcade Classics       " "icons: 4" "banner size: a8a0"
"       10     20000" "Paintball 2009                  " "icons: 1" "banner size: 72a0"
"        0  55570000" "Let's TAP                       " "icons: 8" "banner size: f0a0" //too slow
"        0  57ff0000" "Metroid: Other M                " "icons: 8" "banner size: f0a0" //too fast
"        0  2aaa0000" "Championship foosball           " "icons: 7" "banner size: dea0" //little too slow
"        1  aaaa0000" "Blue World                      " "icons: 8" "banner size: f0a0" //perfect?
"        0  55520000" "零－月蝕の仮面－                        " "icons: 1" "banner size: 72a0"
"        0    2e0000" "Punch-Out!!                     " "icons: 4" "banner size: a8a0" //just a little bit too slow
"        0  15550000" "Balls of Fury                   " "icons: 7" "banner size: dea0"  //mine plays to slow
"        0  aaaa0000" "The Umbrella Chronicles         " "icons: 8" "banner size: f0a0"
"        0     20000" "DU SUPER MARIO BROS. Wii        " "icons: 1" "banner size: 72a0"
"        0  55550000" "Excite Truck                    " "icons: 8" "banner size: f0a0" //plays too slow
"        0   2aa0000" "Guitar Hero III                 " "icons: 5" "banner size: baa0" //to slow
"        0   2aa0000" "Guitar Hero III                 " "icons: 5" "banner size: baa0"
"        0    aa0000" "HOD 2&3 RETURN                  " "icons: 4" "banner size: a8a0" //static
"       10         0" "The House of the Dead           " "icons: 1" "banner size: 72a0"
"        1  aaaa0000" "Quantum of Solace™              " "icons: 8" "banner size: f0a0" //close to original speed
"       10         0" "Trauma Center: Second Opinion   " "icons: 1" "banner size: 72a0"
"        0         0" "Indiana Pwns, by Team Twiizers  " "icons: 1" "banner size: 72a0"
"       10  55550000" "Alien Syndrome                  " "icons: 8" "banner size: f0a0" //should be playing forwards and then reverse 1 or 2 frames and start over?
"       10         0" "Prince of Persia                " "icons: 1" "banner size: 72a0"
"        0  ffff0000" "Mad Dog McCree                  " "icons: 8" "banner size: f0a0" //plays too fast
"       10     30000" "Rubik's World                   " "icons: 1" "banner size: 72a0"
"        1   2000000" "Smash Bros. Brawl               " "icons: 1" "banner size: 72a0"
"        1   2000000" "大乱闘スマッシュブラザーズＸ                  " "icons: 1" "banner size: 72a0"
"        1   2000000" "Smash Bros. Brawl               " "icons: 1" "banner size: 72a0"
"        0     20000" "Wii Sports                      " "icons: 1" "banner size: 72a0"
"       10  aaaa0000" "Downhill Jam                    " "icons: 8" "banner size: f0a0"//correctish
"        1  ffff0000" "Call of Duty: WaW               " "icons: 8" "banner size: f0a0" //plays too fast
"       10    ff0000" "Tony Hawk: RIDE                 " "icons: 4" "banner size: a8a0" //appears static
"        0     20000" "Link's Training                 " "icons: 1" "banner size: 72a0"
"        0     20000" "Wii Sports Resort               " "icons: 1" "banner size: 72a0"
"        0    ff0000" "Iron Man™ 2                     " "icons: 4" "banner size: a8a0"//too slow
"       10  55550000" "Attack of the Movies 3-D        " "icons: 8" "banner size: f0a0" //appears static
"        0     30000" "SUPER MARIO GALAXY 2            " "icons: 1" "banner size: 72a0"
"        1  ffff0000" "Call of Duty: Black Ops         " "icons: 8" "banner size: f0a0" //appears static
"        0  aaaa0000" "DKC Returns                     " "icons: 8" "banner size: f0a0"//correct
"        0  aaaa0000" "Wheel of Fortune                " "icons: 8" "banner size: f0a0"//correct
"        0         0" "Scooby-Doo!                     " "icons: 1" "banner size: 72a0"
"       11  aaaa0000" "GoldenEye 007                   " "icons: 8" "banner size: f0a0"//correctish
"        0  aaaa0000" "Jeopardy!                       " "icons: 8" "banner size: f0a0"//correctish
"       10         0" "All Star Karate                 " "icons: 1" "banner size: 72a0"
"        0     20000" "NewSuperMarioBrosWii            " "icons: 1" "banner size: 72a0"
"        0     20000" "New SUPER MARIO BROS. Wii       " "icons: 1" "banner size: 72a0"
"       10     20000" "Cabela's NAA                    " "icons: 1" "banner size: 72a0"
"       10         0" "NBA JAM                         " "icons: 1" "banner size: 72a0"
"        0     30000" "Spider-Man™: SD                 " "icons: 1" "banner size: 72a0"
"        1  ffff0000" "Tetris® Party Deluxe            " "icons: 8" "banner size: f0a0"//plays too fast
"        0  2aaa0000" "GH World Tour                   " "icons: 7" "banner size: dea0"
"        0   aaa0000" "GH Metallica                    " "icons: 6" "banner size: cca0"
"        0   aaa0000" "GH Smash Hits                   " "icons: 6" "banner size: cca0"
"        0  2aaa0000" "GH™:Warriors of Rock            " "icons: 7" "banner size: dea0"
"        1   3ff0000" "Rock Band™ 2                    " "icons: 5" "banner size: baa0"
"        1   3ff0000" "Rock Band™ 3                    " "icons: 5" "banner size: baa0"
"        1     20000" "Check Mii Out Ch.               " "icons: 1" "banner size: 72a0"
"       10  ffff0000" "Super Smash Bros.               " "icons: 8" "banner size: f0a0" //plays forward, pause, backwards, pause
"        0     20000" "Wii Fit                         " "icons: 1" "banner size: 72a0"
"        0     20000" "Wii Fit™ Plus                   " "icons: 1" "banner size: 72a0"
skipping empty image
skipping empty image
skipping empty image
skipping empty image
skipping empty image
skipping empty image
skipping empty image
"        1     20000" "Rabbids Go Home                 " "icons: 8" "banner size: f0a0"
"        1     20000" "Mario Kart Wii                  " "icons: 1" "banner size: 72a0"
"        1     20000" "Mario Kart Wii                  " "icons: 1" "banner size: 72a0"


0x20000 = static even if theres multiple images ( data east arcade )
*/

