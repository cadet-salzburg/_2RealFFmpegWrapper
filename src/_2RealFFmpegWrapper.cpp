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

	This wrapper uses FFmpeg, and is licensed and credited as follows:

	 * copyright (c) 2001 Fabrice Bellard
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

#include "_2RealFFmpegWrapper.h"
#include <iostream>

// ffmpeg includes
extern "C" {
	#define __STDC_CONSTANT_MACROS
	#include "stdint.h"
	#include "libavformat/avformat.h"
	#include "libavcodec/avcodec.h"
	#include "libavutil/avutil.h"
	#include "libswscale/swscale.h"
	#include "libavutil/rational.h"

	//#include "libavutil/opt.h"
}

namespace _2RealFFmpegWrapper
{

FFmpegWrapper::FFmpegWrapper() : m_bIsFileOpen(false), m_bIsImageDecoded(false), m_pFormatContext(nullptr), m_pCodecContext(nullptr),
	m_pSwScalingContext(nullptr), m_pFrame(nullptr), m_pFrameRGB(nullptr), m_pVideoBuffer(nullptr)
{
	init();
}


FFmpegWrapper::FFmpegWrapper(std::string strFileName) : m_bIsFileOpen(false), m_bIsImageDecoded(false), m_pFormatContext(nullptr), m_pCodecContext(nullptr),
	m_pSwScalingContext(nullptr), m_pFrame(nullptr), m_pFrameRGB(nullptr), m_pVideoBuffer(nullptr)
{
	init();
	open(strFileName);
}

FFmpegWrapper::~FFmpegWrapper() 
{
	close();
}

bool FFmpegWrapper::init()
{
	 avcodec_register_all();	// think this line is redundant but other samples use it, so better leave it
	 av_register_all();
	 return true;
}

bool FFmpegWrapper::open(std::string strFileName, bool bPreLoad)
{
	// init property variables
	m_iWidth = m_iHeight = 0;
	m_iLoopMode = eLoop;
	m_dTargetTimeInMs = 0;
	m_lCurrentFrameNumber = -1;	// set to invalid, as it is not decoded yet
	m_dCurrentTimeInMs = -1;	// set to invalid, as it is not decoded yet
	m_fSpeedMultiplier = 1.0;
	m_iDirection = eForward;
	m_iState = eStopped;
	m_strFileName = strFileName;
	m_bIsImageDecoded = false;
	m_bPreLoad = bPreLoad;
	m_lFramePosInPreLoadedFile = 0;

	if(m_bIsFileOpen)
	{
		stop();
		close();
	}

	// Open video file
	m_pFormatContext = avformat_alloc_context();
	if(avformat_open_input(&m_pFormatContext, strFileName.c_str(), NULL, NULL)!=0)
	   return false; // couldn't open file

	// Retrieve stream information
	if(av_find_stream_info(m_pFormatContext)<0)
		return false; // couldn't find stream information

	// Find the first video stream
	m_iVideoStream = m_iAudioStream = -1;
	for(unsigned int i=0; i<m_pFormatContext->nb_streams; i++)
	{
       if((m_iVideoStream < 0) && (m_pFormatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO))
       {
           m_iVideoStream=i;
       }
	   if((m_iAudioStream < 0) && (m_pFormatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO))
       {
           m_iAudioStream=i;
       }
	}

	if(m_iVideoStream==-1 && m_iAudioStream==-1)
       return false; // Didn't find video or audio stream

	if(m_iVideoStream>=0)
	{
	    // Get a pointer to the codec context for the video stream
		m_pCodecContext = m_pFormatContext->streams[m_iVideoStream]->codec;

		// Find the decoder for the video stream
		AVCodec* pCodec = avcodec_find_decoder(m_pCodecContext->codec_id);	// guess this is deleted with the avcodec_close, that's what the docs say
		if(pCodec==NULL)
		   return false; // Codec not found

		// Open codec
		AVDictionary* options;	
		if(avcodec_open2(m_pCodecContext, pCodec, nullptr)<0)
		   return false; // Could not open codec

		// Allocate video frame
		m_pFrame = avcodec_alloc_frame();

		// Allocate an AVFrame structure
		m_pFrameRGB=avcodec_alloc_frame();
		if(m_pFrameRGB==nullptr)
		   return false;

		retrieveVideoInfo();

		// Determine required buffer size and allocate buffer
		//int numBytes=avpicture_get_size(PIX_FMT_RGB24, m_iWidth, m_iHeight); // there is a bug currently in the newest ffmpeg, so let's calc size manually
		m_pVideoBuffer=new uint8_t[m_iWidth * m_iHeight * 3];


		// Assign appropriate parts of buffer to image planes in pFrameRGB
		avpicture_fill((AVPicture*)m_pFrameRGB, m_pVideoBuffer, PIX_FMT_RGB24,m_iWidth,m_iHeight);

		//Initialize Context
		m_pSwScalingContext = sws_getContext(m_pCodecContext->width,m_iHeight, m_pCodecContext->pix_fmt,m_iWidth,m_iHeight, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
	}
	m_bIsFileOpen = true;
	m_strFileName = strFileName;

	if(m_bPreLoad)
	{
		if(preLoad()<=0)	// no frame loaded
		{
			return false;
		}
	}
	return m_bIsFileOpen;
}

void FFmpegWrapper::close()
{
	// Free the RGB image
	if(m_pVideoBuffer!=nullptr)
	{
		delete m_pVideoBuffer;
		m_pVideoBuffer = nullptr;
	}

	if(m_pFrameRGB!=nullptr)
	{
		av_free(m_pFrameRGB);
		m_pFrameRGB = nullptr;
	}

	// Free the YUV frame
	if(m_pFrame!=nullptr)
	{
		av_free(m_pFrame);
		m_pFrame = nullptr;
	}

	if(m_pSwScalingContext!=nullptr)
	{
		sws_freeContext(m_pSwScalingContext);
		m_pSwScalingContext = nullptr;
	}

	// Close the codec
	if(m_pCodecContext!=nullptr)
	{
		avcodec_close(m_pCodecContext);
		m_pCodecContext = nullptr;
	}

	// Close the video file
	if(m_pFormatContext!=nullptr)
	{
		//avformat_close_input(&m_pFormatContext);
		avformat_free_context(m_pFormatContext);	// this line should free all the associated mem with file, todo seriously check on lost mem blocks
		m_pFormatContext = nullptr;
	}
}

void FFmpegWrapper::update(double dElapsedTimeInMs)
{
	// todo check for very big over and underflow
	if(m_iState == ePlaying)
	{
		m_dTargetTimeInMs += dElapsedTimeInMs * m_fSpeedMultiplier * m_iDirection;

		// check for over underflows and correct according to loop mode and set according state
		if(m_dTargetTimeInMs >= m_dDurationInMs || m_dTargetTimeInMs < 0)
		{
			if(m_iLoopMode == eNoLoop)
			{
				stop();
			}
			else if(m_iLoopMode == eLoop)
			{
				if(m_dTargetTimeInMs < 0) // underflow
				{
					m_dTargetTimeInMs = m_dDurationInMs + m_dTargetTimeInMs;	
				}
				else // overflow
				{
					m_dTargetTimeInMs = mod(m_dTargetTimeInMs, m_dDurationInMs);
				}
			}
			else if(m_iLoopMode == eLoopBidi)
			{
				if(m_dTargetTimeInMs < 0) // underflow
				{
					m_dTargetTimeInMs = fabs(m_dTargetTimeInMs);
					m_iDirection = eForward;
				}
				else // overflow
				{
					m_dTargetTimeInMs = m_dDurationInMs + (m_dDurationInMs - m_dTargetTimeInMs);
					m_iDirection = eBackward;
				}
			}
		}
	}
}

void FFmpegWrapper::play()
{
	m_iState = ePlaying;
}

void FFmpegWrapper::pause()
{
	m_iState = ePaused;
}

void FFmpegWrapper::stop()
{
	m_lCurrentFrameNumber = -1;	// set to invalid, as it is not decoded yet
	m_dTargetTimeInMs = 0;
	m_iState = eStopped;
}

unsigned char* FFmpegWrapper::getFrame()
{
	bool isFrameDecoded = false;
	static bool bIsSeekable = true;

	// make sure always to decode 0 frame first, even if the first delta time would suggest differently
	if(m_dCurrentTimeInMs<0)
		m_dTargetTimeInMs = 0;

	// if image decode it and return
	if(m_lDurationInFrames==0)
	{
		if(!m_bIsImageDecoded)
		{
			decodeImage();
			m_bIsImageDecoded = true;	// don't decode again it's a still so I would just wastes performance
		}
		return m_pFrameRGB->data[0];
	}

	long lTargetFrame = calculateFrameNumberFromTime(m_dTargetTimeInMs);
	if(lTargetFrame != m_lCurrentFrameNumber)
	{
		// probe to jump to target frame directly
		if(bIsSeekable)
		{
			seekFrame(lTargetFrame);
			isFrameDecoded = decodeFrame();
			if(!isFrameDecoded)
			{
				seekFrame(0);
				bIsSeekable = false;
			}
		}
			
		if(!bIsSeekable)	// just forward decoding, this is a huge performance issue when playing backwards
		{
			if(lTargetFrame<m_lCurrentFrameNumber)
			{
				seekFrame(0);
				m_lCurrentFrameNumber = m_dCurrentTimeInMs = 0;
			}

			while(!isFrameDecoded)
			{
				isFrameDecoded = decodeFrame();
				m_lCurrentFrameNumber++;
				if(m_lCurrentFrameNumber+1>=m_lDurationInFrames)
				{
					seekFrame(0);
					isFrameDecoded = decodeFrame();
					m_lCurrentFrameNumber = lTargetFrame = 0;
				}
			}
			m_dTargetTimeInMs = (float)(lTargetFrame) * 1.0 / m_fFps * 1000.0;
		}

		m_lCurrentFrameNumber = lTargetFrame;
		m_dCurrentTimeInMs = m_dTargetTimeInMs;
	}
	
	return m_pFrameRGB->data[0];	// return old decoded when decoding didn't work, otherwise this buffer holds the new image
}

void FFmpegWrapper::setFramePosition(long lTargetFrameNumber)
{
	m_dTargetTimeInMs = ((float)(lTargetFrameNumber) * 1.0 / m_fFps * 1000.0);
}

void FFmpegWrapper::setTimePositionInMs(double dTargetTimeInMs)
{
	m_dTargetTimeInMs = dTargetTimeInMs;
}

void FFmpegWrapper::setPosition(float fPos)
{
	if(fPos<0.0)
	{
		fPos=0.0;
	}
	else if(fPos>1.0)
	{
		fPos=1.0;
	}
	m_dTargetTimeInMs = (fPos * m_dDurationInMs);
}

int	FFmpegWrapper::preLoad()
{
	int frames=0;
	m_vFileBuffer.clear();
	m_vFileBuffer.push_back(new AVPacket());
	while(av_read_frame(m_pFormatContext, m_vFileBuffer.back())>=0) 
	{
		frames++;
		m_vFileBuffer.push_back(new AVPacket());
	}

	if(frames<=0)
		m_vFileBuffer.clear();	// just clear vector, nothing else was allocated

	return frames;
}

AVPacket* FFmpegWrapper::fetchAVPacket()
{
	AVPacket *pAVPacket = nullptr;
	if(m_bPreLoad)
	{
		if(m_vFileBuffer[m_lFramePosInPreLoadedFile])
		{
			pAVPacket = m_vFileBuffer[m_lFramePosInPreLoadedFile];
			m_lFramePosInPreLoadedFile = m_lFramePosInPreLoadedFile % (m_vFileBuffer.size() - 1);
			return m_vFileBuffer[m_lFramePosInPreLoadedFile++];
		}
		else
			return nullptr;
	}
	else
	{
		pAVPacket= new AVPacket();
		if(av_read_frame(m_pFormatContext, pAVPacket)>=0)
			return pAVPacket;
		else 
			return nullptr;
	}
}

bool FFmpegWrapper::decodeFrame()
{
	bool bRet = false;
	int isFrameDecoded;
	AVPacket* pAVPacket = fetchAVPacket();
	
	if(pAVPacket!=nullptr)
	{
		// Is this a packet from the video stream?
		if(pAVPacket->stream_index == m_iVideoStream) 
		{
			// Decode video frame
			int iBytes = avcodec_decode_video2(m_pCodecContext, m_pFrame, &isFrameDecoded, pAVPacket);
			/*if(!isFrameDecoded && iBytes>0)
				fetchAVPacket();*/

			// Did we get a video frame?
			if(isFrameDecoded) 
			{
				//Convert YUV->RGB
				sws_scale(m_pSwScalingContext, m_pFrame->data, m_pFrame->linesize, 0,m_iHeight, m_pFrameRGB->data, m_pFrameRGB->linesize);
				bRet = true;
			}
		}
    
		// Free the packet that was allocated by av_read_frame, if not in preLoad mode
		if(!m_bPreLoad)
			av_free_packet(pAVPacket);
	}
	return bRet;
}

bool FFmpegWrapper::decodeImage()
{
	int isFrameDecoded=-1;

	AVPacket packet;
	// alloc img buffer
	FILE *imgFile = fopen(m_strFileName.c_str(),"rb");
	fseek(imgFile,0,SEEK_END);
	long imgFileSize = ftell(imgFile);
	fseek(imgFile,0,SEEK_SET);
	void *imgBuffer = malloc(imgFileSize);
	fread(imgBuffer,1,imgFileSize,imgFile);
	fclose(imgFile);
	packet.data = (uint8_t*)imgBuffer;
	packet.size = imgFileSize;
	av_init_packet(&packet);
	//decode image
	avcodec_decode_video2(m_pCodecContext, m_pFrame, &isFrameDecoded, &packet);

	if(isFrameDecoded)	// Did we get a video frame? 
	{
		//Convert YUV->RGB
		sws_scale(m_pSwScalingContext, m_pFrame->data, m_pFrame->linesize, 0,m_iHeight, m_pFrameRGB->data, m_pFrameRGB->linesize);
		av_free_packet(&packet);
		free(imgBuffer);			// we have to free this buffer separately don't ask me why, otherwise leak
		return true;
	}
	else
	{
		free(imgBuffer);
	    av_free_packet(&packet);
		return false;
	}
}

bool FFmpegWrapper::seekFrame(long lTargetFrameNumber)
{
	int iDirectionFlag = 0;
	if(m_iDirection == eBackward)
		iDirectionFlag = AVSEEK_FLAG_BACKWARD;
	
	if(avformat_seek_file(m_pFormatContext, m_iVideoStream, lTargetFrameNumber, lTargetFrameNumber, lTargetFrameNumber, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD) < 0)
	{
		return false;
	}
	avcodec_flush_buffers(m_pCodecContext);
	
	return true;
}

bool FFmpegWrapper::seekTime(double dTimeInMs)
{
	return seekFrame( calculateFrameNumberFromTime(dTimeInMs) );
}

unsigned char* FFmpegWrapper::getAudio()
{
	return nullptr;
}
	
unsigned char* FFmpegWrapper::getAudio(int iFrame)
{
	return nullptr;
}

void FFmpegWrapper::setLoopMode(int iLoopMode)
{
	m_iLoopMode = iLoopMode;
}

int	FFmpegWrapper::getLoopMode()
{
	return m_iLoopMode;
}

void FFmpegWrapper::setSpeed(float fSpeed)	
{
	m_fSpeedMultiplier = fabs(fSpeed);	// just positiv values, direction is set separately
}

unsigned int FFmpegWrapper::getWidth()
{
	return m_iWidth;
}
		
unsigned int FFmpegWrapper::getHeight()
{
	return m_iHeight;
}

int FFmpegWrapper::getState()
{
	return m_iState;
}

float FFmpegWrapper::getFps()
{
	return m_fFps;
}

float FFmpegWrapper::getSpeed()
{
	return m_fSpeedMultiplier;
}

int	FFmpegWrapper::getBitrate()
{
	return m_iBitrate;
}

long FFmpegWrapper::getCurrentFrameNumber()
{
	return m_lCurrentFrameNumber;
}

double FFmpegWrapper::getCurrentTimeInMs()
{
	return m_dTargetTimeInMs;
}

unsigned long FFmpegWrapper::getDurationInFrames()
{
	return m_lDurationInFrames;
}

double FFmpegWrapper::getDurationInMs()
{
	return m_dDurationInMs;
}

int	FFmpegWrapper::getDirection()
{
	return m_iDirection;
}

void FFmpegWrapper::setDirection(int iDirection)
{
	m_iDirection = iDirection;
}

std::string FFmpegWrapper::getCodecName()
{
	return m_strCodecName;
}

std::string FFmpegWrapper::getFileName()
{
	return m_strFileName;
}

bool FFmpegWrapper::hasVideo()
{
	return m_bIsFileOpen && m_iVideoStream >= 0;
}
		
bool FFmpegWrapper::hasAudio()
{
	return m_bIsFileOpen && m_iAudioStream >= 0;
}

bool FFmpegWrapper::isNewFrame()
{
	// make sure always to decode 0 frame first, even if the first delta time would suggest differently
	if(m_dCurrentTimeInMs<0)
		m_dTargetTimeInMs = 0;
	long lTargetFrame = calculateFrameNumberFromTime(m_dTargetTimeInMs);
	return (lTargetFrame != m_lCurrentFrameNumber);
}

void FFmpegWrapper::retrieveVideoInfo()
{
	m_iWidth = m_pCodecContext->width;
	m_iHeight = m_pCodecContext->height;
	m_iBitrate = m_pFormatContext->bit_rate / 1000.0;
	m_dDurationInMs = m_pFormatContext->duration * 1000.0 / (float)AV_TIME_BASE;
	m_fFps = (float)m_pCodecContext->time_base.den * (1.0 / (float)m_pCodecContext->time_base.num);
	if(m_fFps>100)
		m_fFps = 1.0 /((m_dDurationInMs / (float)m_lDurationInFrames) / 1000.0);

	//m_lDurationInFrames = m_pFormatContext->streams[m_iVideoStream]->nb_frames;		// for some codec this return wrong numbers so calc from time
	m_lDurationInFrames = calculateFrameNumberFromTime(m_dDurationInMs);
	
	//if(m_pFormatContext->streams[m_iVideoStream]->nb_frames == 0)	// couldn't retrieve it from metainfo so calc from time duration
	//	m_lDurationInFrames = (float)m_dDurationInMs / 1000.0 / (1.0 / m_fFps); 

	m_strCodecName = std::string(m_pCodecContext->codec->long_name);
}

void FFmpegWrapper::dumpFFmpegInfo()
{
	std::cout << "_2RealFFmpegWrapper 0.1" << std::endl;
	std::cout << "http://www.cadet.at" << std::endl << std::endl;
	std::cout << "Please regard the license of the here wrapped FFmpeg library, LGPL or GPL_V3 depending on enabled codecs and configuration" << std::endl;
	std::cout << "FFmpeg license: " << avformat_license() << std::endl;
	std::cout << "AVCodec version " << LIBAVFORMAT_VERSION_MAJOR << "." << LIBAVFORMAT_VERSION_MINOR << "." << LIBAVFORMAT_VERSION_MICRO << std::endl;
	std::cout << "AVFormat configuration: " << avformat_configuration() << std::endl << std::endl;
}

long FFmpegWrapper::calculateFrameNumberFromTime(long lTime)
{
	//av_rescale(lTimeInMs,m_pFormatContext->streams[m_iVideoStream]->time_base.den,m_pFormatContext->streams[m_iVideoStream]->time_base.num) / 1000;
	long lTargetFrame = floor((double)lTime/1000.0 * m_fFps);
	return lTargetFrame;
}

double FFmpegWrapper::mod(double a, double b)
{
	int result = static_cast<int>( a / b );
	return a - static_cast<double>( result ) * b;
}

};