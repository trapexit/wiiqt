#ifndef BOOT2INFODIALOG_H
#define BOOT2INFODIALOG_H

#include "../WiiQt/includes.h"
#include "../WiiQt/blocks0to7.h"

namespace Ui {
    class Boot2InfoDialog;
}

class Boot2InfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit Boot2InfoDialog( QWidget *parent = 0, QList<Boot2Info> boot = QList<Boot2Info>(), quint8 boot1version = BOOT_1_UNK );
    ~Boot2InfoDialog();

private:
    Ui::Boot2InfoDialog *ui;

    QList<Boot2Info> boot2s;

    //quint8 usedBlockmap[ 8 ];

private slots:
    void on_comboBox_boot2_currentIndexChanged( int index );
};

#endif // BOOT2INFODIALOG_H
