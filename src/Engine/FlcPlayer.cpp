/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Based on http://www.libsdl.org/projects/flxplay/
 */
#ifdef _MSC_VER
#ifndef _SCL_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif
#endif
// Fork modification 2026-10-10: default-off mixed-audio observation.
// SPDX-License-Identifier: GPL-3.0-or-later; upstream notices retained.
#include "FlcPlayer.h"
#include <algorithm>
#include <cassert>
#include <string.h>
#include <SDL_mixer.h>
#include <fstream>
#ifdef OPENXCOM_RESPONSIVE_LOADING
#include <cmath>
#include <climits>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <utility>
#endif
#include "Logger.h"
#include "Screen.h"
#include "Surface.h"
#include "Options.h"
#include "Game.h"

namespace OpenXcom
{


// Taken from: http://www.compuphase.com/flic.htm
enum FileTypes
{
	FLI_TYPE = 0xAF11,
	FLC_TYPE = 0xAF12,
};


enum ChunkTypes
{
	COLOR_256 = 0x04,
	FLI_SS2 = 0x07, // or DELTA_FLC
	COLOR_64 = 0x0B,
	FLI_LC = 0x0C, // or DELTA_FLI
	BLACK = 0x0D,
	FLI_BRUN = 0x0F, // or BYTE_RUN
	FLI_COPY = 0x10,

	AUDIO_CHUNK = 0xAAAA, // This is the only exception, it's from TFTD
	PREFIX_CHUNK = 0xF100,
	FRAME_TYPE = 0xF1FA,
};

enum ChunkOpcodes
{
	PACKETS_COUNT = 0x0000, // 0000000000000000
	LAST_PIXEL = 0x8000, // 1000000000000000
	SKIP_LINES = 0xc000, // 1100000000000000
	MASK = SKIP_LINES
};

enum PlayingState
{
	PLAYING,
	FINISHED,
	SKIPPED
};

#ifdef OPENXCOM_RESPONSIVE_LOADING
FlcPlayer::FlcPlayer()
    : _responsiveIndex(0), _responsiveLastTick(0), _responsiveBegun(false),
      _responsiveFirst(true), _responsiveOwnSurface(false), _responsiveSourceTiming(false), _responsivePresent(true),
      _responsiveSourceRate(0), _responsiveFirstAudioBytes(0), _responsiveFailure(ResponsiveFailure::None),
      _fileBuf(nullptr), _fileSize(0), _videoFrameData(nullptr), _chunkData(nullptr), _audioFrameData(nullptr),
      _frameCount(0), _headerSize(0), _headerType(0), _headerFrames(0), _headerWidth(0), _headerHeight(0),
      _headerDepth(0), _headerSpeed(0), _videoFrameSize(0), _videoFrameType(0), _frameChunks(0),
      _chunkSize(0), _chunkType(0), _delayOverride(0), _audioFrameSize(0), _audioFrameType(0),
      _frameCallBack(nullptr), _mainScreen(nullptr), _realScreen(nullptr), _colors(),
      _screenWidth(0), _screenHeight(0), _screenDepth(0), _dx(0), _dy(0), _offset(0),
      _playingState(FINISHED), _hasAudio(false), _useInternalAudio(false), _videoDelay(14), _volume(0),
      _audioData(), _game(nullptr)
{}
#else
FlcPlayer::FlcPlayer() : _fileBuf(0), _mainScreen(0), _realScreen(0), _game(0)
{
	_volume = Game::volumeExponent(Options::musicVolume);
}
#endif

FlcPlayer::~FlcPlayer()
{
	deInit();
}

/**
 * Initialize data structures needed buy the player and read the whole file into memory
 * @param filename Video file name
 * @param frameCallback Function to call each video frame
 * @param game Pointer to the Game instance
 * @param dx An offset on the x axis for the video to be rendered
 * @param dy An offset on the y axis for the video to be rendered
 */
bool FlcPlayer::init(const char *filename, void(*frameCallBack)(), Game *game, bool useInternalAudio, int dx, int dy)
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    (void)filename; (void)frameCallBack; (void)useInternalAudio; (void)dx; (void)dy;
    if (game) game->invalidateResponsiveAudio();
    return false; // Supported ON callers use initResponsive; no legacy audio cycle.
#else
	if (_fileBuf != 0)
	{
		Log(LOG_ERROR) << "Trying to init a video player that is already initialized";
		return false;
	}

	_frameCallBack = frameCallBack;
	_realScreen = game->getScreen();
	_realScreen->clear();
	_game = game;
	_useInternalAudio = useInternalAudio;
	_dx = dx;
	_dy = dy;

	_fileSize = 0;
	_frameCount = 0;
	_audioFrameData = 0;
	_hasAudio = false;
	_audioData.loadingBuffer = 0;
	_audioData.playingBuffer = 0;

	std::ifstream file;
	file.open(filename, std::ifstream::in | std::ifstream::binary | std::ifstream::ate);
	if (!file.is_open())
	{
		Log(LOG_ERROR) << "Could not open FLI/FLC file: " << filename;
		return false;
	}

	std::streamoff size = file.tellg();
	file.seekg(0, std::ifstream::beg);

	// TODO: substitute with a cross-platform memory mapped file?
	_fileBuf = new Uint8[size];
	_fileSize = size;
	file.read((char *)_fileBuf, size);
	file.close();

	_audioFrameData = _fileBuf + 128;

	// Let's read the first 128 bytes
	readFileHeader();

	// If it's a FLC or FLI file, it's ok
	if (_headerType == SDL_SwapLE16(FLI_TYPE) || (_headerType == SDL_SwapLE16(FLC_TYPE)))
	{
		_screenWidth = _headerWidth;
		_screenHeight = _headerHeight;
		_screenDepth = 8;

		Log(LOG_INFO) << "Playing flx, " << _screenWidth << "x" << _screenHeight << ", " << _headerFrames << " frames";
	}
	else
	{
		Log(LOG_ERROR) << "Flx file failed header check.";
		return false;
	}
	if (_screenWidth > _realScreen->getSurface()->getWidth() && Options::displayWidth >= _screenWidth)
	{
		// base resolution of video is higher than our surface width
		// and our display resolution allows a hi-res video
		// set base resolution to video resolution
		Options::baseXResolution = _screenWidth;
		Options::baseYResolution = _screenHeight;
		_realScreen->resetDisplay();
	}
	// If the current surface used is at 8bpp use it
	if (_realScreen->getSurface()->getSurface()->format->BitsPerPixel == 8)
	{
		_mainScreen = _realScreen->getSurface()->getSurface();
	}
	else // Otherwise create a new one
	{
		_mainScreen = SDL_AllocSurface(SDL_SWSURFACE, _realScreen->getSurface()->getWidth(), _realScreen->getSurface()->getHeight(), 8, 0, 0, 0, 0);
	}

