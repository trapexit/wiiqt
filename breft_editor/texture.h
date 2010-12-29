#ifndef TEXTURE_H
#define TEXTURE_H

#include "../WiiQt/includes.h"

#define TPL_FORMAT_I4		0
#define TPL_FORMAT_I8		1

#define TPL_FORMAT_IA4		2
#define TPL_FORMAT_IA8		3

#define TPL_FORMAT_RGB565	4
#define TPL_FORMAT_RGB5A3	5
#define TPL_FORMAT_RGBA8	6

#define TPL_FORMAT_CI4		8
#define TPL_FORMAT_CI8		9
#define TPL_FORMAT_CI14X2	10

#define TPL_FORMAT_CMP		14

#define TPL_MIN_FILTER_NORMAL	0x00010000
#define TPL_MIN_FILTER_CLAMP	0x00000001
#define TPL_MAG_FILTER_NORMAL	0x00010000
#define TPL_MAG_FILTER_CLAMP	0x00000001

QImage ConvertTextureToImage( const QByteArray &ba, quint32 w, quint32 h, quint32 format, const QByteArray &palData = QByteArray() );

#endif // TEXTURE_H
