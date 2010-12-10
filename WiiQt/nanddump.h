#ifndef NANDDUMP_H
#define NANDDUMP_H

#include "nusdownloader.h"
#include "includes.h"
#include "sharedcontentmap.h"
#include "uidmap.h"

//class for handeling an extracted wii nand filesystem
//!  nothing can be done unless basePath is set.  do this either by setting it in the constructor, or by calling SetPath()
//!  current reading and writing is limited to installing a title in the form of NusJob, reading/writing/deleting specific paths
//!  for performance reasons, the uid and content map are cached and only written to the HDD when the destructor or Flush() is called
class NandDump
{
public:
    NandDump( const QString &path = QString() );
    ~NandDump();

    //sets the basepath for this nand
    //if it doesnt exist, the function will try to create it
    //also creates the normal folders in the nand
    bool SetPath( const QString &path );
    const QString GetPath(){ return basePath; }

    //installs a title to the nand dump from an already existing NusJob
    //returns false if something went wrong
    bool InstallNusItem( NusJob job );

    //tries to delete a title from the nand dump
    //deleteData gives the option to just delete the title and leave behind its data
    bool DeleteTitle( quint64 tid, bool deleteData = false );

    //check what version a given title is on this nand, returns 0 if it isnt installed
    //quint16 GetTitleVersion( quint64 tid );

    //get a list of all titles for which there is a ticket & tmd
    // returns a map of < tid, version >
    //QMap< quint64, quint16 > GetInstalledTitles();

    //write the current uid & content.map to the PC
    //failure to make sure this is done can end up with a broken nand
    bool Flush();

    //overloads GetFile() with "/title/00000001/00000002/data/setting.txt"
    QByteArray GetSettingTxt();
    bool SetSettingTxt( const QByteArray ba );

    //reads a file from the nand and returns it as a qbytearray
    const QByteArray GetFile( const QString &path );

    //tries to write the given bytearray to a file of the given path
    bool SaveData( const QByteArray ba, const QString& path );

    //expects a file, not directory
    void DeleteData( const QString & path );


private:
    QString basePath;
    SharedContentMap cMap;
    UIDmap uidMap;

    //write the current uid.sys to disc
    bool uidDirty;
    bool FlushUID();

    //write the content.map to disc
    bool cmDirty;
    bool FlushContentMap();

    bool InstallTicket( const QByteArray ba, quint64 tid );
    bool InstallTmd( const QByteArray ba, quint64 tid );
    bool InstallSharedContent( const QByteArray ba, const QByteArray hash = QByteArray() );
    bool InstallPrivateContent( const QByteArray ba, quint64 tid, const QString &cid );
    void AbortInstalling( quint64 tid );

    //go through and delete all the stuff in a given folder and then delete the folder itself
    //this function expects an absolute path, not a relitive one inside the nand dump
    bool RecurseDeleteFolder( const QString &path );
};

#endif // NANDDUMP_H
