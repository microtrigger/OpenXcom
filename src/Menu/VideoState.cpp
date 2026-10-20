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
#include "VideoState.h"
#include <algorithm>
#ifdef OPENXCOM_RESPONSIVE_LOADING
#include <cstring>
#include <exception>
#include <utility>
#include "../Engine/Surface.h"
#endif
#include <SDL_mixer.h>
#include "../Engine/Adlib/adlplayer.h"
#include "../Engine/Logger.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/FlcPlayer.h"
#include "../Engine/CrossPlatform.h"
#include "../Engine/FileMap.h"
#include "../Engine/Screen.h"
#include "../Engine/Music.h"
#include "../Engine/Sound.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleVideo.h"
#include "CutsceneState.h"
#include "../Interface/Cursor.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Intro screen.
 * @param game Pointer to the core game.
 * @param wasLetterBoxed Was the game letterboxed?
 */
VideoState::VideoState(const std::vector<std::string> *videos, const std::vector<std::string> *tracks, bool useUfoAudioSequence)
		:
#ifdef OPENXCOM_RESPONSIVE_LOADING
          _responsiveFinished(false), _responsiveInitFailed(false),
#endif
          _videos(videos), _tracks(tracks), _useUfoAudioSequence(useUfoAudioSequence)
{
}

/**
 *
 */
#ifndef OPENXCOM_RESPONSIVE_LOADING
VideoState::~VideoState()
{
}
#endif


typedef struct
{
	std::string catFile;
	int sound;
	int volume;
} soundInFile;


// the pure MS-DOS experience
static soundInFile introCatOnlySounds[]=
{
{"INTRO.CAT", 0x0, 32},
{"INTRO.CAT", 0x1, 32},
{"INTRO.CAT", 0x2, 32},
{"INTRO.CAT", 0x3, 32},
{"INTRO.CAT", 0x4, 32},
{"INTRO.CAT", 0x5, 32},
{"INTRO.CAT", 0x6, 32},
{"INTRO.CAT", 0x7, 32},
{"INTRO.CAT", 0x8, 32},
{"INTRO.CAT", 0x9, 32},
{"INTRO.CAT", 0xa, 32},
{"INTRO.CAT", 0xb, 32},
{"INTRO.CAT", 0xc, 32},
{"INTRO.CAT", 0xd, 32},
{"INTRO.CAT", 0xe, 32},
{"INTRO.CAT", 0xf, 32},
{"INTRO.CAT", 0x10, 32},
{"INTRO.CAT", 0x11, 32},
{"INTRO.CAT", 0x12, 32},
{"INTRO.CAT", 0x13, 32},
{"INTRO.CAT", 0x14, 32},
{"INTRO.CAT", 0x15, 32},
{"INTRO.CAT", 0x16, 32},
{"INTRO.CAT", 0x17, 32},
{"INTRO.CAT", 0x18, 32},
{"INTRO.CAT", 0x18, 32}
};


static soundInFile sample3CatOnlySounds[]=
{
{"SAMPLE3.CAT", 24, 32}, // machine gun
{"SAMPLE3.CAT", 5, 32},   // plasma rifle
{"SAMPLE3.CAT", 23, 32}, // rifle
{"SAMPLE3.CAT",  6, 32}, // some kind of death noise, urgh?
{"SAMPLE3.CAT", 9, 64}, // mutdie
{"SAMPLE3.CAT", 7, 64}, // dying alien
{"SAMPLE3.CAT", 27, 64}, // another dying alien
{"SAMPLE3.CAT", 4, 32}, // ??? ship flying? alien screech?
{"SAMPLE3.CAT", 0x8, 32}, // fscream
{"SAMPLE3.CAT", 11, 32}, // alarm
{"SAMPLE3.CAT", 4, 32}, // gun spinning up?
{"INTRO.CAT", 0xb, 32},  // reload; this one's not even in sample3
{"SAMPLE3.CAT",19, 48},  // whoosh
{"INTRO.CAT", 0xd, 32},  // feet, also not in sample3
{"SAMPLE3.CAT", 2, 32},  // low pulsating hum
{"SAMPLE3.CAT", 30, 32}, // energise
{"SAMPLE3.CAT", 21, 32}, // hatch
{"SAMPLE3.CAT", 0, 64}, // phizz -- no equivalent in sample3.cat?
{"SAMPLE3.CAT", 13, 32}, // warning
{"SAMPLE3.CAT", 14, 32}, // detected
{"SAMPLE3.CAT", 19, 64}, // UFO flyby whoosh?
{"SAMPLE3.CAT", 3, 32}, // growl
{"SAMPLE3.CAT", 15, 128}, // voice
{"SAMPLE3.CAT", 12, 32}, // beep 1
{"SAMPLE3.CAT", 18, 32}, // takeoff
{"SAMPLE3.CAT", 20, 32}  // another takeoff/landing sound?? if it exists?
};



