#ifndef NANDWINDOW_H
#define NANDWINDOW_H

#include "includes.h"
#include "../WiiQt/nandbin.h"

namespace Ui {
    class NandWindow;
}

class NandWindow : public QMainWindow {
    Q_OBJECT
public:
    NandWindow(QWidget *parent = 0);
    ~NandWindow();

protected:
    void changeEvent(QEvent *e);

private:
    Ui::NandWindow *ui;

    NandBin nandBin;

    //just put these here and create a slot to make the GUI not
    //look so ugly when it hangs while stuff is extracting
    QString exPath;
    QTreeWidgetItem* exItem;

public slots:
    void GetError( QString );
    void GetStatusUpdate( QString );

private slots:
    void on_actionOpen_Nand_triggered();
    void on_treeWidget_customContextMenuRequested(QPoint pos);
    void ExtractShit();
};

#endif // NANDWINDOW_H
