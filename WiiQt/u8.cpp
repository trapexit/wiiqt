#include "u8.h"
#include "lz77.h"
#include "tools.h"
#include "md5.h"
#include "ash.h"

#define U8_HEADER_ALIGNMENT 0x20 //wiibrew says 0x40, but wii.cs is using 0x20 and i havent heard of any bricks because of it
static quint32 swap24( quint32 i )
{
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    return i & 0xffffff;
#else
    int r = qFromBigEndian( i );
    return r >> 8;
#endif
}

U8::U8( bool initialize, int type, const QStringList &names )
{
    ok = false;
    isLz77 = false;
    paths.clear();
    nestedU8s.clear();
    fst = NULL;
    fstSize = 0;
    headerType = type;
    imetNames = names;
    if( !initialize )
	return;

    //headerType = type;
    //imetNames = names;

    ok = CreateEmptyData();
}

bool U8::CreateEmptyData()
{
    isLz77 = false;
    nestedU8s.clear();
    data = QByteArray( 0x20, '\0' );

    //create a generic u8 with no real info in it.  only a fst with a root node
    QBuffer buf( &data );
    if( !buf.open( QBuffer::WriteOnly ) )
    {
	qWarning() << "Can't create buffer";
	data.clear();
	return false;
    }
    buf.putChar( 'U' );
    buf.putChar( '\xAA' );
    buf.putChar( '8' );
    buf.putChar( '\x2D' );

    rootnode_offset = 0x00000020;
    quint32 tmp = qFromBigEndian( rootnode_offset );
    buf.write( (const char*)&tmp, 4 );

    fstSize = 0x0000000D;
    tmp = qFromBigEndian( fstSize );
    buf.write( (const char*)&tmp, 4 );

    data_offset = qFromBigEndian( 0x00000040 );
    tmp = qFromBigEndian( data_offset );
    buf.write( (const char*)&tmp, 4 );
    buf.close();

    //create the new fst
    QByteArray fstArray = QByteArray( 0x20, '\0' );
    fst = (FEntry*)( fstArray.data() );
    fst[ 0 ].Type = 1;
    fst[ 0 ].FileLength = qFromBigEndian( 0x00000001 );
    NameOff = 0x0C;

    data.append( fstArray );

    //point the fst pointer to the new data
    fst = (FEntry*)( data.data() + rootnode_offset );


    ok = true;
    return true;
}

bool U8::RenameEntry( const QString &path, const QString &newName )
{
    qDebug() << "U8::RenameEntry(" << path << "," << newName << ")";
    if( !ok )
    {
	qWarning() << "U8::RenameEntry -> archive has no data";
	return false;
    }
    if( newName.contains( "/" ) )
    {
	qWarning() << "U8::RenameEntry -> newName cannot contain \'/\'";
	return false;
    }
    //check if this is a path to a file in a nested archive
    QMap<QString, U8 >::iterator i = nestedU8s.begin();
    while( i != nestedU8s.constEnd() )
    {
	if( path.startsWith( i.key() ) && path != i.key() )
	{
	    QString subPath = path;
	    subPath.remove( 0, i.key().size() + 1 );//remove the path of the archive itself + the slash

	    bool ret = i.value().RenameEntry( subPath, newName );
	    if( !ret )
	    {
		qWarning() << "U8::RenameEntry -> error replacing data in child U8 archive";
		return false;
	    }
	    //qDebug() << "child entry updated, now replacing child archive in this one";
	    return ReplaceEntry( i.key(),i.value().GetData() );
	    //NOTE - after replacing the entry, "i" is no longer valid.  keep that in mind when changing this code.  its a bitch to track down this bug
	}
	i++;
    }
    //make sure the new filename doesnt already exist
    QString parentPath = path;
    while( parentPath.startsWith( "/") )//remove leading "/"s
	parentPath.remove( 0, 1 );
    while( parentPath.endsWith( "/") )//remove trailing "/"s
	parentPath.resize( parentPath.size() - 1 );

    int slash = parentPath.lastIndexOf( "/" );
    if( slash != -1 )
    {
	parentPath.chop( ( parentPath.size() - slash ) );
	parentPath += "/" + newName;
    }
    else
	parentPath = newName;

    if( FindEntry( parentPath ) != -1 )
    {
	qWarning() << "U8::RenameEntry ->" << parentPath << "already exists in the archive";
	return false;
    }

    //find the entry to rename
    int entryToRename = FindEntry( path );
    if( entryToRename == -1 )
    {
	qWarning() << "U8::RenameEntry" << path << "doesn\'t exists in the archive";
	qWarning() << "choices are" << paths;
	return false;
    }

    quint32 newNameLen = newName.size();
    quint32 oldNameLen = FstName( entryToRename ).size();
    int difference = newNameLen - oldNameLen;


    quint32 nFstSize = fstSize + difference;
    int dataAdjustment = 0;
    if( RU( U8_HEADER_ALIGNMENT, nFstSize ) < RU( U8_HEADER_ALIGNMENT, fstSize ) )
	dataAdjustment = - RU( U8_HEADER_ALIGNMENT, fstSize - nFstSize );

    else if( RU( U8_HEADER_ALIGNMENT, nFstSize ) > RU( U8_HEADER_ALIGNMENT, fstSize ) )
	dataAdjustment = RU( U8_HEADER_ALIGNMENT, nFstSize - fstSize );

    qDebug() << "old size:" << hex << oldNameLen\
	    << "new size:" << hex << newNameLen\
	    << "difference:" << hex << difference
	    << "dataAdjustment:" << hex << dataAdjustment;
    QByteArray nFstData( ( qFromBigEndian( fst[ 0 ].FileLength ) ) * 0xc, '\0' );
    FEntry *nfst = (FEntry*)( nFstData.data() );
    //make the new root entry
    nfst[ 0 ] = fst[ 0 ];



    //the new data payload ( just copy the existing one )
    QByteArray nPayload = data.right( data.size() - data_offset );
    QByteArray nNameTable;
    nNameTable.append( '\0' );//add a null byte for the root name

    quint32 cnt = qFromBigEndian( fst[ 0 ].FileLength );//the number of entries


    for( quint32 i = 1; i < cnt; i++ )
    {
	FEntry *e = &fst[ i ];//old entry
	FEntry *ne = &nfst[ i ];//new entry

	ne->NameOffset = swap24( nNameTable.size() );//offset to the new entry's name in the string table

	//add the name to the new name table
	if( i == (quint32)entryToRename )
	{
	    nNameTable.append( newName.toLatin1() );
	}
	else
	    nNameTable.append( FstName( e ).toLatin1() );

	nNameTable.append( '\0' );
	if( e->Type )//nothing special to change for directories except the name offset
	{
	    ne->Type = 1;
	    ne->ParentOffset = e->ParentOffset;
	    ne->NextOffset = e->NextOffset;
	}
	else//only need to change the name offset & the data offset for files
	{
	    ne->Type = 0;
	    ne->FileOffset = qFromBigEndian( qFromBigEndian( e->FileOffset ) + dataAdjustment );// + qFromBigEndian( dataAdjustment );
		    //qFromBigEndian( (quint32)( 0x20 + RU( U8_HEADER_ALIGNMENT, nFstSize ) + nPayload.size() ) );
	    ne->FileLength = e->FileLength;
	    qDebug() << "old offset" << hex << qFromBigEndian( e->FileOffset ) << "new offset" << hex << qFromBigEndian( e->FileOffset ) + dataAdjustment;
	}

    }
    //now put all the parts together to make a new U8 archive
    nFstData.append( nNameTable );

    data = QByteArray( 0x20, '\0' );
    QBuffer buf( &data );
    if( !buf.open( QBuffer::WriteOnly ) )
    {
	qWarning() << "U8::AddEntry -> Can't create buffer";
	return -1;
    }
    buf.putChar( 'U' );
    buf.putChar( '\xAA' );
    buf.putChar( '8' );
    buf.putChar( '\x2D' );

    rootnode_offset = 0x00000020;
    quint32 t = qFromBigEndian( rootnode_offset );
    buf.write( (const char*)&t, 4 );

    fstSize = nFstData.size();
    t = qFromBigEndian( fstSize );
    buf.write( (const char*)&t, 4 );

    data_offset = RU( U8_HEADER_ALIGNMENT, 0x20 + fstSize );
    t = qFromBigEndian( data_offset );
    buf.write( (const char*)&t, 4 );
    buf.close();

    data.append( nFstData );//add the fst
    int padding = data_offset - data.size();//pad to 'data_offset' after the fst
    if( padding )
    {
	data.append( QByteArray( padding, '\0' ) );
    }
    data.append( nPayload );//add the actual file data

    padding = RU( 0x20, data.size() ) - data.size();//pad the entire thing to 0x20 bytes TOTO: should probably already be done, and this step is not really necessary
    if( padding )
    {
	data.append( QByteArray( padding, '\0' ) );
    }



    CreateEntryList();

    //hexdump( data, 0, 0x100 );
    //hexdump( data );
    return true;
}

