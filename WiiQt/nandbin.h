#ifndef NANDBIN_H
#define NANDBIN_H

#include "includes.h"
struct fst_t
{
    quint8 filename[ 0xc ];
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

//once InitNand() is called, you can get the contents of the nand in a nice QTreeWidgetItem* with GetTree()
class NandBin : public QObject
{
    Q_OBJECT

public:
    //creates a NandBin object. if a path is given, it will call SetPath() on that path.  though you cant check the return value
    NandBin( QObject * parent = 0, const QString &path = QString() );

    //destroys this object, and all its used resources ( closes the nand.bin file and deletes the filetree )
    ~NandBin();

    //sets the path of this object to path.  returns false if it cannot open an already existing file
    //keys.bin should be in this same path if they are to be used
    bool SetPath( const QString &path );

    //try to read the filesystem and create a tree from its contents
    //this takes care of the stuff like reading the keys, finding teh superblock, and creating the QTreeWidgetItem* tree
    //icons given here will be the ones used when asking for that tree
    bool InitNand( QIcon dirs = QIcon(), QIcon files = QIcon() );

    //get a root item containing children that are actually entries in the nand dump
    //the root itself is just a container to hold them all and can be deleted once its children are taken
    //! all returned items are cloned and it is up to you to delete them !
    //text is assigned to the items as follows...
    // 0 name
    // 1 entry #
    // 2 size
    // 3 uid
    // 4 gid
    // 5 x3
    // 6 mode
    // 7 attr
    QTreeWidgetItem *GetTree();


    //extracts an item( and all its children ) to a directory
    //this function is BLOCKING and will block the current thread, so if done in the gui thread, it will freeze your GUI till it returns
    bool ExtractToDir( QTreeWidgetItem *item, const QString &path );

    //print a little info about the free space
    void ShowInfo();

    //set this to change ":" in names to "-" on etracting.
    //theres more illegal characters in FAT, but thes seems to be the only one that happens on the nand FS
    void SetFixNamesForFAT( bool fix = true );

    //returns the data that makes up the file of a given entry#
    QByteArray GetFile( quint16 entry );


    //get data for a given path
    //! this function is slower than the above one, as it first iterates through the QTreeWidgetItems till it finds the right ono
    //! and then end up calling the above one anyways.
    //the path should be a file, not folder
    //returns an empty array on failure
    //path should start with "/" and items should be delimited by "/"
    //ie...  /title/00000001/00000002/data/setting.txt
    const QByteArray GetData( const QString &path );

    //returns the fats for this nand.
    const QList<quint16> GetFats() { return fats; }

    //get the fats for a given file
    const QList<quint16> GetFatsForFile( quint16 i );


private:
    QByteArray key;
    qint32 loc_super;
    qint32 loc_fat;
    qint32 loc_fst;
    QString extractPath;
    QString nandPath;
    QFile f;
    int type;

    bool fatNames;
    QIcon groupIcon;
    QIcon keyIcon;

    //read all the fst and remember them rather than seeking back and forth in the file all the time
    // uses ~120KiB RAM
    bool fstInited;
    fst_t fsts[ 0x17ff ];

    //cache the fat to keep from having to look them up repeatedly
    // uses ~64KiB
    QList<quint16>fats;

    int GetDumpType( quint64 fileSize );
    bool GetKey( int type );
    QByteArray ReadKeyfile( QString path );
    qint32 FindSuperblock();
    quint16 GetFAT( quint16 fat_entry );
    fst_t GetFST( quint16 entry );
    QByteArray GetCluster( quint16 cluster_entry, bool decrypt = true );
    QByteArray GetFile( fst_t fst );

    QString FstName( fst_t fst );
    bool ExtractFST( quint16 entry, const QString &path, bool singleFile = false );
    bool ExtractDir( fst_t fst, QString parent );
    bool ExtractFile( fst_t fst, QString parent );



    QTreeWidgetItem *root;
    bool AddChildren( QTreeWidgetItem *parent, quint16 entry );
    QTreeWidgetItem *ItemFromPath( const QString &path );
    QTreeWidgetItem *FindItem( const QString &s, QTreeWidgetItem *parent );

signals:
    //connect to these to receive messages from this object
    void SendError( QString );
    void SendText( QString );
};

#endif // NANDBIN_H