// an attempt at a mix of (subjectively) the best sounds from the two versions
// difficult because we can't find a definitive map from old sequence numbers to SAMPLE3.CAT indexes
// probably only the Steam version of the game comes with both INTRO.CAT and SAMPLE3.CAT
static soundInFile hybridIntroSounds[]=
{
{"SAMPLE3.CAT", 24, 32}, // machine gun
{"SAMPLE3.CAT", 5, 32},   // plasma rifle
{"SAMPLE3.CAT", 23, 32}, // rifle
{"INTRO.CAT",  3, 32}, // some kind of death noise, urgh?
{"INTRO.CAT", 0x4, 64}, // mutdie
{"INTRO.CAT", 0x5, 64}, // dying alien
{"INTRO.CAT", 0x6, 64}, // another dying alien
{"INTRO.CAT", 0x7, 32}, // ??? ship flying? alien screech?
{"SAMPLE3.CAT", 0x8, 32}, // fscream
{"SAMPLE3.CAT", 11, 32}, // alarm
{"SAMPLE3.CAT", 4, 32}, // gun spinning up?
{"INTRO.CAT", 0xb, 32},  // reload; this one's not even in sample3
{"SAMPLE3.CAT",19, 48},  // whoosh
{"INTRO.CAT", 0xd, 32},  // feet, also not in sample3
{"INTRO.CAT", 0xe, 32},  // low pulsating hum
{"SAMPLE3.CAT", 30, 32}, // energise
{"SAMPLE3.CAT", 21, 32}, // hatch
{"INTRO.CAT", 0x11, 64}, // phizz
{"SAMPLE3.CAT", 13, 32}, // warning
{"SAMPLE3.CAT", 14, 32}, // detected
{"SAMPLE3.CAT", 19, 64}, // UFO flyby whoosh?
{"INTRO.CAT", 0x15, 32}, // growl
{"SAMPLE3.CAT", 15, 128}, // voice
{"SAMPLE3.CAT", 12, 32}, // beep 1
{"SAMPLE3.CAT", 18, 32}, // takeoff
{"SAMPLE3.CAT", 20, 32}  // another takeoff/landing sound?? if it exists?
};

// sample3: 18 is takeoff, 20 is landing; 19 is flyby whoosh sound, not sure for which craft

static soundInFile *introSounds[] =
{
	hybridIntroSounds,
	introCatOnlySounds,
	sample3CatOnlySounds,
	0
};


typedef struct
{
	int frameNumber;
	int sound;
} introSoundEffect;

static introSoundEffect introSoundTrack[] =
{
{0, 0x200}, // inserting this to keep the code simple
{149, 0x11},
{173, 0x0C},
{183, 0x0E},
{205, 0x15},
{211, 0x201},
{211, 0x407},
{223, 0x7},
{250, 0x1},
{253, 0x1},
{255, 0x1},
{257, 0x1},
{260, 0x1},
{261, 0x3},
{262, 0x1},
{264, 0x1},
{268, 0x1},
{270, 0x1},
{272, 0x5},
{272, 0x1},
{274, 0x1},
{278, 0x1},
{280, 0x1},
{282, 0x8},
{282, 0x1},
{284, 0x1},
{286, 0x1},
{288, 0x1},
{290, 0x1},
{292, 0x6},
{292, 0x1},
{296, 0x1},
{298, 0x1},
{300, 0x1},
{302, 0x1},
{304, 0x1},
{306, 0x1},
{308, 0x1},
{310, 0x1},
{312, 0x1},
{378, 0x202},
{378, 0x9}, // alarm
{386, 0x9},
{393, 0x9},
{399, 0x17}, // bleeps
{433, 0x17},
{463, 0x12}, // warning
{477, 0x12},
{487, 0x13}, // ufo detected
{495, 0x16}, // voice
{501, 0x16},
{512, 0xd},  // feet -- not in original
{514, 0xd},  // feet -- not in original
{522, 0x0B}, // rifle grab
{523, 0xd},  // feet -- not in original
{525, 0xd},  // feet -- not in original
{534, 0x18},
{535, 0x405},
{560, 0x407},
{577, 0x14},
{582, 0x405},
// {582, 0x18}, // landing! correcting to landing sound!
{582, 0x19},
{613, 0x407},
{615, 0x10},
{635, 0x14},
{638, 0x14},
{639, 0x14},
{644, 0x2},
{646, 0x2},
{648, 0x2},
{650, 0x2},
{652, 0x2},
{654, 0x2},
{656, 0x2},
{658, 0x2},
{660, 0x2},
{662, 0x2},
{664, 0x2},
{666, 0x2},
{668, 0x401},
{681, 0x406},
{687, 0x402},
{689, 0x407},
{694, 0x0A},
{711, 0x407},
{711, 0x0},
{714, 0x0},
{716, 0x4},
{717, 0x0},
{720, 0x0},
{723, 0x0},
{726, 0x5},
{726, 0x0},
{729, 0x0},
{732, 0x0},
{735, 0x0},
{738, 0x0},
{741, 0x0},
{742, 0x6},
{744, 0x0},
{747, 0x0},
{750, 0x0},
{753, 0x0},
{756, 0x0},
{759, 0x0},
{762, 0x0},
{765, 0x0},
{768, 0x0},
{771, 0x0},
{774, 0x0},
{777, 0x0},
{780, 0x0},
{783, 0x0},
{786, 0x0},
{790, 0x15},
{790, 0x15},
{807, 0x2},
{810, 0x2},
{812, 0x2},
{814, 0x2},
{816, 0x0},
{819, 0x0},
{822, 0x0},
{824, 0x40A},
{824, 0x5},
{827, 0x6},
{835, 0x0F},
{841, 0x0F},
{845, 0x0F},
{855, 0x407},
{879, 0x0C},
{65535, 0x0FFFF}
};


