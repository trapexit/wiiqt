#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../WiiQt/includes.h"
#include "../WiiQt/nusdownloader.h"
#include "../WiiQt/nandbin.h"
#include "../WiiQt/uidmap.h"
#include "../WiiQt/sharedcontentmap.h"

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
    QTreeWidgetItem *root;
    bool nandInited;
    bool uidDirty;
    bool sharedDirty;
    bool nandDirty;

    NusDownloader nus;
    NandBin nand;
    UIDmap uid;
    SharedContentMap shared;
    bool FlushNand();

    void ShowMessage( const QString& mes );

    bool InitNand( const QString &path );
    bool UpdateTree();

    QString PathFromItem( QTreeWidgetItem *item );
    QTreeWidgetItem *ItemFromPath( const QString &path );
    QTreeWidgetItem *FindItem( const QString &s, QTreeWidgetItem *parent );

    bool InstallNUSItem( NusJob job );
    quint16 CreateIfNeeded( const QString &path, quint32 uid, quint16 gid, quint8 attr, quint8 user_perm, quint8 group_perm, quint8 other_perm );
    bool InstallSharedContent( const QByteArray &stuff, const QByteArray &hash = QByteArray() );

    void SaveSettings();
    void LoadSettings();
#if 0
    void TryToAddDefaultSettings();
#endif
    void AddStuffToMetaFolder();
    QByteArray GenMeta( const QString &desc, quint64 tid, quint16 version );

public slots:
    //slots for getting info from the NUS downloader
    void GetError( const QString &message, const NusJob &job );
    void GetError( const QString &message );
    void NusIsDone();
    void ReceiveTitleFromNus( const NusJob &job );

private slots:
    void on_actionFormat_triggered();
    void on_actionWrite_meta_entries_triggered();
    void on_pushButton_CachePathBrowse_clicked();
    void on_actionAbout_triggered();
    void on_pushButton_initNand_clicked();
    void on_actionNew_nand_from_keys_triggered();
    void on_actionFlush_triggered();
    void on_actionSetting_txt_triggered();
    void on_actionImportWad_triggered();
    void on_pushButton_nandPath_clicked();
    void on_pushButton_GetTitle_clicked();
};

#endif // MAINWINDOW_H
