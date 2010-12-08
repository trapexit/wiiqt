#ifndef SAVELOADTHREAD_H
#define SAVELOADTHREAD_H

#include "savelistitem.h"
#include "includes.h"
enum
{
    LOAD_SNEEK = 0x111,
    LOAD_PC
};
class SaveLoadThread : public QThread
{
    Q_OBJECT

 public:
     SaveLoadThread( QObject *parent = 0 );
     ~SaveLoadThread();

     void GetBanners( const QString &bPath, int mode );
     void ForceQuit();

 protected:
     void run();

 signals:
     void SendProgress( int );
     void SendDone( int );
     void SendItem( QByteArray, const QString&, int, int );

 private:
     QMutex mutex;
     QWaitCondition condition;
     QString basePath;
     int type;

     bool abort;

     int GetFolderSize( const QString& path );
 };

//Q_DECLARE_METATYPE( SaveListItem* )


#endif // SAVELOADTHREAD_H
