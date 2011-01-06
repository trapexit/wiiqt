#include "thpwindow.h"
#include "ui_thpwindow.h"
#include "../WiiQt/tools.h"

#define MAX_BUFFERS 50  //in frames
#define BUFFER_DELAY 200	//time to wait between checking buffer size ( msecs )

ThpWindow::ThpWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::ThpWindow)
{
	ui->setupUi( this );
	ui->mainToolBar->setVisible( false );
	ui->label_fName->clear();
	ui->label_fpsT->clear();
	ui->label_itemNo->clear();
	ui->label_sizeT->clear();
	ui->label_timeCur->clear();
	ui->label_timeFull->clear();
	ui->label_video->clear();
	ui->scrollArea->setWidgetResizable( false );
	EnableGui( false );
	ui->statusBar->addPermanentWidget( ui->label_fName, 0 );
	ui->statusBar->addPermanentWidget( ui->label_itemNo, 0 );

	AudioOutputDevice = NULL;
	AudioOutput = NULL;
	videoFile = NULL;
	dontBuffer = false;
	//currentDir = QDir::currentPath();
	currentDir = "/media/Jenna/jenna/c/gui_fork/SSBB_JAP/DATA/files/movie";

	//workaround for some bullshit bug in the Qt libs
	foreach( const QAudioDeviceInfo &deviceInfo, QAudioDeviceInfo::availableDevices( QAudio::AudioOutput ) )
	{
		qDebug() << "l:" << deviceInfo.deviceName();
	}

	connect( &timer, SIGNAL( timeout() ), this, SLOT( ShowNextFrame() ) );

}

ThpWindow::~ThpWindow()
{
	dontBuffer = true;
	timer.stop();
    delete ui;
	if(videoFile)
	{
		closeVideo( videoFile );
		videoFile = NULL;
	}
	if( AudioOutput )
	{
		AudioOutput->stop();
		delete AudioOutput;
		AudioOutput = NULL;
	}
}

//open file
void ThpWindow::on_actionOpen_triggered()
{
	timer.stop();
	dontBuffer = true;
	playList.clear();
	curPlayListPos = 0;
	QStringList fNames = QFileDialog::getOpenFileNames( this, tr("Open Files"), currentDir, tr("Wii/GC Videos (*.thp *.mth)") );
	if( fNames.isEmpty() )
		return;

	foreach( QString str, fNames )
		playList << str;

	currentDir = QFileInfo( playList.at( 0 ) ).absolutePath();

	PlayPlayListItem( 0 );
}

//open folder
void ThpWindow::on_actionOpen_Folder_triggered()
{
	timer.stop();
	dontBuffer = true;
	playList.clear();
	curPlayListPos = 0;
	QString dirName = QFileDialog::getExistingDirectory( this, tr( "Open Folder" ), currentDir );
	if( dirName.isEmpty() )
		return;

	currentDir = dirName;

	QDir dir( dirName );
	QFileInfoList fil = dir.entryInfoList( QStringList() << "*.thp" << "*.mth" , QDir::Files );
	//qDebug() << "found" << fil.size() << "items";
	foreach( QFileInfo fi, fil )
		playList << fi.absoluteFilePath();

	PlayPlayListItem( 0 );
}

void ThpWindow::PlayPlayListItem( quint32 i )
{
	//qDebug() << "ThpWindow::PlayPlayListItem" << i;
	if( !playList.size() )				//no videos to play
	{
		ui->label_itemNo->clear();
		return;
	}

	if( i >= (quint32)playList.size() )			//all videos are played
	{
		curPlayListPos = 0;				//skip back to beginning
		if( !ui->pushButton_loop->isChecked() )	//dont loop
		{
			ui->pushButton_playPause->setChecked( false );
			timer.stop();
			dontBuffer = true;
		}
	}

	ui->label_itemNo->setText( QString( "%1 / %2").arg( curPlayListPos + 1 ).arg( playList.size() ) );
	LoadVideo( playList.at( curPlayListPos ) );
}

