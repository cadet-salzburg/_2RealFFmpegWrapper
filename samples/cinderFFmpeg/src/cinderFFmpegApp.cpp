/*
	CADET - Center for Advances in Digital Entertainment Technologies
	Copyright 2012 University of Applied Science Salzburg / MultiMediaTechnology

	http://www.cadet.at
	http://multimediatechnology.at/

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.

	CADET - Center for Advances in Digital Entertainment Technologies
	 
	Authors: Robert Praxmarer
	Web: http://www.1n0ut.com
	Email: support@cadet.at

	This sample uses LibCinder and _2RealFFmpegWwrapper, and of course FFmpeg
*/


#include "_2RealFFmpegWrapper.h"

// cinder
#include "cinder/app/AppBasic.h"
#include "cinder/gl/Texture.h"
#include "cinder/audio/Output.h"
#include "cinder/audio/Callback.h"
#include "cinder/params/Params.h"
#include "cinder/Utilities.h"

#include "fmod.hpp"

using namespace ci;
using namespace ci::app;

class cinderFFmpegApp : public AppBasic 
{
public:
	cinderFFmpegApp::cinderFFmpegApp()
	{
		m_Instance = this;
	};

	void prepareSettings(Settings* settings);
	void setup();
	void update();
	void draw();
	void mouseDown( MouseEvent event );
	void fileDrop( FileDropEvent event );
	
private:
	void setupGui();
	void updateGui();
	void open();
	void clearAll();
	void pause();
	void stop();
	void toggleDirection();
	int	 calcTileDivisor(int size);
	int  calcSelectedPlayer(int x, int y);
	void audioCallback( uint64_t inSampleOffset, uint32_t ioSampleCount, audio::Buffer16u *ioBuffer );
	
	static FMOD_RESULT F_CALLBACK pcmreadcallback(FMOD_SOUND *sound, void *data, unsigned int datalen);

	static	cinderFFmpegApp*												m_Instance;
	std::vector<std::shared_ptr<_2RealFFmpegWrapper::FFmpegWrapper> >		m_Players;
	std::vector<ci::gl::Texture>											m_VideoTextures;
	ci::params::InterfaceGl													m_Gui;
	ci::Font																m_Font;
	int																		m_iCurrentVideo;
	double																	m_dLastTime;
	float																	m_fSeekPos;
	float																	m_fOldSeekPos;
	float																	m_fSpeed;
	int																		m_iLoopMode;
	int																		m_iTilesDivisor;
	int																		m_iTileWidth;
	int																		m_iTileHeight;

	//fmod
	FMOD::System           *m_pSystem;
    FMOD::Sound            *m_pSound;
    FMOD::Channel          *m_pChannel;
    FMOD_RESULT             result;
    FMOD_CREATESOUNDEXINFO  createsoundexinfo;
};

cinderFFmpegApp* cinderFFmpegApp::m_Instance;

void cinderFFmpegApp::prepareSettings(Settings* settings)
{							
	FILE* f;
	AllocConsole();
	freopen_s( &f, "CON", "w", stdout );

	settings->setTitle("CADET | _2RealFFmpegWrapper Cinder Sample");
	settings->setWindowSize(800,600);
	settings->setResizable(false);
}

