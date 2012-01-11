#ifndef SAVEBANNER_H
#define SAVEBANNER_H

#include "includes.h"

class SaveBanner
{
public:

    SaveBanner();
    SaveBanner( const QString &bannerpath );
    SaveBanner( QByteArray stuff );

	const QImage &BannerImg() const { return bannerImg; }
	const QList< QImage > &IconImgs() const { return iconImgs; }

	const QString &Title() const { return saveTitle; }
	const QString &SubTitle() const { return saveTitle2; }

	quint32 Attributes() { return attributes; }
	quint16 Speeds() { return speeds; }

private:
    bool ok;
    QImage bannerImg;
    QList< QImage > iconImgs;

	quint32 attributes;		// bit 5 = animation type.  if its set, the animation plays forward and backward
							// if its not set, the animation just plays forward and loops

	quint16 speeds;			// 2 bits per frame.  0 signifies the last frame?

    QString saveTitle;
    QString saveTitle2;

    QImage ConvertTextureToImage( const QByteArray &ba, quint32 w, quint32 h );//expects tpl texture data in rgb5a3 format
};

#endif // SAVEBANNER_H
