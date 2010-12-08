#ifndef WAD_H
#define WAD_H

#include "includes.h"
class Wad
{
public:
    //create a wad instance from a bytearray containing a wad
    Wad( const QByteArray stuff = QByteArray() );

    //create a wad object from a list of byteArrays.
    //the first one should be the tmd, the second one the ticket, and the rest are the contents, in order
    //it will use the global cert unless one is given with SetCert
    Wad( QList< QByteArray > stuff, bool isEncrypted = true );

    //check if this wad is valid
    bool IsOk(){ return ok; }

    //set the cert for this wad
    void SetCert( const QByteArray stuff );

    //returns the tid of the tmd of this wad( which may be different than the one in the ticket if somebody did something stupid )
    quint64 Tid();

    //set the tid in the ticket&tmd and fakesign the wad
    void SetTid( quint64 tid );

    //add a new content to this wad and fakesign
    //if the data is encrypted, set that arguement to true
    //index is the index used for the new entry, default is after all the others
    void AddContent( const QByteArray &stuff, quint16 type, bool isEncrypted = false, quint16 index = 0xffff );

    //remove a content from this wad
    void RemoveContent( quint16 index );

    //set the global cert that will be used for all created
    static void SetGlobalCert( const QByteArray &stuff );

    //pack a wad from the given directory
    static QByteArray FromDirectory( QDir dir );

    //get a assembled wad from the list of parts
    //the first one should be the tmd, the second one the ticket, and the rest are the contents, in order
    //it will use the global cert
    static QByteArray FromPartList( QList< QByteArray > stuff, bool isEncrypted = true );

    //get all the parts of this wad put together in a wad ready for writing to disc or whatever
    const QByteArray Data( quint32 magicWord = 0x49730000, const QByteArray footer = QByteArray() );

    //get the decrypted data from a content
    const QByteArray Content( quint16 i );

    //get the last error encountered while trying to do something
    const QString LastError(){ return errStr; }

    //get a name for a wad as it would be seen in a wii disc update partition
    //if a path is given, it will check that path for existing wads with the name and append a number to the end "(1)" to avoid duplicate files
    //returns an empty string if it cant guess the title based on TID
    static QString WadName( quint64 tid, quint16 version, QString path = QString() );

    QString WadName( QString path = QString() );


private:
    bool ok;
    QString errStr;

    //keep encrypted parts here
    QList< QByteArray > partsEnc;
    QByteArray tmdData;
    QByteArray tikData;
    QByteArray certData;

    void Err( QString str );
};

#endif // WAD_H
