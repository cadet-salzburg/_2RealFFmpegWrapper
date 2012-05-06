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

	This wrapper uses FFmpeg, and is credited as follows:
*/
/*	 * copyright (c) 2001 Fabrice Bellard
	 *
	 * This  FFmpeg.
	 *
	 * FFmpeg is free software; you can redistribute it and/or
	 * modify it under the terms of the GNU Lesser General Public
	 * License as published by the Free Software Foundation; either
	 * version 2.1 of the License, or (at your option) any later version.
	 *
	 * FFmpeg is distributed in the hope that it will be useful,
	 * but WITHOUT ANY WARRANTY; without even the implied warranty of
	 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	 * Lesser General Public License for more details.
	 *
	 * You should have received a copy of the GNU Lesser General Public
	 * License along with FFmpeg; if not, write to the Free Software
	 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA	 
*/

#pragma once

#include <string>
#include <vector>
#include <boost/chrono.hpp>
#include <boost/thread.hpp>

// forward declarations
struct AVFormatContext;
struct AVCodecContext;
struct SwsContext;
struct AVFrame;
struct AVPacket;

namespace _2RealFFmpegWrapper
{
	enum {eNoLoop, eLoop, eLoopBidi};
	enum {eOpened, ePlaying, ePaused, eStopped, eEof, eError};
	enum {eForward=1, eBackward=-1};
	enum {eMajorVersion=0, eMinorVersion=1, ePatchVersion=0}; 

	class FFmpegWrapper
	{
	public:
		FFmpegWrapper();
		FFmpegWrapper(std::string strFilename);
		virtual ~FFmpegWrapper();

		bool init();
		bool open(std::string strFileName);
		void close();
		void play();
		void stop();
		void pause();

		unsigned char*	getFrame();
		void			setFramePosition(long lTargetFrameNumber);
		void			setTimePositionInMs(double dTargetTimeInMs);
		void		    setPosition(float fPos);	// between 0 .. 1 for begin and end of stream
		unsigned char*	getAudio();
		unsigned char*	getAudio(int iFrame);
		unsigned int	getWidth();
		unsigned int	getHeight();
		int				getState();
		float			getFps();
		float			getSpeed();
		int				getBitrate();
		long			getCurrentFrameNumber();
		double			getCurrentTimeInMs();
		unsigned long	getDurationInFrames();
		double			getDurationInMs();
		int				getDirection();
		void			setDirection(int iDirection);
		int				getLoopMode();
		std::string		getCodecName();
		std::string		getFileName();
		void			setLoopMode(int iMode);
		void			setSpeed(float fSpeed);		// multiplier, no negative values, direction is setDirection
		bool			hasVideo();
		bool			hasAudio();
		bool			isImage();
		bool			isNewFrame();
		void			dumpFFmpegInfo();

	private:
		
		bool			openVideoStream();
		bool			openAudioStream();
		void			threadedPlayer();
		bool			seekFrame(long lFrameNumber);
		bool			seekTime(double dTimeInMs);
		bool			decodeFrame();
		bool			decodeVideoFrame(AVPacket* pAVPacket);
		bool			decodeAudioFrame(AVPacket* pAVPacket);
		bool			decodeImage();
		AVPacket*		fetchAVPacket();
		int				preLoad();
		void			retrieveVideoInfo();
		void			update();
		double			getDeltaTime();
		long			calculateFrameNumberFromTime(long lTime);
		double			mod(double a, double b);

		AVFormatContext*		m_pFormatContext;
		AVCodecContext*			m_pCodecContext;
		SwsContext*				m_pSwScalingContext;
		AVFrame*				m_pFrame;
		AVFrame*				m_pFrameRGB;
		std::vector<AVPacket*>	m_vFileBuffer;		// stores all frames of a file
		std::string				m_strFileName;	
		std::string				m_strCodecName;
		unsigned char*			m_pVideoBuffer;
		double					m_dCurrentTimeInMs;
		double					m_dTargetTimeInMs;
		double					m_dDurationInMs;
		float					m_fFps;
		float					m_fSpeedMultiplier;			// default 1.0, no negative values
		unsigned long			m_lDurationInFrames;			// length in frames of file, or if cueIn and out are set frames between this range 
		long					m_lCurrentFrameNumber;		// current framePosition ( if cue positions are set e.g. startCueFrame = 10, currentframe at absolute pos 10 is set to 0 (range between 10 and 500 --> current frame 0 .. 490)
		unsigned long			m_lFramePosInPreLoadedFile;
		unsigned long			m_lCueInFrameNumber;   // default = 0
		unsigned long			m_lCueOutFrameNumber;  // default = maxNrOfFrames (end of file)
		int						m_iVideoStream;
		int						m_iAudioStream;
		int						m_iContentType;				// 0 .. video with audio, 1 .. just video, 2 .. just audio, 3 .. image	// 2RealEnumeration
		int						m_iWidth;
		int						m_iHeight;
		int						m_iBitrate;
		int						m_iDirection;
		int						m_iLoopMode;					// 0 .. once, 1 .. loop normal, 2 .. loop bidirectional, default is loop
		int						m_iState;
		bool					m_bIsFileOpen;
		bool					m_bIsThreadRunning;
		boost::thread			m_PlayerThread;
		boost::mutex			m_Mutex;
	    boost::chrono::system_clock::time_point m_OldTime;
		bool					isFrameDecoded;
	};
};