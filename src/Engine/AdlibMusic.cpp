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
#include "AdlibMusic.h"
#include <fstream>
#include <algorithm>
#ifdef OPENXCOM_RESPONSIVE_LOADING
#include <climits>
#include <exception>
#include <memory>
#endif
#include "Exception.h"
#include "Options.h"
#include "Logger.h"
#include "Game.h"
#include "Adlib/fmopl.h"
#include "Adlib/adlplayer.h"

extern FM_OPL* opl[2];

namespace OpenXcom
{

int AdlibMusic::delay = 0;
int AdlibMusic::rate = 0;
std::map<int, int> AdlibMusic::delayRates;

/**
 * Initializes a new music track.
 * @param volume Music volume modifier (1.0 = 100%).
 */
AdlibMusic::AdlibMusic(float volume) : Music(), _data(0), _size(0), _volume(volume)
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
	// Construction occurs only in the exclusive resource phase. Publish neither
	// half a pair nor a partially allocated tick map, including on OPL failure.
	if ((opl[0] != 0) != (opl[1] != 0)) std::terminate();
	const int requested = Options::audioSampleRate;
	const int rates[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000};
	const int ticks[] = {114*4, 157*4, 228*4, 314*4, 456*4, 629*4, 685*4};
	bool supported = false;
	for (unsigned i = 0; i != 7; ++i) if (rates[i] == requested) supported = true;
	if (!supported || (opl[0] && rate != requested))
		throw Exception("Adlib sample rate is unsupported or differs from the retained OPL pair.");
	std::map<int, int> candidate;
	if (delayRates.empty())
	{
		for (unsigned i = 0; i != 7; ++i) candidate.insert(std::make_pair(rates[i], ticks[i]));
	}
	else
	{
		if (delayRates.size() != 7) std::terminate();
		for (unsigned i = 0; i != 7; ++i)
		{
			const std::map<int, int>::const_iterator tick = delayRates.find(rates[i]);
			if (tick == delayRates.end() || tick->second != ticks[i]) std::terminate();
		}
	}
	std::unique_ptr<FM_OPL, void (*)(FM_OPL*)> first(nullptr, OPLDestroy), second(nullptr, OPLDestroy);
	if (!opl[0])
	{
		first.reset(OPLCreate(OPL_TYPE_YM3812, 3579545, requested));
		if (!first) throw Exception("Unable to allocate the first Adlib OPL chip.");
		second.reset(OPLCreate(OPL_TYPE_YM3812, 3579545, requested));
		if (!second) throw Exception("Unable to allocate the second Adlib OPL chip.");
	}
	// std::allocator map swap and unique_ptr release cannot allocate. The globals
	// remain either their original complete pair or a completely prepared pair.
	if (!candidate.empty()) delayRates.swap(candidate);
	if (first)
	{
		opl[0] = first.release();
		opl[1] = second.release();
		delay = 0;
	}
	rate = requested;
#else
	rate = Options::audioSampleRate;
	if (!opl[0])
	{
		opl[0] = OPLCreate(OPL_TYPE_YM3812, 3579545, rate);
	}
	if (!opl[1])
	{
		opl[1] = OPLCreate(OPL_TYPE_YM3812, 3579545, rate);
	}
	// magic value - length of 1 tick per samplerate
	if (delayRates.empty())
	{
		delayRates[8000] = 114 * 4;
		delayRates[11025] = 157 * 4;
		delayRates[16000] = 228 * 4;
		delayRates[22050] = 314 * 4;
		delayRates[32000] = 456 * 4;
		delayRates[44100] = 629 * 4;
		delayRates[48000] = 685 * 4;
	}
#endif
}

/**
 * Deletes the loaded music content.
 */
AdlibMusic::~AdlibMusic()
{
	if (opl[0])
	{
		stop();
		OPLDestroy(opl[0]);
		opl[0] = 0;
	}
	if (opl[1])
	{
		OPLDestroy(opl[1]);
		opl[1] = 0;
	}
	delete[] _data;
}

/**
 * Loads a music file from a specified filename.
 * @param filename Filename of the music file.
 */
void AdlibMusic::load(const std::string &filename)
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
	if (_data) throw Exception("Adlib track is already loaded.");
	std::ifstream file(filename.c_str(), std::ios::binary);
	if (!file) throw Exception(filename + " not found");
	file.seekg(0, std::ifstream::end);
	const std::streamoff length = file.tellg();
	if (length <= 0 || length > INT_MAX) throw Exception("Invalid Adlib track length.");
	file.seekg(0);
	std::unique_ptr<char[]> data(new char[static_cast<std::size_t>(length)]);
	if (!file.read(data.get(), static_cast<std::streamsize>(length)))
		throw Exception("Incomplete Adlib track read.");
	_data = data.release();
	_size = static_cast<std::size_t>(length);
