#ifndef U8_H
#define U8_H

#include "lz77.h"
#include "includes.h"

/*order of the names in the imet header
japanese
english
german
french
spanish
italian
dutch
simp_chinese
trad_chinese
korean
*/

struct FEntry
{
    union
    {
        struct
        {
            unsigned int Type	:8;
            unsigned int NameOffset	:24;
        };
        unsigned int TypeName;
    };
    union
    {
        struct		// File Entry
        {
            unsigned int FileOffset;
            unsigned int FileLength;
        };
        struct		// Dir Entry
        {
            unsigned int ParentOffset;
            unsigned int NextOffset;
        };
        unsigned int entry[ 2 ];
    };
};

enum
{
    U8_Hdr_none = 0,
    U8_Hdr_IMET_app,//basically the same except the app has 0x40 bytes of padding more than the bnr
    U8_Hdr_IMET_bnr,
    U8_Hdr_IMD5
};

//class to handle U8 archives found in wii games & system files
// handles different header types, lz77 & ash
//!  basic usage is to read an existing U8 archive into a qbytearray and pass it to this class through the second constructor or Load()
//!  you can also create a brand-spankin-new U8 archive by using the first constructor
//!  check IsOk() to see if that it was loaded ok
//!  then call Entries() to get a list of everything that is in the archive
//!  access the data in that archive with AddEntry(), RemoveEntry(), ReplaceEntry(), RenameEntry(), and GetData()
class U8
{
public:
    //creates a U8 object
    //if initialize is true, it will call CreateEmptyData() in this U8
    //you can also set the header type & imet names, though they dont affect the actual data in the archive
    U8( bool initialize = false, int type = U8_Hdr_none, const QStringList &names = QStringList() );
    U8( const QByteArray &ba );

    //check if the u8 constructor finished successfully
    bool IsOK();

    void Load( const QByteArray &ba );

    //set the variables and create an fst with only a root node
    bool CreateEmptyData();

    //adds an entry to this U8 with the given path, and type, and if it is a file entry, the given data
    //returns the index of the created entry or -1 on error
    int AddEntry( const QString &path, int type, const QByteArray &ba = QByteArray() );

    //removes an entry, and all its children, from the archive
    bool RemoveEntry( const QString &path );

    //replace a file in the archive with different data ( only supports files, no directories )
    //if autoCompress is true, it will check if the data to be replaced is ASH/LZ77 compressed and make sure the new data is LZ77'd
    bool ReplaceEntry( const QString &path, const QByteArray &ba = QByteArray(), bool autoCompress = true );

    //remane an entry.  this expects a full path of the entry to be named, and newName is only the replacement name ( not the full path )
    bool RenameEntry( const QString &path, const QString &newName );


    //returns the index of an item in the archive FST, or -1 the item is not present
    //it accepts names like "/arc/anim/".  parent is the directory to start looking in
    int FindEntry( const QString &str, int parent = 0 );
    quint32 NextEntryInFolder( quint32 current, quint32 directory );
    QString FstName( const FEntry *entry );
    QString FstName( quint32 i );

    //gets the data for a path inside the archive
    //if no path is given, returns the data for the whole archive, starting at the U8 tag
    //if onlyPayload is true, it will return the data unLZ77'd ( and without its IMET/IMD5 header for nested U8 archives )
    const QByteArray GetData( const QString &str = QString(), bool onlyPayload = false );

    //get the size of an entry in the archive
    //if no path is given, returns the size for the whole archive, starting at the U8 tag
    quint32 GetSize( const QString &str = QString() );

    //returns a list of all the entries in the u8 archive
    //directories end with "/"
    const QStringList Entries();

    //check the first 5000 bytes of a file for the u8 tag.
    //if necessary, un-LZ77 the data before checking
    //this doesnt check anything else in the archive
    static bool IsU8( const QByteArray &ba );

    //returns the location of the U8 tag in the first 5000 bytes of the given data
    static int GetU8Offset( const QByteArray &ba );

    //adds the IMD5 header to the given bytearray and returns it ( original data is modified )
    static QByteArray AddIMD5( QByteArray ba );

    //returns the U8 archive data with an IMD5 header added
    //the actual data is not changed
    const QByteArray AddIMD5();

    //adds the IMET header to the given bytearray and returns it ( original data is modified )
    static QByteArray AddIMET( QByteArray ba, const QStringList &names );

    //get an IMET header using the given names and sizes
    //padding type will add 0, 0x40, or 0x80 bytes of 0s to the beginning
    static QByteArray GetIMET( const QStringList &names, int paddingType = U8_Hdr_IMET_app, quint32 iconSize = 0, quint32 bannerSize = 0, quint32 soundSize = 0 );

    //returns the U8 archive data with an IMET header added
    //the actual archive data is not changed
    const QByteArray AddIMET( int paddingType = U8_Hdr_IMET_app );

    //ruterns a list of the names as read from the imet header
    const QStringList &IMETNames();

    //set the imet names
    //the data in the archive is not changed, but calling AddIMET() on this banner will use the new names
    void SetImetNames( const QStringList &names );

    //void SetHeaderType( int type );
    int GetHeaderType();



    //reads from the fst and makes the list of paths
    void CreateEntryList();

    // if this archive was packed by any of the wii.cs tools and it is not
    // a standard banner, it will be incorrectly packed, and this will return true
    bool IsBuggy(){ return wii_cs_error; }

private:
    QByteArray data;//data starting at the U8 tag
    QStringList paths;
    bool ok;

    bool wii_cs_error;

    //if this archive as a whole is lz77 compressed
	//bool isLz77;
	LZ77::CompressionType lz77Type;

    QStringList imetNames;
    int headerType;
    void ReadHeader( const QByteArray &ba );



    //just a pointer to the fst data.
    //this pointer needs to be updated any time data is changed
    FEntry* fst;

    //stored in host endian !
    quint32 fstSize;
    quint32 rootnode_offset;
    quint32 data_offset;
    quint32 NameOff;

    //a list of the nested u8 archives
    QMap<QString, U8>nestedU8s;
};


#endif // U8_H