bool U8::ReplaceEntry( const QString &path, const QByteArray &nba, bool autoCompress )
{
    //qDebug() << "U8::ReplaceEntry(" << path << ")";
    if( !ok )
    {
	qWarning() << "U8::ReplaceEntry -> archive has no data";
	return false;
    }
    //check if this is a path to a file in a nested archive
    QMap<QString, U8 >::iterator i = nestedU8s.begin();
    while( i != nestedU8s.constEnd() )
    {
	if( path.startsWith( i.key() ) && path != i.key() )
	{
	    QString subPath = path;
	    subPath.remove( 0, i.key().size() + 1 );//remove the path of the archive itself + the slash
	    //qDebug() << "replacing file" << subPath << "in child U8" << i.key();

	    U8 ch = i.value();
	    QString chPath = i.key();
	    //qDebug() << "replacing" << subPath << "in child archive";
	    bool ret = ch.ReplaceEntry( subPath, nba, autoCompress );
	    if( !ret )
	    {
		qWarning() << "U8::ReplaceEntry -> error replacing data in child U8 archive";
		return false;
	    }
	    //qDebug() << "child entry updated, now replacing child archive in this one";
	    return ReplaceEntry( chPath, ch.GetData() );
	    //NOTE - after replacing the entry, "i" is no longer valid.  keep that in mind when changing this code.  its a bitch to track down this bug
	}
	i++;
    }
    //find the entry to replace
    int entryToReplace = FindEntry( path );
    if( entryToReplace == -1 )
    {
	qWarning() << "U8::ReplaceEntry" << path << "doesn\'t exists in the archive";
	qWarning() << "choices are" << paths;
	return false;
    }
    if( qFromBigEndian( fst[ entryToReplace ].Type ) )
    {
	qWarning() << "U8::ReplaceEntry -> replacing a directory is not supported" << path;
	return false;
    }


    /*qDebug() << "old size:" << hex << oldSizePadded\
	    << "new size:" << hex << newSizePadded\
	    << "difference:" << hex << difference;*/
    QByteArray newData = nba;
    if( autoCompress )
    {
	QByteArray oldData = data.mid( qFromBigEndian( fst[ entryToReplace ].FileOffset ), qFromBigEndian( fst[ entryToReplace ].FileLength ) );
	bool oldCompressed = ( LZ77::GetLz77Offset( oldData ) > -1 || IsAshCompressed( oldData ) );
	if( oldCompressed && LZ77::GetLz77Offset( newData ) == -1 && !IsAshCompressed( newData ) )
	{
	    newData = LZ77::Compress( newData );
	}
    }

    quint32 newSizePadded = RU( 0x20, newData.size() );
    quint32 oldSizePadded = RU( 0x20, qFromBigEndian( fst[ entryToReplace ].FileLength ) );
    int difference = newSizePadded - oldSizePadded;

    data.remove( qFromBigEndian( fst[ entryToReplace ].FileOffset ), oldSizePadded );
    QByteArray newPaddedData = newData;
    if( newSizePadded > (quint32)newPaddedData.size() )
    {
	newPaddedData.append( QByteArray( newSizePadded - newPaddedData.size(), '\0' ) );
    }
    data.insert( qFromBigEndian( fst[ entryToReplace ].FileOffset ), newPaddedData );

    if( newSizePadded == oldSizePadded )//the sized match, so nothing else to change
	return true;

    //point the fst to the new data
    fst = (FEntry*)( data.data() + rootnode_offset );

    //edit the changed size
    FEntry *e = &fst[ entryToReplace ];
    e->FileLength = qFromBigEndian( newData.size() );
    //hexdump( (const void*)e, sizeof( FEntry ) );

    //edit all the file offsets after the changed item
    quint32 cnt = qFromBigEndian( fst[ 0 ].FileLength );
    for( quint32 i = entryToReplace + 1; i < cnt; i++ )
    {
	FEntry *e = &fst[ i ];//old entry
	if( e->Type )//nothing changes for directories
	    continue;

	//qDebug() << "changed" << FstName( e ) << "offset from" << hex << qFromBigEndian( fst[ i ].FileOffset ) << "to" << qFromBigEndian( fst[ i ].FileOffset ) + difference;
	e->FileOffset = qFromBigEndian( qFromBigEndian( fst[ i ].FileOffset ) + difference );
    }
    CreateEntryList();

    //hexdump( data, 0, 0x100 );
    //hexdump( data );
    return true;

}

