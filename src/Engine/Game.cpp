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
#if defined(OPENXCOM_RESPONSIVE_LOADING) && defined(OPENXCOM_RESPONSIVE_TESTING)
#include "../../tests/snapshot-control/loading_game_integration_fixture.h"
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
#include "LoaderThreadLinux.h"
#include "LoadingUiPolicy.h"
#include "LoadingAudioHandoff.h"
#include "Surface.h"
#include "../Menu/StartState.h"
#include "../Menu/MainMenuState.h"
#include "../Menu/CutsceneState.h"
#include "../Menu/OptionsBaseState.h"
#include "../Menu/ModListState.h"
#include "../Menu/VideoState.h"
#include <array>
#include <limits>
#include <stdexcept>
#include <cstdlib>
#include <new>
#include <type_traits>
#include <vector>
#endif
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

#ifdef OPENXCOM_RESPONSIVE_LOADING
namespace
{
// Immutable helper receipts are constructed exactly once into storage already
// owned by the attempt. Recording a native completion can never allocate or
// assign into a const-field receipt; absence stays distinct from invalid proof.
template<class Receipt> class ResponsiveReceiptSlot
{
    typename std::aligned_storage<sizeof(Receipt), alignof(Receipt)>::type _storage;
    bool _present;
    const Receipt *value() const noexcept { return reinterpret_cast<const Receipt *>(&_storage); }
public:
    ResponsiveReceiptSlot() noexcept : _present(false) {}
    ~ResponsiveReceiptSlot() noexcept { if (_present) value()->~Receipt(); }
    void record(const Receipt &receipt) noexcept
    {
        static_assert(std::is_nothrow_copy_constructible<Receipt>::value, "native receipt recording must not throw");
        if (_present) std::terminate();
        new (static_cast<void *>(&_storage)) Receipt(receipt);
        _present = true;
    }
    bool validFor(std::uint64_t generation, const void *lease) const noexcept
    { return _present && value()->validFor(generation, lease); }
    ResponsiveReceiptSlot(const ResponsiveReceiptSlot &) = delete;
    ResponsiveReceiptSlot &operator=(const ResponsiveReceiptSlot &) = delete;
};
}
// These owning objects never contain a borrowed Game/State pointer in worker
// context. The owner keeps raw aliases only while the epoch is published.
struct Game::ResourceEpoch
{
    std::unique_ptr<Mod> mod;
    std::unique_ptr<Language> language;
    LoadingAudioHandoff::Generation generation;
    int openedChunkRequest; // captured successful request; Mix_QuerySpec does not expose it
    bool workerOwned, nativeResources, unknownNativeResources;
    Mod::ResponsiveMusicCatalog catalog;
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    std::unique_ptr<Observer::MixedAudioObserver> observer;
#endif
    ResourceEpoch() : generation(), openedChunkRequest(0), workerOwned(false), nativeResources(false), unknownNativeResources(false), catalog(Mod::ResponsiveMusicCatalog::Empty)
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
        , observer(new Observer::MixedAudioObserver(std::getenv("OPENXCOM_MIXED_AUDIO_FD")))
#endif
    {}
};

struct Game::LoadingWork
{
    enum class Kind { Load, VideoAttach, VideoDetach, Cleanup };
    enum class Disposition { Pending, Complete, Cancelled, RejectedRetained, FailedRetained };
    const std::shared_ptr<ResourceEpoch> epoch;
    const Kind kind;
    const LoadingAudioHandoff::Generation generationAtPreparation;
    const LoadingAudioHandoff::AudioPlan plan;
    const bool persist, mapResources, silentRequested;
    std::unique_ptr<LoadingAudioHandoff> oldAudio, freshAudio;
    std::shared_ptr<FlcPlayer::AudioLease> videoLease;
    ResponsiveReceiptSlot<FlcPlayer::AudioLease::AttachReceipt> attachReceipt;
    ResponsiveReceiptSlot<FlcPlayer::AudioLease::DetachReceipt> detachReceipt;
    bool entered, constructing, workComplete, noHook, detached, closed;
    Disposition disposition;
    LoadingAudioHandoff::Outcome audioOutcome;
    LoadingAudioHandoff::CloseReceipt closeReceipt;
    LoadingWork(std::shared_ptr<ResourceEpoch> e, Kind k,
        const LoadingAudioHandoff::AudioPlan &p, bool save, bool map, bool silent) :
        epoch(std::move(e)), kind(k), generationAtPreparation(epoch->generation), plan(p), persist(save), mapResources(map), silentRequested(silent),
        oldAudio(new LoadingAudioHandoff(
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
            epoch->observer.get()
#endif
        )), freshAudio(new LoadingAudioHandoff(
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
            epoch->observer.get()
#endif
        )), entered(false), constructing(false), workComplete(false), noHook(false), detached(false), closed(false),
        disposition(Disposition::Pending), audioOutcome(), closeReceipt() {}
    void seal() noexcept { oldAudio->sealForWorker(); freshAudio->sealForWorker(); }
};

struct Game::PreparedOperation
{
    std::shared_ptr<LoadingWork> work, cleanup;
    std::shared_ptr<LoaderThreadBackend> backend, cleanupBackend;
    std::shared_ptr<LoaderSession> session, cleanupSession;
    std::unique_ptr<LoadingUiPolicy> policy;
    std::unique_ptr<StartState> terminal, retirementTerminal;
    std::list<State *> terminalNode, retirementNode;
    std::array<std::unique_ptr<Surface>, 3> frames;
    Screen::LoadingPresentation presentation;
    Zoom::Workspace workspace;
    SDL_Surface *frameDestination;
    std::string oldMaster;
    LoadingOrigin origin;
    bool reserved, committed, cleanupStarted, cleanupRetryAllowed, cleanupResultConsumed, cleanupCopyFailed;
    bool primaryRetired, presentationReady, closeRequested, videoAttach, pendingSkip, cleanupOnly, allowResize, startupResizePending;
    VideoState *video; // owner-only, never reachable from either Work context
    LoaderSession::Result cleanupResult;
    PreparedOperation() : frameDestination(nullptr), origin(LoadingOrigin::Startup),
        reserved(false), committed(false), cleanupStarted(false), cleanupRetryAllowed(false),
        cleanupResultConsumed(false), cleanupCopyFailed(false), primaryRetired(false), presentationReady(false),
        closeRequested(false), videoAttach(false), pendingSkip(false), cleanupOnly(false), allowResize(false), startupResizePending(false), video(nullptr) {}
};

struct Game::ResponsiveCore
{
    const std::thread::id owner;
    const std::shared_ptr<ResourceEpoch> epoch;
    std::unique_ptr<PreparedOperation> operation, standby;
    VideoState *video;
    // Set only after a matching typed attach receipt AND actual worker reap.
    // Cleared only after a matching typed detach receipt AND its actual reap.
    std::shared_ptr<FlcPlayer::AudioLease> attachedVideoLease;
    // Exact retired transition, not a generic paused flag. The owner records
    // this only after actual reap and consumes it at a later callback boundary.
    std::shared_ptr<LoadingWork> videoResumeWork;
    // A later clip may fail before work. Retain its exact session/result and
    // context as the no-native-mutation continuation of the earlier grant.
    std::unique_ptr<PreparedOperation> videoResumeUnentered;
    std::uint64_t videoResumeGeneration;
    bool menuResumePending;
    std::unique_ptr<State> deferredReplacement;
    std::list<State *> deferredReplacementNode;
    unsigned callbackDepth;
    bool finalClosed, closeRequested, removeVideo, deferredPush, renderAllowed, resizePending, retiringStates;
    int resizeWidth, resizeHeight;
#ifdef OPENXCOM_RESPONSIVE_TESTING
    std::shared_ptr<LoaderThreadBackend> (*backendFactory)();
    void (*trace)(const char *);
    std::shared_ptr<LoaderSession> (*sessionFactory)(std::shared_ptr<LoaderThreadBackend>, LoaderSession::Work, std::shared_ptr<void>);
#endif
    ResponsiveCore() : owner(std::this_thread::get_id()), epoch(new ResourceEpoch()), video(nullptr),
        videoResumeGeneration(0), menuResumePending(false), callbackDepth(0), finalClosed(false), closeRequested(false), removeVideo(false), deferredPush(false),
        renderAllowed(true), resizePending(false), retiringStates(false), resizeWidth(0), resizeHeight(0)
#ifdef OPENXCOM_RESPONSIVE_TESTING
        , backendFactory(nullptr), trace(nullptr), sessionFactory(nullptr)
#endif
    {}
};

#ifdef OPENXCOM_RESPONSIVE_TESTING
void LoadingGameIntegrationAccess::setBackendFactory(Game &game, BackendFactory factory)
{
    game.assertResponsiveOwnerAccess();
    if (game._responsive->operation || game._responsive->standby) std::terminate();
    game._responsive->backendFactory = factory;
}
void LoadingGameIntegrationAccess::setSessionFactory(Game &game, SessionFactory factory)
{
    game.assertResponsiveOwnerAccess();
    if (game._responsive->operation || game._responsive->standby) std::terminate();
    game._responsive->sessionFactory = factory;
}
void LoadingGameIntegrationAccess::setTrace(Game &game, void (*trace)(const char *))
{
    game.assertResponsiveOwnerAccess();
    game._responsive->trace = trace;
}
bool LoadingGameIntegrationAccess::commitBoundary(Game &game) noexcept { return game.loadingBoundary(); }
void LoadingGameIntegrationAccess::tick(Game &game) noexcept { game.runOneOperationTick(); }
bool LoadingGameIntegrationAccess::videoTick(Game &game) noexcept { return game.runResponsiveVideoTick(); }
bool LoadingGameIntegrationAccess::hasOperation(const Game &game) { return !!game._responsive->operation; }
bool LoadingGameIntegrationAccess::workerOwnsEpoch(const Game &game) { return game._responsive->epoch->workerOwned; }
bool LoadingGameIntegrationAccess::finalClosed(const Game &game) { return game._responsive->finalClosed; }
bool LoadingGameIntegrationAccess::hasStandby(const Game &game) { return !!game._responsive->standby; }
std::shared_ptr<const LoaderSession> LoadingGameIntegrationAccess::primarySession(const Game &game)
{
    const Game::PreparedOperation *operation = game._responsive->operation ? game._responsive->operation.get() : game._responsive->standby.get();
    return operation ? operation->session : std::shared_ptr<const LoaderSession>();
}
std::shared_ptr<const LoaderSession> LoadingGameIntegrationAccess::cleanupSession(const Game &game)
{
    const Game::PreparedOperation *operation = game._responsive->operation ? game._responsive->operation.get() : game._responsive->standby.get();
    return operation ? operation->cleanupSession : std::shared_ptr<const LoaderSession>();
}
State *LoadingGameIntegrationAccess::topState(Game &game) { return game._states.empty() ? nullptr : game._states.back(); }
std::size_t LoadingGameIntegrationAccess::deferredCount(const Game &game) { return game._deleted.size(); }
SDL_Surface *LoadingGameIntegrationAccess::terminalFrameDestination(Game &game)
{
    return game._responsive->operation ? game._responsive->operation->frameDestination : nullptr;
}
void LoadingGameIntegrationAccess::callbackDepth(Game &game, unsigned value)
{
    game.assertResponsiveOwnerAccess();
    game._responsive->callbackDepth = value;
}
#endif

