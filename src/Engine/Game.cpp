// Fork modification 2026-10-11: default-off supervised stance pilot; GPL-3.0-or-later.
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
// Fork modification 2026-10-10: default-off mixed-audio observation.
// SPDX-License-Identifier: GPL-3.0-or-later; upstream notices retained.
// Fork modification 2026-10-10: default-off bounded observer-effects and natural-FOV test hooks.
// See LICENSE-NOTICES.md; existing upstream license retained.
// Fork modification 2026-10-10: default-off battlefield-audio test seam.
// See LICENSE-NOTICES.md; upstream license retained.
#ifdef OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE
#include "../../tests/battle-observer/BattlefieldAudioGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
#include "../../tests/battle-observer/ObserverEffectsGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
#include "../../tests/battle-observer/NaturalFovGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE
#include "../../tests/stance-control/StanceControlGate.h"
#endif
// Fork modification 2026-10-10: opt-in read-only public observation hooks;
// see LICENSE-NOTICES.md. Existing upstream copyright and license retained.
// Fork modification 2026-10-10: default-off render-only test gate.
// SPDX-License-Identifier: GPL-3.0-or-later; upstream notices retained.
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
#include "../../tests/battle-observer/RenderPairGate.h"
#endif
#include "Game.h"
#include "../resource.h"
#include <algorithm>
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
#include <cstdlib>
#endif
#include <cmath>
#include <sstream>
#include <SDL_mixer.h>
#include "State.h"
#include "Screen.h"
#include "Sound.h"
#include "Music.h"
#include "Language.h"
#include "Logger.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "Action.h"
#include "Exception.h"
#include "Options.h"
#include "CrossPlatform.h"
#include "FileMap.h"
#include "Unicode.h"
#include "../Menu/TestState.h"

namespace OpenXcom
{

const double Game::VOLUME_GRADIENT = 10.0;

/**
 * Starts up all the SDL subsystems,
 * creates the display screen and sets up the cursor.
 * @param title Title of the game window.
 */
Game::Game(const std::string &title) :
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _stanceController(this, std::getenv("OPENXCOM_STANCE_FD")),
#endif
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
	_mixedAudioObserver(std::getenv("OPENXCOM_MIXED_AUDIO_FD")),
#endif
	_screen(0), _cursor(0), _lang(0), _save(0), _mod(0), _quit(false), _init(false), _mouseActive(true), _timeUntilNextFrame(0)
{
	Options::reload = false;
	Options::mute = false;

	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		throw Exception(SDL_GetError());
	}
	Log(LOG_INFO) << "SDL initialized successfully.";

	// Initialize SDL_mixer
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		Log(LOG_ERROR) << SDL_GetError();
		Log(LOG_WARNING) << "No sound device detected, audio disabled.";
		Options::mute = true;
	}
	else
	{
		initAudio();
	}

	// trap the mouse inside the window
	SDL_WM_GrabInput(Options::captureMouse);
	
	// Set the window icon
	CrossPlatform::setWindowIcon(IDI_ICON1, FileMap::getFilePath("openxcom.png"));

	// Set the window caption
	SDL_WM_SetCaption(title.c_str(), 0);

	// Set up unicode
	SDL_EnableUNICODE(1);
	Unicode::getUtf8Locale();

	// Create display
	_screen = new Screen();
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _screen->setStanceController(&_stanceController);
#endif

	// Create cursor
	_cursor = new Cursor(9, 13);
	
	// Create invisible hardware cursor to workaround bug with absolute positioning pointing devices
	SDL_ShowCursor(SDL_ENABLE);
	Uint8 cursor = 0;
	SDL_SetCursor(SDL_CreateCursor(&cursor, &cursor, 1,1,0,0));

	// Create fps counter
	_fpsCounter = new FpsCounter(15, 5, 0, 0);

	// Create blank language
	_lang = new Language();

	_timeOfLastFrame = 0;
}

/**
 * Deletes the display screen, cursor, states and shuts down all the SDL subsystems.
 */