	return true;

#endif
}

void FlcPlayer::deInit()
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    if (_responsiveLease && !_responsiveLease->canRelease()) std::terminate();
    _responsiveLease.reset();
    _responsiveFrames.clear();
    if (_responsiveOwnSurface && _mainScreen) SDL_FreeSurface(_mainScreen);
    _mainScreen = nullptr; _realScreen = nullptr; _responsiveOwnSurface = false;
    delete[] _fileBuf; _fileBuf = nullptr; _fileSize = 0;
    _responsiveBegun = false; _playingState = FINISHED;
#else
	if (_mainScreen != 0 && _realScreen != 0)
	{
		if (_mainScreen != _realScreen->getSurface()->getSurface())
			SDL_FreeSurface(_mainScreen);

		_mainScreen = 0;
	}

	if (_fileBuf != 0)
	{
		delete[] _fileBuf;
		_fileBuf = 0;

		deInitAudio();
	}

#endif
}

/**
 * Starts decoding and playing the FLI/FLC file
 */
void FlcPlayer::play(bool skipLastFrame)
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    (void)skipLastFrame;
    std::terminate();
#else
	_playingState = PLAYING;

	// Vertically center the video
	_dy = (_mainScreen->h - _headerHeight) / 2;

	_offset = _dy * _mainScreen->pitch + _mainScreen->format->BytesPerPixel * _dx;

	// Skip file header
	_videoFrameData = _fileBuf + 128;
	_audioFrameData = _videoFrameData;

	while (!shouldQuit())
	{
		if (_frameCallBack)
			(*_frameCallBack)();
		else // TODO: support both, in the case the callback is not some audio?
			decodeAudio(2);

		if (!shouldQuit())
			decodeVideo(skipLastFrame);

		if(!shouldQuit())
			SDLPolling();
	}


#endif
}

void FlcPlayer::delay(Uint32 milliseconds)
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    (void)milliseconds;
    std::terminate();
#else
	Uint32 pauseStart = SDL_GetTicks();
	while(_playingState != SKIPPED && SDL_GetTicks() < (pauseStart + milliseconds))
	{
		SDLPolling();
	}

#endif
}
void FlcPlayer::SDLPolling()
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    std::terminate(); // Game owns all ON events, including quit and deferred resize.
#else
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_MOUSEBUTTONDOWN:
		case SDL_KEYDOWN:
			_playingState = SKIPPED;
			break;
		case SDL_VIDEORESIZE:
			if (Options::allowResize)
			{
				Options::newDisplayWidth = Options::displayWidth = std::max(Screen::ORIGINAL_WIDTH, event.resize.w);
				Options::newDisplayHeight = Options::displayHeight = std::max(Screen::ORIGINAL_HEIGHT, event.resize.h);
				if (_mainScreen != _realScreen->getSurface()->getSurface())
				{
					_realScreen->resetDisplay();
				}
				else
				{
					_realScreen->resetDisplay();
					_mainScreen = _realScreen->getSurface()->getSurface();
				}
			}
			break;
		case SDL_QUIT:
			exit(0);
		default:
			break;
		}
	}

#endif
}

bool FlcPlayer::shouldQuit()
{
	return _playingState == FINISHED || _playingState == SKIPPED;
}

void FlcPlayer::readFileHeader()
{
	readU32(_headerSize, _fileBuf);
	readU16(_headerType, _fileBuf + 4);
	readU16(_headerFrames, _fileBuf + 6);
	readU16(_headerWidth, _fileBuf + 8);
	readU16(_headerHeight, _fileBuf + 10);
	readU16(_headerDepth, _fileBuf + 12);
	readU16(_headerSpeed, _fileBuf + 16);
}

bool FlcPlayer::isValidFrame(Uint8 *frameHeader, Uint32 &frameSize, Uint16 &frameType)
{
	readU32(frameSize, frameHeader);
	readU16(frameType, frameHeader + 4);

	return (frameType == FRAME_TYPE || frameType == AUDIO_CHUNK || frameType == PREFIX_CHUNK);
}

void FlcPlayer::decodeAudio(int frames)
{

	int audioFramesFound = 0;

	while (audioFramesFound < frames && !isEndOfFile(_audioFrameData))
	{
		if (!isValidFrame(_audioFrameData, _audioFrameSize, _audioFrameType))
		{
			_playingState = FINISHED;
			break;
		}

		switch (_audioFrameType)
		{
			case FRAME_TYPE:
			case PREFIX_CHUNK:
				_audioFrameData += _audioFrameSize;
				break;
			case AUDIO_CHUNK:
				Uint16 sampleRate;

				readU16(sampleRate, _audioFrameData + 8);

				_chunkData = _audioFrameData + 16;

				playAudioFrame(sampleRate);

				_audioFrameData += _audioFrameSize + 16;

				++audioFramesFound;

				break;
		}
	}
}

void FlcPlayer::decodeVideo(bool skipLastFrame)
{
	bool videoFrameFound = false;

	while (!videoFrameFound)
	{
		if (!isValidFrame(_videoFrameData, _videoFrameSize, _videoFrameType))
		{
			_playingState = FINISHED;
			break;
		}

		switch (_videoFrameType)
		{
		case FRAME_TYPE:

			Uint32 delay;

			readU16(_frameChunks, _videoFrameData + 6);
			readU16(_delayOverride, _videoFrameData + 8);

			if (_headerType == FLI_TYPE)
			{
				delay = _delayOverride > 0 ? _delayOverride : _headerSpeed * (1000.0 / 70.0);
			}
			else if (_useInternalAudio && !_frameCallBack) // this means TFTD videos are playing
			{
				delay = _videoDelay;
			}
			else
			{
				delay = _headerSpeed;
			}

			waitForNextFrame(delay);

			// Skip the frame header, we are not interested in the rest
			_chunkData = _videoFrameData + 16;

			_videoFrameData += _videoFrameSize;
			// If this frame is the last one, don't play it
			if(isEndOfFile(_videoFrameData))
				_playingState = FINISHED;

			if(!shouldQuit() || !skipLastFrame)
				playVideoFrame();

			videoFrameFound = true;

			break;
		case AUDIO_CHUNK:
			_videoFrameData += _videoFrameSize + 16;
			break;
		case PREFIX_CHUNK:
			// Just skip it
			_videoFrameData += _videoFrameSize;

			break;
		}
	}
}

