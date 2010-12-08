#ifndef SAVELISTITEM_H
#define SAVELISTITEM_H


#include "savebanner.h"
#include "includes.h"


class SaveListItem : public QListWidgetItem
{
public:

    SaveListItem( SaveBanner banner, const QString &tid, quint32 s, QListWidget * parent = 0 );
    ~SaveListItem();
    SaveBanner *Banner(){ return &sb; }
    QString Tid(){ return id; }
    quint32 Size(){ return size; }
    //quint32 Blocks(){ return size / 0x20000; }
    //quint32 MiB(){ return size / 0x100000; }

private:
    SaveBanner sb;
    QString id;
    quint32 size;
};

#endif // SAVELISTITEM_H