namespace
{
Game *responsivePublishedGame = nullptr;
struct ResponsiveCallbackScope
{
    unsigned &depth;
    explicit ResponsiveCallbackScope(unsigned &value) noexcept : depth(value) { ++depth; }
    ~ResponsiveCallbackScope() noexcept { --depth; }
};
bool responsiveRateSupported(int rate) noexcept
{
    return rate == 8000 || rate == 11025 || rate == 16000 || rate == 22050 ||
        rate == 32000 || rate == 44100 || rate == 48000;
}
bool responsiveNoHookFailure(const LoadingAudioHandoff::Outcome &outcome,
    const LoadingAudioHandoff::Generation &generation, const FlcPlayer::AudioLease &lease) noexcept
{
    typedef LoadingAudioHandoff Handoff;
    const bool conversionFailure = outcome.reason == Handoff::Reason::VideoInvalidPCM ||
        outcome.reason == Handoff::Reason::VideoAllocationFailure || outcome.reason == Handoff::Reason::VideoConversionFailure;
    return conversionFailure && outcome.disposition == Handoff::Disposition::FailedRetained &&
        outcome.lastCompletedPhase == Handoff::Phase::ResourceReady &&
        generation.provenance == Handoff::Provenance::KnownSinglePaused &&
        outcome.actual.openCount == 1 && outcome.actual.frequency == generation.spec.frequency &&
        outcome.actual.format == generation.spec.format && outcome.actual.channels == generation.spec.channels &&
        lease.generation() == generation.id &&
        lease.attachmentState() == FlcPlayer::AudioLease::AttachmentState::NeverAttached;
}
LoadingAudioHandoff::AudioPlan responsiveAudioPlan()
{
    LoadingAudioHandoff::AudioPlan p = {Options::audioSampleRate,
        static_cast<Uint16>(Options::audioBitDepth == 8 ? AUDIO_S8 : AUDIO_S16SYS), 2, Options::audioChunkSize};
    return p;
}
LoadingUiPolicy::EventValue responsiveEvent(const SDL_Event &event) noexcept
{
    typedef LoadingUiPolicy::EventKind Kind;
    switch (event.type)
    {
    case SDL_QUIT: return LoadingUiPolicy::EventValue(Kind::Quit);
    case SDL_VIDEORESIZE: return LoadingUiPolicy::EventValue(Kind::Resize, event.resize.w, event.resize.h);
    case SDL_ACTIVEEVENT:
        return LoadingUiPolicy::EventValue(Kind::Focus, 0, 0, SDL_GetAppState());
    case SDL_VIDEOEXPOSE: return LoadingUiPolicy::EventValue(Kind::Expose);
    case SDL_KEYDOWN: return LoadingUiPolicy::EventValue(Kind::KeyDown);
    default: return LoadingUiPolicy::EventValue(Kind::SuppressedInput);
    }
}
}
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
void Game::assertResponsiveOwnerAccess() const noexcept
{
    if (!_responsive || std::this_thread::get_id() != _responsive->owner ||
        _responsive->epoch->workerOwned) std::terminate();
}

const char *Game::loadingAdmissionMessage(LoadingAdmission result) noexcept
{
    switch (result)
    {
    case LoadingAdmission::Accepted: return "Loading reserved.";
    case LoadingAdmission::Busy: return "Loading or video retirement is already in progress. Your staged options are unchanged.";
    case LoadingAdmission::LiveSave: return "Return to the main menu before reloading. Your saved game and staged options are unchanged.";
    case LoadingAdmission::UnsupportedProfile: return "This responsive-loading build has no verified native audio/provider profile. Your staged options are unchanged.";
    case LoadingAdmission::UnknownAudioGeneration: return "The audio device was changed outside the controlled loading path. Your staged options are unchanged.";
    case LoadingAdmission::InvalidDeviceState: return "The audio device or requested rate/format/chunk size is unsupported. Your staged options are unchanged.";
    default: return "Loading preparation failed. The current menu and resources are retained.";
    }
}

Game::LoadingAdmission Game::responsiveLoadEligibility(LoadingOrigin origin, const State *caller) const noexcept
{
    if (!_responsive || std::this_thread::get_id() != _responsive->owner || !_loadingProcessLease.acquired())
        return LoadingAdmission::UnsupportedProfile;
    if (_responsive->operation || _responsive->video || _responsive->closeRequested || _responsive->retiringStates)
        return LoadingAdmission::Busy;
    if (_save) return LoadingAdmission::LiveSave;
    bool exactOrigin = origin == LoadingOrigin::Startup ? _states.empty() && !caller && !_mod : false;
    if (origin != LoadingOrigin::Startup && caller && !_states.empty() && _states.back() == caller)
    {
        bool mainMenu = false;
        for (State *state : _states) if (dynamic_cast<MainMenuState *>(state)) mainMenu = true;
        exactOrigin = mainMenu && ((origin == LoadingOrigin::OptionsMenu && dynamic_cast<const OptionsBaseState *>(caller)) ||
            (origin == LoadingOrigin::ModListMenu && dynamic_cast<const ModListState *>(caller)));
    }
    if (!exactOrigin) return LoadingAdmission::UnsupportedProfile;
#if !defined(OPENXCOM_RESPONSIVE_NATIVE_PROFILE_VERIFIED) && !defined(OPENXCOM_RESPONSIVE_TESTING)
    // Deliberately not a CMake user option. C8 requires a separately reviewed
    // provider/converter/source/runtime closure before native activation.
    return LoadingAdmission::UnsupportedProfile;
#endif
    const ResourceEpoch &epoch = *_responsive->epoch;
    typedef LoadingAudioHandoff::Provenance Provenance;
    if (epoch.workerOwned || epoch.unknownNativeResources || epoch.generation.provenance == Provenance::Invalidated ||
        epoch.generation.id == std::numeric_limits<std::uint64_t>::max())
        return LoadingAdmission::UnknownAudioGeneration;
    const LoadingAudioHandoff::AudioPlan plan = responsiveAudioPlan();
    if (!responsiveRateSupported(plan.frequency) || plan.channels != 2 ||
        plan.chunkSize < 64 || plan.chunkSize > 32768 ||
        (plan.chunkSize & (plan.chunkSize - 1)) ||
        (Options::audioBitDepth != 8 && Options::audioBitDepth != 16))
        return LoadingAdmission::InvalidDeviceState;
#ifndef __NO_MUSIC
    if (plan.format != AUDIO_S16SYS) return LoadingAdmission::InvalidDeviceState;
#endif
    LoadingAudioHandoff::ActualMixerSpec actual = {};
    actual.openCount = Mix_QuerySpec(&actual.frequency, &actual.format, &actual.channels);
    if (!actual.openCount)
    {
        if ((epoch.generation.provenance != Provenance::NeverOpened &&
            epoch.generation.provenance != Provenance::SilentClosedNoNativeResources) || epoch.nativeResources)
            return LoadingAdmission::UnknownAudioGeneration;
    }
    else if (actual.openCount != 1 || actual.frequency <= 0 || actual.channels != 2 ||
        (actual.format != AUDIO_S8 && actual.format != AUDIO_S16SYS)) return LoadingAdmission::InvalidDeviceState;
    else if ((epoch.generation.provenance != Provenance::KnownSinglePaused &&
        epoch.generation.provenance != Provenance::KnownSinglePublished) ||
        epoch.generation.spec.frequency != actual.frequency || epoch.generation.spec.format != actual.format ||
        epoch.generation.spec.channels != actual.channels) return LoadingAdmission::UnknownAudioGeneration;
    return LoadingAdmission::Accepted;
}

Game::LoadingAdmission Game::reserveResponsiveLoad(LoadingOrigin origin, State *caller) noexcept
{
    const LoadingAdmission eligible = responsiveLoadEligibility(origin, caller);
    if (eligible != LoadingAdmission::Accepted) return eligible;
    std::unique_ptr<PreparedOperation> operation;
    try
    {
        operation.reset(new PreparedOperation());
        operation->origin = origin;
        const LoadingAudioHandoff::AudioPlan plan = responsiveAudioPlan();
        operation->work.reset(new LoadingWork(_responsive->epoch, LoadingWork::Kind::Load, plan,
            origin != LoadingOrigin::Startup, origin != LoadingOrigin::Startup, Options::mute));
        operation->cleanup.reset(new LoadingWork(_responsive->epoch, LoadingWork::Kind::Cleanup, plan, false, false, false));
#ifdef OPENXCOM_RESPONSIVE_TESTING
        operation->backend = _responsive->backendFactory ? _responsive->backendFactory() : std::make_shared<LoaderThreadLinux>();
        operation->cleanupBackend = _responsive->backendFactory ? _responsive->backendFactory() : std::make_shared<LoaderThreadLinux>();
#else
        operation->backend = std::make_shared<LoaderThreadLinux>();
        operation->cleanupBackend = std::make_shared<LoaderThreadLinux>();
#endif
#ifdef OPENXCOM_RESPONSIVE_TESTING
        if (_responsive->sessionFactory)
        {
            operation->session = _responsive->sessionFactory(operation->backend, &Game::loadResponsiveResources, operation->work);
            operation->cleanupSession = _responsive->sessionFactory(operation->cleanupBackend, &Game::cleanupResponsiveResources, operation->cleanup);
        }
        else
#endif
        {
        operation->session.reset(new LoaderSession(operation->backend, &Game::loadResponsiveResources, operation->work));
        operation->cleanupSession.reset(new LoaderSession(operation->cleanupBackend, &Game::cleanupResponsiveResources, operation->cleanup));
        }
        LoadingUiPolicy::Config config;
        config.allowResize = Options::allowResize;
        operation->allowResize = config.allowResize;
        config.suppressStartupResize = origin == LoadingOrigin::Startup;
        operation->startupResizePending = config.suppressStartupResize && config.allowResize;
        config.initialFocus = SDL_GetAppState();
        operation->policy.reset(new LoadingUiPolicy(operation->session, config));
        if (operation->policy->reserveView(false) != LoadingUiPolicy::Admission::Accepted) std::terminate();
        operation->reserved = true;
        operation->oldMaster = Options::getActiveMaster();
        operation->terminal.reset(new StartState());
        operation->retirementTerminal.reset(new StartState());
        operation->terminalNode.push_back(operation->terminal.get());
        operation->retirementNode.push_back(operation->retirementTerminal.get());
        operation->terminal->prepareResponsiveFrames(operation->frames);
        operation->frameDestination = _screen->getSurface()->getSurface();
        operation->presentationReady = _screen->captureLoadingPresentation(operation->presentation) == Zoom::RenderStatus::Success;
#ifdef OPENXCOM_RESPONSIVE_TESTING
        if (_responsive->trace) _responsive->trace("reservation");
#endif
        // Option persistence/map/language/audio remain worker-exclusive.
        _responsive->operation = std::move(operation);
        return LoadingAdmission::Accepted;
    }
    catch (...)
    {
        if (operation && operation->policy && operation->reserved) operation->policy->preparationFailed();
        return LoadingAdmission::PreparationFailure;
    }
}

