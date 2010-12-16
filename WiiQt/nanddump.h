#ifndef NANDDUMP_H
#define NANDDUMP_H

#include "nusdownloader.h"
#include "includes.h"
#include "sharedcontentmap.h"
#include "uidmap.h"
#include "wad.h"

struct SaveGame//struct to hold save data
{
    quint64 tid;		//tid this data belongs to
    QStringList	entries;	//paths of all the files & folders
    QList<bool>isFile;		//type of each entry.  false = folder, true = file
    QList<QByteArray> data;	//data for each file.  size of this list should equal the number of files in the above list
};

//class for handeling an extracted wii nand filesystem
//!  nothing can be done unless basePath is set.  do this either by setting it in the constructor, or by calling SetPath()
//!  current reading and writing is limited to installing a title in the form of NusJob, reading/writing/deleting specific paths
//!  for performance reasons, the uid and content map are cached and only written to the HDD when the destructor or Flush() is called
//!  GetData() and SaveData() expect relative paths inside the nand.

//!  to support creating a nand on different filesystems, characters in filenames may have to be changed
//!  use SetReplaceString() to specify a character to replace and the string to replace it with.  these characters
//!  will be stored in "/sys/replace".  only characters allowed on a FAT32 filesystem are allowed in replacement strings
//!  to get a list of these replacement strings, use GetReplacementStrings()
//!  This is ONLY DESIGNED FOR FIXING SAVEGAMES.  All of the normal files on a nand can be stored on any modern filesystem
//!  The only time these replacement characters are used is when dealing with save data.  it is advised to only try to replace
//!  characters that cannot be used on a modern PC filesystem.  trying to replace any common string will probably result in
//!  a broken nand FS
class NandDump
{
public:
    NandDump( const QString &path = QString() );
    ~NandDump();

    //set a character to be replaced, and the string to replace it with
    //giving an empty replacement string will remove that entry and all the actual character will be used in paths
    //! this function will recurse the "/title" folder and apply any change to any existing files if finds
    //! if that fails ( like you tried to tell it to use a ':' while writing to a FAT32 drive), it will try to recurse that folder again and undo the change
    bool SetReplaceString( const QString ch, const QString &replaceWith = QString() );

    //get a list of the replacement strings used when writing save data
    // they are returned as ( character as it would be on the wii nand, string as it is on this nand dump )
    QMap<QString, QString> GetReplacementStrings();

    //sets the basepath for this nand
    //if it doesnt exist, the function will try to create it
    //also creates the normal folders in the nand
    bool SetPath( const QString &path );
    const QString GetPath(){ return basePath; }

    //installs a title to the nand dump from an already existing NusJob
    //returns false if something went wrong
    bool InstallNusItem( NusJob job );

    //installs a title to the nand dump from a wad
    //returns false if something went wrong
    bool InstallWad( Wad wad );

    //tries to delete a title from the nand dump
    //deleteData gives the option to just delete the title and leave behind its data
    bool DeleteTitle( quint64 tid, bool deleteData = false );

    //check what version a given title is on this nand, returns 0 if it isnt installed
    //quint16 GetTitleVersion( quint64 tid );

    //get a list of all titles for which there is a ticket & tmd
    // returns a map of < tid, version >
    QMap< quint64, quint16 > GetInstalledTitles();

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

    //extract save data for a given title
    // if no save is found, it will return a SaveData object with an empty list of entries
    SaveGame GetSaveData( quint64 tid );

    //installs a save to the nand
    bool InstallSave( SaveGame save );

    //convert a name TO the format that will be writen to the nand
    // it would be wise to only give these functions the name of the exact file you want to convert instead of the path
    // as there might be replacements for the path delimiter ('/')
    const QString ToNandName( const QString &name );

    //convert a name FROM the format that will be writen to the nand
    const QString FromNandName( const QString &name );

    //like the above function, but splits a path at '/', converts the parts, and puts it back together into a path again
    // the return string has '/' before EVERY entry
    //! these are not exactly lightweight as they call the above 2 functions for every part
    //! of a path.  they are only meant to be used for converting paths for savedata
    const QString ToNandPath( const QString &path );
    const QString FromNandPath( const QString &path );

    //sanity check a save object
    static bool IsValidSave( SaveGame save );



private:
    QString basePath;
    SharedContentMap cMap;
    UIDmap uidMap;


    QMap<QString, QString> replaceStrings;
    void ReadReplacementStrings();
    bool FlushReplacementStrings();

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

    //recursively replaces strings in filenames.  this one expects an absolute path as well
    bool RecurseRename( const QString &path, const QString &from, const QString &to );

};

#endif // NANDDUMP_H