void FlcPlayer::playVideoFrame()
{
	++_frameCount;
	if (SDL_LockSurface(_mainScreen) < 0)
		return;
	int chunkCount = _frameChunks;

	for (int i = 0; i < chunkCount; ++i)
	{
		readU32(_chunkSize, _chunkData);
		readU16(_chunkType, _chunkData + 4);

		switch (_chunkType)
		{
			case COLOR_256:
				color256();
				break;
			case FLI_SS2:
				fliSS2();
				break;
			case COLOR_64:
				color64();
				break;
			case FLI_LC:
				fliLC();
				break;
			case BLACK:
				black();
				break;
			case FLI_BRUN:
				fliBRun();
				break;
			case FLI_COPY:
				fliCopy();
				break;
			case 18:
				break;
			default:
				Log(LOG_WARNING) << "Ieek an non implemented chunk type:" << _chunkType;
				break;
		}

		_chunkData += _chunkSize;
	}

	SDL_UnlockSurface(_mainScreen);

#ifdef OPENXCOM_RESPONSIVE_LOADING
    // Decode and logical palette updates continue while minimized. Physical
    // presentation is owned by the captured Game focus policy for this tick.
    if (!_responsivePresent) return;
#endif
	/* TODO: Track which rectangles have really changed */
	//SDL_UpdateRect(_mainScreen, 0, 0, 0, 0);
	if (_mainScreen != _realScreen->getSurface()->getSurface())
		SDL_BlitSurface(_mainScreen, 0, _realScreen->getSurface()->getSurface(), 0);

	_realScreen->flip();
}

void FlcPlayer::playAudioFrame(Uint16 sampleRate)
{
	/* TFTD audio header (10 bytes)
	* Uint16 unknown1 - always 0
	* Uint16 sampleRate
	* Uint16 unknown2 - always 1 (Channels? bytes per sample?)
	* Uint16 unknown3 - always 10 (No idea)
	* Uint16 unknown4 - always 0
	* Uint8[] unsigned 1-byte 1-channel PCM data of length _chunkSize_ (so the total chunk is _chunkSize_ + 6-byte flc header + 10 byte audio header */

	if (_useInternalAudio)
	{
		if (!_hasAudio)
		{
			_audioData.sampleRate = sampleRate;
			_hasAudio = true;
			initAudio(AUDIO_S16SYS, 1);
		}
		else
		{
			/* Cannot change sample rate mid-video */
			assert(sampleRate == _audioData.sampleRate);
		}

		SDL_SemWait(_audioData.sharedLock);
		AudioBuffer *loadingBuff = _audioData.loadingBuffer;
		assert(loadingBuff->currSamplePos == 0);
		int newSize = (_audioFrameSize + loadingBuff->sampleCount )*2;
		if (newSize > loadingBuff->sampleBufSize)
		{
			/* If the sample count has changed, we need to reallocate (Handles initial state
			* of '0' sample count too, as realloc(NULL, size) == malloc(size) */
			loadingBuff->samples = (Sint16*)realloc(loadingBuff->samples, newSize);
			loadingBuff->sampleBufSize = newSize;
		}

		for (unsigned int i = 0; i < _audioFrameSize; i++)
		{
			loadingBuff->samples[loadingBuff->sampleCount + i] = (float)((_chunkData[i]) -128) * 240 * _volume;
		}
		loadingBuff->sampleCount += _audioFrameSize;

		SDL_SemPost(_audioData.sharedLock);
	}
	else
	{
		_audioData.sampleRate = sampleRate; // this is used to keep the framerate correct
	}
}

void FlcPlayer::color256()
{
	Uint8 *pSrc;
	Uint16 numColorPackets;
	Uint16 numColors = 0;
	Uint8 numColorsSkip;

	pSrc = _chunkData + 6;
	readU16(numColorPackets, pSrc);
	pSrc += 2;

	while (numColorPackets--)
	{
		numColorsSkip = *(pSrc++) + numColors;
		numColors = *(pSrc++);
		if (numColors == 0)
		{
			numColors = 256;
		}

		for (int i = 0; i < numColors; ++i)
		{
			_colors[i].r = *(pSrc++);
			_colors[i].g = *(pSrc++);
			_colors[i].b = *(pSrc++);
		}

		if (_mainScreen != _realScreen->getSurface()->getSurface())
			SDL_SetColors(_mainScreen, _colors, numColorsSkip, numColors);
#ifdef OPENXCOM_RESPONSIVE_LOADING
		_realScreen->setPalette(_colors, numColorsSkip, numColors, _responsivePresent);
#else
		_realScreen->setPalette(_colors, numColorsSkip, numColors, true);
#endif

		if (numColorPackets >= 1)
		{
			++numColors;
		}
	}
}

void FlcPlayer::fliSS2()
{
	Uint8 *pSrc, *pDst, *pTmpDst;
	Sint8 countData;
	Uint8 columSkip, fill1, fill2;
	Uint16 lines;
	Sint16 count;
	bool setLastByte = false;
	Uint8 lastByte = 0;

	pSrc = _chunkData + 6;
	pDst = (Uint8*)_mainScreen->pixels + _offset;
	readU16(lines, pSrc);

	pSrc += 2;

	while (lines--)
	{
		readS16(count, (Sint8 *)pSrc);
		pSrc += 2;

		if ((count & MASK) == SKIP_LINES)
		{
			pDst += (-count)*_mainScreen->pitch;
			++lines;
			continue;
		}

		else if ((count & MASK) == LAST_PIXEL)
		{
			setLastByte = true;
			lastByte = (count & 0x00FF);
			readS16(count, (Sint8 *)pSrc);
			pSrc += 2;
		}

		if ((count & MASK) == PACKETS_COUNT)
		{
			pTmpDst = pDst;
			while (count--)
			{
				columSkip = *(pSrc++);
				pTmpDst += columSkip;
				countData = *(pSrc++);

				if (countData > 0)
				{
					std::copy(pSrc, pSrc + (2 * countData), pTmpDst);
					pTmpDst += (2 * countData);
					pSrc += (2 * countData);

				}
				else
				{
					if (countData < 0)
					{
						countData = -countData;

						fill1 = *(pSrc++);
						fill2 = *(pSrc++);
						while (countData--)
						{
							*(pTmpDst++) = fill1;
							*(pTmpDst++) = fill2;
						}
					}
				}
			}

			if (setLastByte)
			{
				setLastByte = false;
				#ifdef OPENXCOM_RESPONSIVE_LOADING
                *(pDst + _headerWidth - 1) = lastByte;
#else
				*(pDst + _mainScreen->pitch - 1) = lastByte;
#endif
			}
			pDst += _mainScreen->pitch;
		}
	}
}

