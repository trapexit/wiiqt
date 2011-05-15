#ifndef NUSDOWNLOADER_H
#define NUSDOWNLOADER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "includes.h"
#include "tiktmd.h"
#include "tools.h"

#define SHOPPING_USER_AGENT		"Opera/9.00 (Nintendo Wii; U; ; 1038-58; Wii Shop Channel/1.0; en)"
#define UPDATING_USER_AGENT		"wii libnup/1.0"
#define VIRTUAL_CONSOLE_USER_AGENT	"libec-3.0.7.06111123"
#define WIICONNECT24_USER_AGENT		"WiiConnect24/1.0FC4plus1 (build 061114161108)"
#define NUS_BASE_URL			"http://ccs.shop.wii.com/ccs/download/"



enum
{
    IDX_CETK = 0x9000,
    IDX_TMD
};

struct downloadJob
{
    QString tid;
    QString name;
    quint16 index;
    QByteArray data;
};

//class to download titles from nintendo update servers
class NusDownloader : public QObject
{
    Q_OBJECT
public:
    explicit NusDownloader( QObject *parent = 0, const QString &cPath = QString() );

    //set the path used to cache files
    void SetCachePath( const QString &cPath = QString() );

    //get a title from NUS.  if version is 0, get the latest one
    void GetTitle( const NusJob &job );
    void GetTitles( const QList<NusJob> &jobs );
    void Get( quint64 tid, bool decrypt, quint16 version = TITLE_LATEST_VERSION );

    //TODO!  this function is far from complete
    //starts getting all the titles involved for a given update
    //expects strings like "4.2u" and "3.4e" ( any of the updates listed below should be working )
    //returns false if it doesnt know the IOS for the given string
    // The boot2 update is not uncluded in this.  ask for it separately if you want it
    bool GetUpdate( const QString & upd, bool decrypt = true );

    //get a list of titles for a given update
    //if a title is not available on NUS, a substitute is given instead ( a later version of the same title )
    //to keep people from bulk DLing and installing and messing something up, any boot2 update will NOT be included
    //in the list, ask for it specifically.
    //lists are created from wiimpersonator logs when available.  otherwise they come from examining game update partitions

    static QMap< quint64, quint16 > List20u();//* there are no games ive seen that contain this update.  this is just a guess
    static QMap< quint64, quint16 > List22u();
    static QMap< quint64, quint16 > List30u();
    static QMap< quint64, quint16 > List31u();
    static QMap< quint64, quint16 > List32u();
    static QMap< quint64, quint16 > List33u();
    static QMap< quint64, quint16 > List34u();
    static QMap< quint64, quint16 > List40u();
    static QMap< quint64, quint16 > List41u();
    static QMap< quint64, quint16 > List42u();
    static QMap< quint64, quint16 > List43u();

    static QMap< quint64, quint16 > List20e();//* there are no games ive seen that contain this update.  this is just a guess
    static QMap< quint64, quint16 > List21e();
    static QMap< quint64, quint16 > List22e(); //* there are no games ive seen that contain this update.  this is just a guess
    static QMap< quint64, quint16 > List30e();
    static QMap< quint64, quint16 > List31e();
    static QMap< quint64, quint16 > List32e(); //* there are no games ive seen that contain this update, i have only copied 3.1e and changed the system menu
    static QMap< quint64, quint16 > List33e();
    static QMap< quint64, quint16 > List34e();
    static QMap< quint64, quint16 > List40e();
    static QMap< quint64, quint16 > List41e();
    static QMap< quint64, quint16 > List42e();
    static QMap< quint64, quint16 > List43e();

    static QMap< quint64, quint16 > List35k();
    static QMap< quint64, quint16 > List41k();
    static QMap< quint64, quint16 > List42k();
    static QMap< quint64, quint16 > List43k();

    static QMap< quint64, quint16 > List20j();
    static QMap< quint64, quint16 > List22j();//* there are no games ive seen that contain this update, i have only copied 2.1e and changed the system menu
    static QMap< quint64, quint16 > List30j();
    static QMap< quint64, quint16 > List31j();
    static QMap< quint64, quint16 > List32j();//* there are no games ive seen that contain this update, i have only copied 3.1j and changed the system menu
    static QMap< quint64, quint16 > List33j();
    static QMap< quint64, quint16 > List34j();
    static QMap< quint64, quint16 > List40j();
    static QMap< quint64, quint16 > List41j();
    static QMap< quint64, quint16 > List42j();
    static QMap< quint64, quint16 > List43j();

private:

    //helper function to create a job
    downloadJob CreateJob( const QString &name, quint16 index );

    //path on the PC to try and get files from and save to to avoid downloading deplicates from NUS
    QString cachePath;

    //get data from the cache and put it in the job's data
    //uses the version from currentJob
    QByteArray GetDataFromCache( const downloadJob &job );

    //saves downloaded data to the HDD for later use
    bool SaveDataToCache( const QString &path, const QByteArray &stuff );

    //check the tmd and try to ticket
    void ReadTmdAndGetTicket( const QByteArray &ba );

    bool DecryptCheckHashAndAppendData( const QByteArray &encData, quint16 idx );

    //triggered when a file is done downloading
    void FileIsFinishedDownloading( const downloadJob &job );

    //send a fail message about the current job and skip to the next
    void CurrentJobErrored( const QString &str );

    //print info about a job to qDebug()
    void DbgJoB( const NusJob &job );

    //get the path for a file in the local cache
    QString GetCachePath( quint32 idx );

    //list of stuff to do
    QList<NusJob>jobList;

    //variables to keep track of the current downloading title
    NusJob currentJob;
    Tmd curTmd;

    //variables for actually downloading something from the interwebz
    QNetworkAccessManager manager;
    QQueue< downloadJob > downloadQueue;
    QNetworkReply *currentDownload;
    QTime downloadTime;
    QString currentJobText;
    downloadJob dlJob;

    //should be true if this object is busy getting a title and false if all work is done, or a fatal error
    bool running;

    //used to help in sending the overall progress done, should be reset on fatal error or when all jobs are done
    quint32 totalJobs;

    //used to help in sending the progress for the current title
    quint32 totalTitleSize;
    quint32 TitleSizeDownloaded();

    //remember the ticked key for repeated use
    QByteArray decKey;



signals:
    void SendError( const QString &message, NusJob job );//send an errer and the title the error is about
    //send an errer and the title the error is about, no more jobs will be done, and the SendDone signal will not be emited
    void SendFatalErrorError( const QString &message, NusJob job );//currently not used
    void SendDone();//message that all jobs are done

    //send progress about the currently downloading job
    void SendDownloadProgress( int );

    //send progress about the overall progress
    void SendTotalProgress( int );

    //send progress info about the current title
    void SendTitleProgress( int );

    //sends a completed job to whoever is listening
    void SendData( NusJob );

    //a file is done downloading
    void finished();

    //send pretty text about what is happening
    void SendText( QString );

    //maybe this one is redundant
    //void SendJobFinished( downloadJob );

public slots:
    void StartNextJob();
    void GetNextItemForCurrentTitle();

    //slots for actually downloading something from online
    void StartDownload();
    void downloadProgress( qint64 bytesReceived, qint64 bytesTotal );
    void downloadFinished();
    void downloadReadyRead();
};


#endif // NUSDOWNLOADER_H
