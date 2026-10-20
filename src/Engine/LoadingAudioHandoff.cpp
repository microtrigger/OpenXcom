// Copyright 2026 OpenXcom contributors. GPL-3.0-or-later; see LICENSE.txt and LICENSE-NOTICES.md.
// New reconstruction from reviewed contracts; historical execution does not qualify it.
#include "LoadingAudioHandoff.h"
#ifdef OPENXCOM_LOADING_AUDIO_HANDOFF
#include <features.h>
#if !defined(__linux__) || !defined(__GLIBC__)
#error "LoadingAudioHandoff requires the selected Linux/glibc profile"
#endif
#include <SDL_mixer.h>
#include <cstring>
#include <exception>
#include <utility>
#ifdef OPENXCOM_RESPONSIVE_LOADING
#include <limits>
#endif
#ifndef __NO_MUSIC
#include "Adlib/adlplayer.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
#include "../Observer/MixedAudioObserver.h"
#endif

namespace OpenXcom
{
LoadingAudioHandoff::OwnerSnapshot LoadingAudioHandoff::captureOwner() noexcept
{
    OwnerSnapshot snapshot = {std::this_thread::get_id(), AudioDomain::Unavailable,
        Driver::Unavailable};
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO)) return snapshot;
    snapshot.domain = AudioDomain::Initialized;
    // Use a full bounded buffer, not six bytes which could truncate another
    // name to "dummy". Sentinel fill detects a malformed missing terminator.
    char name[64];
    std::memset(name, 0xff, sizeof(name));
    const char *reported = SDL_AudioDriverName(name, sizeof(name));
    if (reported != name || !std::memchr(name, '\0', sizeof(name))) return snapshot;
    if (!name[0]) return snapshot;
    snapshot.driver = std::strcmp(name, "dummy") == 0 ? Driver::Dummy : Driver::Other;
    return snapshot;
}

#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
LoadingAudioHandoff::LoadingAudioHandoff(Observer::MixedAudioObserver *sameContextObserver) noexcept :
#else
LoadingAudioHandoff::LoadingAudioHandoff() noexcept :
#endif
    _snapshot(captureOwner()), _sealed(false),
    _outcome{Disposition::Unattempted, Phase::OwnerPrepared, Reason::None, {0, 0, 0, 0},
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
        sameContextObserver ? ObserverDisposition::NotReset : ObserverDisposition::Absent},
        _observer(sameContextObserver)
#else
        ObserverDisposition::NotBuilt}
#endif
{}

void LoadingAudioHandoff::sealForWorker() noexcept
{
    if (std::this_thread::get_id() != _snapshot.owner) std::terminate();
    // Owner-only preparation with no worker in existence, not a concurrent gate.
    if (_sealed) std::terminate();
    _sealed = true;
    _outcome.lastCompletedPhase = Phase::WorkerReserved;
}

LoadingAudioHandoff::ResourceMutationPermit::ResourceMutationPermit(
    const LoadingAudioHandoff *handoff) noexcept : _handoff(handoff)
#ifdef OPENXCOM_RESPONSIVE_LOADING
    , _generation(handoff ? handoff->_boundGeneration : nullptr),
      _id(handoff ? handoff->_boundId : 0)
#endif
{}
LoadingAudioHandoff::ResourceMutationPermit::ResourceMutationPermit(
    ResourceMutationPermit &&other) noexcept : _handoff(other._handoff)
#ifdef OPENXCOM_RESPONSIVE_LOADING
    , _generation(other._generation), _id(other._id)
#endif
{ other.invalidate(); }
LoadingAudioHandoff::ResourceMutationPermit &LoadingAudioHandoff::ResourceMutationPermit::operator=(
    ResourceMutationPermit &&other) noexcept
{
    if (this != &other)
    {
        _handoff = other._handoff;
#ifdef OPENXCOM_RESPONSIVE_LOADING
        _generation = other._generation; _id = other._id;
#endif
        other.invalidate();
    }
    return *this;
}
bool LoadingAudioHandoff::ResourceMutationPermit::validFor(const LoadingAudioHandoff &handoff) const noexcept
{ return _handoff == &handoff; }
void LoadingAudioHandoff::ResourceMutationPermit::invalidate() noexcept
{
    _handoff = nullptr;
#ifdef OPENXCOM_RESPONSIVE_LOADING
    _generation = nullptr; _id = 0;
#endif
}
LoadingAudioHandoff::PrepareResult::PrepareResult(const Outcome &value,
    ResourceMutationPermit &&permission) noexcept : outcome(value), permit(std::move(permission)) {}
