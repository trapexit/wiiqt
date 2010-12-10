#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../WiiQt/includes.h"
#include "../WiiQt/nusdownloader.h"
#include "../WiiQt/nanddump.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow( QWidget *parent = 0 );
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    NusDownloader nus;
    NandDump nand;

    void ShowMessage( const QString& mes );

    //do something with a completed download
    void SaveJobToFolder( NusJob job );
    void SaveJobToWad( NusJob job );



public slots:
    //slots for getting info from the NUS downloader
    void GetError( const QString &message, NusJob job );
    void NusIsDone();
    void ReceiveTitleFromNus( NusJob job );


private slots:
    void on_actionFlush_triggered();
    void on_actionSetting_txt_triggered();
    void on_pushButton_wad_clicked();
    void on_pushButton_decFolder_clicked();
    void on_pushButton_nandPath_clicked();
    void on_radioButton_wad_toggled(bool checked);
    void on_radioButton_folder_toggled(bool checked);
    void on_radioButton_nand_toggled(bool checked);
    void on_pushButton_GetTitle_clicked();
};

#endif // MAINWINDOW_H
