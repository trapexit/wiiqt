#ifndef SETTINGTXTDIALOG_H
#define SETTINGTXTDIALOG_H

#include "includes.h"

namespace Ui {
    class SettingTxtDialog;
}

class SettingTxtDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingTxtDialog( QWidget *parent = 0, const QByteArray &old = QByteArray() );
    ~SettingTxtDialog();

    static QByteArray Edit( QWidget *parent = 0, const QByteArray &old = QByteArray() );
    static QByteArray LolCrypt( QByteArray ba );

private:
    Ui::SettingTxtDialog *ui;
    QByteArray ret;



private slots:
    void on_buttonBox_accepted();
};

#endif // SETTINGTXTDIALOG_H