LoadingAudioHandoff::PrepareResult LoadingAudioHandoff::result(Disposition disposition, Reason reason) noexcept
{
    _outcome.disposition = disposition;
    _outcome.reason = reason;
#ifdef OPENXCOM_RESPONSIVE_LOADING
    _entryComplete.test_and_set(std::memory_order_release);
#endif
    return PrepareResult(_outcome, ResourceMutationPermit(
        disposition == Disposition::ResourceReady ? this : nullptr));
}

void LoadingAudioHandoff::claimEntry() noexcept
{
    // Immutable identity and never-cleared claims precede mutable state.
    if (std::this_thread::get_id() == _snapshot.owner) std::terminate();
    if (_claimed.test_and_set(std::memory_order_acq_rel)) std::terminate();
    if (!_sealed) std::terminate();
#ifdef OPENXCOM_RESPONSIVE_LOADING
    _entryThread = std::this_thread::get_id();
#endif
}

LoadingAudioHandoff::PrepareResult LoadingAudioHandoff::prepare() noexcept
{
    claimEntry();
    if (_snapshot.domain != AudioDomain::Initialized)
        return result(Disposition::RejectedRetained, Reason::AudioSubsystemUnavailable);
    if (_snapshot.driver != Driver::Dummy)
        return result(Disposition::RejectedRetained, Reason::DriverNotDummy);

    ActualMixerSpec &actual = _outcome.actual; // zeroed before the native query
    actual.openCount = Mix_QuerySpec(&actual.frequency, &actual.format, &actual.channels);
    _outcome.lastCompletedPhase = Phase::Preflight;
    if (actual.openCount == 0) return result(Disposition::RejectedRetained, Reason::NoMixer);
    if (actual.openCount > 1) return result(Disposition::RejectedRetained, Reason::MultipleMixerOpens);
    if (actual.openCount < 0 || actual.frequency <= 0 || actual.channels != 2 ||
        (actual.format != AUDIO_S8 && actual.format != MIX_DEFAULT_FORMAT))
        return result(Disposition::RejectedRetained, Reason::InvalidMixerState);

    return pauseDrainAndStop();
}

LoadingAudioHandoff::PrepareResult LoadingAudioHandoff::pauseDrainAndStop() noexcept
{
    SDL_PauseAudio(1);
    _outcome.lastCompletedPhase = Phase::PauseIssued;
    SDL_LockAudio();
    SDL_UnlockAudio();
    _outcome.lastCompletedPhase = Phase::Drained;
    // The empty drain critical section has ENDED. Setters internally lock.
    Mix_SetPostMix(NULL, NULL);
    _outcome.lastCompletedPhase = Phase::PostmixDetached;
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    if (_observer)
    {
        const Observer::MixedAudioObserver::LoadingReset reset =
            _observer->resetForExclusiveLoadingAfterPostmixDetach();
        _outcome.observer = reset == Observer::MixedAudioObserver::LoadingReset::EpochClosed ?
            ObserverDisposition::EpochClosed :
            reset == Observer::MixedAudioObserver::LoadingReset::BootstrapPreserved ?
            ObserverDisposition::BootstrapPreserved : ObserverDisposition::DisabledRetained;
    }
#endif
    Mix_HookMusic(NULL, NULL);
    _outcome.lastCompletedPhase = Phase::HooksDetached;
    if (Mix_HaltChannel(-1) != 0)
        return result(Disposition::FailedRetained, Reason::HaltChannelsFailed);
    _outcome.lastCompletedPhase = Phase::ChannelsHalted;
    if (Mix_HaltMusic() != 0)
        return result(Disposition::FailedRetained, Reason::HaltMusicFailed);
    _outcome.lastCompletedPhase = Phase::MusicHalted;
#ifndef __NO_MUSIC
    // Native global player mutation, independent of Options::mute. Caller
    // guarantees both OPL globals null or a fully valid live pair.
    func_mute();
#endif
    _outcome.lastCompletedPhase = Phase::PlaybackStopped;
    _outcome.lastCompletedPhase = Phase::ResourceReady;
    return result(Disposition::ResourceReady, Reason::None);
}

