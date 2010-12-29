#include "texture.h"

#define align(x, y) ((x) + ((x) % (y)))
#define round_up(x,n) (-(-(x) & -(n)))

int TPL_ConvertRGB565ToBitMap( quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height )
{
	quint32 x, y;
	quint32 x1, y1;
	quint32 iv;
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
					//quint8 b = ((oldpixel >> 11) & 0x1F) << 3;
					//quint8 g = ((oldpixel >> 5)  & 0x3F) << 2;
					//quint8 r = ((oldpixel >> 0)  & 0x1F) << 3;
					quint8 b = ((oldpixel >> 11) & 0x1F) *( 255/0x1f );
					quint8 g = ((oldpixel >> 5)  & 0x3F) *( 255/0x3f );
					quint8 r = ((oldpixel >> 0)  & 0x1F) *( 255/0x1f );
					quint8 a = 255;
					quint32 rgba = (r << 0) | (g << 8) | (b << 16) | (a << 24);
					(*(quint32**)bitmapdata)[x + (y * width)] = rgba;
				}
			}
		}
	}
	return outsz;
}
int TPL_ConvertRGBA8ToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height)
{
	//qDebug() << "TPL_ConvertRGBA8ToBitMap";
	quint32 x, y;
	quint32 x1, y1;
	*bitmapdata = (quint8*)calloc(width * height, 4);

	if(*bitmapdata == NULL)
		return -1;
	quint32 outsz = width * height * 4;
	quint8 r, g, b, a;
	int i, iv = 0;
	for(y = 0; y < height; y += 4) {
		for(x = 0; x < width; x += 4) {
			for(i = 0; i < 2; i++) {
				for(y1 = y; y1 < (y + 4); y1++) {
					for(x1 = x; x1 < (x + 4); x1++) {
						quint16 oldpixel = qFromBigEndian( *(quint16*)(tplbuf + ((iv++) * 2)) );
						if((x1 >= width) || (y1 >= height))
							continue;

						if(i == 0) {
							a = (oldpixel >> 8) & 0xFF;
							r = (oldpixel >> 0) & 0xFF;
							(*(quint32**)bitmapdata)[x1 + (y1 * width)] |= (quint32)((r << 16) | (a << 24));
						} else {
							g = (oldpixel >> 8) & 0xFF;
							b = (oldpixel >> 0) & 0xFF;
							(*(quint32**)bitmapdata)[x1 + (y1 * width)] |= (quint32)((g << 8) | (b << 0));
						}
					}
				}
			}
		}
	}
	return outsz;
}
int TPL_ConvertRGB5A3ToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height)
{
	quint32 x, y;
	quint32 x1, y1;
	quint32 iv;
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
int TPL_ConvertI4ToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height)
{
	quint32 x, y;
	quint32 x1, y1;
	quint32 iv;
	*bitmapdata = (quint8*)calloc(width * height, 8);
	if(*bitmapdata == NULL)
		return -1;
	quint32 outsz = align(width, 4) * align(height, 4) * 4;
	for(iv = 0, y1 = 0; y1 < height; y1 += 8) {
		for(x1 = 0; x1 < width; x1 += 8) {
			for(y = y1; y < (y1 + 8); y++) {
				for(x = x1; x < (x1 + 8); x += 2, iv++) {
					if((x >= width) || (y >= height))
						continue;
					quint8 oldpixel = tplbuf[ iv ];
					quint8 b = (oldpixel >> 4) * 255 / 15;
					quint8 g = (oldpixel >> 4) * 255 / 15;
					quint8 r = (oldpixel >> 4) * 255 / 15;
					quint8 a = 255;
					quint32 rgba = (r << 0) | (g << 8) | (b << 16) | (a << 24);
					(*(quint32**)bitmapdata)[x + (y * width)] = rgba;
					b = (oldpixel & 0xF) * 255 / 15;
					g = (oldpixel & 0xF) * 255 / 15;
					r = (oldpixel & 0xF) * 255 / 15;
					a = 255;
					rgba = (r << 0) | (g << 8) | (b << 16) | (a << 24);
					(*(quint32**)bitmapdata)[x + 1 + (y * width)] = rgba;
				}
			}
		}
	}
	return outsz;
}
int TPL_ConvertIA4ToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height)
{
	quint32 x, y;
	quint32 x1, y1;
	quint32 iv;
	*bitmapdata = (quint8*)calloc(width * height, 4);
	if(*bitmapdata == NULL)
		return -1;
	quint32 outsz = width * height * 4;
	for(iv = 0, y1 = 0; y1 < height; y1 += 4) {
		for(x1 = 0; x1 < width; x1 += 8) {
			for(y = y1; y < (y1 + 4); y++) {
				for(x = x1; x < (x1 + 8); x++) {
					quint8 oldpixel = *(quint8*)(tplbuf + ((iv++)));
					if((x >= width) || (y >= height))
						continue;
					quint8 b = ((oldpixel & 0xF) * 255) / 15;
					quint8 g = ((oldpixel & 0xF) * 255) / 15;
					quint8 r = ((oldpixel & 0xF) * 255) / 15;
					quint8 a = ((oldpixel >> 4)  * 255) / 15;
					quint32 rgba = (r << 0) | (g << 8) | (b << 16) | (a << 24);
					(*(quint32**)bitmapdata)[x + (y * width)] = rgba;
				}
			}
		}
	}

	return outsz;
}
int TPL_ConvertI8ToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height)
{
	quint32 x, y;
	quint32 x1, y1;
	quint32 iv;
	*bitmapdata = (quint8*)calloc(width * height, 4);
	if(*bitmapdata == NULL)
		return -1;
	quint32 outsz = width * height * 4;
	for(iv = 0, y1 = 0; y1 < height; y1 += 4) {
		for(x1 = 0; x1 < width; x1 += 8) {
			for(y = y1; y < (y1 + 4); y++) {
				for(x = x1; x < (x1 + 8); x++) {
					quint8 oldpixel = *(quint8*)(tplbuf + ((iv++) * 1));
					if((x >= width) || (y >= height))
						continue;
					quint8 b = oldpixel;
					quint8 g = oldpixel;
					quint8 r = oldpixel;
					quint8 a = 255;
					quint32 rgba = (r << 0) | (g << 8) | (b << 16) | (a << 24);
					(*(quint32**)bitmapdata)[x + (y * width)] = rgba;
				}
			}
		}
	}
	return outsz;
}
int TPL_ConvertCI8ToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height, const QByteArray &palData )
{
	//qDebug() << "TPL_ConvertCI8ToBitMap";
	//hexdump( palData );
	//hexdump( (const void*)tplbuf, 0x100 );
	quint32* output = (quint32*)calloc( width * height, 4 );
	if( *bitmapdata == NULL )
	return -1;
	if( ( palData.size() / 4 ) > 256 )
	qWarning() << "TPL_ConvertCI8ToBitMap -> quint8 is too small to hold enough data for this pallatte";

	quint32 outsz = width * height * 4;
	quint32* palette = (quint32*)palData.constData();
	int i = 0;

	for( quint32 y = 0; y < height; y += 4 )
	{
	for (quint32 x = 0; x < width; x += 8)
	{
		for (quint32 y1 = y; y1 < y + 4; y1++)
		{
		for (quint32 x1 = x; x1 < x + 8; x1++)
		{
			quint8 pixel = tplbuf[ i++ ];

			if( y1 >= height || x1 >= width )

			continue;

			output[ y1 * width + x1 ] = palette[ pixel ];
		}
		}
	}
	}
	*bitmapdata = (quint8*)output;
	return outsz;
}
int TPL_ConvertCI4ToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height, const QByteArray &palData )
{
	//qDebug() << "TPL_ConvertCI4ToBitMap";
	quint32* output = (quint32*)calloc(width * height, 4);
	if( output == NULL )
	return -1;
	if( ( palData.size() / 4 ) > 256 )
	qWarning() << "TPL_ConvertCI4ToBitMap -> quint8 is too small to hold enough data for this pallatte";

	quint32 max = width * height;
	quint32 outsz = width * height;

	quint32* palette = (quint32*)palData.constData();
	int i = 0;
	for( quint32 y = 0; y < height; y += 8 )
	{
	for( quint32 x = 0; x < width; x += 8)
	{
		for( quint32 y1 = y; y1 < y + 8; y1++)
		{
		for( quint32 x1 = x; x1 < x + 8; x1 += 2)
		{
			quint8 pixel = tplbuf[ i++ ];

			if( y1 >= height || x1 >= width )
			continue;

			output[ y1 * width + x1 ] = palette[ pixel >> 4 ];
			if( y1 * width + x1 + 1 < max )
			output[ y1 * width + x1 + 1 ] = palette[ pixel & 0x0F ];
		}
		}
	}
	}
	*bitmapdata = (quint8*)output;

	return outsz;
}
int TPL_ConvertCI14x2ToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height, const QByteArray &palData )
{
	//qDebug() << "TPL_ConvertCI14x2ToBitMap";
	//hexdump( palData );
	//hexdump( (const void*)tplbuf, 0x100 );
	quint32* output = (quint32*)calloc(width * height, 4);
	if( output == NULL )
	return -1;

	quint32 outsz = width * height;

	quint32* palette = (quint32*)palData.constData();
	quint16* tplStuff = (quint16*)tplbuf;
	int i = 0;

	for( quint32 y = 0; y < height; y += 4 )
	{
	for( quint32 x = 0; x < width; x += 4)
	{
		for( quint32 y1 = y; y1 < y + 4; y1++)
		{
		for( quint32 x1 = x; x1 < x + 4; x1++)
		{
			quint16 pixel = qFromBigEndian( tplStuff[ i++ ] );

			if( y1 >= height || x1 >= width )
			continue;

			output[ y1 * width + x1 ] = palette[ pixel & 0x3FFF ];
		}
		}
	}
	}
	*bitmapdata = (quint8*)output;
	return outsz;
}
int TPL_ConvertIA8ToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height)
{
	quint32 x, y;
	quint32 x1, y1;
	quint32 iv;
	*bitmapdata = ( quint8* )calloc( width * height, 4 );
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
					quint8 b = oldpixel >> 8;
					quint8 g = oldpixel >> 8;
					quint8 r = oldpixel >> 8;
					quint8 a = oldpixel & 0xFF;
					quint32 rgba = (r << 0) | (g << 8) | (b << 16) | (a << 24);
					(*(quint32**)bitmapdata)[x + (y * width)] = rgba;
				}
			}
		}
	}
	return outsz;
}
static quint16 avg(quint16 w0, quint16 w1, quint16 c0, quint16 c1)
{
	quint16 a0, a1;
	quint16 a, c;

	a0 = c0 >> 11;
	a1 = c1 >> 11;
	a = (w0*a0 + w1*a1) / (w0 + w1);
	c = a << 11;

	a0 = (c0 >> 5) & 63;
	a1 = (c1 >> 5) & 63;
	a = (w0*a0 + w1*a1) / (w0 + w1);
	c |= a << 5;

	a0 = c0 & 31;
	a1 = c1 & 31;
	a = (w0*a0 + w1*a1) / (w0 + w1);
	c |= a;

	return c;
}
int TPL_ConvertCMPToBitMap(quint8* tplbuf, quint8** bitmapdata, quint32 width, quint32 height)
{
	quint32 x, y;
	quint32 iv = 0;
	*bitmapdata = ( quint8* )calloc( width * height, 4 );

	if(*bitmapdata == NULL)
		return -1;
	quint32 outsz = width * height * 4;
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++) {
			quint8 pix[4];
			quint16 raw;
			quint16 c[4];
			int x0, x1, x2, y0, y1, y2, off;
			int ww = round_up(width, 8);
			int ix;
			quint32 px;

			x0 = x & 3;
			x1 = (x >> 2) & 1;
			x2 = x >> 3;
			y0 = y & 3;
			y1 = (y >> 2) & 1;
			y2 = y >> 3;
			off = (8 * x1) + (16 * y1) + (32 * x2) + (4 * ww * y2);

			c[0] = qFromBigEndian(*(quint16*)(tplbuf + off));
			c[1] = qFromBigEndian(*(quint16*)(tplbuf + off + 2));
			if (c[0] > c[1]) {
				c[2] = avg(2, 1, c[0], c[1]);
				c[3] = avg(1, 2, c[0], c[1]);
			} else {
				c[2] = avg(1, 1, c[0], c[1]);
				c[3] = 0;
			}

			px = qFromBigEndian(*(quint32*)(tplbuf + off + 4));
			ix = x0 + (4 * y0);
			raw = c[(px >> (30 - (2 * ix))) & 3];

			pix[0] = (raw >> 8) & 0xf8;
			pix[1] = (raw >> 3) & 0xf8;
			pix[2] = (raw << 3) & 0xf8;
			pix[3] = 0xff;
			if (((px >> (30 - (2 * ix))) & 0x03) == 3 && c[0] <= c[1])
				pix[3] = 0x00;

			quint32 rgba = (pix[0] << 16) | (pix[1] << 8) | (pix[2] << 0) | (pix[3] << 24);
			(*(quint32**)bitmapdata)[iv++] = rgba;
		}
	return outsz;
}




