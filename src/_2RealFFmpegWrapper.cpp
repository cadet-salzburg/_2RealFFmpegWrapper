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

#define EPS 0.000025	// epsilon for checking unsual results as taken from OpenCV FFmeg player
namespace _2RealFFmpegWrapper
{

FFmpegWrapper::FFmpegWrapper() : m_bIsInitialized(false)
{
	init();
}


FFmpegWrapper::FFmpegWrapper(std::string strFileName) : m_bIsInitialized(false) 
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
	if(!m_bIsInitialized)
	{
		avformat_network_init();
		av_register_all();
		av_log_set_level(AV_LOG_ERROR);   
	}
	return true;
}

void FFmpegWrapper::initPropertyVariables()
{
	// init property variables
	m_bIsFileOpen = false;
	m_bIsThreadRunning = false; 
	m_pFormatContext = nullptr;
	m_pVideoCodecContext = nullptr;
	m_pAudioCodecContext = nullptr;
	m_pSwScalingContext = nullptr;
	m_pVideoFrame = nullptr;
	m_pVideoFrameRGB = nullptr;
	m_pVideoBuffer = nullptr;
	m_pAudioFrame = nullptr;
	
	m_iLoopMode = eLoop;
	m_dTargetTimeInMs = 0;
	m_lCurrentFrameNumber = -1;	// set to invalid, as it is not decoded yet
	m_dCurrentTimeInMs = -1;	// set to invalid, as it is not decoded yet
	m_fSpeedMultiplier = 1.0;
	m_dFps = 0;
	m_iBitrate = 0;
	m_lDurationInFrames = 0;
	m_dDurationInMs = 0;
	m_iDirection = eForward;
	m_iState = eStopped;
	m_lFramePosInPreLoadedFile = 0;

	m_AVData.m_VideoData.m_iWidth = 0;
	m_AVData.m_VideoData.m_iHeight = 0;
	m_AVData.m_VideoData.m_pData = nullptr;
	m_AVData.m_VideoData.m_lPts = 0;
	m_AVData.m_VideoData.m_lDts = 0;
	m_AVData.m_VideoData.m_iChannels = 0;

	m_AVData.m_AudioData.m_iChannels = 0;
	m_AVData.m_AudioData.m_iSampleRate = 0;
	m_AVData.m_AudioData.m_lSizeInBytes = 0;
	m_AVData.m_AudioData.m_lPts = 0;
	m_AVData.m_AudioData.m_lDts = 0;
	m_AVData.m_AudioData.m_pData = nullptr;
}

bool FFmpegWrapper::open(std::string strFileName)
{
	initPropertyVariables();
	m_strFileName = strFileName;

	if(m_bIsFileOpen)
	{
		stop();
		close();
	}

	// Open video file
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

	if(!(hasVideo() || hasAudio()))
       return false; // Didn't find video or audio stream

	if(hasVideo())
	{
		if(!openVideoStream())
			return false;
	}

	if(hasAudio())
	{
		if(!openAudioStream())
			return false;
	}

	// general file info equal for audio and video stream
	retrieveFileInfo();

	m_bIsFileOpen = true;
	m_strFileName = strFileName;

	// content is image, just decode once 
	if(isImage())
	{
		m_dDurationInMs = 0;
		m_dFps = 0;
		m_lCurrentFrameNumber = 1;
		decodeImage();
	}

	// start timer
	m_OldTime = boost::chrono::system_clock::now();

	return m_bIsFileOpen;
}