void Game::applyResponsiveResize() noexcept
{
    if (!_responsive->resizePending || _responsive->epoch->workerOwned ||
        _responsive->operation || _responsive->video) return;
    try
    {
        // State callbacks may reserve a later transition. Snapshot pointers
        // before mutation; normal stack changes defer destruction until a future
        // boundary, and any new operation bypasses the remaining resize tail.
        const std::vector<State *> states(_states.begin(), _states.end());
        const int width = std::max(320, std::min(16384, _responsive->resizeWidth));
        const int height = std::max(200, std::min(16384, _responsive->resizeHeight));
        Options::newDisplayWidth = Options::displayWidth = width;
        Options::newDisplayHeight = Options::displayHeight = height;
        int dx = 0, dy = 0;
        Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, false);
        Screen::updateScale(Options::geoscapeScale, Options::baseXGeoscape, Options::baseYGeoscape, true);
        for (State *state : states)
        {
            { ResponsiveCallbackScope callback(_responsive->callbackDepth); state->resize(dx, dy); }
            if (_responsive->operation || _responsive->video) return;
        }
        _screen->resetDisplay();
        _responsive->resizePending = false;
        if (_responsive->standby)
        {
            _responsive->standby->frameDestination = _screen->getSurface()->getSurface();
            _responsive->standby->presentationReady = _screen->captureLoadingPresentation(_responsive->standby->presentation) == Zoom::RenderStatus::Success;
        }
    }
    catch (...) { /* no worker; keep the resize intent without unwinding Game */ }
}

bool Game::loadingBoundary() noexcept
{
    if (!_responsive || std::this_thread::get_id() != _responsive->owner) std::terminate();
    if (_responsive->callbackDepth) std::terminate();
    if (!_responsive->video && _responsive->videoResumeWork)
    {
        if (_responsive->operation || _responsive->closeRequested)
        {
            // A reload/close proceeds paused. Never unpause merely to retire it.
            _responsive->videoResumeWork.reset();
            _responsive->videoResumeUnentered.reset();
            _responsive->menuResumePending = false;
        }
        else if (_responsive->menuResumePending)
        {
            const LoadingWork &retired = *_responsive->videoResumeWork;
            const auto &generation = _responsive->epoch->generation;
            const bool detached = retired.detached && retired.workComplete &&
                retired.detachReceipt.validFor(_responsive->videoResumeGeneration, retired.videoLease.get());
            const bool neverAttached = retired.noHook && retired.videoLease &&
                !retired.epoch->unknownNativeResources &&
                responsiveNoHookFailure(retired.audioOutcome, generation, *retired.videoLease);
            if (retired.epoch != _responsive->epoch || !retired.videoLease ||
                retired.videoLease->generation() != _responsive->videoResumeGeneration ||
                generation.id != _responsive->videoResumeGeneration ||
                generation.provenance != LoadingAudioHandoff::Provenance::KnownSinglePaused ||
                _responsive->attachedVideoLease || (!detached && !neverAttached)) std::terminate();
            if (_responsive->videoResumeUnentered)
            {
                const PreparedOperation &continuation = *_responsive->videoResumeUnentered;
                const LoadingWork &unentered = *continuation.work;
                const LoaderSession::Result *result = continuation.policy->result();
                const auto &before = unentered.generationAtPreparation;
                if (!continuation.primaryRetired || !continuation.session->quiescent() || !result ||
                    result->terminal != LoaderSession::Terminal::Failure ||
                    (result->reason != LoaderSession::Reason::NoThread && result->reason != LoaderSession::Reason::ThreadSetupFailure) ||
                    unentered.entered || unentered.kind != LoadingWork::Kind::VideoAttach ||
                    unentered.epoch != _responsive->epoch || unentered.epoch->unknownNativeResources || !unentered.videoLease ||
                    unentered.videoLease->attachmentState() != FlcPlayer::AudioLease::AttachmentState::NeverAttached ||
                    unentered.videoLease->generation() != generation.id || before.id != generation.id ||
                    before.provenance != generation.provenance || before.spec.openCount != generation.spec.openCount ||
                    before.spec.frequency != generation.spec.frequency || before.spec.format != generation.spec.format ||
                    before.spec.channels != generation.spec.channels) std::terminate();
            }
            // This boundary follows finishResponsiveVideo AND its caller return.
            // All borrower release is complete, with exact typed/no-hook proof.
            _responsive->videoResumeWork.reset();
            _responsive->videoResumeUnentered.reset();
            _responsive->menuResumePending = false;
            _responsive->epoch->generation.provenance = LoadingAudioHandoff::Provenance::KnownSinglePublished;
            SDL_PauseAudio(0);
        }
    }
    if (!_responsive->operation && !_responsive->video && _responsive->deferredReplacement)
    {
        // All destination nodes were allocated by the requesting callback.
        // This is the first boundary after video detach AND callback return.
        if (!_responsive->deferredPush) _deleted.splice(_deleted.end(), _states);
        _states.splice(_states.end(), _responsive->deferredReplacementNode);
        _responsive->deferredReplacement.release();
        _responsive->deferredPush = false;
        _init = false;
    }
    if (!_responsive->operation && !_responsive->video) applyResponsiveResize();
    PreparedOperation *operation = _responsive->operation.get();
    if (!operation) return _responsive->video || (!_states.empty() && dynamic_cast<VideoState *>(_states.back()));
    if (!operation->reserved || operation->committed) return true;
#ifdef OPENXCOM_RESPONSIVE_TESTING
    if (_responsive->trace) _responsive->trace("callback-return");
#endif
        if (!operation->video)
        {
            // Both list nodes already exist. No popState/setState allocations.
            _deleted.splice(_deleted.end(), _states);
            _states.splice(_states.end(), operation->terminalNode);
            operation->terminal.release();
            _init = true;
        }
#ifdef OPENXCOM_RESPONSIVE_TESTING
        if (_responsive->trace) _responsive->trace("commit");
#endif
        // Pop BEFORE delete so reentrant destructors cannot observe a stale node.
        _responsive->retiringStates = true;
        if (!operation->video)
        {
            // A resource-destroying operation must retire every State borrower.
            // A video-only handoff keeps an untouched deferred destination until
            // detach/reap; that worker never destroys resources in the epoch.
            _responsive->deferredReplacementNode.clear();
            _responsive->deferredReplacement.reset();
        }
        while (!_deleted.empty())
        {
            State *retired = _deleted.back(); _deleted.pop_back(); delete retired;
        }
        _responsive->retiringStates = false;
        if (_responsive->callbackDepth || !_deleted.empty()) std::terminate();
        if (!operation->video && !operation->cleanupOnly && operation->origin == LoadingOrigin::OptionsMenu)
        {
            // This is an admitted COMMITTED transition after callback return,
            // never preparation which can report an unchanged-menu rejection.
            // A failed reset retires the old graph through the prebuilt cleanup
            // session. No worker can start on the partially applied display.
            try
            {
                Options::switchDisplay();
                operation->allowResize = Options::allowResize;
                Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, false);
                Screen::updateScale(Options::geoscapeScale, Options::baseXGeoscape, Options::baseYGeoscape, true);
                _screen->resetDisplay();
                operation->frameDestination = _screen->getSurface()->getSurface();
                operation->presentationReady = _screen->captureLoadingPresentation(operation->presentation) == Zoom::RenderStatus::Success;
            }
            catch (...)
            {
                operation->frameDestination = nullptr;
                operation->presentationReady = false;
                operation->closeRequested = true;
                operation->policy->preparationFailed();
            }
        }
#ifdef OPENXCOM_RESPONSIVE_TESTING
        if (_responsive->trace) _responsive->trace("deleted-drained");
#endif
        _lang = nullptr; _mod = nullptr;
        _responsive->epoch->workerOwned = true;
        operation->committed = true;
        if (!operation->cleanupOnly) { operation->work->seal(); operation->cleanup->seal(); }
#ifdef OPENXCOM_RESPONSIVE_TESTING
        if (_responsive->trace) _responsive->trace("transfer-seal");
#endif
        if (operation->cleanupOnly) beginResponsiveCleanup();
        else
        {
            if (operation->closeRequested || _responsive->closeRequested)
                operation->policy->record(LoadingUiPolicy::EventValue(LoadingUiPolicy::EventKind::Quit));
            operation->policy->startPrepared();
        }
    return true;
}

