#ifndef SAVELOADTHREAD_H
#define SAVELOADTHREAD_H

//#include "savelistitem.h"
#include "../WiiQt/includes.h"
#include "../WiiQt/nanddump.h"
#define DESC_VERSION 1
enum
{
    LOAD_SNEEK = 0x111,
    LOAD_PC
};

//little struct to hold the info about all the saves for 1 game backed up on the PC
struct PcSaveInfo
{
	QString tid;
	QByteArray banner;
	QList<quint32> sizes;
	QStringList descriptions;
	QStringList paths;
};

class SaveLoadThread : public QThread
{
    Q_OBJECT

public:
    SaveLoadThread( QObject *parent = 0 );
    ~SaveLoadThread();

    //get banners
    //if no path is given, it will try to get them from NandDump, otherwise it will use the path on the PC ( not implemented yet )
    void GetBanners( const QString &bPath = QString() );

    //set this object's extracted nand path
    bool SetNandPath( const QString &bPath );

    //TODO: these arent done on the work thread, but instead in the calling thread
    bool DeleteSaveFromSneekNand( quint64 tid );
	bool InstallSaveToSneekNand( SaveGame save );
    SaveGame GetSave( quint64 tid );
	const QString NandBasePath();


    void ForceQuit();

protected:
    void run();
    NandDump nand;
    void GetSavesFromNandDump();
    void GetSavesFromPC();
	void GetPCSaves();

signals:
    void SendProgress( int );
	void SendDone( int );
	void SendSneekItem( QByteArray, const QString&, int );
	//void SendPcItem( QByteArray, const QString&, const QStringList&, int );
	void SendPcItem( PcSaveInfo );

private:
    QMutex mutex;
    QWaitCondition condition;
    QString basePath;
    int type;

    bool abort;

    int GetFolderSize( const QString& path );
};

#endif // SAVELOADTHREAD_H