#ifdef OPENXCOM_RESPONSIVE_LOADING
struct AudioSequence
#else
static struct AudioSequence
#endif
{
	Mod *mod;
	Music *m;
	Sound *s;
	int trackPosition;
	FlcPlayer *_flcPlayer;

	#ifdef OPENXCOM_RESPONSIVE_LOADING
    const int _soundVolume;
    const bool _playAudio;
    AudioSequence(Mod *_mod, FlcPlayer *flcPlayer, int soundVolume, bool playAudio)
        : mod(_mod), m(0), s(0), trackPosition(0), _flcPlayer(flcPlayer), _soundVolume(soundVolume), _playAudio(playAudio) {}
#else
    AudioSequence(Mod *_mod, FlcPlayer *flcPlayer) : mod(_mod), m(0), s(0), trackPosition(0), _flcPlayer(flcPlayer)
    { }
#endif

	void operator()()
	{
		#ifdef OPENXCOM_RESPONSIVE_LOADING
        // At most16 commands per owner tick, and never consume the sentinel.
        unsigned budget = 16;
        while (budget-- && trackPosition < int(sizeof introSoundTrack / sizeof introSoundTrack[0]) - 1 &&
            _flcPlayer->getFrameCount() >= introSoundTrack[trackPosition].frameNumber)
#else
        while (_flcPlayer->getFrameCount() >= introSoundTrack[trackPosition].frameNumber)
#endif
		{
			int command = introSoundTrack[trackPosition].sound;
#ifdef OPENXCOM_RESPONSIVE_LOADING
            // Silence removes only playback, never the source's visual clock.
            // The silent sequence carries no Mod pointer and cannot access its
            // Music/Sound borrowers, but still applies every speed command.
            if (!_playAudio && !(command & 0x400)) { ++trackPosition; continue; }
#endif

			if (command & 0x200)
			{
#ifndef __NO_MUSIC
				switch(command)
				{
				case 0x200:
					Log(LOG_DEBUG) << "Playing gmintro1";
					m = mod->getMusic("GMINTRO1");
					m->play(1);
					break;
				case 0x201:
					Log(LOG_DEBUG) << "Playing gmintro2";
					m = mod->getMusic("GMINTRO2");
					m->play(1);
					break;
				case 0x202:
					Log(LOG_DEBUG) << "Playing gmintro3";
					m = mod->getMusic("GMINTRO3");
					m->play(1);
					//Mix_HookMusicFinished(_FlcPlayer::stop);
					break;
				}
#endif
			}
			else if (command & 0x400)
			{
				int newSpeed = (command & 0xff);
				_flcPlayer->setHeaderSpeed(newSpeed);
				Log(LOG_DEBUG) << "Frame delay now: " << newSpeed;
			}
			else if (command <= 0x19)
			{
				for (soundInFile **sounds = introSounds; *sounds; ++sounds) // try hybrid sound set, then intro.cat or sample3.cat alone
				{
					soundInFile *sf = (*sounds) + command;
					int channel = trackPosition % 4; // use at most four channels to play sound effects
					#ifdef OPENXCOM_RESPONSIVE_LOADING
                    double ratio = double(_soundVolume) / MIX_MAX_VOLUME;
#else
                    double ratio = (double)Options::soundVolume / MIX_MAX_VOLUME;
#endif
					Log(LOG_DEBUG) << "playing: " << sf->catFile << ":" << sf->sound << " for index " << command;
					s = mod->getSound(sf->catFile, sf->sound, false);
					if (s)
					{
						s->play(channel);
						Mix_Volume(channel, sf->volume * ratio);
						break;
					}
					else Log(LOG_DEBUG) << "Couldn't play " << sf->catFile << ":" << sf->sound;
				}
			}
			++trackPosition;
		}

	}
#ifdef OPENXCOM_RESPONSIVE_LOADING
};
#else
} *audioSequence = NULL;


static void audioHandler()
{
	(*audioSequence)();
}

