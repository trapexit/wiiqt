#ifndef ASH_H
#define ASH_H

#include "includes.h"

//returns a byteArray of decompressed ASH data
//returns an empty array if the data doesnt have the ASH magic word
QByteArray DecryptAsh( const QByteArray ba );

//check if data starts with "ASH".  no other check is done
bool IsAshCompressed( const QByteArray ba );

#endif // ASH_H