#else
	std::ifstream file(filename.c_str(), std::ios::binary);
	if (!file)
	{
		throw Exception(filename + " not found");
	}

	file.seekg(0, std::ifstream::end);
	_size = file.tellg();
	file.seekg(0);

	_data = new char[_size];
	file.read(_data, _size);
#endif
}

/**
 * Loads a music file from a specified memory chunk.
 * @param data Pointer to the music file in memory
 * @param size Size of the music file in bytes.
 */
void AdlibMusic::load(const void *data, int size)
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
	if (_data || !data || size <= 0) throw Exception("Invalid or repeated Adlib track adoption.");
	const unsigned first = *static_cast<const unsigned char*>(data);
	const int extra = first <= 56 ? static_cast<int>(first) : 0;
	if (size > INT_MAX - extra) throw Exception("Adlib track length overflows the engine range.");
	// The existing caller contract transfers a new[] allocation including this
	// format's optional leading byte count. Do not adopt it on validation failure.
	_data = const_cast<char*>(static_cast<const char*>(data));
	_size = static_cast<std::size_t>(size + extra);
#else
	_data = (char*)data;
	if (*(unsigned char*)_data<=56) size+=*(unsigned char*)_data;
	_size = (size_t)(size);
#endif
}

/**
 * Plays the contained music track.
 * @param loop Amount of times to loop the track. -1 = infinite
 */
void AdlibMusic::play(int) const
{
#ifndef __NO_MUSIC
#ifdef OPENXCOM_RESPONSIVE_LOADING
	// Owner and operation admission precedes every Options read and stop/hook.
	if (!Game::responsiveMusicPlaybackAllowed(this))
		throw Exception("Adlib playback rejected by the responsive audio capability gate.");
	if (!_data || !_size || _size > INT_MAX || !opl[0] || !opl[1] ||
		rate != Options::audioSampleRate || delayRates.find(rate) == delayRates.end())
		throw Exception("Adlib playback requires loaded data and a complete exact-rate OPL pair.");
	int actualRate = 0, actualChannels = 0;
	Uint16 actualFormat = 0;
	if (Mix_QuerySpec(&actualRate, &actualFormat, &actualChannels) != 1 ||
		actualRate != rate || actualFormat != AUDIO_S16SYS || actualChannels != 2)
		throw Exception("Adlib playback rejected an incompatible actual mixer specification.");
#endif
	if (!Options::mute)
	{
		stop();
		func_setup_music((unsigned char*)_data, _size);
		func_set_music_volume(127 * _volume);
		Mix_HookMusic(player, (void*)this);
	}
#endif
}

/**
 * Custom audio player.
 * @param udata User data to send to the player.
 * @param stream Raw audio to output.
 * @param len Length of audio to output.
 */
void AdlibMusic::player(void *udata, Uint8 *stream, int len)
{
#ifndef __NO_MUSIC
#ifdef OPENXCOM_RESPONSIVE_LOADING
	if (!stream || len <= 0 || (len % 4) != 0 || !udata || !opl[0] || !opl[1]) return;
	AdlibMusic *const owned = static_cast<AdlibMusic*>(udata);
	if (!owned->_data || !owned->_size || owned->_size > INT_MAX) return;
	const std::map<int, int>::const_iterator tick = delayRates.find(rate);
	if (tick == delayRates.end() || tick->second <= 0 || (tick->second % 4) != 0 ||
		delay < 0 || (delay % 4) != 0) return;
#endif
	// Check SDL volume for Background Mute functionality
	if (Options::musicVolume == 0 || Mix_VolumeMusic(-1) == 0)
		return;
	if (Options::musicAlwaysLoop && !func_is_music_playing())
	{
#ifdef OPENXCOM_RESPONSIVE_LOADING
		// Callback restart reuses the owned engine/data. Never call owner play(),
		// stop(), an allocating map subscript, or any mixer hook/setter here.
		func_setup_music(reinterpret_cast<unsigned char*>(owned->_data), static_cast<int>(owned->_size));
		func_set_music_volume(127 * owned->_volume);
#else
		AdlibMusic *music = (AdlibMusic*)udata;
		music->play();
#endif
		return;
	}
	while (len != 0)
	{
		if (!opl[0] || !opl[1])
			return;
		int i = std::min(delay, len);
		if (i)
		{
			float volume = Game::volumeExponent(Options::musicVolume);
			YM3812UpdateOne(opl[0], (INT16*)stream, i / 2, 2, volume);
			YM3812UpdateOne(opl[1], ((INT16*)stream) + 1, i / 2, 2, volume);
			stream += i;
			delay -= i;
			len -= i;
		}
		if (!len)
			return;
		func_play_tick();

#ifdef OPENXCOM_RESPONSIVE_LOADING
		delay = tick->second;
#else
		delay = delayRates[rate];
#endif
	}
#endif
}

bool AdlibMusic::isPlaying()
{
#ifndef __NO_MUSIC
	if (!Options::mute)
	{
		return func_is_music_playing();
	}
#endif
	return false;
}

}