void FlcPlayer::fliBRun()
{
	Uint8 *pSrc, *pDst, *pTmpDst, fill;
	Sint8 countData;
	int heightCount;

	heightCount = _headerHeight;
	pSrc = _chunkData + 6; // Skip chunk header
	pDst = (Uint8*)_mainScreen->pixels + _offset;

	while (heightCount--)
	{
		pTmpDst = pDst;
		++pSrc; // Read and skip the packet count value

		int pixels = 0;
		while (pixels != _headerWidth)
		{
			countData = *(pSrc++);
			if (countData > 0)
			{
				fill = *(pSrc++);

				std::fill_n(pTmpDst, countData, fill);
				pTmpDst += countData;
				pixels += countData;
			}
			else
			{
				if (countData < 0)
				{
					countData = -countData;

					std::copy(pSrc, pSrc + countData, pTmpDst);
					pTmpDst += countData;
					pSrc += countData;
					pixels += countData;
				}
			}
		}
		pDst += _mainScreen->pitch;
	}
}


void FlcPlayer::fliLC()
{
	Uint8 *pSrc, *pDst, *pTmpDst;
	Sint8 countData;
	Uint8 countSkip;
	Uint8 fill;
	Uint16 lines, tmp;
	int packetsCount;

	pSrc = _chunkData + 6;
	pDst = (Uint8*)_mainScreen->pixels + _offset;

	readU16(tmp, pSrc);
	pSrc += 2;
	pDst += tmp*_mainScreen->pitch;
	readU16(lines, pSrc);
	pSrc += 2;

	while (lines--)
	{
		pTmpDst = pDst;
		packetsCount = *(pSrc++);

		while (packetsCount--)
		{
			countSkip = *(pSrc++);
			pTmpDst += countSkip;
			countData = *(pSrc++);
			if (countData > 0)
			{
				while (countData--)
				{
					*(pTmpDst++) = *(pSrc++);
				}
			}
			else
			{
				if (countData < 0)
				{
					countData = -countData;

					fill = *(pSrc++);
					while (countData--)
					{
						*(pTmpDst++) = fill;
					}
				}
			}
		}
		pDst += _mainScreen->pitch;
	}
}

void FlcPlayer::color64()
{
	Uint8 *pSrc;
	Uint16 NumColors, NumColorPackets;
	Uint8 NumColorsSkip;

	pSrc = _chunkData + 6;
	readU16(NumColorPackets, pSrc);
	pSrc += 2;

	while (NumColorPackets--)
	{
		NumColorsSkip = *(pSrc++);
		NumColors = *(pSrc++);

		if (NumColors == 0)
		{
			NumColors = 256;
		}

		for (int i = 0; i < NumColors; ++i)
		{
			_colors[i].r = *(pSrc++) << 2;
			_colors[i].g = *(pSrc++) << 2;
			_colors[i].b = *(pSrc++) << 2;
		}

		if (_mainScreen != _realScreen->getSurface()->getSurface())
			SDL_SetColors(_mainScreen, _colors, NumColorsSkip, NumColors);
#ifdef OPENXCOM_RESPONSIVE_LOADING
		_realScreen->setPalette(_colors, NumColorsSkip, NumColors, _responsivePresent);
#else
		_realScreen->setPalette(_colors, NumColorsSkip, NumColors, true);
#endif
	}
}

void FlcPlayer::fliCopy()
{
	Uint8 *pSrc, *pDst;
	int Lines = _screenHeight;
	pSrc = _chunkData + 6;
	pDst = (Uint8*)_mainScreen->pixels + _offset;

	while (Lines--)
	{
		memcpy(pDst, pSrc, _screenWidth);
		pSrc += _screenWidth;
		pDst += _mainScreen->pitch;
	}
}

void FlcPlayer::black()
{
	Uint8 *pDst;
	int Lines = _screenHeight;
	pDst = (Uint8*)_mainScreen->pixels + _offset;

	while (Lines-- > 0)
	{
#ifdef OPENXCOM_RESPONSIVE_LOADING
        memset(pDst, 0, _headerWidth);
#else
		memset(pDst, 0, _screenHeight);
#endif
		pDst += _mainScreen->pitch;
	}
}

void FlcPlayer::audioCallback(void *userData, Uint8 *stream, int len)
{
	AudioData *audio = (AudioData*)userData;

	AudioBuffer *playBuff = audio->playingBuffer;

	while (len > 0)
	{
		if (playBuff->sampleCount > 0)
		{
			int bytesToCopy = std::min(len, playBuff->sampleCount * 2);
			memcpy(stream, playBuff->samples + playBuff->currSamplePos, bytesToCopy);

			playBuff->currSamplePos += bytesToCopy / 2;
			playBuff->sampleCount -= bytesToCopy / 2;
			len -= bytesToCopy;

			assert(playBuff->sampleCount >= 0);
		}

		if (len > 0)
		{
			/* Need to swap buffers */
			playBuff->currSamplePos = 0;
			SDL_SemWait(audio->sharedLock);
			AudioBuffer *tempBuff = playBuff;
			audio->playingBuffer = playBuff = audio->loadingBuffer;
			audio->loadingBuffer = tempBuff;
			SDL_SemPost(audio->sharedLock);

			if (playBuff->sampleCount == 0)
				break;
		}
	}
}

void FlcPlayer::initAudio(Uint16 format, Uint8 channels)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    if (_useInternalAudio && _game) _game->resetAgentAudioEvents();
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
    if (_game) _game->invalidateResponsiveAudio();
    std::terminate(); // No supported ON call may enter the legacy device transition.
#else
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
	if (_useInternalAudio && _game) _game->resetMixedAudioObserver();