LoaderSession::WorkResult Game::loadResponsiveResources(void *context, const LoaderSession::StopToken &stop)
{
    LoadingWork &work = *static_cast<LoadingWork *>(context);
    ResourceEpoch &epoch = *work.epoch;
    work.entered = true;
    if (stop.requested()) { work.disposition = LoadingWork::Disposition::Cancelled; return LoaderSession::WorkResult::Cancelled; }
    typedef LoadingAudioHandoff Handoff;
    std::unique_ptr<Handoff::PrepareResult> ready;
    try
    {
        if (epoch.generation.provenance == Handoff::Provenance::KnownSinglePublished ||
            epoch.generation.provenance == Handoff::Provenance::KnownSinglePaused)
        {
            Handoff::AudioPlan oldPlan = {epoch.generation.spec.frequency, epoch.generation.spec.format,
                epoch.generation.spec.channels, epoch.openedChunkRequest};
            ready.reset(new Handoff::PrepareResult(work.oldAudio->prepareKnown(epoch.generation, oldPlan)));
            work.audioOutcome = ready->outcome;
            if (ready->outcome.disposition != Handoff::Disposition::ResourceReady)
            { work.disposition = LoadingWork::Disposition::RejectedRetained; return LoaderSession::WorkResult::Cancelled; }
            epoch.generation.provenance = Handoff::Provenance::KnownSinglePaused;
            // Null the epoch aliases before a possibly reentrant destructor.
            std::unique_ptr<Mod> oldMod; oldMod.swap(epoch.mod); oldMod.reset();
            std::unique_ptr<Language> oldLanguage; oldLanguage.swap(epoch.language); oldLanguage.reset();
            epoch.nativeResources = false;
            const bool reconfigure = work.silentRequested || epoch.generation.spec.frequency != work.plan.frequency ||
                epoch.generation.spec.format != work.plan.format || epoch.generation.spec.channels != work.plan.channels ||
                epoch.openedChunkRequest != work.plan.chunkSize;
            if (reconfigure)
            {
                Handoff::CloseReceipt close = work.oldAudio->closePrepared(ready->permit, epoch.generation, true, false);
                work.audioOutcome = close.outcome();
                if (!close.valid()) { work.disposition = LoadingWork::Disposition::FailedRetained; return LoaderSession::WorkResult::Cancelled; }
                Handoff::ClosedProof proof = close.releaseClosedProof();
                epoch.openedChunkRequest = 0;
                ready.reset();
                if (!work.silentRequested)
                    ready.reset(new Handoff::PrepareResult(work.freshAudio->openFresh(proof, work.plan, epoch.generation)));
            }
        }
        else
        {
            Handoff::ClosedProof proof = work.oldAudio->verifyClosed(epoch.generation,
                !epoch.nativeResources && !epoch.unknownNativeResources);
            work.audioOutcome = proof.outcome();
            if (!proof.valid()) { work.disposition = LoadingWork::Disposition::RejectedRetained; return LoaderSession::WorkResult::Cancelled; }
            // No native graph exists in the verified closed domain.
            epoch.mod.reset(); epoch.language.reset();
            if (!work.silentRequested)
                ready.reset(new Handoff::PrepareResult(work.freshAudio->openFresh(proof, work.plan, epoch.generation)));
        }
        bool silent = !ready;
        if (ready)
        {
            work.audioOutcome = ready->outcome;
            silent = ready->outcome.disposition == Handoff::Disposition::ClosedReady;
            if (!silent && ready->outcome.disposition != Handoff::Disposition::ResourceReady)
            { work.disposition = LoadingWork::Disposition::FailedRetained; return LoaderSession::WorkResult::Cancelled; }
        }
        // Muting is a chosen native-free load path only AFTER closed proof.
        Options::mute = silent;
        epoch.openedChunkRequest = silent ? 0 : work.plan.chunkSize;
        if (silent) epoch.generation.provenance = Handoff::Provenance::SilentClosedNoNativeResources;
        if (!silent)
        {
            Mix_AllocateChannels(16); Mix_ReserveChannels(4); Mix_GroupChannels(1, 2, 0);
        }
        if (stop.requested())
        {
            if (ready) ready->permit.invalidate();
            work.disposition = LoadingWork::Disposition::Cancelled;
            return LoaderSession::WorkResult::Cancelled;
        }
        if (work.persist) Options::save();
        if (work.mapResources) Options::mapResources();
        Options::updateMods();
        Mod::resetGlobalStatics();
        // Provider capability is unproved throughout construction and partial
        // loading. A provider can be adopted and then throw before catalog
        // classification; software-mixer authority must never free that graph.
        epoch.unknownNativeResources = true;
        work.constructing = true;
        epoch.mod.reset(new Mod()); // adopt before the fallible loadAll
        work.constructing = false;
        epoch.nativeResources = !silent;
        epoch.mod->loadAll(FileMap::getRulesets());
        epoch.catalog = epoch.mod->responsiveMusicCatalog();
        if (epoch.catalog == Mod::ResponsiveMusicCatalog::Unsupported)
        {
            // Decoder/provider initialization can precede type classification.
            // A rejected catalog is retained; classification grants no free proof.
            epoch.unknownNativeResources = true;
            if (ready) ready->permit.invalidate();
            work.disposition = LoadingWork::Disposition::RejectedRetained;
            return LoaderSession::WorkResult::Cancelled;
        }
        epoch.unknownNativeResources = false; // only the complete supported catalog proves this domain
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
        epoch.observer->publishCatalogSupported(epoch.catalog != Mod::ResponsiveMusicCatalog::Unsupported);
#endif
        loadEpochLanguages(epoch);
        if (ready) ready->permit.invalidate(); // before owner publication/unpause
        work.workComplete = true;
        work.disposition = stop.requested() ? LoadingWork::Disposition::Cancelled : LoadingWork::Disposition::Complete;
        return stop.requested() ? LoaderSession::WorkResult::Cancelled : LoaderSession::WorkResult::Success;
    }
    catch (...)
    {
        if (ready) ready->permit.invalidate();
        if (work.constructing) epoch.unknownNativeResources = true;
        work.disposition = LoadingWork::Disposition::FailedRetained;
        throw;
    }
}

LoaderSession::WorkResult Game::cleanupResponsiveResources(void *context, const LoaderSession::StopToken &)
{
    LoadingWork &work = *static_cast<LoadingWork *>(context);
    ResourceEpoch &epoch = *work.epoch;
    work.entered = true;
    typedef LoadingAudioHandoff Handoff;
    if (epoch.unknownNativeResources || epoch.generation.provenance == Handoff::Provenance::Invalidated)
    { work.disposition = LoadingWork::Disposition::RejectedRetained; return LoaderSession::WorkResult::Cancelled; }
    if (epoch.generation.provenance == Handoff::Provenance::KnownSinglePublished ||
        epoch.generation.provenance == Handoff::Provenance::KnownSinglePaused)
    {
        Handoff::AudioPlan oldPlan = {epoch.generation.spec.frequency, epoch.generation.spec.format,
            epoch.generation.spec.channels, epoch.openedChunkRequest};
        Handoff::PrepareResult ready = work.oldAudio->prepareKnown(epoch.generation, oldPlan);
        work.audioOutcome = ready.outcome;
        if (ready.outcome.disposition != Handoff::Disposition::ResourceReady)
        { work.disposition = LoadingWork::Disposition::FailedRetained; return LoaderSession::WorkResult::Cancelled; }
        epoch.generation.provenance = Handoff::Provenance::KnownSinglePaused;
        if (work.videoLease && !work.videoLease->canRelease())
        {
            auto detached = work.oldAudio->detachVideo(ready.permit, epoch.generation, *work.videoLease);
            work.audioOutcome = detached.outcome;
            if (!detached.receipt.validFor(epoch.generation.id, work.videoLease.get()))
            { work.disposition = LoadingWork::Disposition::FailedRetained; return LoaderSession::WorkResult::Cancelled; }
            // The continuation consumed this handoff. A separate, preconstructed
            // handoff obtains fresh destruction authority in the same generation.
            ready = work.freshAudio->prepareKnown(epoch.generation, oldPlan);
            work.audioOutcome = ready.outcome;
            if (ready.outcome.disposition != Handoff::Disposition::ResourceReady)
            { work.disposition = LoadingWork::Disposition::FailedRetained; return LoaderSession::WorkResult::Cancelled; }
        }
        std::unique_ptr<Mod> oldMod; oldMod.swap(epoch.mod); oldMod.reset();
        std::unique_ptr<Language> oldLanguage; oldLanguage.swap(epoch.language); oldLanguage.reset();
        epoch.nativeResources = false;
        Handoff &closing = ready.permit.validFor(*work.oldAudio) ? *work.oldAudio : *work.freshAudio;
        work.closeReceipt = closing.closePrepared(ready.permit, epoch.generation, true, true);
    }
    else
    {
        Handoff::ClosedProof proof = work.oldAudio->verifyClosed(epoch.generation, !epoch.nativeResources);
        work.audioOutcome = proof.outcome();
        if (!proof.valid()) { work.disposition = LoadingWork::Disposition::RejectedRetained; return LoaderSession::WorkResult::Cancelled; }
        if (work.videoLease && !work.videoLease->canRelease())
        { work.disposition = LoadingWork::Disposition::FailedRetained; return LoaderSession::WorkResult::Cancelled; }
        epoch.mod.reset(); epoch.language.reset();
        work.closeReceipt = work.oldAudio->terminalCloseNeverOpened(proof, epoch.generation);
    }
    work.audioOutcome = work.closeReceipt.outcome();
    work.closed = work.closeReceipt.valid() && !epoch.mod && !epoch.language && !epoch.nativeResources && !epoch.unknownNativeResources;
    work.workComplete = work.closed;
    if (work.closed) epoch.openedChunkRequest = 0;
    work.disposition = work.closed ? LoadingWork::Disposition::Complete : LoadingWork::Disposition::FailedRetained;
    return work.closed ? LoaderSession::WorkResult::Success : LoaderSession::WorkResult::Cancelled;
}

void Game::beginResponsiveCleanup() noexcept
{
    PreparedOperation &operation = *_responsive->operation;
    if (operation.cleanupStarted || !operation.session->quiescent()) return;
    if (operation.video || _responsive->video || _responsive->attachedVideoLease) return; // retire video borrowers first
    operation.primaryRetired = true;
    operation.cleanupStarted = true;
    operation.cleanupRetryAllowed = false;
    operation.cleanupResultConsumed = false;
    operation.cleanupSession->begin(); // never close this reusable session
}

bool Game::reserveResponsiveCleanup() noexcept
{
    if (_responsive->operation || _responsive->video) return false;
    if (!_responsive->standby) return false;
    _responsive->operation = std::move(_responsive->standby);
    PreparedOperation &operation = *_responsive->operation;
    operation.cleanupOnly = true;
    operation.closeRequested = true;
    operation.reserved = true;
    operation.committed = false;
    operation.terminal = std::move(operation.retirementTerminal);
    operation.terminalNode.splice(operation.terminalNode.end(), operation.retirementNode);
    // No allocation and no new callback/session. Rebind only after quiescence;
    // screen resizing since the prior operation cannot leave a stale surface.
    operation.frameDestination = _screen->getSurface()->getSurface();
    operation.presentationReady = _screen->captureLoadingPresentation(operation.presentation) == Zoom::RenderStatus::Success;
    return true;
}

void Game::publishResponsiveLoad() noexcept
{
    PreparedOperation &operation = *_responsive->operation;
    if (!operation.session->quiescent() || operation.closeRequested || _responsive->closeRequested) return;
    ResourceEpoch &epoch = *_responsive->epoch;
    try
    {
        // Checked reap above returns exclusive ownership before owner-side
        // navigation/Options access; aliases stay unpublished until commit.
        epoch.workerOwned = false;
        // Constructors here do not acquire resources. All allocating navigation
        // and list nodes are staged under RAII before alias publication.
        std::unique_ptr<State> menu(new GoToMainMenuState());
        std::unique_ptr<State> intro;
        std::list<State *> navigation;
        navigation.push_back(menu.get());
        if (operation.oldMaster != Options::getActiveMaster() && Options::playIntro)
        {
            intro.reset(new CutsceneState("intro"));
            navigation.push_back(intro.get());
        }
        if (!operation.policy->transitionCompleted())
        {
            epoch.workerOwned = true;
            return;
        }
        _mod = epoch.mod.get(); _lang = epoch.language.get();
        setVolume(Options::soundVolume, Options::musicVolume, Options::uiVolume);
        _deleted.splice(_deleted.end(), _states);
        _states.splice(_states.end(), navigation);
        menu.release(); intro.release();
        _init = false;
        Options::reload = false;
        _cursor->setVisible(true); _fpsCounter->setVisible(Options::fpsCounter);
        if (epoch.generation.provenance == LoadingAudioHandoff::Provenance::KnownSinglePaused)
        {
            epoch.generation.provenance = LoadingAudioHandoff::Provenance::KnownSinglePublished;
            SDL_PauseAudio(0);
        }
        _responsive->standby = std::move(_responsive->operation);
    }
    catch (...)
    {
        // No borrower from failed navigation survives this scope. Publication
        // must never become a destructor fallback for the retained epoch.
        _mod = nullptr; _lang = nullptr; epoch.workerOwned = true;
        if (operation.policy->view().phase == LoadingUiPolicy::Phase::SuccessReserved)
            operation.policy->transitionFailed();
        operation.closeRequested = true;
        beginResponsiveCleanup();
    }
}