bool U8::RemoveEntry( const QString &path )
{
    //qDebug() << "U8::RemoveEntry(" << path << ")";
    if( !ok )
    {
	qWarning() << "U8::RemoveEntry -> archive has no data";
	return false;
    }
    //check if this is a path to a file in a nested archive
    QMap<QString, U8 >::iterator i = nestedU8s.begin();
    while( i != nestedU8s.constEnd() )
    {
	if( path.startsWith( i.key() ) && path != i.key() )
	{
	    QString subPath = path;
	    subPath.remove( 0, i.key().size() + 1 );//remove the path of the archive itself + the slash

	    U8 ch = i.value();
	    QString chPath = i.key();
	    //qDebug() << "deleting" << subPath << "in child archive";

	    bool ret = ch.RemoveEntry( subPath );
	    if( !ret )
	    {
		qWarning() << "U8::RemoveEntry -> error replacing data in child U8 archive";
		return false;
	    }
	    return ReplaceEntry( chPath, ch.GetData() );//insert the new nested archive in this one
	    //NOTE - after replacing the entry, "i" is no longer valid.  keep that in mind when changing this code.  its a bitch to track down this bug
	}
	i++;
    }
    //find the entry to delete
    int entryToDelete = FindEntry( path );
    if( entryToDelete == -1 )
    {
	qWarning() << "U8::RemoveEntry" << path << "doesn\'t exists in the archive";
	return false;
    }

    //create a list of all the directory indexes that my have their size changed
    QList<int> parents;
    int parent;
    quint32 numDeletedEntries;
    quint32 fstLenToDelete;
    if( fst[ entryToDelete ].Type )//deleting a directory
    {
	parent = qFromBigEndian( fst[ entryToDelete ].ParentOffset );
	while( parent )
	{
	    parents << parent;
	    parent = qFromBigEndian( fst[ parent ].ParentOffset );
	}
	numDeletedEntries = qFromBigEndian( fst[ entryToDelete ].NextOffset ) - entryToDelete;
	fstLenToDelete = 0;
	for( quint32 i = entryToDelete; i < qFromBigEndian( fst[ entryToDelete ].NextOffset ); i ++ )
	{
	    fstLenToDelete += FstName( i ).size() + 1 + 0xc;
	}

    }
    else//deleting a file
    {
	parent = entryToDelete - 1;
	while( parent )
	{
	    if( fst[ parent ].Type && qFromBigEndian( fst[ parent ].NextOffset ) > (quint32)entryToDelete )
	    {
		parents << parent;
		parent = qFromBigEndian( fst[ parent ].ParentOffset );
	    }
	    else
		parent--;
	}
	numDeletedEntries = 1;
	fstLenToDelete = FstName( entryToDelete ).size() + 1 + 0xc;

    }


    //qDebug() << "will delete" << numDeletedEntries << "entries";

    quint32 nFstSize = fstSize - fstLenToDelete;//old fst size - 0xc bytes for each deleted entry and the length of its name + 1 null byte
    //qDebug() << "nFstSize" << nFstSize;
    QByteArray nFstData( ( qFromBigEndian( fst[ 0 ].FileLength ) - numDeletedEntries ) * 0xc, '\0' );
    FEntry *nfst = (FEntry*)( nFstData.data() );

    //the new data payload ( all the files )
    QByteArray nPayload;
    QByteArray nNameTable;
    nNameTable.append( '\0' );//add a null byte for the root name

    quint32 cnt = qFromBigEndian( fst[ 0 ].FileLength );//the number of entries + 1 for the new one

    //make the new root entry
    nfst[ 0 ] = fst[ 0 ];
    nfst[ 0 ].FileLength = qFromBigEndian( cnt - numDeletedEntries );

    QList<int> movedDirs;//keep a list of the directories that are shifted back so their children can be shifted too
    for( quint32 i = 1; i < cnt; i++ )
    {
	if( i >= (quint32)entryToDelete && i < (quint32)( entryToDelete + numDeletedEntries ) )//skip deleted entries
	    continue;

	//copy old entries and adjust as needed
	quint8 adj = i < (quint32)entryToDelete ? 0 : numDeletedEntries;
	quint32 ni = i - adj;

	//qDebug() << "keeping" << FstName( i ) << "in the new archive ( moved from" << hex << i << "to" << hex << ni << ")";
	//if( parents.contains( i ) )
	    //qDebug() << "\tthis is a parent of the deleted item";

	FEntry *e = &fst[ i ];//old entry
	FEntry *ne = &nfst[ ni ];//new entry

	nfst[ ni ].NameOffset = swap24( nNameTable.size() );//offset to the new entry's name in the string table

	//add the name to the new name table
	nNameTable.append( FstName( e ).toLatin1() );
	nNameTable.append( '\0' );


	if( e->Type )//directory
	{
	    ne->Type = 1;
	    if( !adj )//directories before the new entry
	    {
		ne->ParentOffset = e->ParentOffset;
		ne->NextOffset = qFromBigEndian( qFromBigEndian( e->NextOffset ) \
						 - ( parents.contains( i ) ? numDeletedEntries : 0 ) );
	    }
	    else//directories after the new entry
	    {
		ne->ParentOffset = qFromBigEndian( qFromBigEndian( e->ParentOffset ) \
						   - ( movedDirs.contains( qFromBigEndian( e->ParentOffset ) ) ? numDeletedEntries : 0 ) );
		ne->NextOffset = qFromBigEndian( qFromBigEndian( e->NextOffset ) - numDeletedEntries );

		movedDirs << i;

		//qDebug() << "e.parent:" << hex << qFromBigEndian( e->ParentOffset ) << "movedDirs:" << movedDirs;
		//hexdump( (const void*)ne, sizeof( FEntry) );
	    }
	}
	else//file
	{
	    ne->Type = 0;
	    ne->FileOffset = \
		    qFromBigEndian( (quint32)( 0x20 + RU( U8_HEADER_ALIGNMENT, nFstSize ) + nPayload.size() ) );
	    ne->FileLength = e->FileLength;
	    nPayload.append( data.mid( qFromBigEndian( e->FileOffset ), qFromBigEndian( e->FileLength ) ) );
	    int padding = RU( 0x20, nPayload.size() ) - nPayload.size();//pad to 0x20 bytes between files
	    if( padding )
	    {
		nPayload.append( QByteArray( padding, '\0' ) );
	    }
	    //qDebug() << "writing fileOffset of" << hex << ni << hex << (quint32)( 0x20 + RU( U8_HEADER_ALIGNMENT, nFstSize ) + nPayload.size() );
	}
	//hexdump( (const void*)ne, sizeof( FEntry) );

    }
    //hexdump( nFstData );

    //now put all the parts together to make a new U8 archive
    nFstData.append( nNameTable );

    data = QByteArray( 0x20, '\0' );
    QBuffer buf( &data );
    if( !buf.open( QBuffer::WriteOnly ) )
    {
	qWarning() << "U8::AddEntry -> Can't create buffer";
	return -1;
    }
    buf.putChar( 'U' );
    buf.putChar( '\xAA' );
    buf.putChar( '8' );
    buf.putChar( '\x2D' );

    rootnode_offset = 0x00000020;
    quint32 t = qFromBigEndian( rootnode_offset );
    buf.write( (const char*)&t, 4 );

    fstSize = nFstData.size();
    t = qFromBigEndian( fstSize );
    buf.write( (const char*)&t, 4 );

    data_offset = RU( U8_HEADER_ALIGNMENT, 0x20 + fstSize );
    t = qFromBigEndian( data_offset );
    buf.write( (const char*)&t, 4 );
    buf.close();

    data.append( nFstData );//add the fst
    int padding = data_offset - data.size();//pad to 'data_offset' after the fst
    if( padding )
    {
	data.append( QByteArray( padding, '\0' ) );
    }
    data.append( nPayload );//add the actual file data

    padding = RU( 0x20, data.size() ) - data.size();//pad the entire thing to 0x20 bytes TOTO: should probably already be done, and this step is not really necessary
    if( padding )
    {
	data.append( QByteArray( padding, '\0' ) );
    }

    //make sure the pirvate variables are correct
    fst = (FEntry*)( data.data() + rootnode_offset );
    NameOff = 0x0C * qFromBigEndian( fst[ 0 ].FileLength );

    CreateEntryList();

    //hexdump( data );
    //qDebug() << "dataSize after removal:" << hex << data.size();

    return true;

}