#ifdef OPENXCOM_RESPONSIVE_LOADING
LoadingAudioHandoff::Outcome LoadingAudioHandoff::emptyOutcome() noexcept
{
    return Outcome{Disposition::Unattempted, Phase::OwnerPrepared, Reason::None,
        {0,0,0,0}, ObserverDisposition::NotBuilt};
}
LoadingAudioHandoff::ClosedProof::ClosedProof() noexcept :
    _handoff(nullptr), _generation(nullptr), _id(0), _outcome(emptyOutcome()) {}
LoadingAudioHandoff::ClosedProof::ClosedProof(const LoadingAudioHandoff *handoff,
    const Generation *generation, const Outcome &outcome, bool valid) noexcept :
    _handoff(valid ? handoff : nullptr), _generation(valid ? generation : nullptr),
    _id(generation ? generation->id : 0), _outcome(outcome) {}
LoadingAudioHandoff::ClosedProof::ClosedProof(ClosedProof &&other) noexcept :
    _handoff(other._handoff), _generation(other._generation), _id(other._id), _outcome(other._outcome)
{ other.invalidate(); }
LoadingAudioHandoff::ClosedProof &LoadingAudioHandoff::ClosedProof::operator=(ClosedProof &&other) noexcept
{
    if (this != &other)
    {
        _handoff=other._handoff; _generation=other._generation; _id=other._id; _outcome=other._outcome;
        other.invalidate();
    }
    return *this;
}
void LoadingAudioHandoff::ClosedProof::invalidate() noexcept { _handoff=nullptr; _generation=nullptr; }
bool LoadingAudioHandoff::ClosedProof::valid() const noexcept { return _handoff && _generation; }
bool LoadingAudioHandoff::ClosedProof::validFor(std::uint64_t generation) const noexcept
{ return valid() && _id == generation; }
LoadingAudioHandoff::CloseReceipt::CloseReceipt() noexcept :
    _valid(false), _id(0), _outcome(emptyOutcome()), _closed() {}
LoadingAudioHandoff::CloseReceipt::CloseReceipt(const Outcome &outcome, std::uint64_t id,
    ClosedProof &&closed) noexcept : _valid(closed.valid()), _id(id), _outcome(outcome), _closed(std::move(closed)) {}
LoadingAudioHandoff::CloseReceipt::CloseReceipt(CloseReceipt &&other) noexcept :
    _valid(other._valid), _id(other._id), _outcome(other._outcome), _closed(std::move(other._closed))
{ other._valid=false; }
LoadingAudioHandoff::CloseReceipt &LoadingAudioHandoff::CloseReceipt::operator=(CloseReceipt &&other) noexcept
{
    if (this != &other)
    {
        _valid=other._valid; _id=other._id; _outcome=other._outcome; _closed=std::move(other._closed);
        other._valid=false;
    }
    return *this;
}
LoadingAudioHandoff::ClosedProof LoadingAudioHandoff::CloseReceipt::releaseClosedProof() noexcept
{ return std::move(_closed); }

