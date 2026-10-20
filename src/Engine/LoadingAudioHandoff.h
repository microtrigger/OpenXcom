#pragma once
// Copyright 2026 OpenXcom contributors. GPL-3.0-or-later; see LICENSE.txt and LICENSE-NOTICES.md.
// New reconstruction from reviewed contracts; not byte-identical recovered source.
// Selected-profile audio prelude; responsive lifecycle continuations are default OFF.
#ifdef OPENXCOM_LOADING_AUDIO_HANDOFF
#include <atomic>
#include <thread>
#include <SDL.h>
#ifdef OPENXCOM_RESPONSIVE_LOADING
#include <cstdint>
#include "FlcPlayer.h"
#endif

namespace OpenXcom
{
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
namespace Observer { class MixedAudioObserver; }
#endif
/** Owner construction/sealing precedes worker existence. The complete attempt
 * context strongly owns this object, any observer subobject, old resources and
 * callback userdata; LoaderSession retains that context through checked reap.
 * Never supply a borrowed Game/StartState observer or a no-op shared_ptr alias.
 *
 * Admission additionally requires the reviewed Linux/glibc closed-world native
 * provider/callback profile and a live paired-or-both-null Adlib OPL state. A
 * driver name is not provider attestation. The owner gives up ALL audio,
 * observer, resource and Options access until the matching session is reaped.
 * Construction/sealing/destruction concurrent with a worker is not supported.
 *
 * This establishes resource mutation permission only, never loader retirement
 * or final audio shutdown. NoThread/setup failure establishes no audio fact.
 * Stuck/failed/quarantined attempts retain their complete context and event-only
 * owner pump; no destructor cleanup, timeout-as-retirement or owner drain.
 */
class LoadingAudioHandoff
{
public:
    enum class Disposition { Unattempted, RejectedRetained, FailedRetained, ResourceReady
#ifdef OPENXCOM_RESPONSIVE_LOADING
        , ClosedReady, AudioClosed, VideoAttached, VideoDetached
#endif
    };
    enum class Phase { OwnerPrepared, WorkerReserved, Preflight, PauseIssued,
        Drained, PostmixDetached, HooksDetached, ChannelsHalted, MusicHalted,
        PlaybackStopped, ResourceReady
#ifdef OPENXCOM_RESPONSIVE_LOADING
        , ClosedVerified, OpenIssued, FreshDrained, DeviceClosed, ObserverTerminalClosed,
        VideoHookAttached, VideoHookDetached
#endif
    };
    enum class Reason { None, AudioSubsystemUnavailable, DriverNotDummy, NoMixer,
        MultipleMixerOpens, InvalidMixerState, HaltChannelsFailed, HaltMusicFailed
#ifdef OPENXCOM_RESPONSIVE_LOADING
        , UnknownGeneration, InvalidAudioPlan, GenerationMismatch, NativeResourcesRemain,
        OpenFailedClosed, OpenFailedRetained, CloseUnverified, InvalidPermit, InvalidLease,
        CounterOverflow, VideoInvalidPCM, VideoAllocationFailure, VideoConversionFailure
#endif
    };
    // NotReset records pointer presence only, without inspecting retained state.
    enum class ObserverDisposition { NotBuilt, Absent, NotReset, BootstrapPreserved,
        EpochClosed, DisabledRetained };
    struct ActualMixerSpec { int openCount; int frequency; Uint16 format; int channels; };
    // Fixed POD, worker-owned until matching checked retirement. No readiness
    // flag or concurrent getter; allocating LoaderSession result details are
    // independent of this value retained by the work's owning attempt context.
    struct Outcome
    {
        Disposition disposition;
        Phase lastCompletedPhase;
        Reason reason;
        ActualMixerSpec actual;
        ObserverDisposition observer;
    };

#ifdef OPENXCOM_RESPONSIVE_LOADING
    enum class Provenance { NeverOpened, KnownSinglePaused, KnownSinglePublished,
        SilentClosedNoNativeResources, Invalidated };
    struct Generation
    {
        std::uint64_t id;
        Provenance provenance;
        ActualMixerSpec spec;
        Generation() noexcept : id(0), provenance(Provenance::NeverOpened), spec{0,0,0,0} {}
    };
    struct AudioPlan { int frequency; Uint16 format; int channels; int chunkSize; };
    // All proofs are move-only, operation/context-bound and passive. Neither
    // valid() nor a native close receipt proves the matching thread was reaped.
    class ClosedProof
    {
    public:
        ClosedProof() noexcept;
        ClosedProof(ClosedProof &&) noexcept;
        ClosedProof &operator=(ClosedProof &&) noexcept;
        bool valid() const noexcept;
        bool validFor(std::uint64_t generation) const noexcept;
        const Outcome &outcome() const noexcept { return _outcome; }
        ClosedProof(const ClosedProof &) = delete;
        ClosedProof &operator=(const ClosedProof &) = delete;
    private:
        friend class LoadingAudioHandoff;
        ClosedProof(const LoadingAudioHandoff *, const Generation *, const Outcome &, bool) noexcept;
        void invalidate() noexcept;
        const LoadingAudioHandoff *_handoff;
        const Generation *_generation;
        std::uint64_t _id;
        Outcome _outcome;
    };
    class CloseReceipt
    {
    public:
        CloseReceipt() noexcept;
        CloseReceipt(CloseReceipt &&) noexcept;
        CloseReceipt &operator=(CloseReceipt &&) noexcept;
        bool valid() const noexcept { return _valid; }
        bool validFor(std::uint64_t generation) const noexcept { return _valid && _id == generation; }
        const Outcome &outcome() const noexcept { return _outcome; }
        ClosedProof releaseClosedProof() noexcept;
        CloseReceipt(const CloseReceipt &) = delete;
        CloseReceipt &operator=(const CloseReceipt &) = delete;
    private:
        friend class LoadingAudioHandoff;
        CloseReceipt(const Outcome &, std::uint64_t, ClosedProof &&) noexcept;
        bool _valid;
        std::uint64_t _id;
        Outcome _outcome;
        ClosedProof _closed;
    };
    struct VideoAttachResult { Outcome outcome; FlcPlayer::AudioLease::AttachReceipt receipt; };
    struct VideoDetachResult { Outcome outcome; FlcPlayer::AudioLease::DetachReceipt receipt; };
#endif