#endif
	_videoDelay = 1000 / (_audioData.sampleRate / _audioFrameSize );
	if (_useInternalAudio)
	{
		if (!Options::mute)
		{
			if (Mix_OpenAudio(_audioData.sampleRate, format, channels, _audioFrameSize * 2) != 0)
			{
				Log(LOG_ERROR) << Mix_GetError();
				Log(LOG_WARNING) << "Failed to init cutscene audio";
				Options::mute = true;
			}
		}

		/* Start runnable */
		_audioData.sharedLock = SDL_CreateSemaphore(1);

		_audioData.loadingBuffer = new AudioBuffer();
		_audioData.loadingBuffer->currSamplePos = 0;
		_audioData.loadingBuffer->sampleCount = 0;
		_audioData.loadingBuffer->samples = (Sint16 *)malloc(_audioFrameSize * 2);
		_audioData.loadingBuffer->sampleBufSize = _audioFrameSize * 2;

		_audioData.playingBuffer = new AudioBuffer();
		_audioData.playingBuffer->currSamplePos = 0;
		_audioData.playingBuffer->sampleCount = 0;
		_audioData.playingBuffer->samples = (Sint16 *)malloc(_audioFrameSize * 2);
		_audioData.playingBuffer->sampleBufSize = _audioFrameSize * 2;

		if (!Options::mute)
		{
			Mix_HookMusic(FlcPlayer::audioCallback, &_audioData);
		}
	}

#endif
}

void FlcPlayer::deInitAudio()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    if (_game) _game->resetAgentAudioEvents();
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
    if (_game) _game->invalidateResponsiveAudio();
    std::terminate(); // Invalidation precedes every retained legacy entry.
#else
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
	if (_game) _game->resetMixedAudioObserver();
#endif
	if (_game)
	{
		if (!Options::mute)
		{
			Mix_HookMusic(NULL, NULL);
			Mix_CloseAudio();
			_game->initAudio();
		}
	}
	else if (_audioData.sharedLock)
		SDL_DestroySemaphore(_audioData.sharedLock);

	if (_audioData.loadingBuffer)
	{
		free(_audioData.loadingBuffer->samples);
		delete _audioData.loadingBuffer;
		_audioData.loadingBuffer = 0;
	}

	if (_audioData.playingBuffer)
	{
		free(_audioData.playingBuffer->samples);
		delete _audioData.playingBuffer;
		_audioData.playingBuffer = 0;
	}


#endif
}

void FlcPlayer::stop()
{
	_playingState = FINISHED;
}

bool FlcPlayer::isEndOfFile(Uint8 *pos)
{
	return (pos - _fileBuf) == (int)(_fileSize); // should be Sint64, but let's assume the videos won't be 2gb
}

int FlcPlayer::getFrameCount()
{
	return _frameCount;
}

void FlcPlayer::setHeaderSpeed(int speed)
{
	_headerSpeed = speed;
}

bool FlcPlayer::wasSkipped()
{
	return _playingState == SKIPPED;
}

void FlcPlayer::waitForNextFrame(Uint32 delay)
{
	static Uint32 oldTick = 0;
	int newTick;
	int currentTick;

	currentTick = SDL_GetTicks();
	if (oldTick == 0)
	{
		oldTick = currentTick;
		newTick = oldTick;
	}
	else
		newTick = oldTick + delay;

	if (_hasAudio)
	{
		while (currentTick < newTick)
		{
			while ((newTick - currentTick) > 10 && !isEndOfFile(_audioFrameData))
			{
				decodeAudio(1);
				currentTick = SDL_GetTicks();
			}
			SDL_Delay(1);
			currentTick = SDL_GetTicks();
		}
	}
	else
	{
		while (currentTick < newTick)
		{
			SDL_Delay(1);
			currentTick = SDL_GetTicks();
		}
	}
	oldTick = SDL_GetTicks();
}

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
inline void FlcPlayer::readU16(Uint16 &dst, const Uint8 * const src)
{
	dst = (src[0] << 8) | src[1];
}
inline void FlcPlayer::readU32(Uint32 &dst, const Uint8 * const src)
{
	dst = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
}
inline void FlcPlayer::readS16(Sint16 &dst, const Sint8 * const src)
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    // FLC is little-endian regardless of host. Never sign-extend the low byte
    // or left-shift a negative high byte before decoding the validated opcode.
    const unsigned word = unsigned(static_cast<Uint8>(src[0])) |
        (unsigned(static_cast<Uint8>(src[1])) << 8);
    dst = static_cast<Sint16>(word < 0x8000u ? int(word) : int(word) - 0x10000);
#else
	dst = (src[0] << 8) | src[1];
