#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "saveloadthread.h"
#include "savelistitem.h"
#include "savebanner.h"
#include "includes.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QProgressBar progressBar;
    void GetSavesFromSneek( const QString &path );

    SaveLoadThread bannerthread;

    //void ShowSneekSaveDetails( SaveListItem *item );
    int currentSneekIcon;
    QTimer sneekIconTimer;
    QList<QPixmap>sneekIcon;


private slots:
    void on_actionSet_Sneek_Path_triggered();
    void on_listWidget_sneekSaves_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);
    //void on_listWidget_sneekSaves_itemClicked(QListWidgetItem* item);
    //void on_listWidget_sneekSaves_itemActivated( QListWidgetItem* item );

    void ReceiveSaveItem( QByteArray, const QString&, int, int );
    void GetProgressUpdate( int );
    void LoadThreadIsDone( int );

    void ShowSneekSaveDetails( SaveListItem *item );
    void ShowNextSneekIcon();

};

#endif // MAINWINDOW_H
