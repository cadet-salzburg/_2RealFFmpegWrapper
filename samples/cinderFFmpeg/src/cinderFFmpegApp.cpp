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
	Created: 16-04-2011

	This sample uses LibCinder and _2RealFFmpegWwrapper, and of course FFmpeg
*/

#include "_2RealFFmpegWrapper.h"

// cinder
#include "cinder/app/AppBasic.h"
#include "cinder/gl/Texture.h"
#include "cinder/params/Params.h"
#include "cinder/Utilities.h"

using namespace ci;
using namespace ci::app;

class cinderFFmpegApp : public AppBasic 
{
public:
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
	
	std::vector<std::shared_ptr<_2RealFFmpegWrapper::FFmpegWrapper> >		m_Players;
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
};

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
	if(testFile->open(".\\data\\morph.avi"))
	{
		m_Players.push_back(testFile);
		m_Players.back()->play();
	}

	m_dLastTime = 0;
	m_iCurrentVideo = 0;
	m_fSpeed = 1;
	m_iLoopMode = _2RealFFmpegWrapper::eLoop;
	m_iTilesDivisor = 1;
	m_fSeekPos = m_fOldSeekPos = 0;
}

void cinderFFmpegApp::update()
{
	// set playing properties of current video
	if(m_Players.size()<=0)
		return;

	m_Players[m_iCurrentVideo]->setSpeed(m_fSpeed);
	m_Players[m_iCurrentVideo]->setLoopMode(m_iLoopMode);
	
	// update the frame calculation
	double dElapsedTime = app::getElapsedSeconds() * 1000.0; 
	long lDeltaTime = long(dElapsedTime - m_dLastTime);
	m_dLastTime = dElapsedTime;

	for(int i=0; i<m_Players.size(); i++)
	{
		m_Players[i]->update(lDeltaTime);
	}
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
		if(m_Players[i]->hasVideo())
		{	
			unsigned char* pImg = m_Players[i]->getFrame();
			if(pImg != nullptr)
			{		
				posX = (i % m_iTilesDivisor) * m_iTileWidth;
				posY = ((int(float(i) / float(m_iTilesDivisor))) % m_iTilesDivisor) * m_iTileHeight;

				Rectf imgRect = Rectf(posX, posY, posX + m_iTileWidth, posY + m_iTileHeight);
				ci::gl::draw(ci::gl::Texture( ci::Surface(pImg, m_Players[i]->getWidth(), m_Players[i]->getHeight(), m_Players[i]->getWidth() * 3, ci::SurfaceChannelOrder::RGB) ), imgRect);
			}
		}
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
			m_Players.back()->play();
		}
	}
}

void cinderFFmpegApp::clearAll()
{
	m_Players.clear();
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
	strTmp << "label='video codec: " << m_Players[m_iCurrentVideo]->getCodecName() << "'";
	m_Gui.setOptions("codec", strTmp.str());
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
	m_Gui.addText("codec", strTmp.str());
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
			if(m_iCurrentVideo>=m_Players.size())
				m_iCurrentVideo = m_Players.size() - 1;
			if(m_iCurrentVideo<=0)
				m_iCurrentVideo = 0;
		}
	}
 }

CINDER_APP_BASIC( cinderFFmpegApp, RendererGl )