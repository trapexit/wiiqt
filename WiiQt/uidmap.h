#ifndef UIDMAP_H
#define UIDMAP_H

#include "includes.h"

//class for handling the uid.sys in a virtual nand
//
class UIDmap
{
public:
    UIDmap( const QByteArray &old = QByteArray() );
    ~UIDmap();

    //makes sure there are the default entries in the map and that the entries are sane
    bool Check();

    //returns the uid for the given tid
    //returns 0 if it is not found. or if autocreate is true, it will be created and the new uid returned
    //numbers are in host endian
    quint32 GetUid( quint64 tid, bool autoCreate = true );

    //creates a new uid.sys with the system menu entry.
	//if addFactorySetupDiscs is anything other than 0, it will add some entries for the setup discs used in the wii factory
	// addFactorySetupDiscs should be the region code:  0x45=E, 0x50=P...
    // ( serve no purpose other than to just exist )
	void CreateNew( quint8 addFactorySetupDiscs = 0 );

    //get th entire uid.sys data back in a state ready for writing to a nand
    const QByteArray Data(){ return data; }

private:
    QByteArray data;
};

#endif // UIDMAP_H
