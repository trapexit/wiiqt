#ifndef WAD_H
#define WAD_H

#include "includes.h"
class Wad
{
public:
    //create a wad instance from a bytearray containing a wad
    Wad( const QByteArray &stuff = QByteArray() );

    //create a wad object from a list of byteArrays.
    //the first one should be the tmd, the second one the ticket, and the rest are the contents, in order
    //it will use the global cert unless one is given with SetCert
    Wad( const QList< QByteArray > &stuff, bool isEncrypted = true );

	//create a wad from the given directory
	//! dir should be a directory containing a tmd ( "*.tmd" or "tmd.*" ), a ticket ( "*.tik" or "cetk" ),
	//! all the contents ( <cid>.app :where cid is the correct cid from the tmd, not the "0, 1, 2, 3..." bullshit some broken wad-unpackers create )
	//! if any of the .apps do not match in size or hash what is in the TMD, then the TMD will be updated and fakesigned
	//! a cert ( "*.cert" ) is also supported, but not required
	Wad( QDir dir );

    //check if this wad is valid
    bool IsOk(){ return ok; }

    //set the cert for this wad
    void SetCert( const QByteArray &stuff );

    //returns the tid of the tmd of this wad( which may be different than the one in the ticket if somebody did something stupid )
    quint64 Tid();

    //set the tid in the ticket&tmd and fakesign the wad
	bool SetTid( quint64 tid, bool fakeSign = true );

	//set the ios in the tmd and fakesign the wad
	bool SetIOS( quint32 ios, bool fakeSign = true );

	//set the version in the tmd and fakesign the wad
	bool SetVersion( quint16 ver, bool fakeSign = true );

	//set the tmd to allow AHBPROT removal
	bool SetAhb( bool remove = true, bool fakeSign = true );

	//set the tmd to allow direct disc access
	bool SetDiskAccess( bool allow = true, bool fakeSign = true );

	bool FakeSign( bool signTmd, bool signTicket );

    //replace a content of this wad, update the size & hash in the tmd and sign it
    //ba should be decrypted
    bool ReplaceContent( quint16 idx, const QByteArray &ba );

    //add a new content to this wad and fakesign
    //if the data is encrypted, set that arguement to true
    //index is the index used for the new entry, default is after all the others
    //void AddContent( const QByteArray &stuff, quint16 type, bool isEncrypted = false, quint16 index = 0xffff );

    //remove a content from this wad
    //void RemoveContent( quint16 index );

    //set the global cert that will be used for all created
    static void SetGlobalCert( const QByteArray &stuff );

    //pack a wad from the given directory
    //returns a bytearray containing a wad reading for writing to a file
	//or an empty bytearray on error
    static QByteArray FromDirectory( QDir dir );

    //get a assembled wad from the list of parts
    //the first one should be the tmd, the second one the ticket, and the rest are the contents, in order
    //it will use the global cert
    static QByteArray FromPartList( const QList< QByteArray > &stuff, bool isEncrypted = true );

    //get all the parts of this wad put together in a wad ready for writing to disc or whatever
    const QByteArray Data( quint32 magicWord = 0x49730000, const QByteArray &footer = QByteArray() );
	
    //get the tmd for the wad
    const QByteArray getTmd();

    //get the tik for the wad
    const QByteArray getTik();

	const QByteArray GetCert();

    //get the decrypted data from a content
    const QByteArray Content( quint16 i );

    //get the number of contents
    quint32 content_count();

    //get the last error encountered while trying to do something
    const QString LastError(){ return errStr; }

    //get a name for a wad as it would be seen in a wii disc update partition
    //if a path is given, it will check that path for existing wads with the name and append a number to the end "(1)" to avoid duplicate files
    //returns an empty string if it cant guess the title based on TID
    static QString WadName( quint64 tid, quint16 version, const QString &path = QString() );

    //get this Wad's name as it would appear on a disc update partition
    QString WadName( const QString &path = QString() );

private:
    bool ok;
    QString errStr;

    //keep encrypted parts here
    QList< QByteArray > partsEnc;
    QByteArray tmdData;
    QByteArray tikData;
    QByteArray certData;

    void Err( const QString &str );
};

#endif // WAD_H