void ThpWindow::LoadVideo( const QString &path )
{
	//qDebug() << "ThpWindow::LoadVideo" << path;
	EnableGui( false );
	std::string filepath = path.toUtf8().constData();

	ui->progressBar_buffer->setValue( 0 );

	//stop current video
	timer.stop();
	if( videoFile )
	{
		closeVideo( videoFile );
		videoFile = NULL;
	}

	Frames.clear();
	SoundBuffers.clear();

	videoFile = openVideo( filepath );
	if( !videoFile )
	{
		QMessageBox::information( this, tr("Player"), tr("Cannot load %1.").arg( path ) );
		return;
	}

	//dontBuffer = false;
	ui->label_video->setFixedSize( videoFile->getWidth(), videoFile->getHeight() );
	frameCnt = videoFile->getFrameCount();
	curFrame = 0;
	for( quint8 i = 0; i < 3; i++ )
		LoadNextFrame();

	CreateAudioOutput();

	//show some info in the gui
	ui->label_fpsT->setText( QString( "%1" ).arg( videoFile->getFps(), 0, 'f', 3 ) );
	ui->label_sizeT->setText( QString( "%1 x %2" ).arg( videoFile->getWidth() ).arg( videoFile->getHeight() ) );
	ui->label_fName->setText( QFileInfo( path ).fileName() );
	ui->horizontalSlider_pos->setMaximum( frameCnt );

	//set timer for animation
	qreal delay = 1000.0f/videoFile->getFps();
	ui->label_timeFull->setText( TimeTxt( delay * videoFile->getFrameCount() ));
	timer.setInterval( delay );

	//if play button is clicked, just play
	if( ui->pushButton_playPause->isChecked() )
		timer.start();
	//otherwise just load the first frame
	else
		ShowNextFrame();

	//allow the buttons to work
	EnableGui( true );
}

QString ThpWindow::TimeTxt( quint64 msecs )
{
	quint32 hours = msecs / 3600000;
	msecs -= ( hours * 3600000 );
	quint32 minutes = msecs / 60000;
	msecs -= ( minutes * 60000 );
	quint32 seconds = msecs / 1000;
	msecs -= ( seconds * 1000 );
	return QString( "%1:%2:%3.%4" ).arg( hours, 2, 10, QChar( '0' ) )
						.arg( minutes, 2, 10, QChar( '0' ) )
						.arg( seconds, 2, 10, QChar( '0' ) )
						.arg( msecs, 3, 10, QChar( '0' ) );
}

void ThpWindow::LoadNextFrame()
{
	//qDebug() << "ThpWindow::LoadNextFrame()";
	VideoFrame VideoF;
	videoFile->loadNextFrame();
	videoFile->getCurrentFrame(VideoF);
	QImage image(VideoF.getData(), VideoF.getWidth(), VideoF.getHeight(), QImage::Format_RGB888);
	if (image.isNull())
		return;

	Frames.push_back(QPixmap::fromImage(image));


	int SoundPos = SoundBuffers.size();
	SoundBuffers.resize(SoundBuffers.size()+1);
	SoundBuffers[SoundPos].Buffer.resize(videoFile->getMaxAudioSamples()*2);
	SoundBuffers[SoundPos].Size = videoFile->getCurrentBuffer(&SoundBuffers[SoundPos].Buffer[0])*2*2;
}

void ThpWindow::ShowNextFrame()
{
	//qDebug() << "ThpWindow::ShowNextFrame()" << Frames.size() << curFrame << frameCnt;
	if( Frames.size() < 3 )
	{
		BufferIfNeeded();
		return;
	}
	if( ++curFrame >= frameCnt )			//end of video
	{
		PlayPlayListItem( ++curPlayListPos );
		return;
	}

	ui->horizontalSlider_pos->setValue( curFrame );
	qreal delay = 1000.0f/videoFile->getFps();
	ui->label_timeCur->setText( TimeTxt( delay * videoFile->getCurrentFrameNr() ) );

	ui->label_video->setPixmap(Frames[2]);
	//ui->label_video->setPixmap(Frames[0]);

	if( AudioOutputDevice && ui->pushButton_vol->isChecked() )
		//&& SoundBuffers.size() > 2
		//&& SoundBuffers[ 2 ].Buffer.size()
		//&& SoundBuffers[ 2 ].Size )
		AudioOutputDevice->write((char *) &SoundBuffers[2].Buffer[0], SoundBuffers[2].Size);

	Frames.erase(Frames.begin());
	SoundBuffers.erase(SoundBuffers.begin());
}

