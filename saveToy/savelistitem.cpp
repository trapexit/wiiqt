#include "savelistitem.h"

SaveListItem::SaveListItem( SaveBanner banner, const QString &tid, quint32 s, QListWidget * parent ) : QListWidgetItem( parent ), size( s )
{
    sb = banner;
    QImage im = banner.BannerImg();
    if( !im.isNull() )
    {
	im.load( ";/noBanner.png" );
    }
    QString tex = banner.Title();
    if( tex.isEmpty() )
	tex = "???";

    this->setText( tex );
    this->setIcon( QIcon( QPixmap::fromImage( im ) ) );
    id = tid;
}

SaveListItem::~SaveListItem()
{
    //qDebug() << "SaveListItem destroyed";
}
