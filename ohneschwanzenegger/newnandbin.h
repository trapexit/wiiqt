#ifndef NEWNANDBIN_H
#define NEWNANDBIN_H

#include "../WiiQt/includes.h"
#include "../WiiQt/nandbin.h"

namespace Ui {
    class NewNandBin;
}

class NewNandBin : public QDialog
{
    Q_OBJECT

public:
    explicit NewNandBin( QWidget *parent = 0, QList<quint16>badBlocks = QList<quint16>() );
    ~NewNandBin();

    static QString GetNewNandPath( QWidget *parent = 0, QList<quint16>badBlocks = QList<quint16>() );

private:
    Ui::NewNandBin *ui;
    QList<quint16> BadBlocks();

    NandBin nand;

    QString ret;
    QString dir;

private slots:
    void on_pushButton_badBlockFile_clicked();
    void on_buttonBox_accepted();
    void on_pushButton_bb_rm_clicked();
    void on_pushButton_bb_add_clicked();
    void on_pushButton_dest_clicked();
    void on_pushButton_boot_clicked();
    void on_pushButton_keys_clicked();
};

#endif // NEWNANDBIN_H