int U8::AddEntry( const QString &path, int type, const QByteArray &newData )
{
    //qDebug() << "U8::AddEntry(" << path << "," << type << "," << hex << newData.size() << ")";
    //make sure there is actually data to manipulate
    if( !ok && !CreateEmptyData() )
    {
	return -1;
    }
    //check if this is a path to a file in a nested archive
    QMap<QString, U8 >::iterator i = nestedU8s.begin();
    while( i != nestedU8s.constEnd() )
    {
	if( path.startsWith( i.key() ) && path != i.key() )
	{
	    QString subPath = path;
	    //make a copy of the string, as "i" will become invalid on replace entry, and create a crash that is a bitch to track down
	    QString thisPath = i.key();
	    U8 chVal = i.value();
	    //qDebug() << "adding entry" << subPath << "in child U8" << thisPath;
	    subPath.remove( 0, thisPath.size() + 1 );//remove the path of the archive itself + the slash
	    bool ret = chVal.AddEntry( subPath, type, newData );
	    if( !ret )
	    {
		qWarning() << "U8::AddEntry -> error replacing data in child U8 archive";
		return false;
	    }

	    if( !ReplaceEntry( thisPath, chVal.GetData() ) )//insert the new nested archive in this one
		return -1;

	    //qDebug() << "done replacing the child entry in this archive, finding the index to return (" << thisPath << ")";
	    return FindEntry( thisPath );//just return the index of the nested archive
	}
	i++;
    }

    //make sure this path doesnt already exist
    if( FindEntry( path ) != -1 )
    {
	qWarning() << "U8::AddEntry" << path << "already exists in the archive";
	return -1;
    }
    //find the parent for this new entry
    QString parentPath = path;
    while( parentPath.startsWith( "/") )//remove leading "/"s
	parentPath.remove( 0, 1 );
    while( parentPath.endsWith( "/") )//remove trailing "/"s
	parentPath.resize( parentPath.size() - 1 );

    int parent = 0;
    int slash = parentPath.lastIndexOf( "/" );
    if( slash != -1 )
    {
	//parentPath.resize( ( parentPath.size() - slash ) - 2 );
	parentPath.chop( ( parentPath.size() - slash ) );
	parent = FindEntry( parentPath );
	if( parent == -1 )//need to create subfolders for this fucker
	{
	    parent = AddEntry( parentPath, 1 );
	    if( parent == -1 )
	    {
		qWarning() << "U8::AddEntry -> could not create subfolders for" << path;
		return -1;
	    }
	}
	if( !fst[ parent ].Type )//parent maps to a file, not folder
	{
	    qWarning() << "U8::AddEntry -> can't add child entry to a file";
	    return -1;
	}
    }
    //qDebug() << "adding entry to" << FstName( parent );

    //find the index to insert the new path
    int newEntry = qFromBigEndian( fst[ parent ].NextOffset );

    //create a list of all the directory indexes that my have their size changed to hold the new entry
    QList<int> parents;
    while( parentPath.contains( "/" ) )
    {
	parents << FindEntry( parentPath );
	int slash = parentPath.lastIndexOf( "/" );
	parentPath.chop( ( parentPath.size() - slash ) );
    }
    parents << FindEntry( parentPath );
    //qDebug() << "parents" << parents;

    //create the new data

    //the new fst
    QString name = path;//get the actual name of this entry
    int sl = name.lastIndexOf( "/" );
    if( sl != -1 )
    {
	name.remove( 0, sl + 1 );
    }

    quint32 nFstSize = fstSize + 0x0C + name.size() + 1;//old fst size + 0xc bytes for the new entry + room for the new name in the string table
    QByteArray nFstData( ( qFromBigEndian( fst[ 0 ].FileLength ) + 1 ) * 0xc, '\0' );
    FEntry *nfst = (FEntry*)( nFstData.data() );

    //the new data payload ( all the files )
    QByteArray nPayload;
    QByteArray nNameTable;
    nNameTable.append( '\0' );//add a null byte for the root name

    quint32 cnt = qFromBigEndian( fst[ 0 ].FileLength ) + 1;//the number of entries + 1 for the new one

    //make the new root entry
    nfst[ 0 ] = fst[ 0 ];
    nfst[ 0 ].FileLength = qFromBigEndian( cnt );

    QList<int> movedDirs;//keep a list of the directories that are shifted back so their children can be shifted too
    for( quint32 i = 1; i < cnt; i++ )
    {
	nfst[ i ].NameOffset = swap24( nNameTable.size() );//offset to the new entry's name in the string table
	if( i == (quint32)newEntry )//add the new entry
	{
	    FEntry *ne = &nfst[ i ];
	    if( type )
	    {
		ne->Type = 1;
		ne->ParentOffset = qFromBigEndian( parent );
		ne->NextOffset = qFromBigEndian( i + 1 );
	    }
	    else
	    {
		ne->Type = 0;
		ne->FileLength = qFromBigEndian( (quint32)newData.size() );
		ne->FileOffset =\
			qFromBigEndian( (quint32)( 0x20 + RU( U8_HEADER_ALIGNMENT, nFstSize ) + nPayload.size() ) );
		nPayload.append( newData );
		int padding = RU( 0x20, nPayload.size() ) - nPayload.size();//pad to 0x20 bytes between files
		if( padding )
		{
		    nPayload.append( QByteArray( padding, '\0' ) );
		}
	    }
	    nNameTable.append( name.toLatin1() );//add this entry's name to the table
	    nNameTable.append( '\0' );
	    continue;
	}

	//copy old entries and adjust as needed
	quint8 adj = i < (quint32)newEntry ? 0 : 1;
	quint32 ni = i - adj;

	FEntry *e = &fst[ ni ];//old entry
	FEntry *ne = &nfst[ i ];//new entry

	//add the name to the new name table
	nNameTable.append( FstName( e ).toLatin1() );
	nNameTable.append( '\0' );
	if( e->Type )
	{
	    ne->Type = 1;
	    if( !adj )//directories before the new entry
	    {
		ne->ParentOffset = e->ParentOffset;
		ne->NextOffset = qFromBigEndian( qFromBigEndian( e->NextOffset ) \
						 + ( parents.contains( i ) ? 1 : 0 ) );
	    }
	    else//directories after the new entry
	    {
		movedDirs << ni;
		ne->ParentOffset = qFromBigEndian( qFromBigEndian( e->ParentOffset ) \
						   + ( movedDirs.contains( qFromBigEndian( e->ParentOffset ) ) ? 1 : 0 ) );
		ne->NextOffset = qFromBigEndian( qFromBigEndian( e->NextOffset ) + 1 );
	    }
	}
	else//all file entries
	{
	    ne->Type = 0;
	    ne->FileOffset = \
		    qFromBigEndian( (quint32)( 0x20 + RU( U8_HEADER_ALIGNMENT, nFstSize ) + nPayload.size() ) );
	    ne->FileLength = e->FileLength;
	    nPayload.append( data.mid( qFromBigEndian( e->FileOffset ), qFromBigEndian( e->FileLength ) ) );
	    int padding = RU( 0x20, nPayload.size() ) - nPayload.size();//pad to 0x20 bytes between files
	    if( padding )
	    {
		nPayload.append( QByteArray( padding, '\0' ) );
	    }
	}

    }
    //now put all the parts together to make a new U8 archive
    nFstData.append( nNameTable );

    data = QByteArray( 0x20, '\0' );
    QBuffer buf( &data );
    if( !buf.open( QBuffer::WriteOnly ) )
    {
	qWarning() << "U8::AddEntry -> Can't create buffer";
	return -1;
    }
    buf.putChar( 'U' );
    buf.putChar( '\xAA' );
    buf.putChar( '8' );
    buf.putChar( '\x2D' );

    rootnode_offset = 0x00000020;
    quint32 t = qFromBigEndian( rootnode_offset );
    buf.write( (const char*)&t, 4 );

    fstSize = nFstData.size();
    t = qFromBigEndian( fstSize );
    buf.write( (const char*)&t, 4 );

    data_offset = RU( U8_HEADER_ALIGNMENT, 0x20 + fstSize );
    t = qFromBigEndian( data_offset );
    buf.write( (const char*)&t, 4 );
    buf.close();

    data.append( nFstData );//add the fst
    int padding = data_offset - data.size();//pad to 'data_offset' after the fst
    if( padding )
    {
	data.append( QByteArray( padding, '\0' ) );
    }
    data.append( nPayload );//add the actual file data

    padding = RU( 0x20, data.size() ) - data.size();//pad the entire thing to 0x20 bytes TOTO: should probably already be done, and this step is not really necessary
    if( padding )
    {
	data.append( QByteArray( padding, '\0' ) );
    }

    //make sure the pirvate variables are correct
    fst = (FEntry*)( data.data() + rootnode_offset );
    NameOff = 0x0C * qFromBigEndian( fst[ 0 ].FileLength );
    //qDebug() << "done adding the entry";

    CreateEntryList();

    //hexdump( data );
    return newEntry;
}

