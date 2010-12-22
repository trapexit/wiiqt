#ifndef SETTINGTXTDIALOG_H
#define SETTINGTXTDIALOG_H

#include "includes.h"

//this class creates a dialog used to create & edit a setting.txt for a wii nand filesystem
// in most cases, the static function Edit() is what you want to use
namespace Ui {
    class SettingTxtDialog;
}
enum
{
    SETTING_TXT_JAP = 0,
    SETTING_TXT_USA,
    SETTING_TXT_PAL,
    SETTING_TXT_KOR,
    SETTING_TXT_UNK
};

class SettingTxtDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingTxtDialog( QWidget *parent = 0, const QByteArray &old = QByteArray(), qint8 region = SETTING_TXT_UNK );
    ~SettingTxtDialog();

    //displays a dialog window with the given parent.  if any data is given as old, it will try to populate the dialog with that
    // otherwise it will use the defaulte values
    // returns empty if the user clicked cancel, or a bytearray containing an encrypted setting.txt if they clicked ok
    // the data is ready for writing to a wii nand
    // if an empty byte array is given as old, a region can be specified to populate default values instaed
    static QByteArray Edit( QWidget *parent = 0, const QByteArray &old = QByteArray(), qint8 region = SETTING_TXT_UNK );
    static QByteArray LolCrypt( QByteArray ba );

private:
    Ui::SettingTxtDialog *ui;
    QByteArray ret;



private slots:
    void on_buttonBox_accepted();
};

#endif // SETTINGTXTDIALOG_H
