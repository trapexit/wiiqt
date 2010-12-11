#ifndef NANDWINDOW_H
#define NANDWINDOW_H

#include "../WiiQt/includes.h"
//#include "../WiiQt/nandbin.h"

#include "nandthread.h"

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

    NandThread nThread;

    //changes the blocks in the blockmap with different colors
    void DrawBlockMap( QList<quint16> newFile = QList<quint16>() );
    QGraphicsScene gv;//scene to hold the blockmap images
    QGraphicsPixmapItem *pmi[ 0x1000 ];//pointers to all the blocks in the display
    //quint16 ClusterToBlock( quint16 cluster );

    QList<quint16> blocks;
    quint32 freeSpace;
    void GetBlocksfromNand();

    QList<quint16> ToBlocks( QList<quint16> clusters );



public slots:
    void GetError( QString );
    void GetStatusUpdate( QString );
    void ThreadIsDone();

private slots:
    void on_treeWidget_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void on_actionShow_Usage_triggered();
    void on_actionOpen_Nand_triggered();
    void on_treeWidget_customContextMenuRequested(QPoint pos);
};

#endif // NANDWINDOW_H
