#include "ngdialog.h"
#include "ui_ngdialog.h"
#include "../WiiQt/keysbin.h"
#include "../WiiQt/savedatabin.h"

NgDialog::NgDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NgDialog)
{
    ui->setupUi(this);
    hex.setPattern( "^[A-Fa-f0-9]+$" );
    ngID = 0;
    ngKeyID = 0;

    QFontMetrics fm( fontMetrics() );
    int max = fm.width( ui->label_ngid->text() );
    max = MAX( max, fm.width( ui->label_ngKeyId->text() ) );
    max = MAX( max, fm.width( ui->label_ngMac->text() ) );
    max = MAX( max, fm.width( ui->label_ngPriv->text() ) );
    max = MAX( max, fm.width( ui->label_ngSig->text() ) );

    max += 5;
    ui->label_ngid->setMinimumWidth( max );
    ui->label_ngKeyId->setMinimumWidth( max );
    ui->label_ngMac->setMinimumWidth( max );
    ui->label_ngPriv->setMinimumWidth( max );
    ui->label_ngSig->setMinimumWidth( max );
}

NgDialog::~NgDialog()
{
    delete ui;
}

int NgDialog::exec()
{
    ui->lineEdit_ngID->setText( QString( "%1" ).arg( ngID, 8, 16, QChar( '0' ) ) );
    ui->lineEdit_ngKeyId->setText( QString( "%1" ).arg( ngKeyID, 8, 16, QChar( '0' ) ) );
    ui->lineEdit_ngSig->setText( ngSig.toHex() );
    ui->lineEdit_ngMac->setText( ngMac.toHex() );
    ui->lineEdit_ngPriv->setText( ngPriv.toHex() );

    return QDialog::exec();
}

bool NgDialog::ValidNGID()
{
    bool ok = false;
    ngID = ui->lineEdit_ngID->text().toInt( &ok, 16 );
    return ok;
}

bool NgDialog::ValidNGKeyID()
{
    bool ok = false;
    ngKeyID = ui->lineEdit_ngKeyId->text().toInt( &ok, 16 );
    return ok;
}

bool NgDialog::ValidNGSig()
{
    if( ui->lineEdit_ngSig->text().size() != 120 )
        return false;
    return hex.exactMatch( ui->lineEdit_ngSig->text() );
}

bool NgDialog::ValidNGMac()
{
    if( ui->lineEdit_ngMac->text().size() != 12 )
        return false;
    return hex.exactMatch( ui->lineEdit_ngMac->text() );
}

bool NgDialog::ValidNGPriv()
{
    if( ui->lineEdit_ngPriv->text().size() != 60 )
        return false;
    return hex.exactMatch( ui->lineEdit_ngPriv->text() );
}

void NgDialog::on_lineEdit_ngID_textChanged( QString str )
{
    if( str.contains( " " ) )//remove spaces ( useful for copy/pasting out of a hexeditor )
    {
        str.remove( " " );
        ui->lineEdit_ngID->setText( str );
        return;
    }
    QString color = ValidNGID() ? "green" : "red";
    ui->lineEdit_ngID->setStyleSheet( "QLineEdit { background-color: " + color + "; }" );
}

void NgDialog::on_lineEdit_ngSig_textChanged( QString str )
{
    if( str.contains( " " ) )//remove spaces ( useful for copy/pasting out of a hexeditor )
    {
        str.remove( " " );
        ui->lineEdit_ngSig->setText( str );
        return;
    }
    QString color = ValidNGSig() ? "green" : "red";
    ui->lineEdit_ngSig->setStyleSheet( "QLineEdit { background-color: " + color + "; }" );
}

void NgDialog::on_lineEdit_ngKeyId_textChanged( QString str )
{
    if( str.contains( " " ) )//remove spaces ( useful for copy/pasting out of a hexeditor )
    {
        str.remove( " " );
        ui->lineEdit_ngKeyId->setText( str );
        return;
    }
    QString color = ValidNGKeyID() ? "green" : "red";
    ui->lineEdit_ngKeyId->setStyleSheet( "QLineEdit { background-color: " + color + "; }" );
}

void NgDialog::on_lineEdit_ngMac_textChanged( QString str )
{
    if( str.contains( " " ) )//remove spaces ( useful for copy/pasting out of a hexeditor )
    {
        str.remove( " " );
        ui->lineEdit_ngMac->setText( str );
        return;
    }
    QString color = ValidNGMac() ? "green" : "red";
    ui->lineEdit_ngMac->setStyleSheet( "QLineEdit { background-color: " + color + "; }" );
}

void NgDialog::on_lineEdit_ngPriv_textChanged( QString str )
{
    if( str.contains( " " ) )//remove spaces ( useful for copy/pasting out of a hexeditor )
    {
        str.remove( " " );
        ui->lineEdit_ngPriv->setText( str );
        return;
    }
    QString color = ValidNGPriv() ? "green" : "red";
    ui->lineEdit_ngPriv->setStyleSheet( "QLineEdit { background-color: " + color + "; }" );
}

//open keys.bin to get stuff from it
void NgDialog::on_pushButton_keys_clicked()
{
    QString fn = QFileDialog::getOpenFileName( this, tr( "Select keys.bin" ) );
    if( fn.isEmpty() )
        return;

    QByteArray ba = ReadFile( fn );
    if( ba.size() != 0x400 )
    {
        ui->label_message->setText( tr( "keys.bin should be 0x400 bytes" ) );
        return;
    }

    KeysBin keys( ba );
    ui->lineEdit_ngID->setText( keys.NG_ID().toHex() );
    ui->lineEdit_ngKeyId->setText( keys.NG_key_ID().toHex() );
    ui->lineEdit_ngPriv->setText( keys.NG_Priv().toHex() );
    ui->lineEdit_ngSig->setText( keys.NG_Sig().toHex() );
}

//read a data.bin and get some keys from it
void NgDialog::on_pushButton_existingSave_clicked()
{
    QString fn = QFileDialog::getOpenFileName( this, tr( "Select data.bin" ) );
    if( fn.isEmpty() )
        return;

    QByteArray ba = ReadFile( fn );
    SaveDataBin sb( ba );
    if( !sb.IsOk() )
    {
        ui->label_message->setText( tr( "error reading the data.bin" ) );
        return;
    }
    ui->lineEdit_ngID->setText( QString( "%1" ).arg( sb.NgID(), 8, 16, QChar( '0' ) ) );
    ui->lineEdit_ngKeyId->setText( QString( "%1" ).arg( sb.NgKeyID(), 8, 16, QChar( '0' ) ) );
    ui->lineEdit_ngSig->setText( sb.NgSig().toHex() );
    ui->lineEdit_ngMac->setText( sb.NgMac().toHex() );
}

void NgDialog::on_pushButton_ok_clicked()
{
    if( !ValidNGID() || !ValidNGKeyID() || !ValidNGSig() || !ValidNGMac() || !ValidNGPriv() )
    {
        ui->label_message->setText( tr( "Please correct mistakes before clicking ok" ) );
        qWarning() << "NgDialog::on_buttonBox_accepted() -> invalid shit";
        return;
    }

    ngSig = QByteArray::fromHex( ui->lineEdit_ngSig->text().toLatin1() );
    ngMac = QByteArray::fromHex( ui->lineEdit_ngMac->text().toLatin1() );;
    ngPriv = QByteArray::fromHex( ui->lineEdit_ngPriv->text().toLatin1() );
    QDialog::accept();
}

void NgDialog::on_pushButton_cancel_clicked()
{
    QDialog::reject();
}