void U8::CreateEntryList()
{
    //qDebug() << "U8::CreateEntryList";
    paths.clear();
    nestedU8s.clear();
    fst = (FEntry*)( data.data() + rootnode_offset );
    quint32 cnt = qFromBigEndian( fst[ 0 ].FileLength );
    if( ( cnt * 0x0d ) + rootnode_offset > (quint32)data.size() )
    {
	qWarning() << "U8::CreateEntryList -> invalid archive";
	ok = false;
	data.clear();
	paths.clear();
	fst = NULL;
	return;
    }
    NameOff = cnt * 0x0C;
    bool fixWarn = false;//ony print the warning 1 time

    //qDebug() << "cnt" << hex << cnt;
    for( quint32 i = 1; i < cnt; ++i )//this is not the most effecient way to do things, but it seems to work ok and these archives are small enough that it happens fast anyways
    {
	//start at the beginning of the fst and enter every directory whos "nextoffset" is greater than this index,
	//adding the names along the way and hopefully finding "i"
	QString path;
	quint32 current = 1;
	quint32 folder = 0;
	while( 1 )
	{
	    if( !current )
	    {
		qWarning() << "U8::CreateEntryList -> error parsing the archive";
		break;
	    }
	    if( current == i )
	    {
		path += FstName( i );
		if( fst[ current ].Type )
		{
		    //sanity check:  make sure the parent offset is correct
		    if( folder != qFromBigEndian( fst[ current ].ParentOffset ) )
		    {
			qWarning() << "U8::CreateEntryList -> error parsing the archive - recursion mismatch in"
				<< path << "expected:" << hex << folder << "got:" << hex << qFromBigEndian( fst[ current ].ParentOffset )\
				<< "(" << FstName( qFromBigEndian( fst[ current ].ParentOffset ) ) << ")";

			//some tools use "recursion" instead of "parent offset".
			//it just coincidentally works for archives such as the opening.bnr and .app since they dont have many folders to mess up
			if( qFromBigEndian( fst[ current ].ParentOffset ) == (quint32)path.count( "/" ) )
			{
			    if( !fixWarn )
			    {
				qWarning() << "This archive was made by a broken tool such as U8Mii.  I'm trying to fix it, but I can't make any promises";
				fixWarn = true;
			    }
			    fst[ current ].ParentOffset = qFromBigEndian( folder );
			}
		    }
		    path += "/";
		    paths << path;
		}
		else
		{
		    //add this file to the list of entries
		    paths << path;

		    //check if this file is really a U8 archive, and add it to the list of children
		    QByteArray chData = data.mid( qFromBigEndian( fst[ current ].FileOffset ), qFromBigEndian( fst[ current ].FileLength ) );
		    if( IsU8( chData ) )
		    {
			U8 child( chData );
			if( child.IsOK() )
			{
			    nestedU8s.insert( path, child );
			    foreach( QString chPath, child.Entries() )
			    {
				QString newPath = path + "/" + chPath;
				paths << newPath;
			    }
			}
		    }
		}
		break;
	    }
	    if( fst[ current ].Type && i < qFromBigEndian( fst[ current ].NextOffset ) )
	    {
		path += FstName( current ) + "/";
		folder = current;
		current++;
		continue;

	    }
	    current = NextEntryInFolder( current, folder );
	}
	//paths << path;
    }
    //qDebug() << "done";
    //qDebug() << "paths" << paths;
}