void VideoState::init()
{
	State::init();

	bool wasLetterboxed = CutsceneState::initDisplay();

	bool ufoIntroSoundFileDosExists = false;
	bool ufoIntroSoundFileWinExists = false;
	int prevMusicVol = Options::musicVolume;
	int prevSoundVol = Options::soundVolume;
	if (_useUfoAudioSequence)
	{
		const std::set<std::string> &soundDir = FileMap::getVFolderContents("SOUND");
		ufoIntroSoundFileDosExists = soundDir.end() != soundDir.find("intro.cat");
		ufoIntroSoundFileWinExists = soundDir.end() != soundDir.find("sample3.cat");

		if (!ufoIntroSoundFileDosExists && !ufoIntroSoundFileWinExists)
		{
			_useUfoAudioSequence = false;
		}
		else
		{
			// ensure user can hear both music and sound effects for the
			// vanilla intro sequence
			Options::musicVolume = Options::soundVolume = std::max(prevMusicVol, prevSoundVol);
			_game->setVolume(Options::soundVolume, Options::musicVolume, -1);
		}
	}
	_game->getCursor()->setVisible(false);

	int dx = (Options::baseXResolution - Screen::ORIGINAL_WIDTH) / 2;
	int dy = (Options::baseYResolution - Screen::ORIGINAL_HEIGHT) / 2;

	// We can only do a fade out in 8bpp, otherwise instantly end it
	bool fade = (_game->getScreen()->getSurface()->getSurface()->format->BitsPerPixel == 8);
	const int FADE_DELAY = 45;
	const int FADE_STEPS = 20;

	FlcPlayer *flcPlayer = NULL;
	size_t audioCounter = 0;
	for (std::vector<std::string>::const_iterator it = _videos->begin(); it != _videos->end(); ++it)
	{
		bool useInternalAudio = true;
		if (!_tracks->empty() && _tracks->size() > audioCounter && _game->getMod()->getMusic(_tracks->at(audioCounter)))
		{
			_game->getMod()->getMusic(_tracks->at(audioCounter))->play(0);
			useInternalAudio = false;
		}
		audioCounter++;
		const std::string& videoFileName = FileMap::getFilePath(*it);

		if (!CrossPlatform::fileExists(videoFileName))
		{
			continue;
		}

		if (!flcPlayer)
		{
			flcPlayer = new FlcPlayer();
		}

		if (_useUfoAudioSequence)
		{
			audioSequence = new AudioSequence(_game->getMod(), flcPlayer);
		}

		flcPlayer->init(videoFileName.c_str(),
			 _useUfoAudioSequence ? &audioHandler : NULL,
			 _game, useInternalAudio, dx, dy);
		flcPlayer->play(_useUfoAudioSequence);
		if (_useUfoAudioSequence)
		{
			flcPlayer->delay(10000);
			delete audioSequence;
			audioSequence = NULL;
		}
		flcPlayer->deInit();

		if (flcPlayer->wasSkipped())
		{
			fade = false;
			break;
		}
	}

	if (flcPlayer)
	{
		delete flcPlayer;
	}

#ifndef __NO_MUSIC
	// fade out!
	if (fade)
	{
		Mix_FadeOutChannel(-1, FADE_DELAY * FADE_STEPS);
		// SDL_Mixer has trouble with native midi and volume on windows,
		// which is the most likely use case, so f@%# it.
		if (Mix_GetMusicType(0) != MUS_MID)
		{
			Mix_FadeOutMusic(FADE_DELAY * FADE_STEPS);
			func_fade();
		}
		else
		{
			Mix_HaltMusic();
		}
	}
	else
	{
		Mix_HaltChannel(-1);
		Mix_HaltMusic();
	}
#endif

	if (fade)
	{
		SDL_Color pal[256];
		SDL_Color pal2[256];
		memcpy(pal, _game->getScreen()->getPalette(), sizeof(SDL_Color) * 256);
		for (int i = FADE_STEPS; i > 0; --i)
		{
			SDL_Event event;
			if (SDL_PollEvent(&event) && event.type == SDL_KEYDOWN) break;
			for (int color = 0; color < 256; ++color)
			{
				pal2[color].r = (((int)pal[color].r) * i) / 20;
				pal2[color].g = (((int)pal[color].g) * i) / 20;
				pal2[color].b = (((int)pal[color].b) * i) / 20;
				pal2[color].unused = pal[color].unused;
			}
			_game->getScreen()->setPalette(pal2, 0, 256, true);
			_game->getScreen()->flip();
			SDL_Delay(FADE_DELAY);
		}
	}
	_game->getScreen()->clear();
	_game->getScreen()->flip();

	if (_useUfoAudioSequence)
	{
		Options::musicVolume = prevMusicVol;
		Options::soundVolume = prevSoundVol;
		_game->setVolume(Options::soundVolume, Options::musicVolume, Options::uiVolume);
	}

#ifndef __NO_MUSIC
	Sound::stop();
	Music::stop();
#endif

	_game->getCursor()->setVisible(true);
	CutsceneState::resetDisplay(wasLetterboxed);
	_game->popState();
}

