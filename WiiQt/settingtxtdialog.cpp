#include "settingtxtdialog.h"
#include "ui_settingtxtdialog.h"
#include "tools.h"

SettingTxtDialog::SettingTxtDialog( QWidget *parent, const QByteArray &old, qint8 region ) : QDialog(parent), ui(new Ui::SettingTxtDialog)
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
    else
    {
        switch( region )
        {
        case SETTING_TXT_PAL:
            ui->lineEdit_area->setText( "EUR" );
            ui->lineEdit_model->setText( "RVL-001(EUR)" );
            ui->lineEdit_code->setText( "LEH" );
            ui->lineEdit_video->setText( "PAL" );
            ui->lineEdit_game->setText( "EU" );
            break;
        case SETTING_TXT_JAP:
            ui->lineEdit_area->setText( "JPN" );
            ui->lineEdit_model->setText( "RVL-001(JPN)" );
            ui->lineEdit_code->setText( "LJF" );
            ui->lineEdit_video->setText( "NTSC" );
            ui->lineEdit_game->setText( "JP" );
            break;
        case SETTING_TXT_KOR:
            ui->lineEdit_area->setText( "KOR" );
            ui->lineEdit_model->setText( "RVL-001(KOR)" );
            ui->lineEdit_code->setText( "LKM" );
            ui->lineEdit_video->setText( "NTSC" );
            ui->lineEdit_game->setText( "KR" );
            break;
        case SETTING_TXT_USA://these are already the default values
        default:
            break;
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
                "GAME=" + ui->lineEdit_game->text() + "\r\n";

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

QByteArray SettingTxtDialog::Edit( QWidget *parent, const QByteArray &old, qint8 region )
{
    SettingTxtDialog d( parent, old, region );
    if( d.exec() )
        return d.ret;
    return QByteArray();
}

/*
 some possible values ( from libogc )
 res = __SYSCONF_GetTxt("GAME", buf, 3);
if(res < 0) return res;
if(!strcmp(buf, "JP")) return SYSCONF_REGION_JP;
if(!strcmp(buf, "US")) return SYSCONF_REGION_US;
if(!strcmp(buf, "EU")) return SYSCONF_REGION_EU;


 res = __SYSCONF_GetTxt("AREA", buf, 4);
if(res < 0) return res;
if(!strcmp(buf, "JPN")) return SYSCONF_AREA_JPN;
if(!strcmp(buf, "USA")) return SYSCONF_AREA_USA;
if(!strcmp(buf, "EUR")) return SYSCONF_AREA_EUR;
if(!strcmp(buf, "AUS")) return SYSCONF_AREA_AUS;
if(!strcmp(buf, "BRA")) return SYSCONF_AREA_BRA;
if(!strcmp(buf, "TWN")) return SYSCONF_AREA_TWN;
if(!strcmp(buf, "ROC")) return SYSCONF_AREA_ROC;
if(!strcmp(buf, "KOR")) return SYSCONF_AREA_KOR;
if(!strcmp(buf, "HKG")) return SYSCONF_AREA_HKG;
if(!strcmp(buf, "ASI")) return SYSCONF_AREA_ASI;
if(!strcmp(buf, "LTN")) return SYSCONF_AREA_LTN;
if(!strcmp(buf, "SAF")) return SYSCONF_AREA_SAF;

 res = __SYSCONF_GetTxt("VIDEO", buf, 5);
if(res < 0) return res;
if(!strcmp(buf, "NTSC")) return SYSCONF_VIDEO_NTSC;
if(!strcmp(buf, "PAL")) return SYSCONF_VIDEO_PAL;
if(!strcmp(buf, "MPAL")) return SYSCONF_VIDEO_MPAL;


 */
