#ifndef BLOCKS0TO7_H
#define BLOCKS0TO7_H

#include "includes.h"
//for now im creating this class to handle getting info about the first 8 blocks of nand
//im not sure what all it should be doing, so it may become a clusterfuck
//for now, just pass it a list of 8 bytearrays, each 0x20000 bytes.  then check IsOk(), and then you can check boot1version and Boot2Infos()

//! currently it checks the hash of boot1 block against known versions, checks blocks 1-7 for valid blockmaps,
//! looks for copies of boot2 in those blocks, then finds the tmd, ticket and encrypted data.  next it checks that
//! the tid in the tmd&ticket is correct, and verifies all the signatures and decrypts the actual content and checks its hash
//! finally it looks at the most recent valid blockmap and determines which copy would be used when booting & when booting from backup
//! Because it does all this, the first time requesting this data will cause the current thread to hang
//! ( checking the signatures  takes the longest ) after the first time, the list returned by Boot2Infos() is remembered
//! and it will be returned any time after that when this function is called

enum	//returned by Boot1Version().  theres no official names for these, so ill just stick with lettering them
{
    BOOT_1_UNK = 0,
    BOOT_1_A,
    BOOT_1_B,
    BOOT_1_C,
    BOOT_1_D
};

#define BOOTMII_UNK 0x2110
#define BOOTMII_11 0x2111
#define BOOTMII_13 0x2113
//TODO: what other versions of bootmii are there?

//enum used to convey the state of each copy of boot2 on a nand
enum
{
    BOOT_2_ERROR_PARSING	= -3,	//error parsing the thing, cant really tell more info, since it wasnt parsed :P
    BOOT_2_BAD_SIGNATURE	= -2,	//RSA signature of boot2 doesnt pass memcmp or strncmp
    BOOT_2_ERROR		= -1,	//some other error
    BOOT_2_BAD_CONTENT_HASH	= 1,	//sha1 of the content did not match what was in the TMD
    BOOT_2_TMD_FAKESIGNED	= 2,	//tmd RSA seems fine, it passed strncmp but not memcmp ( probably bootmii )
    BOOT_2_TMD_SIG_OK		= 4,	//tmd RSA seems fine, signature passed memcmp ( probably official shit )
    BOOT_2_TIK_FAKESIGNED	= 8,	//tik RSA seems fine, it passed strncmp but not memcmp - not really sure why this would happen
    BOOT_2_TIK_SIG_OK		= 0x10,	//tik RSA seems fine, signature passed memcmp
    BOOT_2_MARKED_BAD		= 0x20,	//the most recent blockmap says this copy of boot2 falls on bad blocks
    BOOT_2_USED_TO_BOOT		= 0x40,	//this is the copy used when trying to boot the wii
    BOOT_2_BACKUP_COPY		= 0x80	//this is the copy used when the first copy failed to boot
};


struct Boot2Info    //this little guy is just some container to hold the information about the state of boot2 in these blocks
{		    //the above values are used for the "state".  it will either be < 0 or it will be any of the other values |'d together
		    //if the state is above 0, version will either be the version from the TMD or one of the BOOTMII_... values

    quint8 firstBlock;	    //block that contains the header
    quint8 secondBlock;	    //block that contains the blockmap
    quint32 generation;	    //generation of the blockmap
    quint8 blockMap[ 8 ];   //blockmap found on secondBlock ( only the first 8 blocks, as the rest belong to the encrypted FS )
    qint32 state;	    //information about this copy of boot2
    quint16 version;	    //whatversion of boot2 this is
};


class Blocks0to7
{
public:
    Blocks0to7( QList<QByteArray>blocks = QList<QByteArray>() );
    bool SetBlocks( QList<QByteArray>blocks );
    bool IsOk(){ return _ok; }

    //check which version of boot1 we have
    quint8 Boot1Version();

    //get a list containing info for each copy of boot2 on the given blocks
    QList<Boot2Info> Boot2Infos();

private:
    bool _ok;
    //should hold the blocks, without ecc
    QList<QByteArray>blocks;

    //after teh first time Boot2Infos() is called, store the result here so any subsequent calls can just return this for speed
    QList< Boot2Info > boot2Infos;

    //this one doesnt really return a complete info, it only gets the block map from a block
    //and returns it in an incomplete Boot2Info
    Boot2Info GetBlockMap( QByteArray block );

    //checks the hashes and whatnot in a copy of boot2
    //returns an incomplete Boot2Info
    Boot2Info CheckHashes( Boot2Info info );

};

#endif // BLOCKS0TO7_H