#endif // legacy synchronous VideoState

#ifdef OPENXCOM_RESPONSIVE_LOADING
struct VideoState::ResponsiveVideo
{
    enum class Phase { Prepare, AwaitAttach, Playing, Hold, Fade, NeedDetach, AwaitDetach, Retained, Finish, Done };
    Phase phase;
    std::shared_ptr<void> epoch;
    std::unique_ptr<FlcPlayer> player;
    std::shared_ptr<FlcPlayer::AudioLease> lease;
    std::unique_ptr<AudioSequence> sequence;
    Music *track;
    std::vector<std::string> videos, tracks;
    std::size_t index;
    bool close, skip, failed, displayChanged, volumeChanged, cursorVisible;
    bool ufoAvailable, mute, allowResize, resizePending, wasLetterboxed, fadePossible, fadeStarted, retryDetach;
    int oldMusic, oldSound, oldUi, soundVolume, musicVolume, resizeWidth, resizeHeight;
    Uint32 phaseTick;
    unsigned fadeStep;
    SDL_Color palette[256];
    ResponsiveVideo()
        : phase(Phase::Prepare), track(nullptr), index(0), close(false), skip(false), failed(false),
          displayChanged(false), volumeChanged(false), cursorVisible(true), ufoAvailable(false),
          mute(false), allowResize(false), resizePending(false), wasLetterboxed(false),
          fadePossible(false), fadeStarted(false), retryDetach(false), oldMusic(0), oldSound(0), oldUi(0),
          soundVolume(0), musicVolume(0), resizeWidth(0), resizeHeight(0), phaseTick(0), fadeStep(0), palette() {}
};

VideoState::~VideoState()
{
    // Active state removal is intercepted by Game. An unexpected destructor must
    // fail before releasing a pending operation target, epoch or callback lease.
    if (_responsiveVideo && _responsiveVideo->phase != ResponsiveVideo::Phase::Done) std::terminate();
}

bool VideoState::responsiveVideoActive() const noexcept { return !_responsiveFinished; }

void VideoState::init()
{
    if (_responsiveVideo || _responsiveInitFailed || _responsiveFinished) return;
    try
    {
        _responsiveVideo.reset(new ResponsiveVideo());
        ResponsiveVideo &v = *_responsiveVideo;
        v.epoch = _game->retainResponsiveEpoch(); // genuine strong ownership, never an access permit
        if (!v.epoch) { v.failed = true; v.phase = ResponsiveVideo::Phase::Finish; return; }
        if (_videos) v.videos = *_videos;
        if (_tracks) v.tracks = *_tracks;
        if (v.videos.size() > 256 || v.tracks.size() > 256) { v.failed = true; v.phase = ResponsiveVideo::Phase::Finish; return; }
        v.oldMusic = Options::musicVolume; v.oldSound = Options::soundVolume; v.oldUi = Options::uiVolume;
        v.musicVolume = v.oldMusic; v.soundVolume = v.oldSound;
        v.mute = Options::mute; v.allowResize = Options::allowResize;
        v.cursorVisible = _game->getCursor()->getVisible();
        if (_useUfoAudioSequence)
        {
            const std::set<std::string> &sounds = FileMap::getVFolderContents("SOUND");
            v.ufoAvailable = sounds.count("intro.cat") || sounds.count("sample3.cat");
            if (v.ufoAvailable) v.musicVolume = v.soundVolume = std::max(v.oldMusic, v.oldSound);
        }
        State::init(); // owner-only preparation, before any operation is reserved
        v.wasLetterboxed = Options::keepAspectRatio; v.displayChanged = true;
        CutsceneState::initDisplay();
        _game->getCursor()->setVisible(false);
        v.fadePossible = _game->getScreen()->getSurface()->getSurface()->format->BitsPerPixel == 8;
    }
    catch (...)
    {
        _responsiveInitFailed = true;
        if (_responsiveVideo) { _responsiveVideo->failed = true; _responsiveVideo->phase = ResponsiveVideo::Phase::Finish; }
    }
}

