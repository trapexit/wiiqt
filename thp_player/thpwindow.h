#ifndef THPWINDOW_H
#define THPWINDOW_H

#include <QAudioFormat>
#include <QAudioOutput>
#include "gcvid.h"
#include "../WiiQt/includes.h"

namespace Ui {
    class ThpWindow;
}

typedef struct
{
	std::vector< qint16 > Buffer;
	int Size;
} SoundFrame;

class ThpWindow : public QMainWindow
{
    Q_OBJECT

public:
	explicit ThpWindow( QWidget *parent = 0 );
    ~ThpWindow();

	void LoadVideo( const QString &path );

private:
	Ui::ThpWindow *ui;
	void LoadNextFrame();

	VideoFile * videoFile;
	QAudioOutput*    AudioOutput;
	QIODevice*       AudioOutputDevice;
	//QBuffer       AudioOutputDevice;
	QAudioFormat     AudioFormat;
	std::vector<QPixmap> Frames;
	std::vector<SoundFrame> SoundBuffers;

	void CreateAudioOutput();

	QTimer timer;
	bool dontBuffer;

	QString TimeTxt( quint64 msecs );
	quint64 frameCnt;
	quint64 curFrame;

	QStringList playList;
	quint32 curPlayListPos;
	void PlayPlayListItem( quint32 i );
	QString currentDir;

	void EnableGui( bool enable = true );
	QSize MaxSizeForRatio( qreal w, qreal h );

private slots:
	//void on_actionFit_To_Window_triggered(bool checked);
	void on_pushButton_stop_clicked();
	void on_pushButton_prev_clicked();
	void on_pushButton_next_clicked();
	void on_actionOpen_Folder_triggered();
	void on_pushButton_playPause_clicked();
	void ShowNextFrame();
	void BufferIfNeeded();
	void on_actionOpen_triggered();

//protected:
	//void resizeEvent ( QResizeEvent * event );
};

#endif // THPWINDOW_H
