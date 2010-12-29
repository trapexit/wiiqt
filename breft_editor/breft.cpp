#include "breft.h"

QDataStream
&operator<<( QDataStream &stream, const header &data)
{
	stream << data.magic.value << data.endian;
	stream << data.version_hi << data.version_lo;
	stream << data.length << data.header_size << data.chunk_cnt;
	return stream;
}
QDataStream
&operator>>( QDataStream &stream, header &data)
{
	stream >> data.magic.value >> data.endian;
	stream >> data.version_hi >> data.version_lo;
	stream >> data.length >> data.header_size >> data.chunk_cnt;
	return stream;
}

QDataStream
&operator<<( QDataStream &stream, const reft &data)
{
	stream << data.magic.value << data.length;
	return stream;
}
QDataStream
&operator>>( QDataStream &stream, reft &data)
{
	stream >> data.magic.value >> data.length;
	return stream;
}

QDataStream
&operator<<( QDataStream &stream, const projct &data)
{
	stream << data.length << data.unk1 << data.unk2;
	stream << data.str_length << data.unk3;
	return stream;
}
QDataStream
&operator>>( QDataStream &stream, projct &data)
{
	stream >> data.length >> data.unk1 >> data.unk2;
	stream >> data.str_length >> data.unk3;
	return stream;
}

QDataStream
&operator<<( QDataStream &stream, const section1 &data)
{
	stream << data.length << data.count << data.padding;
	return stream;
}
QDataStream
&operator>>( QDataStream &stream, section1 &data)
{
	stream >> data.length >> data.count >> data.padding;
	return stream;
}

QDataStream
&operator<<( QDataStream &stream, const section1b &data)
{
	stream << data.offset << data.length;
	return stream;
}
QDataStream
&operator>>( QDataStream &stream, section1b &data)
{
	stream >> data.offset >> data.length;
	return stream;
}

QDataStream
&operator<<( QDataStream &stream, const picture_header &data)
{
	stream << data.unk1 << data.height << data.width << data.size;
	stream << data.format << data.unk2 << data.unk3 << data.unk4;
	stream << data.unk5 << data.unk6 << data.rptx << data.rpty;
	stream << data.unk7 << data.unk8 << data.unk9 << data.unkA;
	stream << data.unkB << data.unkC << data.unkD << data.unkE;
	stream << data.unkF << data.unkG;
	return stream;
}
QDataStream
&operator>>( QDataStream &stream, picture_header &data)
{
	stream >> data.unk1 >> data.height >> data.width >> data.size;
	stream >> data.format >> data.unk2 >> data.unk3 >> data.unk4;
	stream >> data.unk5 >> data.unk6 >> data.rptx >> data.rpty;
	stream >> data.unk7 >> data.unk8 >> data.unk9 >> data.unkA;
	stream >> data.unkB >> data.unkC >> data.unkD >> data.unkE;
	stream >> data.unkF >> data.unkG;
	return stream;
}