void Game::runOneOperationTick() noexcept
{
    if (!_responsive || std::this_thread::get_id() != _responsive->owner || !_responsive->operation) std::terminate();
    Logger::ScopedSuppression suppressOwnerLogging;
    PreparedOperation &operation = *_responsive->operation;
    bool retry = false;
    SDL_Event event;
    std::size_t budget = 64;
    if (!operation.cleanupStarted) budget = operation.policy->remainingEventBudget();
    for (std::size_t n = 0; n < budget && SDL_PollEvent(&event); ++n)
    {
        if (event.type == SDL_VIDEORESIZE && operation.startupResizePending)
        {
            operation.startupResizePending = false;
            if (!operation.cleanupStarted) operation.policy->record(LoadingUiPolicy::EventValue(
                LoadingUiPolicy::EventKind::StartupResize, event.resize.w, event.resize.h));
            continue;
        }
        if (event.type == SDL_QUIT)
        {
            operation.closeRequested = true;
            _responsive->closeRequested = true;
        }
        else if (event.type == SDL_KEYDOWN)
        {
            if (operation.video) operation.pendingSkip = true;
            if (event.key.keysym.sym == SDLK_r) retry = true;
        }
        else if (event.type == SDL_ACTIVEEVENT)
        {
            _responsive->renderAllowed = (SDL_GetAppState() & SDL_APPACTIVE) != 0;
        }
        else if (event.type == SDL_VIDEORESIZE && operation.allowResize)
        {
            _responsive->resizePending = true;
            _responsive->resizeWidth = event.resize.w; _responsive->resizeHeight = event.resize.h;
        }
        if (!operation.cleanupStarted) operation.policy->record(responsiveEvent(event));
    }
    if (operation.closeRequested && !operation.cleanupStarted)
        operation.policy->record(LoadingUiPolicy::EventValue(LoadingUiPolicy::EventKind::Quit));
    // Rendering failure is independent of the retirement pump. No generic
    // state/Screen/Options/observer callback is reachable in this active tick.
    if (operation.presentationReady && _responsive->renderAllowed)
    {
        const std::size_t frame = operation.cleanupStarted ? 1 : operation.closeRequested ? 2 : 0;
        if (SDL_BlitSurface(operation.frames[frame]->getSurface(), nullptr, operation.frameDestination, nullptr) != 0 ||
            Screen::presentLoading(operation.presentation, operation.workspace) != Zoom::RenderStatus::Success)
        {
            operation.presentationReady = false;
            if (!operation.cleanupStarted) operation.policy->renderingFailed();
            operation.closeRequested = true;
        }
    }
    if (operation.cleanupStarted)
    {
        // Outer close requests never requestClose on the reusable cleanup
        // session. Only explicit R after a consumed NoThread/setup result retries.
        const LoaderSession::PollResult polled = operation.cleanupSession->poll();
        if (polled == LoaderSession::PollResult::Ready && operation.cleanupSession->quiescent() &&
            !operation.cleanupResultConsumed && !operation.cleanupCopyFailed)
        {
            try
            {
                if (!operation.cleanupSession->copyResult(operation.cleanupResult)) return;
                operation.cleanupResultConsumed = true;
            }
            catch (...) { operation.cleanupRetryAllowed = false; operation.cleanupCopyFailed = true; return; }
            LoadingWork &cleanup = *operation.cleanup; // actual reap, not terminal signal
            if (operation.primaryRetired && cleanup.closed && cleanup.closeReceipt.validFor(cleanup.epoch->generation.id) && cleanup.workComplete &&
                !cleanup.epoch->mod && !cleanup.epoch->language && !_responsive->video && !_responsive->attachedVideoLease)
            {
                cleanup.epoch->workerOwned = false;
                _mod = nullptr; _lang = nullptr;
                _responsive->finalClosed = true;
                while (!_deleted.empty()) { State *state = _deleted.back(); _deleted.pop_back(); delete state; }
                while (!_states.empty()) { State *state = _states.back(); _states.pop_back(); delete state; }
                _responsive->standby.reset();
                _responsive->operation.reset();
                _quit = true;
                return;
            }
            operation.cleanupRetryAllowed = !cleanup.entered &&
                (operation.cleanupResult.reason == LoaderSession::Reason::NoThread ||
                 operation.cleanupResult.reason == LoaderSession::Reason::ThreadSetupFailure);
        }
        if (retry && operation.cleanupRetryAllowed && operation.cleanupResultConsumed && operation.cleanupSession->quiescent())
        {
            try
            {
                // Construct both replacements before replacing either. The
                // immutable session context remains genuinely owned and unchanged.
                std::unique_ptr<LoadingAudioHandoff> old(new LoadingAudioHandoff(
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
                    operation.cleanup->epoch->observer.get()
#endif
                ));
                std::unique_ptr<LoadingAudioHandoff> fresh(new LoadingAudioHandoff(
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
                    operation.cleanup->epoch->observer.get()
#endif
                ));
                old->sealForWorker(); fresh->sealForWorker();
                operation.cleanup->oldAudio = std::move(old); operation.cleanup->freshAudio = std::move(fresh);
                operation.cleanupRetryAllowed = false; operation.cleanupResultConsumed = false;
                operation.cleanupSession->begin();
            }
            catch (...) { /* retained; another explicit retry may prepare storage */ }
        }
        SDL_Delay(1); // finite idle yield, never a wait for worker progress
        return;
    }
    const LoadingUiPolicy::Decision decision = operation.policy->tick(); // exactly one primary poll
    if (decision == LoadingUiPolicy::Decision::Wait || decision == LoadingUiPolicy::Decision::RetainQuarantine)
    { SDL_Delay(1); return; }
    if (!operation.session->quiescent()) return;
    operation.primaryRetired = true;
    if (operation.video)
    {
        LoadingWork &work = *operation.work;
        const bool receipt = operation.videoAttach ?
            work.attachReceipt.validFor(work.epoch->generation.id, work.videoLease.get()) :
            work.detachReceipt.validFor(work.epoch->generation.id, work.videoLease.get());
        const bool success = receipt && work.disposition == LoadingWork::Disposition::Complete && work.workComplete;
        const bool rejectedBeforeWork = !work.entered && operation.videoAttach;
        const LoaderSession::Result *primaryResult = operation.policy->result();
        const auto &before = work.generationAtPreparation;
        const auto &current = work.epoch->generation;
        const bool provedUnentered = rejectedBeforeWork && primaryResult &&
            primaryResult->terminal == LoaderSession::Terminal::Failure &&
            (primaryResult->reason == LoaderSession::Reason::NoThread || primaryResult->reason == LoaderSession::Reason::ThreadSetupFailure) &&
            work.videoLease && work.videoLease->attachmentState() == FlcPlayer::AudioLease::AttachmentState::NeverAttached &&
            !_responsive->attachedVideoLease && !work.epoch->unknownNativeResources &&
            before.id == current.id && before.provenance == current.provenance &&
            before.spec.openCount == current.spec.openCount && before.spec.frequency == current.spec.frequency &&
            before.spec.format == current.spec.format && before.spec.channels == current.spec.channels;
        // An older clip's grant cannot cross an outcome-copy/missing-result
        // ambiguity. Keep this whole operation rather than infer no mutation.
        if (rejectedBeforeWork && _responsive->videoResumeWork && !provedUnentered)
        { SDL_Delay(1); return; }
        const bool safeFailure = work.detached || (operation.videoAttach && work.noHook);
        if (!success && !rejectedBeforeWork && !safeFailure) { SDL_Delay(1); return; }
        VideoState *video = operation.video;
        const bool attach = operation.videoAttach;
        const bool close = operation.closeRequested || _responsive->closeRequested;
        const bool skip = operation.pendingSkip;
        const bool remove = _responsive->removeVideo;
        if (success)
        {
            if (attach)
            {
                if (_responsive->attachedVideoLease || work.videoLease->attachmentState() != FlcPlayer::AudioLease::AttachmentState::Attached)
                    std::terminate();
                _responsive->attachedVideoLease = work.videoLease;
                _responsive->videoResumeWork.reset();
                _responsive->videoResumeUnentered.reset();
                _responsive->menuResumePending = false;
            }
            else
            {
                if (_responsive->attachedVideoLease != work.videoLease ||
                    work.videoLease->attachmentState() != FlcPlayer::AudioLease::AttachmentState::Detached) std::terminate();
                _responsive->attachedVideoLease.reset();
                if (work.epoch->generation.provenance == LoadingAudioHandoff::Provenance::KnownSinglePaused)
                {
                    _responsive->videoResumeUnentered.reset();
                    _responsive->videoResumeWork = operation.work;
                    _responsive->videoResumeGeneration = work.epoch->generation.id;
                }
            }
        }
        else if (operation.videoAttach && work.noHook &&
            work.epoch->generation.provenance == LoadingAudioHandoff::Provenance::KnownSinglePaused)
        {
            _responsive->videoResumeUnentered.reset();
            _responsive->videoResumeWork = operation.work;
            _responsive->videoResumeGeneration = work.epoch->generation.id;
        }
        _responsive->epoch->workerOwned = false;
        _mod = _responsive->epoch->mod.get(); _lang = _responsive->epoch->language.get();
        std::unique_ptr<PreparedOperation> retired = std::move(_responsive->operation);
        if (provedUnentered && _responsive->videoResumeWork)
        {
            if (_responsive->videoResumeUnentered) std::terminate();
            _responsive->videoResumeUnentered = std::move(retired);
        }
        // Callback starts only after actual reap and owner restoration. The
        // retired operation (local or the retained unentered continuation) owns
        // every dependency until this callback returns completely.
        try
        {
            ResponsiveCallbackScope callback(_responsive->callbackDepth);
            if (close || remove) video->responsiveRequestClose();
            if (skip) { SDL_Event key = {}; key.type = SDL_KEYDOWN; video->responsiveVideoEvent(key); }
            video->responsiveAudioComplete(attach, success, close);
        }
        catch (...)
        {
            _responsive->closeRequested = true;
            video->responsiveRequestClose();
        }
        return;
    }
    if (decision == LoadingUiPolicy::Decision::PublishSuccess && !operation.closeRequested &&
        !_responsive->closeRequested && operation.work->disposition == LoadingWork::Disposition::Complete)
    { publishResponsiveLoad(); return; }
    beginResponsiveCleanup();
}

