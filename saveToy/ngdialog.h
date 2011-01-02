#ifndef NGDIALOG_H
#define NGDIALOG_H

#include "../WiiQt/includes.h"
#include "../WiiQt/tools.h"

namespace Ui {
    class NgDialog;
}

class NgDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NgDialog( QWidget *parent = 0 );
    ~NgDialog();

    quint32 ngID;
    quint32 ngKeyID;
    QByteArray ngSig;
    QByteArray ngMac;
    QByteArray ngPriv;

private:
    Ui::NgDialog *ui;

    QRegExp hex;

    bool ValidNGID();
    bool ValidNGKeyID();
    bool ValidNGSig();
    bool ValidNGMac();
    bool ValidNGPriv();

private slots:
    void on_pushButton_cancel_clicked();
    void on_pushButton_ok_clicked();
    void on_pushButton_existingSave_clicked();
    void on_pushButton_keys_clicked();
    void on_lineEdit_ngPriv_textChanged(QString );
    void on_lineEdit_ngMac_textChanged(QString );
    void on_lineEdit_ngKeyId_textChanged(QString );
    void on_lineEdit_ngSig_textChanged(QString );
    void on_lineEdit_ngID_textChanged(QString );

public slots:
    int exec();
};

#endif // NGDIALOG_H