    /** Scope is this exact one-shot handoff/context and continuously paused,
     * unchanged audio generation. Sole control-plane ownership establishes
     * that fact; there is no fabricated SDL generation counter. Spend/invalidate
     * before any later open/close/unpause/hook/playback or owner publication.
     * The destructor does NOTHING to audio, threads or resources.
     */
    class ResourceMutationPermit
    {
    public:
        ResourceMutationPermit(ResourceMutationPermit &&other) noexcept;
        ResourceMutationPermit &operator=(ResourceMutationPermit &&other) noexcept;
        ~ResourceMutationPermit() noexcept = default;
        bool validFor(const LoadingAudioHandoff &handoff) const noexcept;
        void invalidate() noexcept;
        ResourceMutationPermit(const ResourceMutationPermit &) = delete;
        ResourceMutationPermit &operator=(const ResourceMutationPermit &) = delete;
    private:
        friend class LoadingAudioHandoff;
        explicit ResourceMutationPermit(const LoadingAudioHandoff *handoff) noexcept;
        const LoadingAudioHandoff *_handoff;
#ifdef OPENXCOM_RESPONSIVE_LOADING
        const Generation *_generation;
        std::uint64_t _id;
#endif
    };
    struct PrepareResult
    {
        Outcome outcome;
        ResourceMutationPermit permit;
        PrepareResult(PrepareResult &&) noexcept = default;
        PrepareResult &operator=(PrepareResult &&) noexcept = default;
        PrepareResult(const PrepareResult &) = delete;
        PrepareResult &operator=(const PrepareResult &) = delete;
    private:
        friend class LoadingAudioHandoff;
        PrepareResult(const Outcome &, ResourceMutationPermit &&) noexcept;
    };

#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    explicit LoadingAudioHandoff(Observer::MixedAudioObserver *sameContextObserver = nullptr) noexcept;
#else
    LoadingAudioHandoff() noexcept;
#endif
    ~LoadingAudioHandoff() noexcept = default;
    void sealForWorker() noexcept;
    // Owner entry and EVERY duplicate/reentrant entry fail-stop before mutable
    // outcome/audio/observer access. Never a recoverable device rejection.
    PrepareResult prepare() noexcept;
#ifdef OPENXCOM_RESPONSIVE_LOADING
    PrepareResult prepareKnown(const Generation &, const AudioPlan &) noexcept;
    ClosedProof verifyClosed(const Generation &, bool noNativeResources) noexcept;
    PrepareResult openFresh(ClosedProof &, const AudioPlan &, Generation &) noexcept;
    CloseReceipt closePrepared(ResourceMutationPermit &, Generation &, bool resourcesEmpty,
                               bool terminalObserver) noexcept;
    CloseReceipt terminalCloseNeverOpened(ClosedProof &, Generation &) noexcept;
    VideoAttachResult attachVideo(ResourceMutationPermit &, const Generation &, FlcPlayer::AudioLease &) noexcept;
    VideoDetachResult detachVideo(ResourceMutationPermit &, const Generation &, FlcPlayer::AudioLease &) noexcept;
    VideoAttachResult attachVideoClosed(ClosedProof &, const Generation &, FlcPlayer::AudioLease &) noexcept;
    VideoDetachResult detachVideoClosed(ClosedProof &, const Generation &, FlcPlayer::AudioLease &) noexcept;
#endif
    LoadingAudioHandoff(const LoadingAudioHandoff &) = delete;
    LoadingAudioHandoff &operator=(const LoadingAudioHandoff &) = delete;
    LoadingAudioHandoff(LoadingAudioHandoff &&) = delete;
    LoadingAudioHandoff &operator=(LoadingAudioHandoff &&) = delete;
private:
    enum class AudioDomain { Initialized, Unavailable };
    enum class Driver { Dummy, Other, Unavailable };
    struct OwnerSnapshot { std::thread::id owner; AudioDomain domain; Driver driver; };
    static OwnerSnapshot captureOwner() noexcept;
    void claimEntry() noexcept;
    PrepareResult pauseDrainAndStop() noexcept;
#ifdef OPENXCOM_RESPONSIVE_LOADING
    void claimContinuation() noexcept;
    static bool validPlan(const AudioPlan &) noexcept;
    static bool exactSpec(const ActualMixerSpec &, const AudioPlan &) noexcept;
    static Outcome emptyOutcome() noexcept;
    bool knownGeneration(const Generation &) const noexcept;
    bool permitBound(const ResourceMutationPermit &, const Generation &) const noexcept;
    bool closedBound(const ClosedProof &, const Generation &) const noexcept;
    void queryActual() noexcept;
    void terminalObserverAfterDetach() noexcept;
    ClosedProof closedResult(const Generation &, bool) noexcept;
    const Generation *_boundGeneration = nullptr;
    std::uint64_t _boundId = 0;
    std::atomic_flag _continued = ATOMIC_FLAG_INIT;
    std::atomic_flag _entryComplete = ATOMIC_FLAG_INIT;
    std::thread::id _entryThread;
#endif
    PrepareResult result(Disposition disposition, Reason reason) noexcept;
    const OwnerSnapshot _snapshot;
    // C++11 atomic_flag is guaranteed lock-free, including the selected build.
    // This is a one-shot admission claim ONLY, never cleared/polled for drain,
    // result, resource access, retirement or shutdown. Lock-free is not a hard
    // elapsed-time bound and makes no whole-program real-time guarantee.
    std::atomic_flag _claimed = ATOMIC_FLAG_INIT;
    bool _sealed;
    Outcome _outcome;
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    Observer::MixedAudioObserver *const _observer;
#endif
};
}
#endif