void ThpWindow::BufferIfNeeded()
{

	if( dontBuffer )
		return;			//break the buffer loop

	if( Frames.size() < MAX_BUFFERS )//we need to read a frame
	{
		LoadNextFrame();
	}

	//show buffer in the gui
	int b = ((float)Frames.size() / (float)MAX_BUFFERS) * 100.0f;
	ui->progressBar_buffer->setValue( b );

	//wait a bit and call this function again
	QTimer::singleShot( BUFFER_DELAY, this, SLOT( BufferIfNeeded() ) );
}

void ThpWindow::CreateAudioOutput()
{
	//qDebug() << "ThpWindow::CreateAudioOutput()" << timer.isActive();

	if( AudioOutput )
	{
		AudioOutput->stop();
		delete AudioOutput;
		AudioOutput = NULL;
	}
	AudioOutputDevice = NULL;

	AudioFormat.setFrequency( videoFile->getFrequency() );
	AudioFormat.setChannels( videoFile->getNumChannels() );
	AudioFormat.setSampleSize( 16 );
	AudioFormat.setCodec( "audio/pcm" );
	AudioFormat.setByteOrder( QAudioFormat::LittleEndian );
	AudioFormat.setSampleType( QAudioFormat::SignedInt );

	QAudioDeviceInfo info( QAudioDeviceInfo::defaultOutputDevice() );
	if( !info.isFormatSupported( AudioFormat ) )
	{
		AudioFormat = info.nearestFormat( AudioFormat );//try to find a usable audio playback format
		if( !info.isFormatSupported( AudioFormat ) )
		{
			qWarning() << "unsupported audio format: can't play anything";
			ui->statusBar->showMessage( tr( "Can't find suitable audio format" ), 5000 );
			return;
		}
	}
	AudioOutput = new QAudioOutput( AudioFormat, this );
	if( !AudioOutput )
	{
		ui->statusBar->showMessage( tr( "Audio output error" ), 5000 );
		qWarning() << "!AudioOutput";
		return;
	}
	AudioOutputDevice = AudioOutput->start();
	if( AudioOutput->error() )
	{
		ui->statusBar->showMessage( tr( "Audio output error" ), 5000 );
		qWarning() << "AudioOutput->error()" << AudioOutput->error();
		AudioOutput->stop();
		AudioOutputDevice = NULL;
	}
}

//enable/disable buttons
void ThpWindow::EnableGui( bool enable )
{
	ui->pushButton_ffw->setEnabled( enable );
	ui->pushButton_loop->setEnabled( enable );
	ui->pushButton_next->setEnabled( enable );
	ui->pushButton_playPause->setEnabled( enable );
	ui->pushButton_prev->setEnabled( enable );
	ui->pushButton_rewind->setEnabled( enable );
	ui->pushButton_stop->setEnabled( enable );
	ui->pushButton_vol->setEnabled( enable );
}

//play button
void ThpWindow::on_pushButton_playPause_clicked()
{
	if( ui->pushButton_playPause->isChecked() )
	{
		dontBuffer = false;//start buffering again after stopped
		timer.start();
	}
	else
	{
		//dontBuffer = true;//ok to buffer while paused
		timer.stop();
	}
}

//next button
void ThpWindow::on_pushButton_next_clicked()
{
	PlayPlayListItem( ++curPlayListPos );
}

//prev button
void ThpWindow::on_pushButton_prev_clicked()
{
	if( !curPlayListPos )
		curPlayListPos = playList.size();
	PlayPlayListItem( --curPlayListPos );
}

//stop button
void ThpWindow::on_pushButton_stop_clicked()
{
	//stop playback
	timer.stop();
	ui->pushButton_playPause->setChecked( false );

	//clear buffer
	dontBuffer = true;
	Frames.clear();
	SoundBuffers.clear();
	ui->progressBar_buffer->setValue( 0 );

	//set video to first frame
	videoFile->SetFrameNo( 0 );

	//read a few frames into buffer
	curFrame = 0;
	for( quint8 i = 0; i < 3; i++ )
		LoadNextFrame();

	//show first frame in gui
	ShowNextFrame();
}