void cinderFFmpegApp::setup()
{
	m_Font = ci::Font("Consolas", 48);
	setupGui();
	gl::enableAlphaBlending();

	std::shared_ptr<_2RealFFmpegWrapper::FFmpegWrapper> testFile = std::shared_ptr<_2RealFFmpegWrapper::FFmpegWrapper>(new _2RealFFmpegWrapper::FFmpegWrapper());
	testFile->dumpFFmpegInfo();
	//if(testFile->open(".\\data\\morph.avi"))
	if(testFile->open("d:\\vjing\\final4hallein.avi"))
	{
		m_Players.push_back(testFile);
		m_VideoTextures.push_back(gl::Texture());
		m_Players.back()->play();
	}

	m_dLastTime = 0;
	m_iCurrentVideo = 0;
	m_fSpeed = 1;
	m_iLoopMode = _2RealFFmpegWrapper::eLoop;
	m_iTilesDivisor = 1;
	m_fSeekPos = m_fOldSeekPos = 0;

	//setup fmod
	FMOD::System_Create(&m_pSystem);
    m_pSystem->init(32, FMOD_INIT_NORMAL, 0);
   
	memset(&createsoundexinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
    createsoundexinfo.cbsize            = sizeof(FMOD_CREATESOUNDEXINFO);              /* required. */
    createsoundexinfo.decodebuffersize  = 1024;                                       /* Chunk size of stream update in samples.  This will be the amount of data passed to the user callback. */
    createsoundexinfo.numchannels       = 2;                        /* Number of channels in the sound. */
    createsoundexinfo.length            = -1;                     /* Length of PCM data in bytes of whole song.  -1 = infinite. */
    createsoundexinfo.defaultfrequency  = (int)44100;                       /* Default playback rate of sound. */
	createsoundexinfo.format            = FMOD_SOUND_FORMAT_PCM16;                    /* Data format of sound. */
	createsoundexinfo.pcmreadcallback   =  pcmreadcallback;                             /* User callback for reading. */

	result = m_pSystem->createStream(0, FMOD_2D | FMOD_OPENUSER | FMOD_LOOP_OFF, &createsoundexinfo, &m_pSound);
	result = m_pSystem->playSound(FMOD_CHANNEL_FREE, m_pSound, 0, &m_pChannel);

	//std::shared_ptr<audio::Callback<cinderFFmpegApp,unsigned short>> audioCallback = audio::createCallback( this, &cinderFFmpegApp::audioCallback );
	//audio::Output::play( audioCallback );
}

void cinderFFmpegApp::update()
{
	// set playing properties of current video
	if(m_Players.size()<=0)
		return;

	m_Players[m_iCurrentVideo]->setSpeed(m_fSpeed);
	m_Players[m_iCurrentVideo]->setLoopMode(m_iLoopMode);
	
	updateGui();

	// scrubbing through stream
	if(m_fSeekPos != m_fOldSeekPos)
	{
		m_fOldSeekPos = m_fSeekPos;
		m_Players[m_iCurrentVideo]->pause();
		m_Players[m_iCurrentVideo]->setPosition(m_fSeekPos);
	}

	m_iTilesDivisor = calcTileDivisor(m_Players.size());
	m_iTileWidth = getWindowWidth() / m_iTilesDivisor;
	m_iTileHeight = getWindowHeight() / m_iTilesDivisor;
}

void cinderFFmpegApp::draw()
{
	Rectf imgRect;
	int posX;
	int posY;

	gl::clear();
	
	for(int i=0; i<m_Players.size(); i++)
	{
		if(m_Players[i]->hasVideo()) //&& m_Players[i]->isNewFrame())
		{	
		//	m_Players[i]->update();
			unsigned char* pImg = m_Players[i]->getVideoData().m_pData;
			if(pImg != nullptr)
			{		
				m_VideoTextures[i] = gl::Texture(ci::Surface(pImg, m_Players[i]->getWidth(), m_Players[i]->getHeight(), m_Players[i]->getWidth() * 3, ci::SurfaceChannelOrder::RGB) );
			}
		}
		posX = (i % m_iTilesDivisor) * m_iTileWidth;
		posY = ((int(float(i) / float(m_iTilesDivisor))) % m_iTilesDivisor) * m_iTileHeight;
		Rectf imgRect = Rectf(posX, posY, posX + m_iTileWidth, posY + m_iTileHeight);
		if(m_VideoTextures[i])
			ci::gl::draw( m_VideoTextures[i] , imgRect);
	}
	
	// draw green selection frame
	if(m_Players.size()>0)
	{
		posX = (m_iCurrentVideo % m_iTilesDivisor) * m_iTileWidth;
		posY = ((int(float(m_iCurrentVideo) / float(m_iTilesDivisor))) % m_iTilesDivisor) * m_iTileHeight;
		gl::color(0,1,0,1);
		glLineWidth(3);
		gl::drawStrokedRect(Rectf(posX, posY, posX + m_iTileWidth, posY + m_iTileHeight));
		gl::color(1,1,1,1);
	}
			
	// draw fps and gui
	gl::drawString( toString(getAverageFps()), Vec2f( float(getWindowWidth() - 240), 10.0 ), Color(1,0,0), m_Font);
	m_Gui.draw();
}

void cinderFFmpegApp::open()
{
	fs::path moviePath = getOpenFilePath();
	if( ! moviePath.empty() )
	{
		std::shared_ptr<_2RealFFmpegWrapper::FFmpegWrapper> fileToLoad = std::shared_ptr<_2RealFFmpegWrapper::FFmpegWrapper>(new _2RealFFmpegWrapper::FFmpegWrapper());
		if(fileToLoad->open(moviePath.string()))
		{
			m_Players.push_back(fileToLoad);
			m_VideoTextures.push_back(gl::Texture());
			m_Players.back()->play();
		}
	}
}

void cinderFFmpegApp::fileDrop( FileDropEvent event )
{
	for(int i=0; i<event.getFiles().size(); i++)
	{
		std::shared_ptr<_2RealFFmpegWrapper::FFmpegWrapper> fileToLoad = std::shared_ptr<_2RealFFmpegWrapper::FFmpegWrapper>(new _2RealFFmpegWrapper::FFmpegWrapper());
		if(fileToLoad->open(event.getFile(i).string()))
		{
			m_Players.push_back(fileToLoad);
			m_VideoTextures.push_back(gl::Texture());
			m_Players.back()->play();
		}
	}
}

void cinderFFmpegApp::clearAll()
{
	m_Players.clear();
	m_VideoTextures.clear();
}

void cinderFFmpegApp::pause()
{
	if(m_Players[m_iCurrentVideo]->getState() == _2RealFFmpegWrapper::ePlaying)
		m_Players[m_iCurrentVideo]->pause();
	else
		m_Players[m_iCurrentVideo]->play();
}

void cinderFFmpegApp::stop()
{
	m_Players[m_iCurrentVideo]->stop();
}

void cinderFFmpegApp::toggleDirection()
{
	if(m_Players[m_iCurrentVideo]->getDirection() == _2RealFFmpegWrapper::eForward)
		m_Players[m_iCurrentVideo]->setDirection(_2RealFFmpegWrapper::eBackward);
	else
		m_Players[m_iCurrentVideo]->setDirection(_2RealFFmpegWrapper::eForward);
}

void cinderFFmpegApp::updateGui()
{
	// Video / Audio Infos
	// update dynamic info of video/audio
	std::stringstream strTmp;
	strTmp << "label='file:  " << m_Players[m_iCurrentVideo]->getFileName() << "'";
	m_Gui.setOptions( "file", strTmp.str() );
	strTmp.clear();	strTmp.str("");
	strTmp << "label='frame:  " << m_Players[m_iCurrentVideo]->getCurrentFrameNumber() << "/" << m_Players[m_iCurrentVideo]->getDurationInFrames() << "'";
	m_Gui.setOptions( "frame", strTmp.str() );
	strTmp.clear();	strTmp.str("");
	strTmp << "label='time:  " << m_Players[m_iCurrentVideo]->getCurrentTimeInMs() << ":" << m_Players[m_iCurrentVideo]->getDurationInMs() << "'";
	m_Gui.setOptions( "time", strTmp.str() );
	strTmp.clear();	strTmp.str("");
	strTmp << "label='video codec: " << m_Players[m_iCurrentVideo]->getVideoCodecName() << "'";
	m_Gui.setOptions("video codec", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='audio codec: " << m_Players[m_iCurrentVideo]->getAudioCodecName() << "'";
	m_Gui.setOptions("audio codec", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='width: " << m_Players[m_iCurrentVideo]->getWidth() << "'";
	m_Gui.setOptions("width", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='height: " << m_Players[m_iCurrentVideo]->getHeight() << "'";
	m_Gui.setOptions("height", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='fps: " << m_Players[m_iCurrentVideo]->getFps() << "'";
	m_Gui.setOptions("fps", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='bitrate:  " << m_Players[m_iCurrentVideo]->getBitrate() << "'";
	m_Gui.setOptions("bitrate", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='audio channels:  " << m_Players[m_iCurrentVideo]->getAudioChannels() << "'";
	m_Gui.setOptions("audio channels", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='audio sample rate:  " << m_Players[m_iCurrentVideo]->getAudioSampleRate() << "'";
	m_Gui.setOptions("audio sample rate", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='frame:  " << m_Players[m_iCurrentVideo]->getCurrentFrameNumber() << "/" << m_Players[m_iCurrentVideo]->getDurationInFrames() << "'";
	m_Gui.setOptions("frame", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='time:  " << m_Players[m_iCurrentVideo]->getCurrentTimeInMs() << "/" << m_Players[m_iCurrentVideo]->getDurationInMs() << "'";
	m_Gui.setOptions("time", strTmp.str());
}

void cinderFFmpegApp::setupGui()
{
	std::stringstream strTmp;
	m_Gui = ci::params::InterfaceGl("FFmpeg Player", ci::Vec2i(300,340));

	// Video / Audio Infos
	m_Gui.addButton( "open", std::bind( &cinderFFmpegApp::open, this ) );
	m_Gui.addButton( "clear all", std::bind( &cinderFFmpegApp::clearAll, this ) );

	m_Gui.addSeparator();
	strTmp << "label='file: '";
	m_Gui.addText("file", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='video codec: '";
	m_Gui.addText("video codec", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='audio codec: '";
	m_Gui.addText("audio codec", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='width: '";
	m_Gui.addText("width", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='height: '";
	m_Gui.addText("height", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='fps: '";
	m_Gui.addText("fps", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='bitrate: '";
	m_Gui.addText("bitrate", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='audio channels: '";
	m_Gui.addText("audio channels", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='audio sample rate: '";
	m_Gui.addText("audio sample rate", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='frame: '";
	m_Gui.addText("frame", strTmp.str());
	strTmp.clear();	strTmp.str("");
	strTmp << "label='time: '";
	m_Gui.addText("time", strTmp.str());
	m_Gui.addSeparator();
	m_Gui.addButton( "play/pause", std::bind( &cinderFFmpegApp::pause, this ) );
	m_Gui.addButton( "stop", std::bind( &cinderFFmpegApp::stop, this ) );
	m_Gui.addButton( "toggle direction", std::bind( &cinderFFmpegApp::toggleDirection, this ) );
	m_Gui.addSeparator();
	m_Gui.addParam("speed", &m_fSpeed, "min=0 max=8.0 step=0.05");
	m_Gui.addParam("0..none, 1..loop, 2..loopBidi", &m_iLoopMode, "min=0 max=2 step=1");
	m_Gui.addParam("seek frame", &m_fSeekPos, "min=0.0 max=1.0 step=0.01");
}


FMOD_RESULT F_CALLBACK cinderFFmpegApp::pcmreadcallback(FMOD_SOUND *sound, void *data, unsigned int datalen)
{
  static long oldDts=-1;

  _2RealFFmpegWrapper::AudioData audioData = m_Instance->m_Players[m_Instance->m_iCurrentVideo]->getAudioData();
 
	
	int len = 2048*sizeof(short);
	memcpy(data, audioData.m_pData, datalen);
  
	oldDts = audioData.m_lDts;
    return FMOD_OK;
}

void cinderFFmpegApp::audioCallback( uint64_t inSampleOffset, uint32_t ioSampleCount, audio::Buffer16u *ioBuffer ) 
{
	_2RealFFmpegWrapper::AudioData audioData = m_Players[m_iCurrentVideo]->getAudioData();
	unsigned char* data = audioData.m_pData;
	

	/*int lSize = audioData.m_lSizeInBytes;
	static short buffer[4096];
	
	long lPts = audioData.m_lPts;
	int part = (lPts/1024) % 2;
	memcpy(&buffer[part*2048], data, lSize);

	if( part == 0)
		return;

	memcpy(ioBuffer->mData, buffer, 4096*sizeof(short));*/

	int j=0;
	for( int  i = 0; i < ioSampleCount; i++ ) 
	{
		short val = *(reinterpret_cast<unsigned short*>(&data[j]));
		//short val1 = *(reinterpret_cast<short*>(&m_Players[m_iCurrentVideo]->getAudioData().m_pData[j+2]));

		ioBuffer->mData[i*ioBuffer->mNumberChannels] = val;
		ioBuffer->mData[i*ioBuffer->mNumberChannels + 1] = val;
		j+=2;
	}
}

int	cinderFFmpegApp::calcTileDivisor(int size)
{
	float fTmp = sqrt(double(size));
	if((fTmp - int(fTmp))>0)
		fTmp++;
	return int(fTmp);
}

 int cinderFFmpegApp::calcSelectedPlayer(int x, int y)
 {
	int selected = int((float(x) / float(m_iTileWidth))) + m_iTilesDivisor * int( (float(y) / float(m_iTileHeight)));
	
	return selected;
 }

 void cinderFFmpegApp::mouseDown( MouseEvent event )
 {
	if(event.isLeft())
	{
		int selected = calcSelectedPlayer(event.getX(), event.getY()); 
		if(selected < m_Players.size())
			m_iCurrentVideo = selected;
	}
	else if(event.isRight())
	{
		int selected = calcSelectedPlayer(event.getX(), event.getY()); 
		if(selected < m_Players.size())
		{
			m_iCurrentVideo = selected;
			m_Players.erase(m_Players.begin() + selected);		
			m_VideoTextures.erase(m_VideoTextures.begin() + selected);		
			if(m_iCurrentVideo>=m_Players.size())
				m_iCurrentVideo = m_Players.size() - 1;
			if(m_iCurrentVideo<=0)
				m_iCurrentVideo = 0;
		}
	}
 }


CINDER_APP_BASIC( cinderFFmpegApp, RendererGl )