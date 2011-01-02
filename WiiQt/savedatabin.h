#ifndef SAVEDATABIN_H
#define SAVEDATABIN_H

#include "includes.h"
#include "tools.h"

// class for dealing with a wii save (data.bin)

//! basic usage: read a data.bin into a bytearray and pass it to the first constructor.
//! if IsOk() returns true, then everything is good.  all the decrypted data and filenames
//! can be gotten from SaveStruct()

//! the static should take care of most of what you need to do - converting to/from/extracting a file
class SaveDataBin
{
public:
    SaveDataBin( QByteArray stuff = QByteArray() );
    SaveDataBin( const SaveGame &old );
    bool IsOk(){ return _ok; }

    const SaveGame SaveStruct(){ return sg; }

    //these only return something useful if the first constructor is used and was successful
    quint32 NgID();
    quint32 NgKeyID();
    const QByteArray NgSig();
    const QByteArray NgMac();

    //get a bytearray containing a encoded data.bin
    //! ngPriv must be supplied.  the rest of the items are gotten if the first constructor is used successfully
    //! if the first constructon was not used, then all these other items need to be supplied
    const QByteArray Data( const QByteArray &ngPriv,
                           const QByteArray &ngSig = QByteArray(), const QByteArray &ngMac = QByteArray(), quint32 ngId = 0, quint32 ngKeyId = 0 );

    //static functions for common stuff...
    //encode a data.bin from a save struct and the necessary nd_* stuff
    static const QByteArray DataBinFromSaveStruct( const SaveGame &old, const QByteArray &ngPriv,
                                       const QByteArray &ngSig, const QByteArray &ngMac, quint32 ngId, quint32 ngKeyId );

    //get a savegame struct from a bytearray containing a data.bin
    static const SaveGame StructFromDataBin( const QByteArray &dataBin );

    //get a file from a data.bin
    //! path should start with a "/".  this one is slow as it decrypts all data & checks all certs and whatnot ( probably will optimize it sometime later )
    //! return an empty bytearray on error
    static const QByteArray GetFile( const QByteArray &dataBin, const QString &path );

    //get a "banner.bin" from a save.  this one should be MUCH faster than the above one.  it does not check any hashes
    // or signatures.  it simply decrypts the first chunk of the data.bin, reads the banner size, and returns it
    static const QByteArray GetBanner( const QByteArray &dataBin );

	//get the size of a save
	//this is the combined size of the banner + all the files
	static quint32 GetSize( QByteArray dataBin );
private:
    bool _ok;
    quint32 ngID;
    QByteArray ngSig;
    quint32 ngKeyID;
    SaveGame sg;

    //not sure where else to get this except from reading an existing save
    QByteArray ngMac;


};

#endif // SAVEDATABIN_H
