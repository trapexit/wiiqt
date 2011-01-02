#ifndef NANDTHREAD_H
#define NANDTHREAD_H

#include "../WiiQt/includes.h"
#include "../WiiQt/nandbin.h"


//TODO:  this thread really isnt setup to be inturruptable.  especially while it is extracting the full nand.bin
class NandThread : public QThread
{
    Q_OBJECT

public:
    NandThread( QObject *parent = 0 );
    ~NandThread();

    bool IsRunning();

    //these just call the same function of the nand.bin object
    // dont use them if the thread is running ( it is busy extracting )
    // wait till after SendExtractDone() is emitted
    bool SetPath( const QString &path );
    QTreeWidgetItem *GetTree();
    bool InitNand( QIcon dirs = QIcon(), QIcon files = QIcon() );
    const QList<quint16> GetFats();
    const QList<quint16> GetFatsForFile( quint16 i );


    //extract some stuff from a nand.bin in a thread so the gui doesn't freeze up
    void Extract( QTreeWidgetItem *item, const QString &path );
    void ForceQuit();

    const Blocks0to7 BootBlocks();
    const QList<Boot2Info> Boot2Infos();
    quint8 Boot1Version();

protected:
    void run();

signals:
    void SendProgress( int );
    void SendExtractDone();
    void SendText( QString );
    void SendError( QString );

private:
    QMutex mutex;
    QWaitCondition condition;

    QString extractPath;
    QTreeWidgetItem *itemToExtract;
    NandBin nandBin;


    quint32 fileCnt;
    quint32 idx;

    bool abort;

    //count the number of files in a given folder
    quint32 FileCount( QTreeWidgetItem *item );


public slots:
    void GetError( QString );
    void GetStatusUpdate( QString );
};

#endif // NANDTHREAD_H
