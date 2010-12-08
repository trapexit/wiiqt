#ifndef SHAREDCONTENTMAP_H
#define SHAREDCONTENTMAP_H

#include "includes.h"

class SharedContentMap
{
public:
    SharedContentMap( QByteArray old = QByteArray() );

    //checks that the content map is sane
    //size should be correct, contents should be in numerical order
    //if a path is given, it will check that the hashes in the map match up with the contents in the folder
    bool Check( const QString &path = QString() );

    QString GetAppFromHash( QByteArray hash );
    QString GetNextEmptyCid();

    void AddEntry( const QString &app, const QByteArray &hash );

    const QByteArray Data(){ return data; }

private:
    QByteArray data;
};

#endif // SHAREDCONTENTMAP_H