U8::U8( const QByteArray &ba )
{
    //qDebug() << "U8::U8 dataSize:" << hex << ba.size();
    Load( ba );
}

void U8::Load( const QByteArray &ba )
{
    ok = false;
    isLz77 = false;
    headerType = U8_Hdr_none;
    paths.clear();
    imetNames.clear();
    /*if( ba.size() < 0x80 )
    {
	//qWarning() << "U8::Load:" << hex << ba.size();
	//qWarning() << "U8::Load -> where is the rest of the data?";
	return;
    }*/
    data = ba;
    if( IsAshCompressed( data ) )//decrypt ASH0 files
	data = DecryptAsh( data );

    quint32 tmp;
    int off = LZ77::GetLz77Offset( data );
    int off2 = GetU8Offset( data );
    if( off != -1 && ( off2 == -1 || ( off2 != -1 && off < off2 ) ) )
    {
	isLz77 = true;
	data = LZ77::Decompress( data );
	off2 = GetU8Offset( data );
    }

    if( off2 == -1 )
    {
	qDebug() << "This isnt a U8";
	return;
    }

    data = data.mid( off2 );

    QBuffer buf( &data );
    if( !buf.open( QBuffer::ReadOnly ) )
    {
	qWarning() << "Can't create buffer";
	return;
    }

    buf.seek( 4 );
    buf.read( ( char*)&tmp, 4 );
    rootnode_offset = qFromBigEndian( tmp );
    if( rootnode_offset != 0x20 )
    {
	qWarning() << "rootnodeOffset" << hex << rootnode_offset;
	qWarning() << hex << data.size();
	hexdump( data );
    }

    buf.read( ( char*)&tmp, 4 );
    fstSize = qFromBigEndian( tmp );

    buf.read( ( char*)&tmp, 4 );
    data_offset = qFromBigEndian( tmp );

    buf.close();

    CreateEntryList();


    ReadHeader( ba );
    ok = true;
    //qDebug() << "loaded" << paths;
}

bool U8::IsOK()
{
    return ok;
}

QString U8::FstName( const FEntry *entry )
{
    //qDebug() << "U8::FstName";
    if( entry == &fst[ 0 ] )
	return QString();

    int nameStart = swap24( entry->NameOffset ) + NameOff + rootnode_offset;
    return data.mid( nameStart, nameStart + 0x100 );
}