bool FFmpegWrapper::openVideoStream()
{
	// Get a pointer to the codec context for the video stream
	m_pVideoCodecContext = m_pFormatContext->streams[m_iVideoStream]->codec;

	// Find the decoder for the video stream
	AVCodec* pCodec = avcodec_find_decoder(m_pVideoCodecContext->codec_id);	// guess this is deleted with the avcodec_close, that's what the docs say
	if(pCodec==NULL)
		return false; // Codec not found

	// Open codec
	AVDictionary* options;	
	if(avcodec_open2(m_pVideoCodecContext, pCodec, nullptr)<0)
		return false; // Could not open codec

	// Allocate video frame
	m_pVideoFrame = avcodec_alloc_frame();

	// Allocate an AVFrame structure
	m_pVideoFrameRGB=avcodec_alloc_frame();
	if(m_pVideoFrameRGB==nullptr)
		return false;

	retrieveVideoInfo();

	// Determine required buffer size and allocate buffer
	m_pVideoBuffer=new uint8_t[ avpicture_get_size( PIX_FMT_RGB24, getWidth(), getHeight())];

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	avpicture_fill((AVPicture*)m_pVideoFrameRGB, m_pVideoBuffer, PIX_FMT_RGB24, getWidth(), getHeight());
	 
	//Initialize Context
	m_pSwScalingContext = sws_getContext(getWidth(), getHeight(), m_pVideoCodecContext->pix_fmt, getWidth(), getHeight(), PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	return true;
}

bool FFmpegWrapper::openAudioStream()
{
	// Get a pointer to the codec context for the video stream
	m_pAudioCodecContext = m_pFormatContext->streams[m_iAudioStream]->codec;

	// Find the decoder for the video stream
	AVCodec* pCodec = avcodec_find_decoder(m_pAudioCodecContext->codec_id);	// guess this is deleted with the avcodec_close, that's what the docs say
	if(pCodec==NULL)
		return false; // Codec not found

	// Open codec
	AVDictionary* options;	
	if(avcodec_open2(m_pAudioCodecContext, pCodec, nullptr)<0)
		return false; // Could not open codec

	// Allocate video frame
	m_pAudioFrame = avcodec_alloc_frame();

	retrieveAudioInfo();

	return true;
}

void FFmpegWrapper::close()
{
	stop();

	// Free the RGB image
	if(m_pVideoBuffer!=nullptr)
	{
		delete m_pVideoBuffer;
		m_pVideoBuffer = nullptr;
	}

	if(m_pVideoFrameRGB!=nullptr)
	{
		av_free(m_pVideoFrameRGB);
		m_pVideoFrameRGB = nullptr;
	}

	// Free the YUV frame
	if(m_pVideoFrame!=nullptr)
	{
		av_free(m_pVideoFrame);
		m_pVideoFrame = nullptr;
	}

	if(m_pAudioFrame!=nullptr)
	{
		av_free(m_pAudioFrame);
		m_pAudioFrame = nullptr;
	}


	if(m_pSwScalingContext!=nullptr)
	{
		sws_freeContext(m_pSwScalingContext);
		m_pSwScalingContext = nullptr;
	}

	// Close the codecs
	if(m_pVideoCodecContext!=nullptr)
	{
		avcodec_close(m_pVideoCodecContext);
		m_pVideoCodecContext = nullptr;
	}
	if(m_pAudioCodecContext!=nullptr)
	{
		avcodec_close(m_pAudioCodecContext);
		m_pAudioCodecContext = nullptr;
	}

	// Close the file
	if(m_pFormatContext!=nullptr)
	{
		//avformat_close_input(&m_pFormatContext);
		avformat_free_context(m_pFormatContext);	// this line should free all the associated mem with file, todo seriously check on lost mem blocks
		m_pFormatContext = nullptr;
	}
}

void FFmpegWrapper::updateTimer()
{
	// todo check for very big over and underflow
	double deltaTime = getDeltaTime();
	if(m_iState == ePlaying)
	{
		m_dTargetTimeInMs +=  deltaTime * m_fSpeedMultiplier * m_iDirection;

		// check for over underflows and correct according to loop mode and set according state
		if(m_dTargetTimeInMs > m_dDurationInMs || m_dTargetTimeInMs < 0)
		{
			if(m_iLoopMode == eNoLoop)
			{
				pause();
			}
			else if(m_iLoopMode == eLoop)
			{
				seekFrame(0);
				if(m_dTargetTimeInMs < 0) // underflow
				{
					m_dTargetTimeInMs = m_dDurationInMs;	
				}
				else // overflow
				{
					m_dTargetTimeInMs = 0;
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

void FFmpegWrapper::update()
{
	boost::mutex::scoped_lock scopedLock(m_Mutex);

	isFrameDecoded = false;
	static bool bIsSeekable = true;

	if(isImage())	// no update needed for already decoded image
		return;
	
	// update timer for correct video sync to fps
	updateTimer();

	// make sure always to decode 0 frame first, even if the first delta time would suggest differently
	if(m_dCurrentTimeInMs<0)
		m_dTargetTimeInMs = 0;

	long lTargetFrame = calculateFrameNumberFromTime(m_dTargetTimeInMs);
	if(lTargetFrame != m_lCurrentFrameNumber)
	{
		// probe to jump to target frame directly
			
		if(bIsSeekable)
		{
			//bIsSeekable = seekFrame(lTargetFrame);
			isFrameDecoded = decodeFrame();
			if(!isFrameDecoded)
			{
			/*	seekFrame(0);
				bIsSeekable = false;*/
			}
		}
			
		if(!bIsSeekable)	// just forward decoding, this is a huge performance issue when playing backwards
		{
			/*if(lTargetFrame<=0)
			{
				seekFrame(0);
				m_lCurrentFrameNumber = m_dCurrentTimeInMs = 0;
			}*/

			if( m_iState == ePlaying)
			{
				isFrameDecoded = decodeFrame();
				m_dTargetTimeInMs = (float)(lTargetFrame) * 1.0 / m_dFps * 1000.0;
					
			}
		}

		m_lCurrentFrameNumber = lTargetFrame;
		m_dCurrentTimeInMs = m_dTargetTimeInMs;
	}
}

void FFmpegWrapper::play()
{
	if(!isImage())
	{
		m_iState = ePlaying;
	/*	if(!m_bIsThreadRunning)
		{
			m_bIsThreadRunning = true;
			m_PlayerThread = boost::thread(&FFmpegWrapper::threadedPlayer, this);
		}*/
	}
}

void FFmpegWrapper::pause()
{
	m_iState = ePaused;
}

void FFmpegWrapper::stop()
{
	//if(m_bIsThreadRunning)
	//{
	//	m_bIsThreadRunning = false;
	//	m_PlayerThread.join();
	//}
	m_lCurrentFrameNumber = -1;	// set to invalid, as it is not decoded yet
	m_dTargetTimeInMs = 0;
	m_iState = eStopped;
	if(m_bIsFileOpen)
		seekFrame(0);	// so unseekable files get reset too
}

AVData& FFmpegWrapper::getAVData()
{
	update();
	return m_AVData;
}

VideoData& FFmpegWrapper::getVideoData()
{
	update();
	return m_AVData.m_VideoData;
}

AudioData& FFmpegWrapper::getAudioData()
{
	update();
	return m_AVData.m_AudioData;
}

void FFmpegWrapper::setFramePosition(long lTargetFrameNumber)
{
	m_dTargetTimeInMs = ((float)(lTargetFrameNumber) * 1.0 / m_dFps * 1000.0);
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

AVPacket* FFmpegWrapper::fetchAVPacket()
{
	AVPacket *pAVPacket = nullptr;

	pAVPacket = new AVPacket();
	if(av_read_frame(m_pFormatContext, pAVPacket)>=0)
		return pAVPacket;
	else 
		return nullptr;
}

bool FFmpegWrapper::decodeFrame()
{
	bool bRet = false;
	
	for(int i=0; i<m_pFormatContext->nb_streams; i++)
	{
		AVPacket* pAVPacket = fetchAVPacket();
		
		if(pAVPacket!=nullptr)
		{
			// Is this a packet from the video stream?
			if(pAVPacket->stream_index == m_iVideoStream) 
			{
				bRet = decodeVideoFrame(pAVPacket);
			}
			else if(pAVPacket->stream_index == m_iAudioStream)
			{
				bRet = decodeAudioFrame(pAVPacket);
			}
    
			av_free_packet(pAVPacket);

			if(!bRet)
				return false;
		}
	}
	return true;
}

bool FFmpegWrapper::decodeVideoFrame(AVPacket* pAVPacket)
{
	int isFrameDecoded=0;

	// Decode video frame
	if(avcodec_decode_video2(m_pVideoCodecContext, m_pVideoFrame, &isFrameDecoded, pAVPacket)<0)
		return false;
			
	// Did we get a video frame?
	if(isFrameDecoded) 
	{
		//Convert YUV->RGB
		sws_scale(m_pSwScalingContext, m_pVideoFrame->data, m_pVideoFrame->linesize, 0, getHeight(), m_pVideoFrameRGB->data, m_pVideoFrameRGB->linesize);
		m_AVData.m_VideoData.m_pData =  m_pVideoFrameRGB->data[0];
		return true;
	}
	return false;
}

bool FFmpegWrapper::decodeAudioFrame(AVPacket* pAVPacket)
{
	int isFrameDecoded=0;

	if(avcodec_decode_audio4(m_pAudioCodecContext, m_pAudioFrame, &isFrameDecoded, pAVPacket)<0)
	{
		m_AVData.m_AudioData.m_pData = nullptr;
		return false;
	}
	m_AVData.m_AudioData.m_pData = m_pAudioFrame->data[0];
	m_AVData.m_AudioData.m_lSizeInBytes = av_samples_get_buffer_size(NULL, m_pAudioCodecContext->channels, m_pAudioFrame->nb_samples, m_pAudioCodecContext->sample_fmt, 1);	// 1 stands for don't align size
	m_AVData.m_AudioData.m_lPts = m_pAudioFrame->pkt_pts;
	m_AVData.m_AudioData.m_lDts = m_pAudioFrame->pkt_dts;
	if(m_AVData.m_AudioData.m_lPts == AV_NOPTS_VALUE)
		m_AVData.m_AudioData.m_lPts = 0;
	if(m_AVData.m_AudioData.m_lDts == AV_NOPTS_VALUE)
		m_AVData.m_AudioData.m_lDts = 0;
	return true;
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
	avcodec_decode_video2(m_pVideoCodecContext, m_pVideoFrame, &isFrameDecoded, &packet);

	if(isFrameDecoded)	// Did we get a video frame? 
	{
		//Convert YUV->RGB
		sws_scale(m_pSwScalingContext, m_pVideoFrame->data, m_pVideoFrame->linesize, 0,getHeight(), m_pVideoFrameRGB->data, m_pVideoFrameRGB->linesize);
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
	
	int iStream = m_iVideoStream;
	if(iStream<0)						// we just have an audio stream so seek in this stream
		iStream = m_iAudioStream;

	if(iStream>=0)
	{
		if(avformat_seek_file(m_pFormatContext, iStream, lTargetFrameNumber, lTargetFrameNumber, lTargetFrameNumber, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD) < 0)
		{
			return false;
		}
		if( m_pVideoCodecContext != nullptr)
			avcodec_flush_buffers(m_pVideoCodecContext);
		if( m_pAudioCodecContext != nullptr)
			avcodec_flush_buffers(m_pAudioCodecContext);
	
		return true;
	}
	return false;
}

bool FFmpegWrapper::seekTime(double dTimeInMs)
{
	return seekFrame( calculateFrameNumberFromTime(dTimeInMs) );
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
	return m_AVData.m_VideoData.m_iWidth;
}
		
unsigned int FFmpegWrapper::getHeight()
{
	return m_AVData.m_VideoData.m_iHeight;
}

int FFmpegWrapper::getState()
{
	return m_iState;
}

float FFmpegWrapper::getFps()
{
	return m_dFps;
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

std::string FFmpegWrapper::getVideoCodecName()
{
	return m_strVideoCodecName;
}

std::string FFmpegWrapper::getAudioCodecName()
{
	return m_strAudioCodecName;
}

int FFmpegWrapper::getAudioChannels()
{
	return m_AVData.m_AudioData.m_iChannels;
}

int FFmpegWrapper::getAudioSampleRate()
{
	return m_AVData.m_AudioData.m_iSampleRate;
}

std::string FFmpegWrapper::getFileName()
{
	return m_strFileName;
}

bool FFmpegWrapper::hasVideo()
{
	return m_iVideoStream >= 0;
}
		
bool FFmpegWrapper::hasAudio()
{
	return m_iAudioStream >= 0;
}

bool FFmpegWrapper::isNewFrame()
{
	update();
	long lTargetFrame = calculateFrameNumberFromTime(m_dTargetTimeInMs);
	return (lTargetFrame != m_lCurrentFrameNumber);
}

void FFmpegWrapper::retrieveFileInfo()
{
	m_iBitrate = m_pFormatContext->bit_rate / 1000.0;

	int iStream = m_iVideoStream;
	if(iStream<0)						// we just have an audio stream so seek in this stream
		iStream = m_iAudioStream;

	if(iStream>=0)
	{
		m_dDurationInMs = m_pFormatContext->duration * 1000.0 / (float)AV_TIME_BASE;
		if(m_dDurationInMs/1000.0 < EPS)
			m_dDurationInMs =  m_pFormatContext->streams[iStream]->duration * r2d( m_pFormatContext->streams[iStream]->time_base) * 1000.0;
		m_dFps = r2d(m_pFormatContext->streams[iStream]->r_frame_rate);
		if(m_dFps < EPS)
			m_dFps = r2d(m_pFormatContext->streams[iStream]->avg_frame_rate);
	
		m_lDurationInFrames = m_pFormatContext->streams[iStream]->nb_frames;		// for some codec this return wrong numbers so calc from time
		if(m_lDurationInFrames == 0)
			m_lDurationInFrames = calculateFrameNumberFromTime(m_dDurationInMs);
	}
}

void FFmpegWrapper::retrieveVideoInfo()
{
	m_strVideoCodecName = std::string(m_pVideoCodecContext->codec->long_name);
	m_AVData.m_VideoData.m_iWidth = m_pVideoCodecContext->width;
	m_AVData.m_VideoData.m_iHeight = m_pVideoCodecContext->height;
}

void FFmpegWrapper::retrieveAudioInfo()
{
	m_strAudioCodecName = std::string(m_pAudioCodecContext->codec->long_name);
	m_AVData.m_AudioData.m_iSampleRate = m_pAudioCodecContext->sample_rate;
	m_AVData.m_AudioData.m_iChannels = m_pAudioCodecContext->channels;
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

double FFmpegWrapper::getDeltaTime()
{
	boost::chrono::system_clock::time_point newTime = boost::chrono::system_clock::now();
	boost::chrono::duration<double> delta = newTime - m_OldTime; 
	m_OldTime = newTime;
	return delta.count() * 1000.0;
}

long FFmpegWrapper::calculateFrameNumberFromTime(long lTime)
{
	long lTargetFrame = floor((double)lTime/1000.0 * m_dFps );	//the 0.5 is taken from the opencv player, this might be useful for floating point rounding problems to be on the safe side not to miss one frame
	return lTargetFrame;
}

double FFmpegWrapper::mod(double a, double b)
{
	int result = static_cast<int>( a / b );
	return a - static_cast<double>( result ) * b;
}

bool FFmpegWrapper::isImage()
{
	return m_iBitrate<=0 && m_AVData.m_AudioData.m_iSampleRate<=0;
}

// helper function as taken from OpenCV ffmpeg reader
double FFmpegWrapper::r2d(AVRational r)
{
    return r.num == 0 || r.den == 0 ? 0. : (double)r.num / (double)r.den;
}

};