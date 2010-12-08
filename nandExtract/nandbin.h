#ifndef NANDBIN_H
#define NANDBIN_H

#include "includes.h"
struct fst_t
{
    quint8 filename[ 0xc ];//showmii wads has size 0xb here but reads 0xc bytes into name??
    quint8 mode;
    quint8 attr;
    quint16 sub;
    quint16 sib;
    quint32 size;
    quint32 uid;
    quint16 gid;
    quint32 x3;
};
// class to deal with an encrypted wii nand dump
// basic usage... create an object, set a path, call InitNand.  then you can get the detailed list of entries with GetTree()
// extract files with GetFile()
class NandBin : public QObject
{
    Q_OBJECT

public:
    NandBin( QObject * parent = 0, const QString &path = QString() );
    ~NandBin();
    bool SetPath( const QString &path );

    //try to read the filesystem and create a tree from its contents
    //icons given here will be the ones used when asking for that tree
    bool InitNand( QIcon dirs = QIcon(), QIcon files = QIcon() );

    //get a root item containing children that are actually entries in the nand dump
    //the root itself is just a container to hold them all and can be deleted once its children are taken
    QTreeWidgetItem *GetTree();

    //returns the data that makes up the file of a given entry#
    QByteArray GetFile( quint16 entry );

    //extracts an item( and all its children ) to a directory
    //this function is BLOCKING and will block the current thread, so if done in the gui thread, it will freeze your GUI till it returns
    bool ExtractToDir( QTreeWidgetItem *item, const QString &path );

    //print a little info about the free space
    void ShowInfo();

    //set this to change ":" in names to "-" on etracting.
    //theres more illegal characters in FAT, but thes seems to be the only one that happens on the nand FS
    void SetFixNamesForFAT( bool fix = true );

private:
    QByteArray key;
    qint32 loc_super;
    qint32 loc_fat;
    qint32 loc_fst;
    QString extractPath;
    QString nandFile;
    QFile f;
    int type;

    bool fatNames;
    QIcon groupIcon;
    QIcon keyIcon;

    int GetDumpType( quint64 fileSize );
    bool GetKey( int type );
    QByteArray ReadKeyfile( QString path );
    qint32 FindSuperblock();
    quint16 GetFAT( quint16 fat_entry );
    fst_t GetFST( quint16 entry );
    QByteArray GetCluster( quint16 cluster_entry, bool decrypt = true );
    //QByteArray ReadPage( quint32 i, bool withEcc = false );
    QByteArray GetFile( fst_t fst );

    QString FstName( fst_t fst );
    bool ExtractFST( quint16 entry, const QString &path, bool singleFile = false );
    bool ExtractDir( fst_t fst, QString parent );
    bool ExtractFile( fst_t fst, QString parent );



    QTreeWidgetItem *root;
    bool AddChildren( QTreeWidgetItem *parent, quint16 entry );

signals:
    void SendError( QString );
    void SendText( QString );
};

#endif // NANDBIN_H