QString U8::FstName( quint32 i )
{
    if( i > fst[ 0 ].FileLength )
    {
	qWarning() << "U8::FstName -> index is out of range";
	return QString();
    }
    return FstName( &fst[ i ] );
}

quint32 U8::NextEntryInFolder( quint32 current, quint32 directory )
{
    //qDebug() << "U8::NextEntryInFolder(" << hex << current << "," << hex << directory << ")";
    quint32 next = ( fst[ current ].Type ? qFromBigEndian( fst[ current ].FileLength ) : current + 1 );
    //qDebug() << "next" << next << "len" << hex << qFromBigEndian( fst[ directory ].FileLength );
    if( next < qFromBigEndian( fst[ directory ].FileLength ) )
	return next;

    return 0;
}

int U8::FindEntry( const QString &str, int d )
{
    //qDebug() << "U8::FindEntry(" << str << "," << d << ")";
    if( str.isEmpty() )
	return 0;

    if( str.startsWith( "/" ) )
    {
	QString r = str;
	r.remove( 0, 1 );
	return FindEntry( r, d );
    }

    if( !fst[ d ].Type )
    {
	qDebug() << "ERROR!!" << FstName( &fst[ d ] ) << "is not a directory";
	return -1;
    }

    int next = d + 1;
    FEntry *entry = &fst[ next ];

    while( next )
    {
	QString item;
	int sl = str.indexOf( "/" );
	if( sl > -1 )
	    item = str.left( sl );
	else
	    item = str;

	if( FstName( entry ) == item )
	{
	    if( item == str || item + "/" == str )//this is the item we are looking for
	    {
		return next;
	    }
	    //this item is a parent folder of one we are looking for
	    return FindEntry( str.right( ( str.size() - sl ) - 1 ), next );
	}
	//find the next entry in this folder
	next = NextEntryInFolder( next, d );
	entry = &fst[ next ];
    }

    //no entry with the given path was found
    return -1;
}

const QByteArray U8::GetData( const QString &str, bool onlyPayload )
{
    //qDebug() << "U8::GetData(" << str << ")";
    if( str.isEmpty() )//give the data for this whole archive
    {
	if( onlyPayload )
	    return data;

	//qDebug() << "U8::GetData -> returning all the data for this archive.  headerType:" << headerType;

	QByteArray ret = data;

	switch( headerType )
	{
	case U8_Hdr_IMET_app:
	case U8_Hdr_IMET_bnr:
	    ret = AddIMET( headerType );//data is lz77 compressed in this function if it needs to be
	    break;
	case U8_Hdr_IMD5:
	    {
		if( isLz77 )
		    ret = LZ77::Compress( ret );

		ret = AddIMD5( ret );

		//hexdump( ret, 0, 0x40 );
	    }
	    break;
	default:
	    break;
	}
	return ret;
    }
    //check if this is a path to a file in a nested archive
    QMap<QString, U8 >::iterator i = nestedU8s.begin();
    while( i != nestedU8s.constEnd() )
    {
	if( str.startsWith( i.key() ) && str != i.key() )
	{
	    QString subPath = str;
	    subPath.remove( 0, i.key().size() + 1 );//remove the path of the archive itself + the slash
	    return i.value().GetData( subPath, onlyPayload );
	}
	i++;
    }
    int index = FindEntry( str );
    if( index < 0 )
    {
	qWarning() << "U8::GetData" << str << "was not found in the archive";
	return QByteArray();
    }
    if( fst[ index ].Type )
    {
	qWarning() << "U8::GetData" << str << "is a directory";
	return QByteArray();
    }
    QByteArray ret = data.mid( qFromBigEndian( fst[ index ].FileOffset ), qFromBigEndian( fst[ index ].FileLength ) );
    //hexdump( ret, 0, 0x40 );
    if( onlyPayload )
    {
	if( LZ77::GetLz77Offset( ret ) != -1 )
	    ret = LZ77::Decompress( ret );

	else if( IsAshCompressed( ret ) )
	    ret = DecryptAsh( ret );
    }
    return ret;
}

quint32 U8::GetSize( const QString &str )
{
    if( str.isEmpty() )
    {
	return data.size();
    }
    int index = FindEntry( str );
    if( index < 0 )
    {
	qWarning() << "U8::GetSize" << str << "was not found in the archive";
	return 0;
    }
    if( fst[ index ].Type )
    {
	qWarning() << "U8::GetSize" << str << "is a directory";
	return 0;
    }
    return  qFromBigEndian( fst[ index ].FileLength );
}

const QStringList U8::Entries()
{
    return paths;
}

int U8::GetU8Offset( const QByteArray &ba )
{
    QByteArray start = ba.left( 5000 );
    return start.indexOf( "U\xAA\x38\x2d" );
}

bool U8::IsU8( const QByteArray &ba )
{
    QByteArray data = ba;
    if( IsAshCompressed( data ) )//decrypt ASH0 files
	data = DecryptAsh( data );

    int off = LZ77::GetLz77Offset( data );//decrypt LZ77
    if( off != -1 )
	data = LZ77::Decompress( data );

    QByteArray start = data.left( 5000 );
    return start.indexOf( "U\xAA\x38\x2d" ) != -1;
}

#define IMET_MAX_NAME_LEN 0x2a
typedef struct
{
	quint32 sig; // "IMET"
	quint32 unk1;
	quint32 unk2;
	quint32 filesizes[ 3 ];
	quint32 unk3;
	quint16 names[ 10 ][ IMET_MAX_NAME_LEN ];
	quint8 zeroes2[ 0x24c ];
	quint8 md5[ 0x10 ];
} IMET;