QImage ConvertTextureToImage( const QByteArray &ba, quint32 w, quint32 h, quint32 format, const QByteArray &palData )
{
	quint8* bitmapdata = NULL;//this will hold the converted image
	int ret = 0;
	switch( format )
	{
	case TPL_FORMAT_RGB565:
		ret = TPL_ConvertRGB565ToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h );
		break;
	case TPL_FORMAT_RGB5A3:
		ret = TPL_ConvertRGB5A3ToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h );
		break;
	case TPL_FORMAT_RGBA8:
		ret = TPL_ConvertRGBA8ToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h );
		break;
	case TPL_FORMAT_I4:
		ret = TPL_ConvertI4ToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h );
		break;
	case TPL_FORMAT_I8:
		ret = TPL_ConvertI8ToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h );
		break;
	case TPL_FORMAT_IA4:
		ret = TPL_ConvertIA4ToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h );
		break;
	case TPL_FORMAT_IA8:
		ret = TPL_ConvertIA8ToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h );
		break;
	case TPL_FORMAT_CI8:
		ret = TPL_ConvertCI8ToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h, palData );
		break;
	case TPL_FORMAT_CI4:
		ret = TPL_ConvertCI4ToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h, palData );
		break;
	case TPL_FORMAT_CI14X2:
		ret = TPL_ConvertCI14x2ToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h, palData );
		break;
	case TPL_FORMAT_CMP:
		ret = TPL_ConvertCMPToBitMap( (quint8*)ba.constData(), &bitmapdata, w, h );
		break;
	default:
		qWarning() << "ConvertTextureToImage -> Unsupported format" << hex << format;
		return QImage();
		break;
	}
	if( !ret )//should only happen if calloc fails
	{
		qWarning() << "ConvertTextureToImage-> error converting image";
		return QImage();
	}
	QImage im = QImage( (const uchar*)bitmapdata, w, h, QImage::Format_ARGB32 );
	QImage im2 = im.copy( im.rect() );//make a copy of the image so the "free" wont delete any data we still want
	free( bitmapdata );
	return im2;
}
