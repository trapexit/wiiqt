#include "sharedcontentmap.h"
#include "tools.h"

SharedContentMap::SharedContentMap( const QByteArray &old )
{
    data = old;
    if( data.size() )
	Check();
}

bool SharedContentMap::Check( const QString &path )
{
    if( !data.size() || data.size() % 28 )
    {
	qWarning() << "SharedContentMap::Check -> bad size" << hex << data.size();
	return false;
    }

    QByteArray cid;
    quint32 cnt = data.size() / 28;
    //check that all the cid's can be converted to numbers, and make sure that ther are in numerical order
    for( quint32 i = 0; i < cnt; i++ )
    {
	bool ok;
	cid = data.mid( ( i * 28 ), 8 );
	quint32 num = cid.toInt( &ok, 16 );
	if( !ok )
	{
	    qWarning() << "SharedContentMap::Check -> error converting" << cid << "to a number";
	    return false;
	}
	if( i != num )
	{
	    qWarning() << "SharedContentMap::Check -> something is a miss" << num << i;
	    return false;
	}
    }
    //this is all there is to check right now
    if( path.isEmpty() )
	return true;

    QDir dir( path );
    if( !dir.exists() )
    {
	qWarning() << "SharedContentMap::Check ->" << path << "doesnt exist";
	return false;
    }

    //check hashes for all the contents in the map
    for( quint32 i = 0; i < cnt; i++ )
    {
	cid = data.mid( ( i * 28 ), 8 );
	QString appName( cid );
	QFileInfo fi = dir.absoluteFilePath( appName + ".app" );
	if( !fi.exists() )
	{
	    qWarning() << "SharedContentMap::Check -> content in the map isnt found in" << path;
	    return false;
	}
	QFile f( fi.absoluteFilePath() );
	if( !f.open( QIODevice::ReadOnly ) )
	{
	    qWarning() << "SharedContentMap::Check -> cant open" << fi.absoluteFilePath();
	    return false;
	}

	QByteArray goods = f.readAll();
	f.close();

	QByteArray expectedHash = data.mid( ( i * 28 ) + 8, 20 );
	QByteArray actualHash = GetSha1( goods );
	if( expectedHash != actualHash )
	{
	    qWarning() << "SharedContentMap::Check -> hash mismatch in" << fi.absoluteFilePath() << "\n"
		    << expectedHash << actualHash;
	    return false;
	}
    }
    return true;
}

QString SharedContentMap::GetAppFromHash( const QByteArray &hash )
{
    quint32 cnt = data.size() / 28;
    for( quint32 i = 0; i < cnt; i++ )
    {
	QString appName( data.mid( ( i * 28 ), 8 ) );
	QByteArray mapHash = data.mid( ( i * 28 ) + 8, 20 );
	//qDebug() << QString( mapHash ) << hash;
	if( mapHash == hash )
	{
	    return appName;
	}
    }
    //hash not found
    return QString();
}

QString SharedContentMap::GetNextEmptyCid()
{
    quint32 cnt = data.size() / 28;
    quint32 ret = 0;
    QList<quint32>cids;
    for( quint32 i = 0; i < cnt; i++ )
    {
	QByteArray cid = data.mid( ( i * 28 ), 8 );
	quint32 num = cid.toInt( NULL, 16 );
	cids << num;
    }
    //find the lowest number cid that isnt used
    while( cids.contains( ret ) )
	ret++;
    //qDebug() << hex << ret;
    return QString( "%1" ).arg( ret, 8, 16, QChar( '0' ) );
}

void SharedContentMap::AddEntry( const QString &app, const QByteArray &hash )
{
    data += app.toAscii() + hash;
}

const QByteArray SharedContentMap::Hash( quint16 i )
{
    if( Count() < i )
	return QByteArray();
    return data.mid( ( i * 28 ) + 8, 20 );
}

const QString SharedContentMap::Cid( quint16 i )
{
    if( Count() < i )
	return QByteArray();
    return QString( data.mid( ( i * 28 ), 8 ) );
}

quint16 SharedContentMap::Count()
{
    return ( data.size() / 28 );
}