void U8::ReadHeader( const QByteArray &ba )
{
    //qDebug() << "U8::ReadHeader(" << hex << ba.size() << ")";
    //hexdump( ba );
    headerType = U8_Hdr_none;
    imetNames.clear();
    if( ba.startsWith( "IMD5" ) )//dont bother to read any more data
    {
	//qDebug() << "IMD5 header";
	headerType = U8_Hdr_IMD5;
	return;
    }

    QByteArray start = ba.left( sizeof( IMET ) + 0x80 );
    QBuffer buf( &start );
    if( !buf.open( QBuffer::ReadOnly ) )
    {
	qWarning() << "U8::ReadHeader Can't create buffer";
	return;
    }
    int off = start.indexOf( "IMET" );
    //qDebug() << "imet offset" << hex << off << "u8 offset" << hex << GetU8Offset( ba );
    if( off == 0x40 || off == 0x80 )//read imet header
    {
	if( off > GetU8Offset( ba ) )//in case somebody wants to put a IMET archive inside another U8 for whatever reason
	    return;

	if( off == 0x40 )
	    headerType = U8_Hdr_IMET_bnr;
	else
	    headerType = U8_Hdr_IMET_app;

	buf.seek( off );
	IMET imet;
	buf.read( (char*)&imet, sizeof( IMET ) );
	buf.close();

	for( int h = 0; h < 10; h ++ )
	{
	    QString name;
	    for( int i = 0; i < IMET_MAX_NAME_LEN && imet.names[ h ][ i ] != 0; i++ )//no need to switch endian, 0x0000 is a palendrome
		name += QChar( qFromBigEndian( imet.names[ h ][ i ] ) );

	    imetNames << name;
	}
	//done
	return;
    }
}
QByteArray U8::AddIMD5( QByteArray ba )
{
    quint32 size = ba.size();

    MD5 hash;
    hash.update( ba.data(), size );
    hash.finalize();

    size = qFromBigEndian( size );

    QByteArray imd5;
    QBuffer buf( &imd5 );
    if( !buf.open( QBuffer::WriteOnly ) )
    {
	qWarning() << "U8::AddIMD5 Can't create buffer";
	return QByteArray();
    }
    buf.putChar( 'I' );
    buf.putChar( 'M' );
    buf.putChar( 'D' );
    buf.putChar( '5' );

    buf.write( (const char*)&size, 4 );
    buf.seek( 0x10 );

    buf.write( (const char*)hash.hexdigestChar(), 16 );
    buf.close();
    //qDebug() << hash.hexdigest().c_str();
    //hexdump( (void*)imd5.data(), 0x20 );

    ba.prepend( imd5 );

    return ba;
}

const QByteArray U8::AddIMD5()
{
    QByteArray ba = data;
    ba = AddIMD5( ba );
    return ba;
}

QByteArray U8::GetIMET( const QStringList &names, int paddingType, quint32 iconSize, quint32 bannerSize, quint32 soundSize )
{
    QByteArray ret = QByteArray( sizeof( IMET ) + 0x40, '\0' );
    QBuffer buf( &ret );
    if( !buf.open( QBuffer::WriteOnly ) )
    {
	qWarning() << "U8::GetIMET Can't create buffer";
	return QByteArray();
    }
    buf.seek( 0 + 0x40 );
    buf.putChar( 'I' );
    buf.putChar( 'M' );
    buf.putChar( 'E' );
    buf.putChar( 'T' );
    buf.seek( 6 + 0x40 );
    buf.putChar( '\x6' );
    buf.seek( 11 + 0x40 );
    buf.putChar( '\x3' );

    quint32 tmp = qFromBigEndian( iconSize );
    buf.write( (char*)&tmp, 4 );
    tmp = qFromBigEndian( bannerSize );
    buf.write( (char*)&tmp, 4 );
    tmp = qFromBigEndian( soundSize );
    buf.write( (char*)&tmp, 4 );

    int nameOffset = 0x1c + 0x40;
    int numNames = names.size();
    for( int i = 0; i < numNames; i++ )
    {
	QString name = names.at( i );
	int nameLen = name.size();
	buf.seek( nameOffset + ( IMET_MAX_NAME_LEN * i * 2 ) );
	for( int j = 0; j < nameLen; j++ )
	{
	    quint16 letter = qFromBigEndian( name.at( j ).unicode() );
	    buf.write( ( const char*)&letter, 2 );
	}
    }

    MD5 hash;
    hash.update( ret.data(), sizeof( IMET ) + 0x40 );
    hash.finalize();

    buf.seek( 0x5f0 );
    buf.write( (const char*)hash.hexdigestChar(), 16 );
    buf.close();

    switch( paddingType )
    {
    case U8_Hdr_IMET_app:
	ret.prepend( QByteArray( 0x40, '\0' ) );
	break;
    case U8_Hdr_IMET_bnr:
	break;
    default:
	ret.remove( 0, 0x40 );
	break;
    }
    return ret;
}

const QStringList U8::IMETNames()
{
    return imetNames;
}

void U8::SetImetNames( const QStringList &names )
{
    imetNames.clear();
    imetNames = names;
}

const QByteArray U8::AddIMET( int paddingType )
{
    QByteArray ret = GetData( "meta/icon.bin" );

    quint32 iconSize;
    quint32 bannerSize;
    quint32 soundSize;

    //if these are lz77 compressed, write the uncompressed size to the header  ?bannerbomb?
    //otherwise write the size - 0x20 ( imd5 header size )

    if( LZ77::GetLz77Offset( ret ) != -1 )
	iconSize = LZ77::GetDecompressedSize( ret );
    else
	iconSize = ret.size() - 0x20;

    ret = GetData( "meta/banner.bin" );
    if( LZ77::GetLz77Offset( ret ) != -1 )
	bannerSize = LZ77::GetDecompressedSize( ret );
    else
	bannerSize = ret.size() - 0x20;

    ret = GetData( "meta/sound.bin" );
    if( LZ77::GetLz77Offset( ret ) != -1 )
	soundSize = LZ77::GetDecompressedSize( ret );
    else
	soundSize = ret.size() - 0x20;

    ret = GetIMET( imetNames, paddingType, iconSize, bannerSize, soundSize );
    if( isLz77 )//really?  can the entire banner be lz77 compressed?
	ret += LZ77::Compress( data );
    else
	ret += data;

    return ret;
}

int U8::GetHeaderType()
{
    return headerType;
}