Game::~Game()
{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _stanceController.close();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
	_mixedAudioObserver.close();
#endif
	Sound::stop();
	Music::stop();

	for (std::list<State*>::iterator i = _states.begin(); i != _states.end(); ++i)
	{
		delete *i;
	}

	SDL_FreeCursor(SDL_GetCursor());

	delete _cursor;
	delete _lang;
	delete _save;
	delete _mod;
	delete _screen;
	delete _fpsCounter;

	Mix_CloseAudio();

	SDL_Quit();
}

/**
 * The state machine takes care of passing all the events from SDL to the
 * active state, running any code within and blitting all the states and
 * cursor to the screen. This is run indefinitely until the game quits.
 */
void Game::run()
{
	enum ApplicationState { RUNNING = 0, SLOWED = 1, PAUSED = 2 } runningState = RUNNING;
	static const ApplicationState kbFocusRun[4] = { RUNNING, RUNNING, SLOWED, PAUSED };
	static const ApplicationState stateRun[4] = { SLOWED, PAUSED, PAUSED, PAUSED };
	// this will avoid processing SDL's resize event on startup, workaround for the heap allocation error it causes.
	bool startupEvent = Options::allowResize;
	while (!_quit)
	{
		// Clean up states
		while (!_deleted.empty())
		{
			delete _deleted.back();
			_deleted.pop_back();
		}

#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
		RenderPairGate::afterCleanup(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
		ObserverEffectsGate::afterCleanup(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
		NaturalFovGate::afterCleanup(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE
		StanceControlGate::afterCleanup(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE
		BattlefieldAudioGate::afterCleanup(this);
#endif
		// Initialize active state
		if (!_init)
		{
			_init = true;
			_states.back()->init();

			// Unpress buttons
			_states.back()->resetAll();

			// Refresh mouse position
			SDL_Event ev;
			int x, y;
			SDL_GetMouseState(&x, &y);
			ev.type = SDL_MOUSEMOTION;
			ev.motion.x = x;
			ev.motion.y = y;
			Action action = Action(&ev, _screen->getXScale(), _screen->getYScale(), _screen->getCursorTopBlackBand(), _screen->getCursorLeftBlackBand());
			_states.back()->handle(&action);
		}

#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
		ObserverEffectsGate::beforeEvents(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
		NaturalFovGate::beforeEvents(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE
		StanceControlGate::beforeEvents(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE
		BattlefieldAudioGate::beforeEvents(this);
#endif
		// Process events
		while (SDL_PollEvent(&_event))
		{
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
#ifdef OPENXCOM_EXPERIMENTAL_TIMING_CALIBRATION
			RenderPairGate::timingEvent(_event);
#endif
			// Fixed render-fixture input; still permit closing the test window.
			if (RenderPairGate::active() && _event.type != SDL_QUIT && _event.type != SDL_ACTIVEEVENT) continue;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
			if (!ObserverEffectsGate::acceptEvent(_event)) continue;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
			if (!NaturalFovGate::acceptEvent(_event)) continue;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE
			if (!StanceControlGate::acceptEvent(_event)) continue;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE
			if (!BattlefieldAudioGate::acceptEvent(_event)) continue;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
            _stanceController.beforeEvent(_event, _states.back());
#endif
            if (CrossPlatform::isQuitShortcut(_event))
				_event.type = SDL_QUIT;
			switch (_event.type)
			{
				case SDL_QUIT:
					quit();
					break;
				case SDL_ACTIVEEVENT:
					// An event other than SDL_APPMOUSEFOCUS change happened.
					if (reinterpret_cast<SDL_ActiveEvent*>(&_event)->state & ~SDL_APPMOUSEFOCUS)
					{
						Uint8 currentState = SDL_GetAppState();
						// Game is minimized
						if (!(currentState & SDL_APPACTIVE))
						{
							runningState = stateRun[Options::pauseMode];
							if (Options::backgroundMute)
							{
								setVolume(0, 0, 0);
							}
						}
						// Game is not minimized but has no keyboard focus.
						else if (!(currentState & SDL_APPINPUTFOCUS))
						{
							runningState = kbFocusRun[Options::pauseMode];
							if (Options::backgroundMute)
							{
								setVolume(0, 0, 0);
							}
						}
						// Game has keyboard focus.
						else
						{
							runningState = RUNNING;
							if (Options::backgroundMute)
							{
								setVolume(Options::soundVolume, Options::musicVolume, Options::uiVolume);
							}
						}
					}
					break;
				case SDL_VIDEORESIZE:
					if (Options::allowResize)
					{
						if (!startupEvent)
						{
							Options::newDisplayWidth = Options::displayWidth = std::max(Screen::ORIGINAL_WIDTH, _event.resize.w);
							Options::newDisplayHeight = Options::displayHeight = std::max(Screen::ORIGINAL_HEIGHT, _event.resize.h);
							int dX = 0, dY = 0;
							Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, false);
							Screen::updateScale(Options::geoscapeScale, Options::baseXGeoscape, Options::baseYGeoscape, false);
							for (std::list<State*>::iterator i = _states.begin(); i != _states.end(); ++i)
							{
								(*i)->resize(dX, dY);
							}
							_screen->resetDisplay();
						}
						else
						{
							startupEvent = false;
						}
					}
					break;
				case SDL_MOUSEMOTION:
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
					// Skip mouse events if they're disabled
					if (!_mouseActive) continue;
					// re-gain focus on mouse-over or keypress.
					runningState = RUNNING;
					// Go on, feed the event to others
				default:
					Action action = Action(&_event, _screen->getXScale(), _screen->getYScale(), _screen->getCursorTopBlackBand(), _screen->getCursorLeftBlackBand());
					_screen->handle(&action);
					_cursor->handle(&action);
					_fpsCounter->handle(&action);
					if (action.getDetails()->type == SDL_KEYDOWN)
					{
						// "ctrl-g" grab input
						if (action.getDetails()->key.keysym.sym == SDLK_g && (SDL_GetModState() & KMOD_CTRL) != 0)
						{
							Options::captureMouse = (SDL_GrabMode)(!Options::captureMouse);
							SDL_WM_GrabInput(Options::captureMouse);
						}
						else if (Options::debug)
						{
							if (action.getDetails()->key.keysym.sym == SDLK_t && (SDL_GetModState() & KMOD_CTRL) != 0)
							{
								setState(new TestState);
							}
							// "ctrl-u" debug UI
							else if (action.getDetails()->key.keysym.sym == SDLK_u && (SDL_GetModState() & KMOD_CTRL) != 0)
							{
								Options::debugUi = !Options::debugUi;
								_states.back()->redrawText();
							}
						}
					}
					_states.back()->handle(&action);
					break;
			}
			if (!_init)
			{
				// States stack was changed, break the loop so new state
				// can be initialized before processing new events
				break;
			}
		}
		
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
		ObserverEffectsGate::afterEvents(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
		NaturalFovGate::afterEvents(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE
		StanceControlGate::afterEvents(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE
		BattlefieldAudioGate::afterEvents(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
        _stanceController.poll();
#endif
        // Process rendering
		if (runningState != PAUSED)
		{
			// Process logic
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
			// The gate covers rendering/animation only, not simulation or timers.
			if (!RenderPairGate::active())
#endif
			_states.back()->think();
			_fpsCounter->think();
			if (Options::FPS > 0 && !(Options::useOpenGL && Options::vSyncForOpenGL))
			{
				// Update our FPS delay time based on the time of the last draw.
				int fps = SDL_GetAppState() & SDL_APPINPUTFOCUS ? Options::FPS : Options::FPSInactive;

				_timeUntilNextFrame = (1000.0f / fps) - (SDL_GetTicks() - _timeOfLastFrame);
			}
			else
			{
				_timeUntilNextFrame = 0;
			}

#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
			if (RenderPairGate::active()) _timeUntilNextFrame = 0;
#endif
			if (_init && _timeUntilNextFrame <= 0)
			{
				// make a note of when this frame update occurred.
				_timeOfLastFrame = SDL_GetTicks();
				_fpsCounter->addFrame();
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
				RenderPairGate::beforeFrame(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
				ObserverEffectsGate::beforeFrame(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
				NaturalFovGate::beforeFrame(this, _states.back());
#endif
#ifdef OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE
				StanceControlGate::beforeFrame(this, _states.back());
#endif
#ifdef OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE
				BattlefieldAudioGate::beforeFrame(this, _states.back());
#endif
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
                _stanceController.beginFrame();
                if (!_init) continue;
#endif
                _screen->beginObserverFrame();
#endif
				_screen->clear();
				std::list<State*>::iterator i = _states.end();
				do
				{
					--i;
				}
				while (i != _states.begin() && !(*i)->isScreen());

				for (; i != _states.end(); ++i)
				{
					(*i)->blit();
				}
				_fpsCounter->blit(_screen->getSurface());
				_cursor->blit(_screen->getSurface());
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
				if (_fpsCounter->getVisible())
					_screen->excludePresentedHud(_fpsCounter->getX(), _fpsCounter->getY(), _fpsCounter->getWidth(), _fpsCounter->getHeight());
				if (_cursor->getVisible())
					_screen->excludePresentedHud(_cursor->getX(), _cursor->getY(), _cursor->getWidth(), _cursor->getHeight());
				const bool cursorPresented = _cursor->getVisible();
				_screen->flip(cursorPresented ? _cursor->getX() : -1, cursorPresented ? _cursor->getY() : -1);
#else
				_screen->flip();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
				RenderPairGate::afterFrame(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
				ObserverEffectsGate::afterFrame(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
				NaturalFovGate::afterFrame(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE
				StanceControlGate::afterFrame(this);
#endif
			}
		}

		// Independent observer polling also runs while application is PAUSED.
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
#ifdef OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE
		BattlefieldAudioGate::beforePoll();
#endif
		_mixedAudioObserver.poll();
#ifdef OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE
		BattlefieldAudioGate::afterPoll(this);
#endif
#endif

		// Save on CPU
		switch (runningState)
		{
			case RUNNING: 
				SDL_Delay(1); //Save CPU from going 100%
				break;
			case SLOWED: case PAUSED:
				SDL_Delay(100); break; //More slowing down.
		}
	}

	Options::save();
}

/**
 * Stops the state machine and the game is shut down.
 */
void Game::quit()
{
	// Always save ironman
	if (_save != 0 && _save->isIronman() && !_save->getName().empty())
	{
		std::string filename = CrossPlatform::sanitizeFilename(Unicode::convUtf8ToPath(_save->getName())) + ".sav";
		_save->save(filename);
	}
	_quit = true;
}

/**
 * Changes the audio volume of the music and
 * sound effect channels.
 * @param sound Sound volume, from 0 to MIX_MAX_VOLUME.
 * @param music Music volume, from 0 to MIX_MAX_VOLUME.
 * @param ui UI volume, from 0 to MIX_MAX_VOLUME.
 */
void Game::setVolume(int sound, int music, int ui)
{
	if (!Options::mute)
	{
		if (sound >= 0)
		{
			sound = volumeExponent(sound) * (double)SDL_MIX_MAXVOLUME;
			Mix_Volume(-1, sound);
			if (_save && _save->getSavedBattle())
			{
				Mix_Volume(3, sound * _save->getSavedBattle()->getAmbientVolume());
			}
			else
			{
				// channel 3: reserved for ambient sound effect.
				Mix_Volume(3, sound / 2);
			}
		}
		if (music >= 0)
		{
			music = volumeExponent(music) * (double)SDL_MIX_MAXVOLUME;
			Mix_VolumeMusic(music);
		}
		if (ui >= 0)
		{
			ui = volumeExponent(ui) * (double)SDL_MIX_MAXVOLUME;
			Mix_Volume(1, ui);
			Mix_Volume(2, ui);
		}
	}
}

double Game::volumeExponent(int volume)
{
	return (exp(log(Game::VOLUME_GRADIENT + 1.0) * volume / (double)SDL_MIX_MAXVOLUME) -1.0 ) / Game::VOLUME_GRADIENT;
}

/**
 * Returns the display screen used by the game.
 * @return Pointer to the screen.
 */
Screen *Game::getScreen() const
{
	return _screen;
}

/**
 * Returns the mouse cursor used by the game.
 * @return Pointer to the cursor.
 */
Cursor *Game::getCursor() const
{
	return _cursor;
}

/**
 * Returns the FpsCounter used by the game.
 * @return Pointer to the FpsCounter.
 */
FpsCounter *Game::getFpsCounter() const
{
	return _fpsCounter;
}

/**
 * Pops all the states currently in stack and pushes in the new state.
 * A shortcut for cleaning up all the old states when they're not necessary
 * like in one-way transitions.
 * @param state Pointer to the new state.
 */
void Game::setState(State *state)
{
	while (!_states.empty())
	{
		popState();
	}
	pushState(state);
	_init = false;
}

/**
 * Pushes a new state into the top of the stack and initializes it.
 * The new state will be used once the next game cycle starts.
 * @param state Pointer to the new state.
 */
void Game::pushState(State *state)
{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _stanceController.beforePush(state);
#endif
	_states.push_back(state);
	_init = false;
}

/**
 * Pops the last state from the top of the stack. Since states
 * can't actually be deleted mid-cycle, it's moved into a separate queue
 * which is cleared at the start of every cycle, so the transition
 * is seamless.
 */
void Game::popState()
{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _stanceController.beforePop(_states.back());
#endif
	_deleted.push_back(_states.back());
	_states.pop_back();
	_init = false;
}

/**
 * Returns the language currently in use by the game.
 * @return Pointer to the language.
 */
Language *Game::getLanguage() const
{
	return _lang;
}

/**
 * Returns the saved game currently in use by the game.
 * @return Pointer to the saved game.
 */
SavedGame *Game::getSavedGame() const
{
	return _save;
}

/**
 * Sets a new saved game for the game to use.
 * @param save Pointer to the saved game.
 */
void Game::setSavedGame(SavedGame *save)
{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _stanceController.invalidate(true);
#endif
	delete _save;
	_save = save;
}

/**
 * Returns the mod currently in use by the game.
 * @return Pointer to the mod.
 */
Mod *Game::getMod() const
{
	return _mod;
}

/**
 * Loads the mods specified in the game options.
 */
void Game::loadMods()
{
	Mod::resetGlobalStatics();
	delete _mod;
	_mod = new Mod();
	_mod->loadAll(FileMap::getRulesets());
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
	// This executes on the loader thread; only an atomic aggregate crosses over.
	_mixedAudioObserver.publishCatalogSupported(_mod->isMusicCatalogSoftwareMixed());
#endif
}

/**
 * Sets whether the mouse is activated.
 * If it is, mouse events are processed, otherwise
 * they are ignored and the cursor is hidden.
 * @param active Is mouse activated?
 */
void Game::setMouseActive(bool active)
{
	_mouseActive = active;
	_cursor->setVisible(active);
}

/**
 * Returns whether current state is *state
 * @param state The state to test against the stack state
 * @return Is state the current state?
 */
bool Game::isState(State *state) const
{
	return !_states.empty() && _states.back() == state;
}

/**
 * Checks if the game is currently quitting.
 * @return whether the game is shutting down or not.
 */
bool Game::isQuitting() const
{
	return _quit;
}

/**
 * Loads the most appropriate languages
 * given current system and game options.
 */
void Game::loadLanguages()
{
	const std::string defaultLang = "en-US";
	std::string currentLang = defaultLang;

	std::ostringstream ss;
	ss << "common/Language/" << defaultLang << ".yml";
	std::string defaultPath = CrossPlatform::searchDataFile(ss.str());
	std::string path = defaultPath;

	// No language set, detect based on system
	if (Options::language.empty())
	{
		std::string locale = CrossPlatform::getLocale();
		std::string lang = locale.substr(0, locale.find_first_of('-'));
		// Try to load full locale
		Unicode::replace(path, defaultLang, locale);
		if (Language::isSupported(locale) && CrossPlatform::fileExists(path))
		{
			currentLang = locale;
		}
		else
		{
			// Try to load language locale
			Unicode::replace(path, locale, lang);
			if (Language::isSupported(lang) && CrossPlatform::fileExists(path))
			{
				currentLang = lang;
			}
			// Give up, use default
			else
			{
				currentLang = defaultLang;
			}
		}
	}
	else
	{
		// Use options language
		Unicode::replace(path, defaultLang, Options::language);
		if (CrossPlatform::fileExists(path))
		{
			currentLang = Options::language;
		}
		// Language not found, use default
		else
		{
			currentLang = defaultLang;
		}
	}
	Options::language = currentLang;

	delete _lang;
	_lang = new Language();

	// Load default and current language
	std::ostringstream ssDefault, ssCurrent;
	ssDefault << "/Language/" << defaultLang << ".yml";
	ssCurrent << "/Language/" << currentLang << ".yml";

	_lang->loadFile(CrossPlatform::searchDataFile("common" + ssDefault.str()));
	if (currentLang != defaultLang)
		_lang->loadFile(CrossPlatform::searchDataFile("common" + ssCurrent.str()));

	// if this is a master but it has a master of its own, allow it to
	// chainload the "super" master, including its languages
	ModInfo modInfo = Options::getModInfo(Options::getActiveMaster());
	if (!modInfo.getMaster().empty())
	{
		ModInfo masterInfo = Options::getModInfo(modInfo.getMaster());
		_lang->loadFile(masterInfo.getPath() + ssDefault.str());
		if (currentLang != defaultLang)
			_lang->loadFile(masterInfo.getPath() + ssCurrent.str());
	}

	std::vector<const ModInfo*> activeMods = Options::getActiveMods();
	for (std::vector<const ModInfo*>::const_iterator i = activeMods.begin(); i != activeMods.end(); ++i)
	{
		_lang->loadFile((*i)->getPath() + ssDefault.str());
		if (currentLang != defaultLang)
			_lang->loadFile((*i)->getPath() + ssCurrent.str());
	}

	_lang->loadRule(_mod->getExtraStrings(), defaultLang);
	if (currentLang != defaultLang)
		_lang->loadRule(_mod->getExtraStrings(), currentLang);
}

#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
void Game::resetMixedAudioObserver(bool resourceReload)
{
	_mixedAudioObserver.beforeAudioReset(resourceReload);
}
#endif

/**
 * Initializes the audio subsystem.
 */
void Game::initAudio()
{
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
	resetMixedAudioObserver();
#endif
	Uint16 format = MIX_DEFAULT_FORMAT;
	if (Options::audioBitDepth == 8)
		format = AUDIO_S8;

	if (Options::audioSampleRate % 11025 != 0)
	{
		Log(LOG_WARNING) << "Custom sample rate " << Options::audioSampleRate << "Hz, audio that doesn't match will be distorted!";
		Log(LOG_WARNING) << "SDL_mixer only supports multiples of 11025Hz.";
	}
	int minChunk = Options::audioSampleRate / 11025 * 512;
	Options::audioChunkSize = std::max(minChunk, Options::audioChunkSize);

	if (Mix_OpenAudio(Options::audioSampleRate, format, MIX_DEFAULT_CHANNELS, Options::audioChunkSize) != 0)
	{
		Log(LOG_ERROR) << Mix_GetError();
		Log(LOG_WARNING) << "No sound device detected, audio disabled.";
		Options::mute = true;
	}
	else
	{
		Mix_AllocateChannels(16);
		// Set up UI channels
		Mix_ReserveChannels(4);
		Mix_GroupChannels(1, 2, 0);
		Log(LOG_INFO) << "SDL_mixer initialized successfully.";
		setVolume(Options::soundVolume, Options::musicVolume, Options::uiVolume);
	}
}

}
