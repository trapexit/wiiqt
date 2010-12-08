#include "settingtxtdialog.h"
#include "ui_settingtxtdialog.h"
#include "tools.h"

SettingTxtDialog::SettingTxtDialog( QWidget *parent, const QByteArray &old ) : QDialog(parent), ui(new Ui::SettingTxtDialog)
{
    ui->setupUi( this );
    if( !old.isEmpty() )
    {
	QByteArray copy = old;
	copy = LolCrypt( copy );

	QString str( copy );
	str.replace( "\r\n", "\n" );//maybe not needed to do this in 2 steps, but there may be some reason the file only uses "\n", so do it this way to be safe
	QStringList parts = str.split( "\n", QString::SkipEmptyParts );
	foreach( QString part, parts )
	{
	    QString p = part;
	    if( part.startsWith( "AREA=" ) )
	    {
		p.remove( 0, 5 );
		ui->lineEdit_area->setText( p );
	    }
	    else if( part.startsWith( "MODEL=" ) )
	    {
		p.remove( 0, 6 );
		ui->lineEdit_model->setText( p );
	    }
	    else if( part.startsWith( "DVD=" ) )
	    {
		p.remove( 0, 4 );
		ui->lineEdit_dvd->setText( p );
	    }
	    else if( part.startsWith( "MPCH=" ) )
	    {
		p.remove( 0, 5 );
		ui->lineEdit_mpch->setText( p );
	    }
	    else if( part.startsWith( "CODE=" ) )
	    {
		p.remove( 0, 5 );
		ui->lineEdit_code->setText( p );
	    }
	    else if( part.startsWith( "SERNO=" ) )
	    {
		p.remove( 0, 6 );
		ui->lineEdit_serno->setText( p );
	    }
	    else if( part.startsWith( "VIDEO=" ) )
	    {
		p.remove( 0, 6 );
		ui->lineEdit_video->setText( p );
	    }
	    else if( part.startsWith( "GAME=" ) )
	    {
		p.remove( 0, 5 );
		ui->lineEdit_game->setText( p );
	    }
	    else
	    {
		qDebug() << "SettingTxtDialog::SettingTxtDialog -> unhandled shit" << p;
	    }
	}
    }
}

SettingTxtDialog::~SettingTxtDialog()
{
    delete ui;
}

//ok button clicked
void SettingTxtDialog::on_buttonBox_accepted()
{
    QString s = "AREA=" + ui->lineEdit_area->text() + "\r\n" +
		"MODEL=" + ui->lineEdit_model->text() + "\r\n" +
		"DVD=" + ui->lineEdit_dvd->text() + "\r\n" +
		"MPCH=" + ui->lineEdit_mpch->text() + "\r\n" +
		"CODE=" + ui->lineEdit_code->text() + "\r\n" +
		"SERNO=" + ui->lineEdit_serno->text() + "\r\n" +
		"VIDEO=" + ui->lineEdit_video->text() + "\r\n" +
		"GAME=" + ui->lineEdit_game->text();

    ret = s.toAscii();
    ret = PaddedByteArray( ret, 0x100 );
    //hexdump( ret );
    ret = LolCrypt( ret );
    //hexdump( ret );
}

QByteArray SettingTxtDialog::LolCrypt( QByteArray ba )
{
    int s;
    for( s = ba.size() - 1; s > 0; s-- )
	if( ba.at( s ) != '\0' )
	    break;

    QByteArray ret = ba;
    quint32 key = 0x73b5dbfa;
    quint8 * stuff = (quint8 *)ret.data();
    for( int i = 0; i < s + 1; i++ )
    {
	*stuff ^= ( key & 0xff );
	stuff++;
	key = ( ( key << 1 ) | ( key >> 31 ) );
    }
    return ret;
}

QByteArray SettingTxtDialog::Edit( QWidget *parent, const QByteArray &old )
{
    SettingTxtDialog d( parent, old );
    if( d.exec() )
	return d.ret;
    return QByteArray();
}