void LoadingAudioHandoff::claimContinuation() noexcept
{
    if (std::this_thread::get_id() == _snapshot.owner) std::terminate();
    if (_continued.test_and_set(std::memory_order_acq_rel)) std::terminate();
    // No mutable entry state is read until the entry has finished publication.
    // These flags are single-shot admission guards, never a polling protocol.
    if (!_entryComplete.test_and_set(std::memory_order_acquire)) std::terminate();
    if (std::this_thread::get_id() != _entryThread) std::terminate();
}
bool LoadingAudioHandoff::validPlan(const AudioPlan &plan) noexcept
{
    const bool rate = plan.frequency==8000 || plan.frequency==11025 || plan.frequency==16000 ||
        plan.frequency==22050 || plan.frequency==32000 || plan.frequency==44100 || plan.frequency==48000;
    if (!rate || plan.channels!=2 || plan.chunkSize<=0 || plan.chunkSize>65535) return false;
#ifndef __NO_MUSIC
    return plan.format==AUDIO_S16SYS;
#else
    return plan.format==AUDIO_S8 || plan.format==AUDIO_S16SYS;
#endif
}
bool LoadingAudioHandoff::exactSpec(const ActualMixerSpec &actual, const AudioPlan &plan) noexcept
{
    return actual.openCount==1 && actual.frequency==plan.frequency &&
        actual.format==plan.format && actual.channels==plan.channels;
}
bool LoadingAudioHandoff::knownGeneration(const Generation &g) const noexcept
{
    return g.id!=0 && (g.provenance==Provenance::KnownSinglePaused ||
        g.provenance==Provenance::KnownSinglePublished) && g.spec.openCount==1;
}
bool LoadingAudioHandoff::permitBound(const ResourceMutationPermit &p, const Generation &g) const noexcept
{
    return p._handoff==this && p._generation==&g && p._id==g.id && knownGeneration(g) &&
        _outcome.disposition==Disposition::ResourceReady;
}
bool LoadingAudioHandoff::closedBound(const ClosedProof &p, const Generation &g) const noexcept
{
    return p.valid() && p._generation==&g && p._id==g.id && g.spec.openCount==0 &&
        (g.provenance==Provenance::NeverOpened || g.provenance==Provenance::SilentClosedNoNativeResources);
}
void LoadingAudioHandoff::queryActual() noexcept
{
    _outcome.actual=ActualMixerSpec{0,0,0,0};
    _outcome.actual.openCount=Mix_QuerySpec(&_outcome.actual.frequency,&_outcome.actual.format,&_outcome.actual.channels);
}
LoadingAudioHandoff::ClosedProof LoadingAudioHandoff::closedResult(const Generation &g, bool valid) noexcept
{
    _entryComplete.test_and_set(std::memory_order_release);
    return ClosedProof(this,&g,_outcome,valid);
}
LoadingAudioHandoff::PrepareResult LoadingAudioHandoff::prepareKnown(const Generation &g,
    const AudioPlan &plan) noexcept
{
    claimEntry();
    if (!knownGeneration(g)) return result(Disposition::RejectedRetained,Reason::UnknownGeneration);
    if (!validPlan(plan)) return result(Disposition::RejectedRetained,Reason::InvalidAudioPlan);
    if (_snapshot.domain!=AudioDomain::Initialized)
        return result(Disposition::RejectedRetained,Reason::AudioSubsystemUnavailable);
    if (_snapshot.driver!=Driver::Dummy) return result(Disposition::RejectedRetained,Reason::DriverNotDummy);
    queryActual(); _outcome.lastCompletedPhase=Phase::Preflight;
    if (_outcome.actual.openCount>1) return result(Disposition::RejectedRetained,Reason::MultipleMixerOpens);
    if (!exactSpec(_outcome.actual,plan) || !exactSpec(g.spec,plan))
        return result(Disposition::RejectedRetained,Reason::GenerationMismatch);
    _boundGeneration=&g; _boundId=g.id;
    return pauseDrainAndStop();
}
LoadingAudioHandoff::ClosedProof LoadingAudioHandoff::verifyClosed(const Generation &g,
    bool noNativeResources) noexcept
{
    claimEntry();
    _outcome.disposition=Disposition::RejectedRetained;
    if (!noNativeResources) { _outcome.reason=Reason::NativeResourcesRemain; return closedResult(g,false); }
    if ((g.provenance!=Provenance::NeverOpened && g.provenance!=Provenance::SilentClosedNoNativeResources) || g.spec.openCount!=0)
    { _outcome.reason=Reason::UnknownGeneration; return closedResult(g,false); }
    if (_snapshot.domain==AudioDomain::Unavailable)
    {
        if (g.provenance!=Provenance::NeverOpened)
        { _outcome.reason=Reason::AudioSubsystemUnavailable; return closedResult(g,false); }
    }
    else
    {
        if (_snapshot.driver!=Driver::Dummy)
        { _outcome.reason=Reason::DriverNotDummy; return closedResult(g,false); }
        queryActual(); _outcome.lastCompletedPhase=Phase::Preflight;
        if (_outcome.actual.openCount!=0 || SDL_GetAudioStatus()!=SDL_AUDIO_STOPPED)
        { _outcome.reason=Reason::CloseUnverified; return closedResult(g,false); }
    }
    _outcome.disposition=Disposition::ClosedReady; _outcome.reason=Reason::None;
    _outcome.lastCompletedPhase=Phase::ClosedVerified;
    return closedResult(g,true);
}
LoadingAudioHandoff::PrepareResult LoadingAudioHandoff::openFresh(ClosedProof &closed,
    const AudioPlan &plan, Generation &g) noexcept
{
    claimEntry();
    const bool closedWasBound=closedBound(closed,g);
    closed.invalidate(); // Every admitted attempt spends the supplied one-shot proof.
    if (!closedWasBound) return result(Disposition::RejectedRetained,Reason::UnknownGeneration);
    if (!validPlan(plan)) return result(Disposition::RejectedRetained,Reason::InvalidAudioPlan);
    if (_snapshot.domain!=AudioDomain::Initialized)
        return result(Disposition::RejectedRetained,Reason::AudioSubsystemUnavailable);
    if (_snapshot.driver!=Driver::Dummy) return result(Disposition::RejectedRetained,Reason::DriverNotDummy);
    if (g.id==std::numeric_limits<std::uint64_t>::max())
        return result(Disposition::RejectedRetained,Reason::CounterOverflow);
    queryActual(); _outcome.lastCompletedPhase=Phase::Preflight;
    if (_outcome.actual.openCount!=0 || SDL_GetAudioStatus()!=SDL_AUDIO_STOPPED)
        return result(Disposition::RejectedRetained,Reason::CloseUnverified);
    g.provenance=Provenance::Invalidated;
    const int opened=Mix_OpenAudio(plan.frequency,plan.format,plan.channels,plan.chunkSize);
    _outcome.lastCompletedPhase=Phase::OpenIssued;
    if (opened!=0)
    {
        queryActual();
        if (_outcome.actual.openCount==0 && SDL_GetAudioStatus()==SDL_AUDIO_STOPPED)
        {
            g.spec=ActualMixerSpec{0,0,0,0}; g.provenance=Provenance::SilentClosedNoNativeResources;
            _outcome.lastCompletedPhase=Phase::ClosedVerified;
            return result(Disposition::ClosedReady,Reason::OpenFailedClosed);
        }
        // An unsuccessful native open can still leave an uncertain callback.
        // Exclude it, but never pretend we own enough opens to close/free it.
        SDL_PauseAudio(1); SDL_LockAudio(); SDL_UnlockAudio();
        _outcome.lastCompletedPhase=Phase::FreshDrained;
        return result(Disposition::FailedRetained,Reason::OpenFailedRetained);
    }
    ++g.id;
    // OpenAudio auto-unpauses. Drain BEFORE checking negotiated output, even
    // when a valid-but-different requested/actual rate will be rejected.
    SDL_PauseAudio(1); SDL_LockAudio(); SDL_UnlockAudio();
    _outcome.lastCompletedPhase=Phase::FreshDrained;
    queryActual(); g.spec=_outcome.actual;
    if (!exactSpec(_outcome.actual,plan))
    {
        if (_outcome.actual.openCount==1)
        {
            Mix_CloseAudio(); queryActual();
            if (_outcome.actual.openCount==0 && SDL_GetAudioStatus()==SDL_AUDIO_STOPPED)
            {
                g.spec=ActualMixerSpec{0,0,0,0}; g.provenance=Provenance::SilentClosedNoNativeResources;
                _outcome.lastCompletedPhase=Phase::DeviceClosed;
            }
        }
        return result(Disposition::FailedRetained,Reason::GenerationMismatch);
    }
    g.provenance=Provenance::KnownSinglePaused;
    _boundGeneration=&g; _boundId=g.id;
    // Finish the same complete detach/halt prelude after the mandatory fresh
    // drain. The second pause/drain is conservative and never substitutes for
    // the one immediately following native open.
    return pauseDrainAndStop();
}
void LoadingAudioHandoff::terminalObserverAfterDetach() noexcept
{
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    if (_observer)
    {
        _observer->terminalCloseForExclusiveLoadingAfterPostmixDetach();
        _outcome.observer=ObserverDisposition::EpochClosed;
    }
#endif
    _outcome.lastCompletedPhase=Phase::ObserverTerminalClosed;
}
LoadingAudioHandoff::CloseReceipt LoadingAudioHandoff::closePrepared(ResourceMutationPermit &permit,
    Generation &g, bool resourcesEmpty, bool terminalObserver) noexcept
{
    claimContinuation();
    const bool bound=permitBound(permit,g); permit.invalidate();
    if (!bound || !resourcesEmpty)
    {
        _outcome.disposition=Disposition::FailedRetained;
        _outcome.reason=bound ? Reason::NativeResourcesRemain : Reason::InvalidPermit;
        return CloseReceipt(_outcome,g.id,closedResult(g,false));
    }
    queryActual();
    if (_outcome.actual.openCount!=1 || _outcome.actual.frequency!=g.spec.frequency ||
        _outcome.actual.format!=g.spec.format || _outcome.actual.channels!=g.spec.channels ||
        SDL_GetAudioStatus()!=SDL_AUDIO_PAUSED)
    {
        _outcome.disposition=Disposition::FailedRetained; _outcome.reason=Reason::GenerationMismatch;
        return CloseReceipt(_outcome,g.id,closedResult(g,false));
    }
    if (terminalObserver) terminalObserverAfterDetach();
    g.provenance=Provenance::Invalidated;
    Mix_CloseAudio(); queryActual();
    if (_outcome.actual.openCount!=0 || SDL_GetAudioStatus()!=SDL_AUDIO_STOPPED)
    {
        _outcome.disposition=Disposition::FailedRetained; _outcome.reason=Reason::CloseUnverified;
        return CloseReceipt(_outcome,g.id,closedResult(g,false));
    }
    g.spec=ActualMixerSpec{0,0,0,0}; g.provenance=Provenance::SilentClosedNoNativeResources;
    _outcome.disposition=Disposition::AudioClosed; _outcome.reason=Reason::None;
    _outcome.lastCompletedPhase=Phase::DeviceClosed;
    return CloseReceipt(_outcome,g.id,closedResult(g,true));
}
LoadingAudioHandoff::CloseReceipt LoadingAudioHandoff::terminalCloseNeverOpened(ClosedProof &closed,
    Generation &g) noexcept
{
    claimContinuation();
    const bool valid=closed._handoff==this && closedBound(closed,g); closed.invalidate();
    if (!valid)
    {
        _outcome.disposition=Disposition::FailedRetained; _outcome.reason=Reason::UnknownGeneration;
        return CloseReceipt(_outcome,g.id,closedResult(g,false));
    }
    // Proven empty native domain: no stale-zero halt, setter or close calls.
    terminalObserverAfterDetach();
    _outcome.disposition=Disposition::AudioClosed; _outcome.reason=Reason::None;
    return CloseReceipt(_outcome,g.id,closedResult(g,true));
}

