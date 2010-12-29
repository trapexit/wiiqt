#ifndef BREFT_H
#define BREFT_H

#include <QtCore>

union Magic {
	quint32 value;
	char string[4];
};

struct header {
	Magic magic;
	quint16 endian;
	quint8 version_hi;
	quint8 version_lo;
	quint32 length;
	quint16 header_size;
	quint16 chunk_cnt;
};

struct reft {
	Magic magic;
	quint32 length;
};

struct projct {
	quint32 length;
	quint32 unk1;
	quint32 unk2;
	quint16 str_length;
	quint16 unk3;
};

struct section1 {
	quint32 length;
	quint16 count;
	quint16 padding;
};

struct section1b {
	quint32 offset;
	quint32 length;
};

struct picture_header {
	quint32 unk1;	//00
	quint16 height;	//04
	quint16 width;	//06
	quint32 size;	//08
	quint8 format;	//0c
	quint8 unk2;
	quint8 unk3;
	quint8 unk4;
	quint32 unk5;	//10
	quint8 unk6;	//14
	quint8 rptx;	//15
	quint8 rpty;	//16
	quint8 unk7;	//17
	quint32 unk8;	//18
	quint32 unk9;	//1c
	quint32 unkA;	//20
	quint32 unkB;	//24
	quint32 unkC;	//28
	quint32 unkD;	//30
	quint32 unkE;	//34
	quint32 unkF;	//38
	quint32 unkG;	//3c
};

struct tpl_header {
	Magic magic;
	quint32 count;
	quint32 header_size;
	quint32 offset;
};

struct tpl_header2 {
	quint16 height;
	quint16 width;
	quint32 format;
	quint32 offset;
	quint32 wrap_s;
	quint32 wrap_t;
	quint32 min;
	quint32 mag;
	float lod_bias;
	quint8 edge_lod;
	quint8 min_lod;
	quint8 max_lod;
	quint8 unpacked;
};

QDataStream &operator<<( QDataStream &stream, const header &data);
QDataStream &operator>>( QDataStream &stream, header &data);
QDataStream &operator<<( QDataStream &stream, const reft &data);
QDataStream &operator>>( QDataStream &stream, reft &data);
QDataStream &operator<<( QDataStream &stream, const projct &data);
QDataStream &operator>>( QDataStream &stream, projct &data);
QDataStream &operator<<( QDataStream &stream, const section1 &data);
QDataStream &operator>>( QDataStream &stream, section1 &data);
QDataStream &operator<<( QDataStream &stream, const section1b &data);
QDataStream &operator>>( QDataStream &stream, section1b &data);
QDataStream &operator<<( QDataStream &stream, const picture_header &data);
QDataStream &operator>>( QDataStream &stream, picture_header &data);

#endif	/*BREFT_H*/
