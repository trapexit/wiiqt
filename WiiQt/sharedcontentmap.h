#ifndef SHAREDCONTENTMAP_H
#define SHAREDCONTENTMAP_H

#include "includes.h"

//class for handling a content.map from a wii nand
class SharedContentMap
{
public:
    SharedContentMap( const QByteArray &old = QByteArray() );

    //checks that the content map is sane
    //size should be correct, contents should be in numerical order
    //if a path is given, it will check that the hashes in the map match up with the contents in the folder
    bool Check( const QString &path = QString() );

    //gets a string containing the 8 letter app that matches the given hash.
    //returns an empty string if the hash is not found in the map
    QString GetAppFromHash( const QByteArray &hash );

    //gets the first available u32 that is not already in the map and returns it as a string
    QString GetNextEmptyCid();

    //adds an entry to the end of the map
    //! this function doesnt check if the entry already exists
    void AddEntry( const QString &app, const QByteArray &hash );

    //get the entire data ready for writing to a wii nand
    const QByteArray &Data(){ return data; }

    const QByteArray Hash( quint16 i );
    const QString Cid( quint16 i );
    quint16 Count();

private:
    QByteArray data;
};

#endif // SHAREDCONTENTMAP_H