Game::VideoAudioResult Game::reserveVideoAudio(VideoState *video, std::shared_ptr<FlcPlayer::AudioLease> lease, bool attach) noexcept
{
    assertResponsiveOwnerAccess();
    if (!video || !lease || _responsive->operation || _responsive->video != video ||
        lease->generation() != _responsive->epoch->generation.id || !_responsive->epoch->mod ||
        _responsive->epoch->generation.provenance == LoadingAudioHandoff::Provenance::Invalidated)
        return VideoAudioResult::RejectedUnchanged;
    if ((attach && (_responsive->attachedVideoLease || lease->attachmentState() != FlcPlayer::AudioLease::AttachmentState::NeverAttached)) ||
        (!attach && (_responsive->attachedVideoLease != lease || lease->attachmentState() != FlcPlayer::AudioLease::AttachmentState::Attached)))
        return VideoAudioResult::RejectedUnchanged;
    std::unique_ptr<PreparedOperation> operation;
    try
    {
        operation.reset(new PreparedOperation());
        const LoadingAudioHandoff::AudioPlan plan = responsiveAudioPlan();
        operation->video = video; operation->videoAttach = attach;
        operation->work.reset(new LoadingWork(_responsive->epoch,
            attach ? LoadingWork::Kind::VideoAttach : LoadingWork::Kind::VideoDetach, plan, false, false, false));
        operation->work->videoLease = std::move(lease);
        operation->cleanup.reset(new LoadingWork(_responsive->epoch, LoadingWork::Kind::Cleanup, plan, false, false, false));
#ifdef OPENXCOM_RESPONSIVE_TESTING
        operation->backend = _responsive->backendFactory ? _responsive->backendFactory() : std::make_shared<LoaderThreadLinux>();
        operation->cleanupBackend = _responsive->backendFactory ? _responsive->backendFactory() : std::make_shared<LoaderThreadLinux>();
#else
        operation->backend = std::make_shared<LoaderThreadLinux>();
        operation->cleanupBackend = std::make_shared<LoaderThreadLinux>();
#endif
#ifdef OPENXCOM_RESPONSIVE_TESTING
        if (_responsive->sessionFactory)
        {
            operation->session = _responsive->sessionFactory(operation->backend, &Game::transitionResponsiveVideo, operation->work);
            operation->cleanupSession = _responsive->sessionFactory(operation->cleanupBackend, &Game::cleanupResponsiveResources, operation->cleanup);
        }
        else
#endif
        {
        operation->session.reset(new LoaderSession(operation->backend, &Game::transitionResponsiveVideo, operation->work));
        operation->cleanupSession.reset(new LoaderSession(operation->cleanupBackend, &Game::cleanupResponsiveResources, operation->cleanup));
        }
        LoadingUiPolicy::Config config; config.allowResize = Options::allowResize; config.suppressStartupResize = false;
        operation->allowResize = config.allowResize;
        config.initialFocus = SDL_GetAppState();
        operation->policy.reset(new LoadingUiPolicy(operation->session, config));
        if (operation->policy->reserveView(false) != LoadingUiPolicy::Admission::Accepted) std::terminate();
        operation->reserved = true;
        operation->terminal.reset(new StartState());
        operation->retirementTerminal.reset(new StartState());
        operation->retirementNode.push_back(operation->retirementTerminal.get());
        operation->terminal->prepareResponsiveFrames(operation->frames);
        operation->frameDestination = _screen->getSurface()->getSurface();
        operation->presentationReady = _screen->captureLoadingPresentation(operation->presentation) == Zoom::RenderStatus::Success;
        _responsive->operation = std::move(operation);
        return VideoAudioResult::Reserved;
    }
    catch (...)
    {
        if (operation && operation->policy && operation->reserved) operation->policy->preparationFailed();
        return VideoAudioResult::PreparationFailure;
    }
}

LoaderSession::WorkResult Game::transitionResponsiveVideo(void *context, const LoaderSession::StopToken &)
{
    LoadingWork &work = *static_cast<LoadingWork *>(context);
    ResourceEpoch &epoch = *work.epoch;
    work.entered = true;
    typedef LoadingAudioHandoff Handoff;
    const bool attach = work.kind == LoadingWork::Kind::VideoAttach;
    if (epoch.unknownNativeResources || !work.videoLease || work.videoLease->generation() != epoch.generation.id)
    { work.disposition = LoadingWork::Disposition::RejectedRetained; return LoaderSession::WorkResult::Cancelled; }
    bool valid = false;
    if (epoch.generation.provenance == Handoff::Provenance::KnownSinglePublished ||
        epoch.generation.provenance == Handoff::Provenance::KnownSinglePaused)
    {
        Handoff::AudioPlan oldPlan = {epoch.generation.spec.frequency, epoch.generation.spec.format,
            epoch.generation.spec.channels, epoch.openedChunkRequest};
        Handoff::PrepareResult ready = work.oldAudio->prepareKnown(epoch.generation, oldPlan);
        work.audioOutcome = ready.outcome;
        if (ready.outcome.disposition != Handoff::Disposition::ResourceReady)
        { work.disposition = LoadingWork::Disposition::FailedRetained; return LoaderSession::WorkResult::Cancelled; }
        epoch.generation.provenance = Handoff::Provenance::KnownSinglePaused;
        if (attach)
        {
            auto attached = work.oldAudio->attachVideo(ready.permit, epoch.generation, *work.videoLease);
            work.audioOutcome = attached.outcome;
            work.attachReceipt.record(attached.receipt);
            valid = work.attachReceipt.validFor(epoch.generation.id, work.videoLease.get());
            work.noHook = !valid && !epoch.unknownNativeResources &&
                responsiveNoHookFailure(work.audioOutcome, epoch.generation, *work.videoLease);
        }
        else
        {
            auto detached = work.oldAudio->detachVideo(ready.permit, epoch.generation, *work.videoLease);
            work.audioOutcome = detached.outcome;
            work.detachReceipt.record(detached.receipt);
            valid = work.detachReceipt.validFor(epoch.generation.id, work.videoLease.get());
            work.detached = valid;
        }
        ready.permit.invalidate();
    }
    else
    {
        Handoff::ClosedProof proof = work.oldAudio->verifyClosed(epoch.generation, !epoch.nativeResources);
        work.audioOutcome = proof.outcome();
        if (!proof.valid()) { work.disposition = LoadingWork::Disposition::RejectedRetained; return LoaderSession::WorkResult::Cancelled; }
        if (attach)
        {
            auto attached = work.oldAudio->attachVideoClosed(proof, epoch.generation, *work.videoLease);
            work.audioOutcome = attached.outcome;
            work.attachReceipt.record(attached.receipt);
            valid = work.attachReceipt.validFor(epoch.generation.id, work.videoLease.get());
            work.noHook = !valid && !epoch.unknownNativeResources &&
                responsiveNoHookFailure(work.audioOutcome, epoch.generation, *work.videoLease);
        }
        else
        {
            auto detached = work.oldAudio->detachVideoClosed(proof, epoch.generation, *work.videoLease);
            work.audioOutcome = detached.outcome;
            work.detachReceipt.record(detached.receipt);
            valid = work.detachReceipt.validFor(epoch.generation.id, work.videoLease.get());
            work.detached = valid;
        }
    }
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    // Reset preserved the original bootstrap clock. Re-publish only the retained
    // validated catalog fact; a closed observer epoch is never revived.
    epoch.observer->publishCatalogSupported(epoch.catalog != Mod::ResponsiveMusicCatalog::Unsupported);
#endif
    work.workComplete = valid;
    work.disposition = valid ? LoadingWork::Disposition::Complete : LoadingWork::Disposition::FailedRetained;
    return valid ? LoaderSession::WorkResult::Success : LoaderSession::WorkResult::Cancelled;
}

bool Game::runResponsiveVideoTick() noexcept
{
    if (!_responsive || std::this_thread::get_id() != _responsive->owner) std::terminate();
    if (_responsive->operation) return false;
    VideoState *video = _responsive->video;
    if (!video && !_states.empty()) video = dynamic_cast<VideoState *>(_states.back());
    if (!video) return false;
    if (!_responsive->video) _responsive->renderAllowed = (SDL_GetAppState() & SDL_APPACTIVE) != 0;
    _responsive->video = video;
    try
    {
        if (!_init)
        {
            _init = true;
            { ResponsiveCallbackScope callback(_responsive->callbackDepth); video->init(); }
            if (_responsive->operation || !_responsive->video) return true;
        }
        SDL_Event event;
        for (unsigned n = 0; n < 64 && SDL_PollEvent(&event); ++n)
        {
            if (event.type == SDL_VIDEORESIZE)
            {
                if (!Options::allowResize) continue;
                _responsive->resizePending = true;
                _responsive->resizeWidth = event.resize.w; _responsive->resizeHeight = event.resize.h;
                continue; // keep Flc's borrowed surface intact until detach
            }
            if (event.type == SDL_ACTIVEEVENT)
            {
                _responsive->renderAllowed = (SDL_GetAppState() & SDL_APPACTIVE) != 0;
                continue; // timeline/audio continue; never generic PAUSED
            }
            if (event.type == SDL_QUIT) _responsive->closeRequested = true;
            { ResponsiveCallbackScope callback(_responsive->callbackDepth); video->responsiveVideoEvent(event); }
            if (_responsive->operation || !_responsive->video) return true;
        }
        // Flc owns clear/decode/present. Game must never clear or flip this frame.
        { ResponsiveCallbackScope callback(_responsive->callbackDepth); video->responsiveVideoTick(SDL_GetTicks(), _responsive->renderAllowed); }
    }
    catch (...)
    {
        _responsive->closeRequested = true;
        video->responsiveRequestClose();
    }
    SDL_Delay(1);
    return true;
}

