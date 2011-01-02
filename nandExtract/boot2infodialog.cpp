#include "boot2infodialog.h"
#include "ui_boot2infodialog.h"

Boot2InfoDialog::Boot2InfoDialog( QWidget *parent, QList<Boot2Info> boot, quint8 boot1version ) : QDialog(parent), ui(new Ui::Boot2InfoDialog)
{
    ui->setupUi( this );
    ui->label_boot2Version->clear();

    switch( boot1version )
    {
    case BOOT_1_A:
        ui->label_boot1->setText( "Boot1 A (vulnerable)");
        break;
    case BOOT_1_B:
        ui->label_boot1->setText( "Boot1 B (vulnerable)");
        break;
    case BOOT_1_C:
        ui->label_boot1->setText( "Boot1 C (fixed)");
        break;
    case BOOT_1_D:
        ui->label_boot1->setText( "Boot1 D (fixed)");
        break;
    }

    quint8 c = boot.size();
    if( !c )
        return;

    ui->label_foundTxt->setText( QString( "Found %1 copies of boot2" ).arg( c ) );
    boot2s = boot;

    for( quint8 i = 0; i < c; i++ )
    {
        QString entry = QString( "%1 - %2" ).arg( boot.at( i ).firstBlock ).arg( boot.at( i ).secondBlock );
        ui->comboBox_boot2->addItem( entry );
        if( boot.at( i ).state & BOOT_2_USED_TO_BOOT )
        {
            ui->comboBox_boot2->setCurrentIndex( i );
        }
    }
}

Boot2InfoDialog::~Boot2InfoDialog()
{
    delete ui;
}

void Boot2InfoDialog::on_comboBox_boot2_currentIndexChanged( int index )
{
    if( index > boot2s.size() )
        return;
    Boot2Info bi = boot2s.at( index );
    ui->label_boot2Version->clear();

    ui->lineEdit_contentStatus->clear();
    ui->lineEdit_bootStatus->clear();
    ui->lineEdit_tikSig->setText( tr( "Not Bootable" ) );
    ui->lineEdit_tmdSig->clear();
    ui->lineEdit_gen->setText( tr( "Generation %1").arg( bi.generation ) );


    if( bi.state == BOOT_2_ERROR_PARSING || bi.state == BOOT_2_ERROR )
        ui->lineEdit_contentStatus->setText( tr( "Error parsing boot2" ) );
    else if( bi.state == BOOT_2_BAD_SIGNATURE )
        ui->lineEdit_contentStatus->setText( tr( "Bad RSA Signature" ) );
    else if( bi.state == BOOT_2_BAD_CONTENT_HASH )
        ui->lineEdit_contentStatus->setText( tr( "Content hash doesn't match TMD" ) );
    else if( bi.state == BOOT_2_ERROR_PARSING )
        ui->lineEdit_contentStatus->setText( tr( "Error parsing boot2" ) );
    else
    {

        if( bi.state & BOOT_2_MARKED_BAD )
            ui->lineEdit_bootStatus->setText( tr( "Marked as bad blocks" ) );
        else if( bi.state & BOOT_2_USED_TO_BOOT )
            ui->lineEdit_bootStatus->setText( tr( "Used for booting" ) + "   " );//in case the backup copy is used for booting
        else if( bi.state & BOOT_2_BACKUP_COPY )
            ui->lineEdit_bootStatus->setText( ui->lineEdit_bootStatus->text() + tr( "Backup copy" ) );


        ui->lineEdit_contentStatus->setText( tr( "Content Sha1 matches TMD" ) );

        if( bi.state & BOOT_2_TMD_FAKESIGNED )
            ui->lineEdit_tmdSig->setText( tr( "TMD is fakesigned" ) );
        else if( bi.state & BOOT_2_TMD_SIG_OK )
            ui->lineEdit_tmdSig->setText( tr( "TMD officially signed" ) );
        else
            ui->lineEdit_tmdSig->setText( tr( "Error checking TMD" ) );

        if( bi.state & BOOT_2_TIK_FAKESIGNED )
            ui->lineEdit_tikSig->setText( tr( "Ticket is fakesigned" ) );
        else if( bi.state & BOOT_2_TIK_SIG_OK )
            ui->lineEdit_tikSig->setText( tr( "Ticket officially signed" ) );
        else
            ui->lineEdit_tikSig->setText( tr( "Error checking ticket" ) );

        QString ver;
        switch( bi.version )
        {
        case BOOTMII_11:
            ver = "BootMii 1.1";
            break;
        case BOOTMII_13:
            ver = "BootMii 1.3";
            break;
        case BOOTMII_UNK:
            ver = "BootMii (Unk)";
            break;
        default:
            ver = QString( tr( "Version %1" ).arg( bi.version ) );
            break;
        }
        ui->label_boot2Version->setText( ver );
    }


    /*QString bls = QByteArray( (const char*)&bi.blockMap, 8 ).toHex();
    QString str = QString( "copy:%1\tblocks%2 & %3  gen: %4  state: %5\n%7").arg( index ).arg( bi.firstBlock )
		  .arg( bi.secondBlock ).arg( bi.generation, 8, 16, QChar( '0' ) )
		  .arg( bi.state, 8, 16, QChar( ' ' ) ).arg( bls );

    qDebug() << str;*/
}
