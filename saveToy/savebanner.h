#ifndef SAVEBANNER_H
#define SAVEBANNER_H

#include "includes.h"

class SaveBanner
{
public:
    SaveBanner();
    SaveBanner( const QString &bannerpath );
    SaveBanner( QByteArray stuff );

    QImage BannerImg(){ return bannerImg; }
    QList< QImage > IconImgs() { return iconImgs; }

    QString Title(){ return saveTitle; }
    QString SubTitle(){ return saveTitle2; }

private:
    bool ok;
    QImage bannerImg;
    QList< QImage > iconImgs;
    QString saveTitle;
    QString saveTitle2;

    QImage ConvertTextureToImage( const QByteArray &ba, quint32 w, quint32 h );//expects tpl texture data in rgb5a3 format
};

#endif // SAVEBANNER_H