void VideoState::responsivePrepareVideo()
{
    ResponsiveVideo &v = *_responsiveVideo;
    if (v.close || v.skip || v.failed || v.index >= v.videos.size()) { v.phase = ResponsiveVideo::Phase::Finish; return; }
    // One candidate per tick. Missing files never start an external track.
    const std::string filename = FileMap::getFilePath(v.videos[v.index]);
    if (!CrossPlatform::fileExists(filename)) { ++v.index; return; }
    using Route = FlcPlayer::AudioLease::Route;
    Route selected = v.ufoAvailable ? Route::UfoSequence : Route::InternalPCM;
    Music *track = nullptr;
#ifndef __NO_MUSIC
    if (!v.ufoAvailable && v.index < v.tracks.size())
    {
        track = _game->getMod()->getMusic(v.tracks[v.index]);
        if (track) selected = Route::ExplicitTrack;
    }
#endif
    const bool sourceTiming = selected == Route::InternalPCM;
    // Deliberate ON precedence: UFO > explicit track > internal PCM. The old
    // transient explicit track, immediately replaced by UFO frame zero, is gone.
    if (v.mute || !_game->responsiveAudioAvailable()) selected = Route::Silent;
    std::unique_ptr<FlcPlayer> player(new FlcPlayer());
    int dx = (Options::baseXResolution - Screen::ORIGINAL_WIDTH) / 2;
    int dy = (Options::baseYResolution - Screen::ORIGINAL_HEIGHT) / 2;
    if (!player->initResponsive(filename.c_str(), _game, selected, _game->responsiveAudioGeneration(),
            Game::volumeExponent(v.musicVolume), sourceTiming, dx, dy))
    {
        ++v.index; return; // typed file/parse/cap failure: no lease was submitted
    }
    std::unique_ptr<AudioSequence> sequence;
    if (v.ufoAvailable) sequence.reset(new AudioSequence(selected == Route::UfoSequence ? _game->getMod() : nullptr,
        player.get(), v.soundVolume, selected == Route::UfoSequence));
    v.player.swap(player); v.sequence.swap(sequence); v.track = selected == Route::ExplicitTrack ? track : nullptr;
    v.lease = v.player->responsiveLease();
    Game::VideoAudioResult result = _game->reserveVideoAudio(this, v.lease, true);
    if (result == Game::VideoAudioResult::Reserved) v.phase = ResponsiveVideo::Phase::AwaitAttach;
    else
    {
        // Rejection/preparation failure did not enter work or mutate hooks.
        if (!v.lease->canRelease()) std::terminate();
        v.sequence.reset(); v.track = nullptr; v.lease.reset(); v.player.reset();
        v.failed = true; v.phase = ResponsiveVideo::Phase::Finish;
    }
}

void VideoState::responsiveReserveDetach() noexcept
{
    ResponsiveVideo &v = *_responsiveVideo;
    if (!v.lease) { v.phase = ResponsiveVideo::Phase::Finish; return; }
    if (v.player) v.player->skipResponsive(); // stop all visual/sequence production first
    Game::VideoAudioResult result = _game->reserveVideoAudio(this, v.lease, false);
    v.retryDetach = result != Game::VideoAudioResult::Reserved;
    v.phase = v.retryDetach ? ResponsiveVideo::Phase::Retained : ResponsiveVideo::Phase::AwaitDetach;
    // NoThread/setup/ambiguous entered failures are owned by Game's separate
    // operation gate; no owner stop/drain or destructor fallback is permitted.
}

void VideoState::responsiveStartFade(Uint32 now)
{
    ResponsiveVideo &v = *_responsiveVideo;
    if (!v.fadePossible || v.skip || v.close || v.failed) { v.phase = ResponsiveVideo::Phase::NeedDetach; return; }
    std::memcpy(v.palette, _game->getScreen()->getPalette(), sizeof v.palette);
    v.phaseTick = now; v.fadeStep = 20; v.fadeStarted = true; v.phase = ResponsiveVideo::Phase::Fade;
#ifndef __NO_MUSIC
    if (v.lease && v.lease->route() != FlcPlayer::AudioLease::Route::Silent)
    {
        // Ordinary owner playback API, not a hard-real-time/retirement claim.
        Mix_FadeOutChannel(-1, 900); Mix_FadeOutMusic(900); func_fade();
    }
#endif
}