#endif
}
inline void FlcPlayer::readS32(Sint32 &dst, const Sint8 * const src)
{
	dst = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
}
#else
inline void FlcPlayer::readU16(Uint16 &dst, const Uint8 * const src)
{
	dst = (src[1] << 8) | src[0];
}
inline void FlcPlayer::readU32(Uint32 &dst, const Uint8 * const src)
{
	dst = (src[3] << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
}
inline void FlcPlayer::readS16(Sint16 &dst, const Sint8 * const src)
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    // FLC is little-endian regardless of host. Never sign-extend the low byte
    // or left-shift a negative high byte before decoding the validated opcode.
    const unsigned word = unsigned(static_cast<Uint8>(src[0])) |
        (unsigned(static_cast<Uint8>(src[1])) << 8);
    dst = static_cast<Sint16>(word < 0x8000u ? int(word) : int(word) - 0x10000);
#else
	dst = (src[1] << 8) | src[0];
#endif
}
inline void FlcPlayer::readS32(Sint32 &dst, const Sint8 * const src)
{
	dst = (src[3] << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
}
#endif

#ifdef OPENXCOM_RESPONSIVE_LOADING
namespace
{
const std::size_t VideoFileCap = 64u * 1024u * 1024u;
const std::size_t VideoRawAudioCap = 32u * 1024u * 1024u;
const std::size_t VideoConvertedCap = 128u * 1024u * 1024u;
Uint16 videoLE16(const Uint8 *p) { return Uint16(p[0]) | (Uint16(p[1]) << 8); }
Uint32 videoLE32(const Uint8 *p) { return Uint32(p[0]) | (Uint32(p[1]) << 8) | (Uint32(p[2]) << 16) | (Uint32(p[3]) << 24); }

// Validate exactly the legacy visual codecs before the resumable owner can
// enter them. Every packet consumes bounded input; BRUN must also advance x.
// This prevents malformed records from turning one owner tick into a loop or
// an out-of-bounds decode. Unknown codecs fail before playback starts.
bool videoVisualChunk(const Uint8 *p, std::size_t n, unsigned width, unsigned height)
{
    if (n < 6) return false;
    const unsigned type = videoLE16(p + 4);
    std::size_t at = 6;
    const auto take = [&](std::size_t count) -> bool {
        if (count > n - at) return false;
        at += count; return true;
    };
    if (type == BLACK || type == 18) return true;
    if (type == FLI_COPY) return std::size_t(width) * height <= n - at;
    if (type == COLOR_256 || type == COLOR_64)
    {
        if (!take(2)) return false;
        unsigned packets = videoLE16(p + 6), prior = 0;
        while (packets--)
        {
            if (!take(2)) return false;
            unsigned skip = p[at - 2], count = p[at - 1] ? p[at - 1] : 256u;
            if (type == COLOR_256) skip = Uint8(skip + prior);
            if (skip + count > 256 || !take(3u * count)) return false;
            prior = count + (packets ? 1u : 0u);
        }
        return true;
    }
    if (type == FLI_BRUN)
    {
        for (unsigned y = 0; y < height; ++y)
        {
            if (!take(1)) return false; // packet count is unused by this codec
            unsigned x = 0;
            while (x < width)
            {
                if (!take(1)) return false;
                const int count = static_cast<Sint8>(p[at - 1]);
                // -128 cannot be negated in the legacy Sint8 local.
                if (!count || count == -128) return false;
                const unsigned pixels = unsigned(count < 0 ? -count : count);
                if (pixels > width - x || !take(count < 0 ? pixels : 1u)) return false;
                x += pixels;
            }
        }
        return true;
    }
    if (type == FLI_LC)
    {
        if (!take(4)) return false;
        unsigned y = videoLE16(p + 6), lines = videoLE16(p + 8);
        if (y > height || lines > height - y) return false;
        while (lines--)
        {
            if (!take(1)) return false;
            unsigned packets = p[at - 1], x = 0;
            while (packets--)
            {
                if (!take(2)) return false;
                unsigned skip = p[at - 2];
                const int count = static_cast<Sint8>(p[at - 1]);
                if (count == -128 || skip > width - x) return false;
                x += skip;
                const unsigned pixels = unsigned(count < 0 ? -count : count);
                if (pixels > width - x || !take(count < 0 ? 1u : pixels)) return false;
                x += pixels;
            }
        }
        return true;
    }
    if (type == FLI_SS2)
    {
        if (!take(2)) return false;
        unsigned lines = videoLE16(p + 6), y = 0;
        if (lines > height) return false;
        while (lines)
        {
            if (!take(2)) return false;
            unsigned word = videoLE16(p + at - 2);
            if ((word & 0xc000u) == 0xc000u)
            {
                unsigned skip = unsigned(-static_cast<Sint16>(word));
                if (!skip || skip > height - y) return false;
                y += skip; continue;
            }
            if ((word & 0xc000u) == 0x8000u)
            {
                if (!take(2)) return false;
                word = videoLE16(p + at - 2);
            }
            if ((word & 0xc000u) || y >= height) return false;
            unsigned x = 0;
            while (word--)
            {
                if (!take(2)) return false;
                unsigned skip = p[at - 2];
                const int count = static_cast<Sint8>(p[at - 1]);
                if (count == -128 || skip > width - x) return false;
                x += skip;
                const unsigned pixels = 2u * unsigned(count < 0 ? -count : count);
                if (pixels > width - x || !take(count < 0 ? 2u : pixels)) return false;
                x += pixels;
            }
            ++y; --lines;
        }
        return true;
    }
    return false;
}
}

FlcPlayer::AudioLease::AudioLease(Route route, std::uint64_t generation,
    std::vector<Uint8> &&source, unsigned rate, double gain)
    : _route(route), _generation(generation), _owner(std::this_thread::get_id()),
      _source(std::move(source)), _sourceRate(rate), _gain(gain), _cursor(0),
      _frameBytes(0), _status(PrepareStatus::InvalidPCM),
      _attachment(AttachmentState::NeverAttached), _hookInstalled(false), _playbackStarted(false)
{}

FlcPlayer::AudioLease::~AudioLease() noexcept
{
    if (!canRelease()) std::terminate();
}

void FlcPlayer::AudioLease::requireWorker() const noexcept
{
    if (std::this_thread::get_id() == _owner) std::terminate();
}

FlcPlayer::AudioLease::PrepareStatus FlcPlayer::AudioLease::prepareForDevice(int rate, Uint16 format, int channels) noexcept
{
    requireWorker();
    if (_prepareClaim.test_and_set(std::memory_order_acq_rel)) std::terminate();
    if (_attachment != AttachmentState::NeverAttached) std::terminate();
    if (_route != Route::InternalPCM || _source.empty()) return _status = PrepareStatus::NoPCM;
    if (rate <= 0 || rate > 192000 || channels != 2 || (format != AUDIO_S8 && format != AUDIO_S16SYS))
        return _status = PrepareStatus::InvalidSpec;
    if (!_sourceRate || _sourceRate > 65535 || _source.size() > VideoRawAudioCap ||
        _source.size() > std::size_t(INT_MAX) / 2 || !std::isfinite(_gain) || _gain < 0 || _gain > 1)
        return _status = PrepareStatus::InvalidPCM;
    try
    {
        // Deliberate exact amplitude rule: source U8 centered at128, multiplied
        // by240 and captured gain; truncate toward zero then saturate S16.
        // Convert this native-endian S16 mono intermediate, not a quantized U8
        // gain approximation. Native converter fidelity remains a separate gate.
        std::vector<Uint8> input(_source.size() * 2);
        for (std::size_t i = 0; i < _source.size(); ++i)
        {
            double scaled = (int(_source[i]) - 128) * 240.0 * _gain;
            if (scaled < -32768.0) scaled = -32768.0;
            if (scaled > 32767.0) scaled = 32767.0;
            Sint16 sample = static_cast<Sint16>(scaled);
            std::memcpy(&input[i * 2], &sample, sizeof sample);
        }
        SDL_AudioCVT cvt = {};
        int built = SDL_BuildAudioCVT(&cvt, AUDIO_S16SYS, 1, int(_sourceRate), format, 2, rate);
        if (built < 0 || cvt.len_mult <= 0 || cvt.len_mult > 64 ||
            !std::isfinite(cvt.len_ratio) || cvt.len_ratio <= 0 || cvt.len_ratio > 64 ||
            input.size() > VideoConvertedCap / std::size_t(cvt.len_mult) ||
            input.size() > std::size_t(INT_MAX)) return _status = PrepareStatus::ConversionFailure;
        const std::size_t capacity = input.size() * std::size_t(cvt.len_mult);
        std::vector<Uint8> converted(capacity);
        std::memcpy(converted.data(), input.data(), input.size());
        cvt.buf = converted.data(); cvt.len = int(input.size());
        if (SDL_ConvertAudio(&cvt) < 0 || cvt.len_cvt < 0 || std::size_t(cvt.len_cvt) > capacity)
            return _status = PrepareStatus::ConversionFailure;
        const unsigned frameBytes = format == AUDIO_S8 ? 2u : 4u;
        if (!cvt.len_cvt || unsigned(cvt.len_cvt) % frameBytes)
            return _status = PrepareStatus::ConversionFailure;
        converted.resize(std::size_t(cvt.len_cvt));
        _pcm.swap(converted); _frameBytes = frameBytes; _cursor = 0;
        return _status = PrepareStatus::Ready;
    }
    catch (const std::bad_alloc &) { return _status = PrepareStatus::AllocationFailure; }
    catch (...) { return _status = PrepareStatus::ConversionFailure; }
}

void FlcPlayer::AudioLease::acceptAttach(const AttachReceipt &receipt) noexcept
{
    requireWorker();
    if (_attachment != AttachmentState::NeverAttached || !receipt.validFor(_generation, this) || receipt._route != _route ||
        (receipt._hook && (_route != Route::InternalPCM || !hasPCM())) || (!receipt._hook && hasPCM())) std::terminate();
    _hookInstalled = receipt._hook; _attachment = AttachmentState::Attached;
}

void FlcPlayer::AudioLease::acceptNoHook(const NoHookReceipt &receipt) noexcept
{
    requireWorker();
    if (_attachment != AttachmentState::NeverAttached || receipt._lease != this ||
        receipt._generation != _generation || receipt._route != _route || hasPCM()) std::terminate();
    _hookInstalled = false; _attachment = AttachmentState::Attached;
}

void FlcPlayer::AudioLease::acceptDetach(const DetachReceipt &receipt) noexcept
{
    requireWorker();
    if (_attachment != AttachmentState::Attached || !receipt.validFor(_generation, this) || receipt._route != _route) std::terminate();
    _hookInstalled = false; _playbackStarted = false; _attachment = AttachmentState::Detached;
}

void FlcPlayer::AudioLease::notePlaybackStarted() noexcept
{
    if (std::this_thread::get_id() != _owner || _attachment != AttachmentState::Attached || _playbackStarted) std::terminate();
    _playbackStarted = true;
}

void SDLCALL FlcPlayer::AudioLease::callback(void *userdata, Uint8 *stream, int len) noexcept
{
    // Validate signed/null inputs before ANY conversion, memset or cursor access.
    if (len <= 0 || !stream) return;
    std::memset(stream, 0, std::size_t(len));
    AudioLease *lease = static_cast<AudioLease *>(userdata);
    if (!lease || !lease->_frameBytes || unsigned(len) % lease->_frameBytes) return;
    if (lease->_cursor > lease->_pcm.size()) return;
    const std::size_t count = std::min(std::size_t(len), lease->_pcm.size() - lease->_cursor);
    if (!count) return;
    std::memcpy(stream, lease->_pcm.data() + lease->_cursor, count);
    stream += count; // Destination and private cursor advance together; remainder was silenced.
    lease->_cursor += count;
    (void)stream;
}

FlcPlayer::ResponsiveFailure FlcPlayer::inspectResponsive(const Uint8 *bytes, std::size_t size, ResponsiveData &out)
{
    if (!bytes || size < 128) return ResponsiveFailure::Header;
    if (size > VideoFileCap) return ResponsiveFailure::SizeLimit;
    if (videoLE32(bytes) != size) return ResponsiveFailure::Header;
    out.type = videoLE16(bytes + 4); out.width = videoLE16(bytes + 8);
    out.height = videoLE16(bytes + 10); out.speed = videoLE16(bytes + 16);
    if ((out.type != FLI_TYPE && out.type != FLC_TYPE) || !out.width || !out.height ||
        out.width > 4096 || out.height > 4096 || videoLE16(bytes + 12) != 8) return ResponsiveFailure::Header;
    std::size_t offset = 128, records = 0;
    while (offset < size)
    {
        if (++records > 262144 || size - offset < 16) return ResponsiveFailure::Record;
        Uint32 recordSize = videoLE32(bytes + offset);
        Uint16 type = videoLE16(bytes + offset + 4);
        std::size_t extent = recordSize;
        if (type == AUDIO_CHUNK)
        {
            // AUDIO size excludes its16-byte header; frame/prefix sizes include it.
            if (!recordSize || recordSize > size - offset - 16) return ResponsiveFailure::Record;
            extent = std::size_t(recordSize) + 16;
            unsigned rate = videoLE16(bytes + offset + 8);
            if (!rate || (out.sourceRate && out.sourceRate != rate)) return ResponsiveFailure::Rate;
            if (recordSize > VideoRawAudioCap - out.audio.size()) return ResponsiveFailure::SizeLimit;
            if (!out.sourceRate) { out.sourceRate = rate; out.firstAudioBytes = recordSize; }
            out.audio.insert(out.audio.end(), bytes + offset + 16, bytes + offset + extent);
        }
        else if (type == FRAME_TYPE || type == PREFIX_CHUNK)
        {
            if (recordSize < 16 || recordSize > size - offset) return ResponsiveFailure::Record;
            if (type == FRAME_TYPE)
            {
                if (out.frames.size() >= 65535) return ResponsiveFailure::SizeLimit;
                Uint16 chunks = videoLE16(bytes + offset + 6);
                std::size_t chunk = offset + 16;
                for (unsigned i = 0; i < chunks; ++i)
                {
                    if (offset + extent - chunk < 6) return ResponsiveFailure::Record;
                    Uint32 n = videoLE32(bytes + chunk);
                    if (n < 6 || n > offset + extent - chunk ||
                        !videoVisualChunk(bytes + chunk, n, out.width, out.height)) return ResponsiveFailure::Record;
                    chunk += n;
                }
                out.frames.push_back(ResponsiveFrame{offset, extent, chunks, videoLE16(bytes + offset + 8)});
            }
        }
        else return ResponsiveFailure::Record;
        offset += extent; // Strictly positive, checked against the complete file.
    }
    return ResponsiveFailure::None;
}

bool FlcPlayer::initResponsive(const char *filename, Game *game, AudioLease::Route route,
    std::uint64_t generation, double gain, bool sourceTiming, int dx, int dy) noexcept
{
    if (_fileBuf || _responsiveLease || !game || !filename) { _responsiveFailure = ResponsiveFailure::State; return false; }
    try
    {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) { _responsiveFailure = ResponsiveFailure::File; return false; }
        const std::streamoff length = file.tellg();
        if (length < 128 || length > std::streamoff(VideoFileCap)) { _responsiveFailure = ResponsiveFailure::SizeLimit; return false; }
        std::unique_ptr<Uint8[]> bytes(new Uint8[std::size_t(length)]);
        file.seekg(0, std::ios::beg); file.read(reinterpret_cast<char *>(bytes.get()), length);
        if (!file || file.gcount() != length) { _responsiveFailure = ResponsiveFailure::File; return false; }
        ResponsiveData parsed;
        _responsiveFailure = inspectResponsive(bytes.get(), std::size_t(length), parsed);
        if (_responsiveFailure != ResponsiveFailure::None) return false;
        std::vector<Uint8> raw;
        if (route == AudioLease::Route::InternalPCM) raw.swap(parsed.audio);
        std::shared_ptr<AudioLease> lease(new AudioLease(route, generation, std::move(raw), parsed.sourceRate, gain));
        _realScreen = game->getScreen();
        SDL_Surface *screen = _realScreen->getSurface()->getSurface();
        if (!screen || !screen->format || dx < 0 || parsed.width > screen->w || parsed.height > screen->h ||
            dx > screen->w - parsed.width) { _responsiveFailure = ResponsiveFailure::Surface; _realScreen = nullptr; return false; }
        SDL_Surface *surface = screen;
        const bool ownSurface = screen->format->BitsPerPixel != 8;
        if (ownSurface) surface = SDL_AllocSurface(SDL_SWSURFACE, screen->w, screen->h, 8, 0, 0, 0, 0);
        if (!surface || !surface->pixels || !surface->format || surface->format->BitsPerPixel != 8 ||
            surface->pitch < unsigned(screen->w) || screen->h <= 0 ||
            std::uint64_t(surface->pitch) * unsigned(screen->h) > unsigned(INT_MAX))
        {
            if (ownSurface && surface) SDL_FreeSurface(surface);
            _responsiveFailure = ResponsiveFailure::Surface; _realScreen = nullptr; return false;
        }
        // From this no-throw ownership commit onward, deInit owns exactly these.
        _mainScreen = surface; _responsiveOwnSurface = ownSurface;
        _fileBuf = bytes.release(); _fileSize = Uint32(length); _headerSize = Uint32(length);
        _headerType = parsed.type; _headerWidth = parsed.width; _headerHeight = parsed.height;
        _headerSpeed = parsed.speed; _headerDepth = 8; _headerFrames = Uint16(parsed.frames.size());
        _screenWidth = parsed.width; _screenHeight = parsed.height; _screenDepth = 8;
        _dx = dx; _dy = dy; _game = game; _frameCount = 0;
        _responsiveFrames.swap(parsed.frames); _responsiveLease.swap(lease);
        _responsiveSourceRate = parsed.sourceRate; _responsiveFirstAudioBytes = parsed.firstAudioBytes;
        _responsiveSourceTiming = sourceTiming; _responsiveIndex = 0; _playingState = FINISHED;
        _realScreen->clear();
        return true;
    }
    catch (const std::bad_alloc &) { _responsiveFailure = ResponsiveFailure::Allocation; return false; }
    catch (...) { _responsiveFailure = ResponsiveFailure::File; return false; }
}

