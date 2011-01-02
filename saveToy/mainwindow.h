#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "saveloadthread.h"
#include "savelistitem.h"
#include "../WiiQt/savebanner.h"
#include "../WiiQt/includes.h"

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
    void GetSavesFromPC( const QString &path );

    SaveLoadThread bannerthread;

	int currentSneekIcon;
	QTimer sneekIconTimer;
	QList<QPixmap>sneekIcon;

	int currentPcIcon;
	QTimer pcIconTimer;
	QList<QPixmap>pcIcon;
	quint32 currentPcSave;

    //basepath used for local save backups
    QString pcPath;

    //get available filename for backup save inside pcPath
    QString GetSaveName( quint64 tid );

    //variables used for creating the data.bins on extraction
    quint32 ngID;
    quint32 ngKeyID;
    QByteArray ngSig;
    QByteArray ngMac;
    QByteArray ngPriv;

	//for remembering PC saves' TID and the descriptions of each save
	QList< PcSaveInfo > pcInfos;

    bool NG_Ok();

    bool WriteZipFile( const QByteArray &dataBin, const QByteArray &desc, const QString &path );
	void AddNewPCSave( const QString &desc, const QString &tid, quint32 size, const QString &path, SaveBanner banner );


private slots:
	void on_comboBox_pcSelect_currentIndexChanged(int index);
	void on_listWidget_pcSaves_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);
	void on_actionSet_NG_Keys_triggered();
    void on_pushButton_sneekExtract_clicked();
    void on_pushButton_sneekDelete_clicked();
    void on_actionSet_Sneek_Path_triggered();
    void on_listWidget_sneekSaves_currentItemChanged( QListWidgetItem* current, QListWidgetItem* previous );

	void ReceiveSneekBanner( QByteArray, const QString&, int );
	void ReceivePcItem( PcSaveInfo );
    void GetProgressUpdate( int );
    void LoadThreadIsDone( int );

	void ShowSneekSaveDetails( SaveListItem *item );
	void ShowPCSaveDetails( SaveListItem *item );
	void ShowNextSneekIcon();
	void ShowNextPcIcon();

};

#endif // MAINWINDOW_H