void VideoState::responsiveAudioComplete(bool attach, bool success, bool closeRequested) noexcept
{
    if (!_responsiveVideo || _responsiveFinished) std::terminate();
    ResponsiveVideo &v = *_responsiveVideo;
    if (closeRequested) v.close = true;
    try
    {
        if (attach)
        {
            if (v.phase != ResponsiveVideo::Phase::AwaitAttach || !v.lease) std::terminate();
            if (!success)
            {
                // Game may report failure here only with no-hook/detach proof AND reap.
                if (!v.lease->canRelease()) std::terminate();
                v.sequence.reset(); v.track = nullptr; v.lease.reset(); v.player.reset();
                v.failed = true; v.phase = ResponsiveVideo::Phase::Finish; return;
            }
            if (v.lease->attachmentState() != FlcPlayer::AudioLease::AttachmentState::Attached) std::terminate();
            // Game delivers same-tick pending skip/close before this completion.
            if (v.close || v.skip) { v.phase = ResponsiveVideo::Phase::NeedDetach; return; }
            if (v.lease->route() == FlcPlayer::AudioLease::Route::UfoSequence && !v.volumeChanged)
            {
                Options::musicVolume = v.musicVolume; Options::soundVolume = v.soundVolume;
                v.volumeChanged = true; _game->setVolume(v.soundVolume, v.musicVolume, -1);
            }
            v.lease->notePlaybackStarted(); // includes a possibly throwing attempt
#ifndef __NO_MUSIC
            if (v.track) v.track->play(0);
#endif
            if (v.sequence) (*v.sequence)(); // actual frame-zero UFO commands, after reap
            if (v.close || v.skip) { v.phase = ResponsiveVideo::Phase::NeedDetach; return; }
            _game->unpauseResponsiveVideo(this); // exactly once, validated by Game provenance
            v.player->beginResponsive(SDL_GetTicks());
            v.phase = ResponsiveVideo::Phase::Playing;
        }
        else
        {
            if (v.phase != ResponsiveVideo::Phase::AwaitDetach || !v.lease) std::terminate();
            if (!success || v.lease->attachmentState() != FlcPlayer::AudioLease::AttachmentState::Detached)
            { v.retryDetach = false; v.phase = ResponsiveVideo::Phase::Retained; return; }
            // This callback is AFTER checked reap; release every per-video borrower.
            v.sequence.reset(); v.track = nullptr; v.lease.reset(); v.player.reset();
            ++v.index;
            v.phase = (v.close || v.skip || v.failed || v.index >= v.videos.size()) ?
                ResponsiveVideo::Phase::Finish : ResponsiveVideo::Phase::Prepare;
        }
    }
    catch (...) { v.failed = true; v.phase = v.lease && !v.lease->canRelease() ? ResponsiveVideo::Phase::NeedDetach : ResponsiveVideo::Phase::Finish; }
}

void VideoState::responsiveVideoTick(Uint32 now, bool present) noexcept
{
    if (_responsiveFinished) return;
    if (!_responsiveVideo)
    {
        if (_responsiveInitFailed) responsiveFinish(present);
        else init();
        return;
    }
    ResponsiveVideo &v = *_responsiveVideo;
    try
    {
        if ((v.close || v.skip) && (v.phase == ResponsiveVideo::Phase::Playing || v.phase == ResponsiveVideo::Phase::Hold || v.phase == ResponsiveVideo::Phase::Fade))
            v.phase = ResponsiveVideo::Phase::NeedDetach;
        switch (v.phase)
        {
        case ResponsiveVideo::Phase::Prepare: responsivePrepareVideo(); break;
        case ResponsiveVideo::Phase::Playing:
        {
            if (v.sequence) (*v.sequence)();
            FlcPlayer::ResponsiveStep step = v.player->stepResponsive(now, v.ufoAvailable, present);
            if (step == FlcPlayer::ResponsiveStep::Failed) { v.failed = true; v.phase = ResponsiveVideo::Phase::NeedDetach; }
            else if (step == FlcPlayer::ResponsiveStep::Finished || step == FlcPlayer::ResponsiveStep::Skipped)
            {
                if (v.ufoAvailable && !v.skip) { v.phase = ResponsiveVideo::Phase::Hold; v.phaseTick = now; }
                else if (v.index + 1 == v.videos.size()) responsiveStartFade(now);
                else v.phase = ResponsiveVideo::Phase::NeedDetach;
            }
            break;
        }
        case ResponsiveVideo::Phase::Hold:
            if (Uint32(now - v.phaseTick) >= 10000u)
            {
                if (v.index + 1 == v.videos.size()) responsiveStartFade(now);
                else v.phase = ResponsiveVideo::Phase::NeedDetach;
            }
            break;
        case ResponsiveVideo::Phase::Fade:
            if (Uint32(now - v.phaseTick) >= 45u)
            {
                SDL_Color palette[256];
                for (unsigned i = 0; i < 256; ++i)
                {
                    palette[i].r = Uint8(unsigned(v.palette[i].r) * v.fadeStep / 20u);
                    palette[i].g = Uint8(unsigned(v.palette[i].g) * v.fadeStep / 20u);
                    palette[i].b = Uint8(unsigned(v.palette[i].b) * v.fadeStep / 20u);
                    palette[i].unused = v.palette[i].unused;
                }
                _game->getScreen()->setPalette(palette, 0, 256, present);
                if (present) _game->getScreen()->flip();
                v.phaseTick = now;
                if (!--v.fadeStep) v.phase = ResponsiveVideo::Phase::NeedDetach;
            }
            break;
        case ResponsiveVideo::Phase::NeedDetach: responsiveReserveDetach(); break;
        case ResponsiveVideo::Phase::Finish: responsiveFinish(present); break;
        case ResponsiveVideo::Phase::AwaitAttach:
        case ResponsiveVideo::Phase::AwaitDetach:
        case ResponsiveVideo::Phase::Retained:
        case ResponsiveVideo::Phase::Done: break; // no poll/play/render while audio is worker-owned
        }
    }
    catch (...)
    {
        v.failed = true;
        v.phase = v.lease && !v.lease->canRelease() ? ResponsiveVideo::Phase::NeedDetach : ResponsiveVideo::Phase::Finish;
    }
}

