#ifndef KEYSBIN_H
#define KEYSBIN_H

#include "includes.h"

//quick class for grabbing some stuff from a keys.bin from bootmii ( should be 0x400 bytes )
// TODO - this isnt complete at all
class KeysBin
{
public:
    KeysBin( QByteArray stuff = QByteArray() );

    const QByteArray NG_ID();
    const QByteArray NG_key_ID();
    const QByteArray NG_Sig();
    const QByteArray NG_Priv();
    const QByteArray NandKey();
    const QByteArray HMac();

private:
    QByteArray data;
};

#endif // KEYSBIN_H