LoadingAudioHandoff::VideoAttachResult LoadingAudioHandoff::attachVideo(ResourceMutationPermit &permit,
    const Generation &g, FlcPlayer::AudioLease &lease) noexcept
{
    claimContinuation();
    const bool bound=permitBound(permit,g); permit.invalidate();
    _outcome.disposition=Disposition::FailedRetained;
    if (!bound) _outcome.reason=Reason::InvalidPermit;
    else if (lease.generation()!=g.id || lease.attachmentState()!=FlcPlayer::AudioLease::AttachmentState::NeverAttached)
        _outcome.reason=Reason::InvalidLease;
    else
    {
        const FlcPlayer::AudioLease::PrepareStatus status=lease.prepareForDevice(g.spec.frequency,g.spec.format,g.spec.channels);
        if (status==FlcPlayer::AudioLease::PrepareStatus::Ready || status==FlcPlayer::AudioLease::PrepareStatus::NoPCM)
        {
            const bool hook=lease.hasPCM();
            if (hook) Mix_HookMusic(&FlcPlayer::AudioLease::callback,&lease);
            const FlcPlayer::AudioLease::AttachReceipt receipt(&lease,g.id,lease.route(),hook);
            lease.acceptAttach(receipt);
            _outcome.disposition=Disposition::VideoAttached; _outcome.reason=Reason::None;
            _outcome.lastCompletedPhase=Phase::VideoHookAttached;
            return VideoAttachResult{_outcome,receipt};
        }
        _outcome.reason=status==FlcPlayer::AudioLease::PrepareStatus::AllocationFailure ? Reason::VideoAllocationFailure :
            status==FlcPlayer::AudioLease::PrepareStatus::ConversionFailure ? Reason::VideoConversionFailure : Reason::VideoInvalidPCM;
    }
    return VideoAttachResult{_outcome,FlcPlayer::AudioLease::AttachReceipt()};
}
LoadingAudioHandoff::VideoDetachResult LoadingAudioHandoff::detachVideo(ResourceMutationPermit &permit,
    const Generation &g, FlcPlayer::AudioLease &lease) noexcept
{
    claimContinuation();
    const bool bound=permitBound(permit,g); permit.invalidate();
    if (!bound || lease.generation()!=g.id || lease.attachmentState()!=FlcPlayer::AudioLease::AttachmentState::Attached)
    {
        _outcome.disposition=Disposition::FailedRetained; _outcome.reason=bound ? Reason::InvalidLease : Reason::InvalidPermit;
        return VideoDetachResult{_outcome,FlcPlayer::AudioLease::DetachReceipt()};
    }
    // prepareKnown paused/drained/detached and halted mixer producers,
    // including external/UFO/Adlib routes which installed no FLC PCM hook.
    // Callback/resource borrowing ended; compatibility-owned copied PCM may remain.
    const FlcPlayer::AudioLease::DetachReceipt receipt(&lease,g.id,lease.route());
    lease.acceptDetach(receipt);
    _outcome.disposition=Disposition::VideoDetached; _outcome.reason=Reason::None;
    _outcome.lastCompletedPhase=Phase::VideoHookDetached;
    return VideoDetachResult{_outcome,receipt};
}
LoadingAudioHandoff::VideoAttachResult LoadingAudioHandoff::attachVideoClosed(ClosedProof &closed,
    const Generation &g, FlcPlayer::AudioLease &lease) noexcept
{
    claimContinuation();
    const bool bound=closed._handoff==this && closedBound(closed,g); closed.invalidate();
    if (!bound || lease.generation()!=g.id || lease.route()!=FlcPlayer::AudioLease::Route::Silent ||
        lease.attachmentState()!=FlcPlayer::AudioLease::AttachmentState::NeverAttached)
    {
        _outcome.disposition=Disposition::FailedRetained; _outcome.reason=bound ? Reason::InvalidLease : Reason::UnknownGeneration;
        return VideoAttachResult{_outcome,FlcPlayer::AudioLease::AttachReceipt()};
    }
    const FlcPlayer::AudioLease::AttachReceipt receipt(&lease,g.id,lease.route(),false);
    lease.acceptAttach(receipt);
    _outcome.disposition=Disposition::VideoAttached; _outcome.reason=Reason::None;
    _outcome.lastCompletedPhase=Phase::VideoHookAttached;
    return VideoAttachResult{_outcome,receipt};
}
LoadingAudioHandoff::VideoDetachResult LoadingAudioHandoff::detachVideoClosed(ClosedProof &closed,
    const Generation &g, FlcPlayer::AudioLease &lease) noexcept
{
    claimContinuation();
    const bool bound=closed._handoff==this && closedBound(closed,g); closed.invalidate();
    if (!bound || lease.generation()!=g.id || lease.route()!=FlcPlayer::AudioLease::Route::Silent ||
        lease.attachmentState()!=FlcPlayer::AudioLease::AttachmentState::Attached)
    {
        _outcome.disposition=Disposition::FailedRetained; _outcome.reason=bound ? Reason::InvalidLease : Reason::UnknownGeneration;
        return VideoDetachResult{_outcome,FlcPlayer::AudioLease::DetachReceipt()};
    }
    const FlcPlayer::AudioLease::DetachReceipt receipt(&lease,g.id,lease.route());
    lease.acceptDetach(receipt);
    _outcome.disposition=Disposition::VideoDetached; _outcome.reason=Reason::None;
    _outcome.lastCompletedPhase=Phase::VideoHookDetached;
    return VideoDetachResult{_outcome,receipt};
}
#endif
}
#endif