void VideoState::responsiveRequestClose() noexcept
{
    if (_responsiveVideo) _responsiveVideo->close = true;
    else _responsiveInitFailed = true;
}

void VideoState::responsiveVideoEvent(const SDL_Event &event) noexcept
{
    if (event.type == SDL_QUIT) { responsiveRequestClose(); return; }
    if (!_responsiveVideo) return;
    ResponsiveVideo &v = *_responsiveVideo;
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN)
    {
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r && v.phase == ResponsiveVideo::Phase::Retained && v.retryDetach)
            v.phase = ResponsiveVideo::Phase::NeedDetach; // explicit pre-work reservation retry only
        else v.skip = true;
    }
    else if (event.type == SDL_VIDEORESIZE && v.allowResize)
    {
        v.resizePending = true; v.resizeWidth = std::max(Screen::ORIGINAL_WIDTH, event.resize.w);
        v.resizeHeight = std::max(Screen::ORIGINAL_HEIGHT, event.resize.h);
    }
    // Focus/minimize do not pause the timeline; generic debug/grab/events never run.
}

void VideoState::responsiveFinish(bool present) noexcept
{
    if (_responsiveFinished) return;
    if (_responsiveVideo)
    {
        ResponsiveVideo &v = *_responsiveVideo;
        if (v.lease && !v.lease->canRelease()) { v.phase = ResponsiveVideo::Phase::Retained; return; }
        v.sequence.reset(); v.track = nullptr; v.lease.reset(); v.player.reset();
        try
        {
            if (v.volumeChanged)
            {
                Options::musicVolume = v.oldMusic; Options::soundVolume = v.oldSound;
                v.volumeChanged = false; _game->setVolume(v.oldSound, v.oldMusic, v.oldUi);
            }
            if (v.displayChanged)
            {
                if (v.resizePending)
                {
                    Options::newDisplayWidth = Options::displayWidth = v.resizeWidth;
                    Options::newDisplayHeight = Options::displayHeight = v.resizeHeight;
                }
                v.displayChanged = false; CutsceneState::resetDisplay(v.wasLetterboxed);
                _game->getScreen()->clear();
                if (present) _game->getScreen()->flip();
            }
        }
        catch (...) { /* Presentation failure cannot resurrect audio or suppress retirement. */ }
        try { _game->getCursor()->setVisible(v.cursorVisible); } catch (...) {}
        v.epoch.reset(); v.phase = ResponsiveVideo::Phase::Done;
    }
    _responsiveFinished = true;
    _game->finishResponsiveVideo(this); // all borrowers gone, before state removal
    _game->popState();
}
#ifdef OPENXCOM_RESPONSIVE_TESTING
const char *LoadingVideoIntegrationTestAccess::phase(const VideoState &video) noexcept
{
    if (!video._responsiveVideo) return video._responsiveFinished ? "Done" : "Uninitialized";
    using P = VideoState::ResponsiveVideo::Phase;
    switch (video._responsiveVideo->phase)
    {
    case P::Prepare: return "Prepare"; case P::AwaitAttach: return "AwaitAttach";
    case P::Playing: return "Playing"; case P::Hold: return "Hold";
    case P::Fade: return "Fade"; case P::NeedDetach: return "NeedDetach";
    case P::AwaitDetach: return "AwaitDetach"; case P::Retained: return "Retained";
    case P::Finish: return "Finish"; case P::Done: return "Done";
    }
    return "Invalid";
}
std::weak_ptr<void> LoadingVideoIntegrationTestAccess::lease(const VideoState &video) noexcept
{ return video._responsiveVideo ? std::weak_ptr<void>(video._responsiveVideo->lease) : std::weak_ptr<void>(); }
unsigned LoadingVideoIntegrationTestAccess::frameCount(const VideoState &video) noexcept
{ return video._responsiveVideo && video._responsiveVideo->player ? unsigned(video._responsiveVideo->player->getFrameCount()) : 0u; }
unsigned LoadingVideoIntegrationTestAccess::headerSpeed(const VideoState &video) noexcept
{ return video._responsiveVideo && video._responsiveVideo->player ? video._responsiveVideo->player->_headerSpeed : 0u; }
unsigned LoadingVideoIntegrationTestAccess::route(const VideoState &video) noexcept
{ return video._responsiveVideo && video._responsiveVideo->lease ? unsigned(video._responsiveVideo->lease->route()) : 99u; }
bool LoadingVideoIntegrationTestAccess::closeRequested(const VideoState &video) noexcept
{ return video._responsiveVideo ? video._responsiveVideo->close : video._responsiveInitFailed; }
bool LoadingVideoIntegrationTestAccess::skipped(const VideoState &video) noexcept
{ return video._responsiveVideo && video._responsiveVideo->skip; }
#endif

#endif

}
