#ifndef SAVELISTITEM_H
#define SAVELISTITEM_H


#include "../WiiQt/savebanner.h"
#include "../WiiQt/includes.h"


class SaveListItem : public QListWidgetItem
{
public:

    SaveListItem( SaveBanner banner, const QString &tid, quint32 s, QListWidget * parent = 0 );
    ~SaveListItem();
    SaveBanner *Banner(){ return &sb; }
    QString Tid(){ return id; }
    quint32 Size(){ return size; }

private:
    SaveBanner sb;
    QString id;
    quint32 size;
};

#endif // SAVELISTITEM_H