std::shared_ptr<void> Game::retainResponsiveEpoch() const
{ assertResponsiveOwnerAccess(); return _responsive->epoch; }
std::uint64_t Game::responsiveAudioGeneration() const noexcept
{ assertResponsiveOwnerAccess(); return _responsive->epoch->generation.id; }
bool Game::responsiveAudioAvailable() const noexcept
{
    assertResponsiveOwnerAccess();
    const auto p = _responsive->epoch->generation.provenance;
    return p == LoadingAudioHandoff::Provenance::KnownSinglePublished || p == LoadingAudioHandoff::Provenance::KnownSinglePaused;
}
void Game::invalidateResponsiveAudio() noexcept
{
    if (!_responsive) return;
    assertResponsiveOwnerAccess();
    _responsive->epoch->generation.provenance = LoadingAudioHandoff::Provenance::Invalidated;
}
void Game::unpauseResponsiveVideo(VideoState *video) noexcept
{
    assertResponsiveOwnerAccess();
    if (_responsive->operation || _responsive->video != video || _responsive->closeRequested ||
        !_responsive->attachedVideoLease ||
        _responsive->attachedVideoLease->attachmentState() != FlcPlayer::AudioLease::AttachmentState::Attached ||
        _responsive->attachedVideoLease->generation() != _responsive->epoch->generation.id) return;
    auto &generation = _responsive->epoch->generation;
    if (generation.provenance == LoadingAudioHandoff::Provenance::KnownSinglePaused)
    {
        generation.provenance = LoadingAudioHandoff::Provenance::KnownSinglePublished;
        SDL_PauseAudio(0);
    }
}
void Game::finishResponsiveVideo(VideoState *video) noexcept
{
    assertResponsiveOwnerAccess();
    if (_responsive->operation || _responsive->video != video || _responsive->attachedVideoLease ||
        _states.empty() || _states.back() != video) std::terminate();
    _responsive->video = nullptr;
    _responsive->removeVideo = false;
    _responsive->renderAllowed = true;
    // VideoState releases its lease/sequence/epoch first. State deletion remains
    // deferred until the callback returns through the same boundary as loading.
    if (_responsive->closeRequested)
    {
        _responsive->videoResumeWork.reset();
        _responsive->videoResumeUnentered.reset();
        _responsive->menuResumePending = false;
        quit();
    }
    else _responsive->menuResumePending = !!_responsive->videoResumeWork;
}
bool Game::responsiveMusicPlaybackAllowed(const Music *music) noexcept
{
    Game *game = responsivePublishedGame;
    if (!game || !game->_responsive || std::this_thread::get_id() != game->_responsive->owner || !music) return false;
    const ResponsiveCore &core = *game->_responsive;
    if (core.operation || core.closeRequested || core.epoch->workerOwned || core.epoch->unknownNativeResources ||
        core.epoch->catalog != Mod::ResponsiveMusicCatalog::SoftwareMixed) return false;
    const auto provenance = core.epoch->generation.provenance;
    if (provenance != LoadingAudioHandoff::Provenance::KnownSinglePublished && provenance != LoadingAudioHandoff::Provenance::KnownSinglePaused) return false;
    return music->isSoftwareMixedAudio();
}
#endif // OPENXCOM_RESPONSIVE_LOADING


const double Game::VOLUME_GRADIENT = 10.0;

/**
 * Starts up all the SDL subsystems,
 * creates the display screen and sets up the cursor.
 * @param title Title of the game window.
 */
Game::Game(const std::string &title) :
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentController(this),
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _stanceController(this, std::getenv("OPENXCOM_STANCE_FD")),
#endif
#if defined(OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER) && !defined(OPENXCOM_RESPONSIVE_LOADING)
	_mixedAudioObserver(std::getenv("OPENXCOM_MIXED_AUDIO_FD")),
#endif
	_screen(0), _cursor(0), _lang(0), _save(0), _mod(0), _quit(false), _init(false), _mouseActive(true), _timeUntilNextFrame(0)
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    if (!_loadingProcessLease.acquired()) throw Exception("Another responsive Game owns the process.");
#endif
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
#ifndef OPENXCOM_RESPONSIVE_LOADING
		initAudio();
#endif
	}
#ifdef OPENXCOM_RESPONSIVE_LOADING
    _responsive.reset(new ResponsiveCore());
#endif

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
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    _screen->attachSnapshotPresentation(&_snapshotPresentation);
#endif
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
#ifdef OPENXCOM_RESPONSIVE_LOADING
    _responsive->epoch->language.reset(_lang);
#endif

	_timeOfLastFrame = 0;
#ifdef OPENXCOM_RESPONSIVE_LOADING
    responsivePublishedGame = this;
#endif
}

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
Observer::SnapshotPaintedState Game::snapshotPaintedState(const State *address) const noexcept
{
    Observer::SnapshotPaintedState f;
    State *live = snapshotLiveState(address);
    if (!live) return f;
    f.address = live; f.life = live->snapshotLifetime();
    f.initialization = live->snapshotInitialization(); f.ui = live->snapshotUiRevision();
    return f;
}
bool Game::snapshotFrameStateCurrent() const noexcept
{
    const Observer::SnapshotFrameFacts &frame = _snapshotPresentation.facts();
    if (_states.empty() || _states.back() != frame.top.address || !snapshotStateInitialized(_states.back()) ||
        snapshotStackRevision() != frame.stack) return false;
    const Observer::SnapshotPaintedState top = snapshotPaintedState(frame.top.address);
    if (!top.life || top.life != frame.top.life || top.initialization != frame.top.initialization || top.ui != frame.top.ui)
        return false;
    for (std::size_t n = 0; n < frame.count; ++n)
    {
        const Observer::SnapshotPaintedState now = snapshotPaintedState(frame.painted[n].address);
        const Observer::SnapshotPaintedState &then = frame.painted[n];
        if (!now.life || now.life != then.life || now.initialization != then.initialization || now.ui != then.ui)
            return false;
    }
    return true;
}
#endif

/**
 * Deletes the display screen, cursor, states and shuts down all the SDL subsystems.
 */
Game::~Game()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentController.close();
    _agentAudioEvents.close();
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
    // Destruction is never an implicit join, drain, or quarantine escape.
    if (_responsive && (_responsive->operation || _responsive->video || _responsive->attachedVideoLease)) std::terminate();
    const bool responsiveClosed = _responsive && (_responsive->finalClosed ||
        (_responsive->epoch->generation.provenance == LoadingAudioHandoff::Provenance::NeverOpened &&
         !_responsive->epoch->nativeResources && !_responsive->epoch->unknownNativeResources));
    if (!responsiveClosed) invalidateResponsiveAudio(); // ordinary gameplay legacy shutdown only
    _responsive->deferredReplacementNode.clear();
    _responsive->deferredReplacement.reset();
    while (!_deleted.empty()) { State *state = _deleted.back(); _deleted.pop_back(); delete state; }
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    _screen->attachSnapshotPresentation(0);
    _snapshotPresentation.abandon();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _stanceController.close();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
#ifdef OPENXCOM_RESPONSIVE_LOADING
    if (!responsiveClosed) _responsive->epoch->observer->close();
#else
	_mixedAudioObserver.close();
#endif
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
    if (!responsiveClosed) { Sound::stop(); Music::stop(); }
#else
	Sound::stop();
	Music::stop();
#endif

	for (std::list<State*>::iterator i = _states.begin(); i != _states.end(); ++i)
	{
		delete *i;
	}

	SDL_FreeCursor(SDL_GetCursor());

	delete _cursor;
#ifdef OPENXCOM_RESPONSIVE_LOADING
    _lang = nullptr; _mod = nullptr;
    _responsive->epoch->language.reset();
    _responsive->epoch->mod.reset();
#else
	delete _lang;
	delete _mod;
#endif
	delete _save;
	delete _screen;
	delete _fpsCounter;

#ifdef OPENXCOM_RESPONSIVE_LOADING
    if (!responsiveClosed) Mix_CloseAudio();
    responsivePublishedGame = nullptr;
    _responsive.reset();
#else
	Mix_CloseAudio();
#endif

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
#ifdef OPENXCOM_RESPONSIVE_LOADING
    bool responsiveLoopYielded = false;
#endif
	while (!_quit)
	{
#ifdef OPENXCOM_RESPONSIVE_LOADING
        if (loadingBoundary())
        {
            responsiveLoopYielded = true;
            if (_responsive->operation) runOneOperationTick();
            else runResponsiveVideoTick();
            continue;
        }
        if (runResponsiveVideoTick()) { responsiveLoopYielded = true; continue; }
        if (responsiveLoopYielded)
        {
            // Responsive branches consumed focus events without generic pause
            // or volume callbacks. Reconcile only after they returned ownership.
            const Uint8 focus = SDL_GetAppState();
            const int pause = std::max(0, std::min(3, Options::pauseMode));
            runningState = !(focus & SDL_APPACTIVE) ? stateRun[pause] :
                !(focus & SDL_APPINPUTFOCUS) ? kbFocusRun[pause] : RUNNING;
            if (Options::backgroundMute)
            {
                if (!(focus & SDL_APPACTIVE) || !(focus & SDL_APPINPUTFOCUS)) setVolume(0, 0, 0);
                else setVolume(Options::soundVolume, Options::musicVolume, Options::uiVolume);
            }
            if (_responsive->standby && _responsive->standby->origin == LoadingOrigin::Startup &&
                !_responsive->standby->startupResizePending) startupEvent = false;
            responsiveLoopYielded = false;
        }
#endif
		// Clean up states
#ifdef OPENXCOM_RESPONSIVE_LOADING
        _responsive->retiringStates = true;
        while (!_deleted.empty())
        {
            State *state = _deleted.back(); _deleted.pop_back(); delete state;
        }
        _responsive->retiringStates = false;
#else
		while (!_deleted.empty())
		{
			delete _deleted.back();
			_deleted.pop_back();
		}
#endif

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
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
            State *snapshotInitializing = _states.back();
            const std::uint64_t snapshotInitLife = snapshotInitializing->snapshotLifetime();
            snapshotInitializing->_snapshotCompletedInitLife = 0;
            {
                Observer::SnapshotScope snapshotInitScope(_snapshotInitActivity);
#endif
			_init = true;
			#ifdef OPENXCOM_RESPONSIVE_LOADING
            { ResponsiveCallbackScope callback(_responsive->callbackDepth); _states.back()->init(); }
#else
            _states.back()->init();
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
            if (loadingBoundary()) continue;
#endif

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
			#ifdef OPENXCOM_RESPONSIVE_LOADING
            { ResponsiveCallbackScope callback(_responsive->callbackDepth); _states.back()->handle(&action); }
#else
            _states.back()->handle(&action);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
            }
            // Completion includes ordinary reset and pointer refresh. A changed
            // stack, exception, or new lifetime never inherits this milestone.
            if (_snapshotInitActivity.idle() && _init && !_states.empty() &&
                _states.back() == snapshotInitializing && snapshotInitLife != 0 &&
                snapshotInitializing->snapshotLifetime() == snapshotInitLife)
            {
                snapshotInitializing->_snapshotCompletedInitRevision.renew();
                snapshotInitializing->_snapshotCompletedInitLife = snapshotInitLife;
            }
#endif
		}

#ifdef OPENXCOM_RESPONSIVE_LOADING
        if (loadingBoundary()) continue;
#endif
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
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            _agentController.beforeEvent(_event);
#endif
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
            if (_stanceController.beforeEvent(_event, _states.back())) continue;
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
					#ifdef OPENXCOM_RESPONSIVE_LOADING
            { ResponsiveCallbackScope callback(_responsive->callbackDepth); _states.back()->handle(&action); }
#else
            _states.back()->handle(&action);
#endif
					break;
			}
#ifdef OPENXCOM_RESPONSIVE_LOADING
            if (_responsive->operation || _responsive->video) break;
#endif
			if (!_init)
			{
				// States stack was changed, break the loop so new state
				// can be initialized before processing new events
				break;
			}
		}
		