void FlcPlayer::beginResponsive(Uint32 now) noexcept
{
    if (!_responsiveLease || _responsiveLease->attachmentState() != AudioLease::AttachmentState::Attached || !_mainScreen) std::terminate();
    _responsiveBegun = true; _responsiveFirst = true; _responsiveLastTick = now; _responsiveIndex = 0;
    _playingState = PLAYING; _frameCount = 0;
    _dy = (_mainScreen->h - _headerHeight) / 2;
    _offset = _dy * _mainScreen->pitch + _dx;
}

Uint32 FlcPlayer::responsiveFrameDelay(const ResponsiveFrame &frame) const noexcept
{
    // FLI override is milliseconds; otherwise truncate rational70Hz ticks.
    // Internal FLC timing truncates first-source-chunk duration, never converted
    // length. All zero/no-audio values have a defined14ms fallback (minimum1ms).
    std::uint64_t delay = _headerSpeed;
    if (_headerType == FLI_TYPE) delay = frame.delayOverride ? frame.delayOverride : (std::uint64_t(_headerSpeed) * 1000u) / 70u;
    else if (_responsiveSourceTiming && _responsiveSourceRate && _responsiveFirstAudioBytes)
        delay = (std::uint64_t(_responsiveFirstAudioBytes) * 1000u) / _responsiveSourceRate;
    if (!delay) delay = _headerSpeed ? 1u : 14u;
    return Uint32(std::min<std::uint64_t>(delay, 0x7fffffffu));
}

