// Fork modification 2026-10-11: default-off supervised stance pilot; GPL-3.0-or-later.
#pragma once
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentController.h"
#include "../Observer/AgentAudioEvents.h"
#endif
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
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
#include "../Observer/MixedAudioObserver.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
#include "../Observer/StanceController.h"
#endif
#include <list>
#include <string>
#include <SDL.h>
#ifdef OPENXCOM_RESPONSIVE_LOADING
#include <memory>
#include <cstdint>
#include "LoaderSession.h"
#include "FlcPlayer.h"
#endif

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
#include "../Observer/SnapshotStamp.h"
#include "../Observer/SnapshotScope.h"
#include "../Observer/SnapshotPresentation.h"
#endif

namespace OpenXcom
{

class State;
class Screen;
class Cursor;
class Language;
class SavedGame;
class Mod;
class FpsCounter;
class Music;
#ifdef OPENXCOM_RESPONSIVE_LOADING
class VideoState;
struct LoadingGameIntegrationAccess;
#endif

/**
 * The core of the game engine, manages the game's entire contents and structure.
 * Takes care of encapsulating all the core SDL commands, provides access to all
 * the game's resources and contains a stack state machine to handle all the
 * initializations, events and blits of each state, as well as transitions.
 */
class Game
{
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE
    friend struct InventoryCloseGateAccess;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    friend struct InventoryClickGateAccess;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    friend struct InventoryRightClickGateAccess;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    friend struct InventoryUnloadGateAccess;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
    friend struct InventoryGroundGateAccess;
#endif
private:
#ifdef OPENXCOM_RESPONSIVE_LOADING
    // Constructed first: a denied process lease cannot publish resources.
    LoaderProcessLease _loadingProcessLease;
    struct ResourceEpoch;
    struct LoadingWork;
    struct PreparedOperation;
    struct ResponsiveCore;
    std::unique_ptr<ResponsiveCore> _responsive;
    bool loadingBoundary() noexcept;
    void applyResponsiveResize() noexcept;
    void runOneOperationTick() noexcept;
    bool runResponsiveVideoTick() noexcept;
    bool reserveResponsiveCleanup() noexcept;
    void beginResponsiveCleanup() noexcept;
    void publishResponsiveLoad() noexcept;
    void assertResponsiveOwnerAccess() const noexcept;
    static LoaderSession::WorkResult loadResponsiveResources(void *, const LoaderSession::StopToken &);
    static LoaderSession::WorkResult cleanupResponsiveResources(void *, const LoaderSession::StopToken &);
    static LoaderSession::WorkResult transitionResponsiveVideo(void *, const LoaderSession::StopToken &);
    static void loadEpochLanguages(ResourceEpoch &);
#ifdef OPENXCOM_RESPONSIVE_TESTING
    friend struct LoadingGameIntegrationAccess;
#endif
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    Observer::SnapshotStamp _snapshotSavedRevision;
    Observer::SnapshotStamp _snapshotModRevision;
    Observer::SnapshotStamp _snapshotStackRevision;
    Observer::SnapshotActivity _snapshotInitActivity;
    Observer::SnapshotPresentation _snapshotPresentation;
    Observer::SnapshotPaintedState snapshotPaintedState(const State *state) const noexcept;
    bool snapshotFrameStateCurrent() const noexcept;
#endif

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    Observer::StanceController _stanceController;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    Observer::AgentController _agentController;
    Observer::AgentAudioEvents _agentAudioEvents;
#endif
#if defined(OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER) && !defined(OPENXCOM_RESPONSIVE_LOADING)
	Observer::MixedAudioObserver _mixedAudioObserver;
#endif
	SDL_Event _event;
	Screen *_screen;
	Cursor *_cursor;
	Language *_lang;
	std::list<State*> _states, _deleted;
	SavedGame *_save;
	Mod *_mod;
	bool _quit, _init;
	FpsCounter *_fpsCounter;
	bool _mouseActive;
	unsigned int _timeOfLastFrame;
	int _timeUntilNextFrame;
	static const double VOLUME_GRADIENT;

public:
#ifdef OPENXCOM_RESPONSIVE_LOADING
    enum class LoadingOrigin { Startup, OptionsMenu, ModListMenu };
    enum class LoadingAdmission { Accepted, Busy, LiveSave, UnsupportedProfile, UnknownAudioGeneration, InvalidDeviceState, PreparationFailure };
    enum class VideoAudioResult { RejectedUnchanged, Reserved, PreparationFailure };
    LoadingAdmission responsiveLoadEligibility(LoadingOrigin, const State *caller) const noexcept;
    LoadingAdmission reserveResponsiveLoad(LoadingOrigin, State *caller = nullptr) noexcept;
    static const char *loadingAdmissionMessage(LoadingAdmission) noexcept;
    VideoAudioResult reserveVideoAudio(VideoState *, std::shared_ptr<FlcPlayer::AudioLease>, bool attach) noexcept;
    std::shared_ptr<void> retainResponsiveEpoch() const;
    std::uint64_t responsiveAudioGeneration() const noexcept;
    bool responsiveAudioAvailable() const noexcept;
    void invalidateResponsiveAudio() noexcept;
    void finishResponsiveVideo(VideoState *) noexcept;
    void unpauseResponsiveVideo(VideoState *) noexcept;
    static bool responsiveMusicPlaybackAllowed(const Music *) noexcept;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    const Observer::SnapshotFrameFacts &snapshotFrameFacts() const noexcept { return _snapshotPresentation.facts(); }
    std::uint64_t snapshotSavedRevision() const noexcept { return _snapshotSavedRevision.value(); }
    std::uint64_t snapshotModRevision() const noexcept { return _snapshotModRevision.value(); }
    std::uint64_t snapshotStackRevision() const noexcept { return _snapshotStackRevision.value(); }
    State *snapshotLiveState(const State *state) const noexcept;
    bool snapshotStateInStack(const State *state) const noexcept;
    bool snapshotStateInitialized(const State *state) const noexcept;
    bool snapshotInitializationIdle() const noexcept { return _snapshotInitActivity.idle(); }
#endif

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    Observer::StanceController *getStanceController() { return &_stanceController; }
#endif
	/// Creates a new game and initializes SDL.
	Game(const std::string &title);
	/// Cleans up all the game's resources and shuts down SDL.
	~Game();
	/// Starts the game's state machine.
	void run();
	/// Quits the game.
	void quit();
	/// Sets the game's audio volume.
	void setVolume(int sound, int music, int ui);
	/// Adjusts a linear volume level to an exponential one.
	static double volumeExponent(int volume);
	/// Gets the game's display screen.
	Screen *getScreen() const;
	/// Gets the game's cursor.
	Cursor *getCursor() const;
	/// Gets the FpsCounter.
	FpsCounter *getFpsCounter() const;
	/// Resets the state stack to a new state.
	void setState(State *state);
	/// Pushes a new state into the state stack.
	void pushState(State *state);
	/// Pops the last state from the state stack.
	void popState();
	/// Gets the currently loaded language.
	Language *getLanguage() const;
	/// Gets the currently loaded saved game.
	SavedGame *getSavedGame() const;
	/// Sets a new saved game for the game.
	void setSavedGame(SavedGame *save);
	/// Gets the currently loaded mod.
	Mod *getMod() const;
	/// Loads the mods specified in the game options.
	void loadMods();
	/// Sets whether the mouse cursor is activated.
	void setMouseActive(bool active);
	/// Returns whether current state is the param state
	bool isState(State *state) const;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    State *agentTopState() const;
    bool agentOverlayClear(const Observer::AgentUiState &ui) const;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
    // Bind a pending observation to its admission state, not a previous action.
    State *turnControlState() const { return _states.empty() ? 0 : _states.back(); }
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    // Private-observer lifetime check: the inventory's live immediate parent.
    bool inventoryParentState(State *parent) const;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
    // Only reports natural deferred cleanup and top-state initialization.
    bool inventoryCloseRestored(State *parent) const { return _init && _deleted.empty() && isState(parent); }
#endif
#endif
	/// Returns whether the game is shutting down.
	bool isQuitting() const;
	/// Loads the default and current language.
	void loadLanguages();
	/// Sets up the audio.
	void initAudio();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    Observer::AgentAudioEvents &agentAudioEvents() { return _agentAudioEvents; }
    void resetAgentAudioEvents(bool resourceReload = false);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
	/// Invalidate observation before mixer or resource changes (main thread).
	void resetMixedAudioObserver(bool resourceReload = false);
#endif
};

}