#ifdef OPENXCOM_RESPONSIVE_LOADING
        if (loadingBoundary()) continue;
#endif
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
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        _agentAudioEvents.poll(_agentController.enabled());
        _agentController.poll();
#endif
        // Process rendering
		if (runningState != PAUSED)
		{
			// Process logic
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
			// The gate covers rendering/animation only, not simulation or timers.
			if (!RenderPairGate::active())
#endif
			#ifdef OPENXCOM_RESPONSIVE_LOADING
            { ResponsiveCallbackScope callback(_responsive->callbackDepth); _states.back()->think(); }
#else
            _states.back()->think();
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
            if (loadingBoundary()) continue;
#endif
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
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
                _snapshotPresentation.begin(snapshotPaintedState(_states.back()), snapshotStackRevision(), _screen->snapshotDisplayFacts());
                Observer::SnapshotFrameScope snapshotFrameScope(_snapshotPresentation);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
                _agentController.beginFrame();
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
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
                    const State *snapshotPainted = *i;
                    const std::uint64_t snapshotPaintedLife = (*i)->snapshotLifetime();
#endif
					(*i)->blit();
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
                    Observer::SnapshotPaintedState snapshotReturned = snapshotPaintedState(snapshotPainted);
                    if (snapshotReturned.life != snapshotPaintedLife) snapshotReturned = Observer::SnapshotPaintedState();
                    _snapshotPresentation.painted(snapshotReturned);
#endif
				}
				_fpsCounter->blit(_screen->getSurface());
				_cursor->blit(_screen->getSurface());
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
                _snapshotPresentation.layersReturned();
#endif
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
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
                _agentController.presented();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
                _screen->snapshotFinishFrame(snapshotFrameStateCurrent());
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
#ifdef OPENXCOM_RESPONSIVE_LOADING
        _responsive->epoch->observer->poll();
#else
		_mixedAudioObserver.poll();
#endif
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
#ifdef OPENXCOM_RESPONSIVE_LOADING
    if (!_responsive || std::this_thread::get_id() != _responsive->owner) std::terminate();
    if (!_responsive->finalClosed)
    {
        _responsive->closeRequested = true;
        if (_responsive->operation) { _responsive->operation->closeRequested = true; return; }
        if (_responsive->video) { _responsive->video->responsiveRequestClose(); return; }
        if (!_save) { reserveResponsiveCleanup(); return; }
    }
#endif
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
#ifdef OPENXCOM_RESPONSIVE_LOADING
    assertResponsiveOwnerAccess();
#endif
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
#ifdef OPENXCOM_RESPONSIVE_LOADING
    assertResponsiveOwnerAccess();
    if (_responsive->retiringStates) std::terminate();
    if (_responsive->video)
    {
        std::unique_ptr<State> replacement(state);
        std::list<State *> node;
        node.push_back(state); // allocate before altering the deferred transition
        _responsive->deferredReplacementNode.swap(node);
        _responsive->deferredReplacement = std::move(replacement);
        _responsive->deferredPush = false;
        _responsive->removeVideo = true;
        if (!_responsive->operation) _responsive->video->responsiveRequestClose();
        return;
    }
#endif
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
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentController.invalidate(false);
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
    assertResponsiveOwnerAccess();
    if (_responsive->retiringStates) std::terminate();
    if (_responsive->video)
    {
        std::unique_ptr<State> replacement(state);
        std::list<State *> node;
        node.push_back(state);
        _responsive->deferredReplacementNode.swap(node);
        _responsive->deferredReplacement = std::move(replacement);
        _responsive->deferredPush = true;
        _responsive->removeVideo = true;
        if (!_responsive->operation) _responsive->video->responsiveRequestClose();
        return;
    }
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    _snapshotStackRevision.renew();
#endif
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
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentController.invalidate(false);
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
    // Removal is an owner-only intent, even during a video worker/TLS tail.
    // Validate immutable identity before touching the latch or operation.
    if (!_responsive || std::this_thread::get_id() != _responsive->owner) std::terminate();
    if (_responsive->video)
    {
        _responsive->removeVideo = true;
        if (!_responsive->operation) _responsive->video->responsiveRequestClose();
        return;
    }
    assertResponsiveOwnerAccess();
    if (_responsive->retiringStates) std::terminate();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    _snapshotStackRevision.renew();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _stanceController.beforePop(_states.back());
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    const std::uint64_t snapshotPoppedLife = _states.back()->snapshotLifetime();
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
    // Moving an existing list node makes post-detach VideoState removal
    // nonallocating. The object is still deleted only after callback return.
    if (_states.empty()) std::terminate();
    auto retired = _states.end(); --retired;
    _deleted.splice(_deleted.end(), _states, retired);
#else
	_deleted.push_back(_states.back());
	_states.pop_back();
#endif
	_init = false;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    if (!_states.empty())
    {
        State *parent = _states.back();
        parent->_snapshotReturnedRevision.renew();
        parent->_snapshotReturnedParentLife = parent->snapshotLifetime();
        parent->_snapshotReturnedChildLife = snapshotPoppedLife;
    }
#endif
}

/**
 * Returns the language currently in use by the game.
 * @return Pointer to the language.
 */
Language *Game::getLanguage() const
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    assertResponsiveOwnerAccess();
#endif
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
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentController.invalidate(false);
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
    assertResponsiveOwnerAccess();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    _snapshotSavedRevision.renew();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
    _stanceController.beforeSaveChange();
#endif
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
#ifdef OPENXCOM_RESPONSIVE_LOADING
    assertResponsiveOwnerAccess();
#endif
	return _mod;
}

/**
 * Loads the mods specified in the game options.
 */
void Game::loadMods()
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    throw Exception("Responsive resource loading requires an admitted operation.");
#else
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    _snapshotModRevision.renew();
#endif
	Mod::resetGlobalStatics();
	delete _mod;
	_mod = new Mod();
	_mod->loadAll(FileMap::getRulesets());
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentAudioEvents.publishCatalogSupported(_mod->isMusicCatalogSoftwareMixed());
#endif
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
	// This executes on the loader thread; only an atomic aggregate crosses over.
#ifdef OPENXCOM_RESPONSIVE_LOADING
    _responsive->epoch->observer->publishCatalogSupported(_mod->isMusicCatalogSoftwareMixed());
#else
	_mixedAudioObserver.publishCatalogSupported(_mod->isMusicCatalogSoftwareMixed());
#endif
#endif
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

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
bool Game::inventoryParentState(State *parent) const
{
    if (!parent || _states.size() < 2) return false;
    auto state = _states.rbegin();
    return *++state == parent;
}
#endif

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
#ifdef OPENXCOM_RESPONSIVE_LOADING
    assertResponsiveOwnerAccess();
    _lang = nullptr;
    try { loadEpochLanguages(*_responsive->epoch); }
    catch (...) { _lang = _responsive->epoch->language.get(); throw; }
    _lang = _responsive->epoch->language.get();
#else

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
#endif
}

#ifdef OPENXCOM_RESPONSIVE_LOADING
void Game::loadEpochLanguages(ResourceEpoch &epoch)
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

	epoch.language.reset();
	epoch.language.reset(new Language());

	// Load default and current language
	std::ostringstream ssDefault, ssCurrent;
	ssDefault << "/Language/" << defaultLang << ".yml";
	ssCurrent << "/Language/" << currentLang << ".yml";

	epoch.language->loadFile(CrossPlatform::searchDataFile("common" + ssDefault.str()));
	if (currentLang != defaultLang)
		epoch.language->loadFile(CrossPlatform::searchDataFile("common" + ssCurrent.str()));

	// if this is a master but it has a master of its own, allow it to
	// chainload the "super" master, including its languages
	ModInfo modInfo = Options::getModInfo(Options::getActiveMaster());
	if (!modInfo.getMaster().empty())
	{
		ModInfo masterInfo = Options::getModInfo(modInfo.getMaster());
		epoch.language->loadFile(masterInfo.getPath() + ssDefault.str());
		if (currentLang != defaultLang)
			epoch.language->loadFile(masterInfo.getPath() + ssCurrent.str());
	}

	std::vector<const ModInfo*> activeMods = Options::getActiveMods();
	for (std::vector<const ModInfo*>::const_iterator i = activeMods.begin(); i != activeMods.end(); ++i)
	{
		epoch.language->loadFile((*i)->getPath() + ssDefault.str());
		if (currentLang != defaultLang)
			epoch.language->loadFile((*i)->getPath() + ssCurrent.str());
	}

	epoch.language->loadRule(epoch.mod->getExtraStrings(), defaultLang);
	if (currentLang != defaultLang)
		epoch.language->loadRule(epoch.mod->getExtraStrings(), currentLang);
}
#endif

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
void Game::resetAgentAudioEvents(bool resourceReload)
{
    _agentAudioEvents.beforeAudioReset(resourceReload);
}
#endif

#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
void Game::resetMixedAudioObserver(bool resourceReload)
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    assertResponsiveOwnerAccess();
    _responsive->epoch->observer->beforeAudioReset(resourceReload);
#else
	_mixedAudioObserver.beforeAudioReset(resourceReload);
#endif
}
#endif

/**
 * Initializes the audio subsystem.
 */
void Game::initAudio()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    resetAgentAudioEvents();
#endif
#ifdef OPENXCOM_RESPONSIVE_LOADING
    invalidateResponsiveAudio(); // before every legacy guard or native call
    assertResponsiveOwnerAccess();
#endif
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

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
namespace OpenXcom {
State *Game::snapshotLiveState(const State *state) const noexcept
{
    if (!Observer::snapshotRuntimeEnabled() || !state) return 0;
    for (State *live : _states) if (live == state) return live;
    return 0;
}
bool Game::snapshotStateInStack(const State *state) const noexcept
{
    return snapshotLiveState(state) != 0;
}
bool Game::snapshotStateInitialized(const State *state) const noexcept
{
    return Observer::snapshotRuntimeEnabled() && _snapshotInitActivity.idle() && _init &&
        state && !_states.empty() && _states.back() == state && _states.back()->snapshotInitialization() != 0;
}
}
#endif

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace OpenXcom {
State *Game::agentTopState() const
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
    if (!_responsive || _responsive->operation || _responsive->video || _responsive->closeRequested) return 0;
#endif
    return _init && !_quit && !_states.empty() ? _states.back() : 0;
}
}
#endif

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace OpenXcom {
bool Game::agentOverlayClear(const Observer::AgentUiState &ui) const
{
    const Surface *overlays[] = {_cursor, _fpsCounter};
    for (const Surface *overlay : overlays)
    {
        if (!overlay || !overlay->getVisible()) continue;
        for (const auto &rect : ui.regions)
            if (overlay->getX() < rect.x + rect.width && overlay->getX() + overlay->getWidth() > rect.x &&
                overlay->getY() < rect.y + rect.height && overlay->getY() + overlay->getHeight() > rect.y)
                return false;
    }
    return true;
}
}
#endif