FlcPlayer::ResponsiveStep FlcPlayer::stepResponsive(Uint32 now, bool skipLastFrame, bool present)
{
    if (_playingState == SKIPPED) return ResponsiveStep::Skipped;
    if (!_responsiveBegun || !_responsiveLease || _responsiveLease->attachmentState() != AudioLease::AttachmentState::Attached)
        return ResponsiveStep::Failed;
    if (_responsiveIndex >= _responsiveFrames.size()) { _playingState = FINISHED; return ResponsiveStep::Finished; }
    const ResponsiveFrame &frame = _responsiveFrames[_responsiveIndex];
    Uint32 delay = responsiveFrameDelay(frame);
    if (!_responsiveFirst && Uint32(now - _responsiveLastTick) < delay) return ResponsiveStep::Waiting;
    _responsiveFirst = false; _responsiveLastTick = now;
    const bool last = _responsiveIndex + 1 == _responsiveFrames.size();
    ++_responsiveIndex;
    if (last && skipLastFrame) { _playingState = FINISHED; return ResponsiveStep::Finished; }
    _frameChunks = frame.chunks; _chunkData = _fileBuf + frame.offset + 16;
    _responsivePresent = present;
    playVideoFrame(); // Decode once; at most one allowed presentation, no Game tail.
    if (last) _playingState = FINISHED;
    return ResponsiveStep::Presented;
}

void FlcPlayer::skipResponsive() noexcept { _playingState = SKIPPED; }
#endif

}
