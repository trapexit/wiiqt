#include "newnandbin.h"
#include "ui_newnandbin.h"
#include "../WiiQt/tools.h"

NewNandBin::NewNandBin( QWidget *parent, QList<quint16> badBlocks ) : QDialog(parent), ui(new Ui::NewNandBin), nand( this )
{
    dir = QDir::currentPath();
    ui->setupUi(this);
    foreach( quint16 block, badBlocks )
    {
	QString txt = QString( "%1" ).arg( block );
	if( !ui->listWidget_badBlocks->findItems( txt, Qt::MatchExactly ).isEmpty() )
	    ui->listWidget_badBlocks->addItem( txt );
    }
}

NewNandBin::~NewNandBin()
{
    delete ui;
}

void NewNandBin::on_pushButton_keys_clicked()
{
    QString f = QFileDialog::getOpenFileName( this, tr( "Select Keys.bin" ), dir );
    if( f.isEmpty() )
	return;
    ui->lineEdit_keys->setText( f );
    dir = QFileInfo( f ).canonicalPath();
}

void NewNandBin::on_pushButton_boot_clicked()
{
    QString f = QFileDialog::getOpenFileName( this, tr( "Select Boot 1 & 2" ), dir );
    if( f.isEmpty() )
	return;
    ui->lineEdit_boot->setText( f );
    dir = QFileInfo( f ).canonicalPath();
}

void NewNandBin::on_pushButton_dest_clicked()
{
    QString f = QFileDialog::getSaveFileName( this, tr( "Output file" ), dir );
    if( f.isEmpty() )
	return;
    ui->lineEdit_dest->setText( f );
    dir = QFileInfo( f ).canonicalPath();
}

QList<quint16> NewNandBin::BadBlocks()
{
    QList<quint16> ret;
    quint16 cnt = ui->listWidget_badBlocks->count();
    for( quint16 i = 0; i < cnt; i++ )
    {
	bool ok = false;
	quint16 num = ui->listWidget_badBlocks->item( i )->text().toInt( &ok );
	if( ok )
	    ret << num;
    }
    return ret;
}

void NewNandBin::on_pushButton_bb_add_clicked()
{
    quint16 val = ui->spinBox->value();
    if( !BadBlocks().contains( val ) )
    {
	ui->listWidget_badBlocks->addItem( QString( "%1" ).arg( val ) );
    }
}

void NewNandBin::on_pushButton_bb_rm_clicked()
{
    QList<QListWidgetItem *> items = ui->listWidget_badBlocks->selectedItems();
    foreach( QListWidgetItem *item, items )
    {
	ui->listWidget_badBlocks->removeItemWidget( item );
	delete item;
    }
}

//ok clicked
void NewNandBin::on_buttonBox_accepted()
{
    if( ui->lineEdit_keys->text().isEmpty() || ui->lineEdit_boot->text().isEmpty() || ui->lineEdit_dest->text().isEmpty() )
    {
	QMessageBox::warning( this, tr( "Error" ), tr( "Required feilds are empty" ) );
	return;
    }
    QByteArray keys = ReadFile( ui->lineEdit_keys->text() );
    QByteArray boots = ReadFile( ui->lineEdit_boot->text() );
    if( keys.size() != 0x400 || boots.size() != 0x108000 )
    {
	QMessageBox::warning( this, tr( "Error" ), tr( "The keys or boot1/2 is not correct" ) );
	return;
    }
    if( !nand.CreateNew( ui->lineEdit_dest->text(), keys, boots, BadBlocks() ) )
    {
	qDebug() << "error creating nand.bin";
	return;
    }
    //qDebug() << "created nand, trying to add default entries";
    if( !nand.CreateEntry( "/sys", 0, 0, NAND_DIR, NAND_RW, NAND_RW, 0 )
	|| !nand.CreateEntry( "/ticket", 0, 0, NAND_DIR, NAND_RW, NAND_RW, 0 )
	|| !nand.CreateEntry( "/title", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_READ )
	|| !nand.CreateEntry( "/shared1", 0, 0, NAND_DIR, NAND_RW, NAND_RW, 0 )
	|| !nand.CreateEntry( "/shared2", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_RW )
	|| !nand.CreateEntry( "/import", 0, 0, NAND_DIR, NAND_RW, NAND_RW, 0 )
	|| !nand.CreateEntry( "/meta", 0x1000, 1, NAND_DIR, NAND_RW, NAND_RW, NAND_RW )
	|| !nand.CreateEntry( "/tmp", 0, 0, NAND_DIR, NAND_RW, NAND_RW, NAND_RW )
	|| !nand.WriteMetaData() )
	{
	qWarning() << "NewNandBin::on_buttonBox_accepted -> error creating the new nand";
	return;
    }

    ret = ui->lineEdit_dest->text();
}

QString NewNandBin::GetNewNandPath( QWidget *parent, QList<quint16> badBlocks )
{
    NewNandBin d( parent, badBlocks );
    if( !d.exec() )
	return QString();
    return d.ret;
}

//read bad blocks from a txt file
void NewNandBin::on_pushButton_badBlockFile_clicked()
{
    QString f = QFileDialog::getOpenFileName( this, tr( "Select File with Bad Block List" ), dir );
    if( f.isEmpty() )
	return;
    dir = QFileInfo( f ).canonicalPath();
    QString str = QString( ReadFile( f ) );
    if( str.isEmpty() )
    {
	qWarning() << "NewNandBin::on_pushButton_badBlockFile_clicked -> error reading file";
	return;
    }
    ui->listWidget_badBlocks->clear();

    str.replace( "\r\n", "\n" );
    QStringList lines = str.split( "\n", QString::SkipEmptyParts );
    foreach( QString line, lines )
    {
	if( line.size() > 5 )
	    continue;
	bool ok = false;

	if( ui->listWidget_badBlocks->findItems( line, Qt::MatchExactly ).size() )//this one is already in the list
	    continue;

	quint16 bb = line.toInt( &ok );
	if( !ok || bb < 8 || bb > 4079 )
	    continue;

	ui->listWidget_badBlocks->addItem( line );
    }
}
