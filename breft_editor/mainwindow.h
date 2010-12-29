#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../WiiQt/includes.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow( QWidget *parent = 0 );
    ~MainWindow();
    void ShowMessage( const QString& mes );

private:
	void setupForms();
	void setupActions();
	void setupMenu();
	QGridLayout* Layout;
	QAction* actionQuit;
	QAction* actionLoad;
	QMenuBar* menubar;
	QMenu* menuFile;
	QPlainTextEdit* plainTextEdit_log;

public slots:

private slots:
	void on_actionQuit_triggered();
	void on_actionLoad_triggered();
	/*
    void on_pushButton_CachePathBrowse_clicked();
    void on_actionFlush_triggered();
    void on_actionSetting_txt_triggered();
	void on_actionImportWad_triggered();
    void on_pushButton_wad_clicked();
    void on_pushButton_decFolder_clicked();
    void on_pushButton_nandPath_clicked();
    void on_radioButton_wad_toggled(bool checked);
    void on_radioButton_folder_toggled(bool checked);
    void on_radioButton_nand_toggled(bool checked);
    void on_pushButton_GetTitle_clicked();
	*/
};

#endif // MAINWINDOW_H
