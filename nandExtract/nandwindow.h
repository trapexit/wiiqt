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

    void SetUpBlockMap();

    //changes the blocks in the blockmap with different colors
    void DrawBlockMap( QList<quint16> newFile = QList<quint16>() );
    QGraphicsScene gv;				//scene to hold the blockmap images
    QGraphicsPixmapItem *pmi[ 0x1000 ];		//pointers to all the blocks in the display

    QGraphicsTextItem *nandSize;		//pointers to the items to show info under the blockmap
    QGraphicsTextItem *fileSize;
    /*QGraphicsTextItem *badText;
    QGraphicsTextItem *freeText;
    QGraphicsTextItem *usedText;
    QGraphicsTextItem *resText;*/

    QList<quint16> blocks;			//hold a list of what the blocks in the nand are used for
    void GetBlocksfromNand();

    QList<quint16> ToBlocks( QList<quint16> clusters );



public slots:
    void GetError( QString );
    void GetStatusUpdate( QString );
    void ThreadIsDone();

private slots:
    void on_actionBoot2_triggered();
    void on_treeWidget_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void on_actionShow_Usage_triggered();
    void on_actionOpen_Nand_triggered();
    void on_treeWidget_customContextMenuRequested(QPoint pos);
    //void ScaleBlockMap();

protected:
  //void resizeEvent( QResizeEvent* event );
};

#endif // NANDWINDOW_H
