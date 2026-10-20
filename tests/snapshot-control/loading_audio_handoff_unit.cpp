// SPDX-License-Identifier: GPL-3.0-or-later
// NEW RECONSTRUCTION, 2026-10-15. Authored from retained contracts and conversation
// material after executor loss. This is not byte-identical recovery of the lost
// fixture, and historical compilation/test acceptance does not cover these bytes.
//
// Link actual LoadingAudioHandoff.cpp, LoaderSession.cpp, LoadingUiPolicy.cpp,
// Options.cpp and their reviewed dependencies. Define
// OPENXCOM_LOADER_LIFETIME_TESTING consistently on session/policy/fixture objects
// only; shipping objects must omit it. There is no handoff testing dispatch,
// implementation inclusion, copied algorithm, or replacement session protocol.
//
// Intended ELF --wrap leaves: SDL_WasInit, SDL_AudioDriverName, SDL_PauseAudio,
// SDL_LockAudio, SDL_UnlockAudio, Mix_QuerySpec, Mix_SetPostMix, Mix_HookMusic,
// Mix_HaltChannel, Mix_HaltMusic. Forbidden-operation wraps: SDL_GetAudioStatus,
// SDL_Quit, SDL_QuitSubSystem, Mix_OpenAudio, Mix_CloseAudio, Mix_FreeChunk,
// Mix_FreeMusic, _ZN8OpenXcom5Music4stopEv, _ZN8OpenXcom5Sound4stopEv.
// Music-enabled builds also wrap _Z9func_mutev and link actual adlplayer.cpp and
// fmopl.cpp. That wrapper forwards to the real function with both OPL globals
// legitimately null. C++ linker names require fresh object-level confirmation.
//
// Observer-ON Attempts own real stable disabled observers. Standalone handoffs
// exercise Absent. Live observer protocol and source-faithful zero-open query
// behavior belong to the separate observer fixture. Here the query leaf writes
// synthetic configured spec fields even for nonpositive counts, intentionally
// testing initial zeroing and retained metadata rather than native closed-state
// behavior. All native leaves are conditional models, not real SDL evidence.
//
// Separately held callback, live permit, work epilogue, actual controller join
// and backend receipt establish distinct boundaries. Fixture waits/joins/yields
// are orchestration, never a proposed owner/UI blocking path. No sleeps are
// ordering oracles. Watchdog expiry, signal or an unexpected exit is a failure.
// No source, compiler, fixture, Game, audio device or provider run is authorized
// by this file. Real runtime/provider mapping, Game integration, final shutdown,
// codec safety and real-time/UI latency remain separate qualification gates.
#include "../../src/Engine/LoadingAudioHandoff.h"
#include "../../src/Engine/LoaderSession.h"
#include "../../src/Engine/LoadingUiPolicy.h"
#include "../../src/Engine/Options.h"
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
#include "../../src/Observer/MixedAudioObserver.h"
#endif
#include <SDL_mixer.h>
#ifndef OPENXCOM_LOADING_AUDIO_HANDOFF
#error "This fixture requires the selected loading-audio profile"
#endif
#ifndef OPENXCOM_LOADER_LIFETIME_TESTING
#error "Use the real session's existing nonshipping immutable hooks"
#endif
#ifndef __NO_MUSIC
#include "../../src/Engine/Adlib/adlplayer.h"
#include "../../src/Engine/Adlib/fmopl.h"
extern FM_OPL *opl[2];
#endif
#include <array>
#include <atomic>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <type_traits>
#include <utility>

namespace OpenXcom
{
// This friend only installs the existing immutable construction hooks. It does
// not inspect/replace session state, native handles, retirement or results.
struct LoaderSessionTestAccess
{
    typedef LoaderSession::Trace Trace;
    typedef bool (*FailureHook)(void *);
    static std::shared_ptr<LoaderSession> make(
        std::shared_ptr<LoaderThreadBackend> backend, LoaderSession::Work work,
        std::shared_ptr<void> context, void *hookContext,
        void (*trace)(void *, Trace), FailureHook detail, FailureHook copy)
    {
        const LoaderSession::Hooks hooks = {hookContext, trace, detail, copy};
        return std::shared_ptr<LoaderSession>(new LoaderSession(
            std::move(backend), work, std::move(context), hooks));
    }
};
}
namespace
{
using OpenXcom::LoadingAudioHandoff;
using OpenXcom::LoaderSession;
using OpenXcom::LoaderSessionTestAccess;
using OpenXcom::LoaderThreadBackend;
using OpenXcom::LoadingUiPolicy;
typedef LoadingAudioHandoff H;
typedef H::ResourceMutationPermit Permit;
static_assert(!std::is_copy_constructible<H>::value, "handoff cannot copy");
static_assert(!std::is_copy_assignable<H>::value, "handoff cannot copy assign");
static_assert(!std::is_move_constructible<H>::value, "handoff storage is stable");
static_assert(!std::is_move_assignable<H>::value, "handoff identity is stable");
static_assert(!std::is_default_constructible<Permit>::value, "no public permit mint");
static_assert(!std::is_constructible<Permit, const H *>::value, "private permit constructor");
static_assert(!std::is_constructible<Permit, H::Outcome>::value, "outcome cannot mint");
static_assert(!std::is_copy_constructible<Permit>::value, "permit is move-only");
static_assert(!std::is_copy_assignable<Permit>::value, "permit cannot duplicate");
static_assert(std::is_nothrow_move_constructible<Permit>::value, "fixed permit move");
static_assert(std::is_nothrow_move_assignable<Permit>::value, "fixed permit assignment");
static_assert(std::is_trivially_destructible<Permit>::value, "no permit cleanup");
static_assert(!std::is_convertible<Permit, bool>::value, "context-bound permit");
static_assert(!std::is_default_constructible<H::PrepareResult>::value, "no result mint");
static_assert(!std::is_copy_constructible<H::PrepareResult>::value, "move-only result");
static_assert(std::is_nothrow_move_constructible<H::PrepareResult>::value, "fixed result move");
static_assert(std::is_trivially_copyable<H::Outcome>::value, "fixed outcome");
static_assert(std::is_standard_layout<H::Outcome>::value, "no outcome ownership");
static_assert(noexcept(std::declval<H &>().prepare()), "noexcept native prelude");
static_assert(std::is_nothrow_destructible<H>::value, "inert handoff destruction");

enum class E
{
    WasInit, Driver, Query, Pause, LockEnter, LockAcquired, Unlock, Postmix,
    MusicHook, HaltChannels, HaltMusic, Adlib, PrepareReturned, PermitSpent,
    CallbackEntered, CallbackPulse, CallbackLeaving, CallbackJoined,
    ResourceDestroyed, PartialDestroyed, ResourceLoaded, StorageDestroyed,
    ObserverDestroyed, ContextDestroyed, BackendStart, WorkerDispatched,
    WorkReturned, EpilogueReleased, WorkerJoined, ReceiptPublished, BackendReaped,
    SessionTerminal, SessionLastAccess, SessionReaped, SessionDestroying,
    OwnerTick, OwnerPublish, Forbidden, Last
};
const char *name(E event) noexcept
{
    static const char *const names[] = {
        "WasInit", "Driver", "Query", "Pause", "LockEnter", "LockAcquired",
        "Unlock", "Postmix", "MusicHook", "HaltChannels", "HaltMusic", "Adlib",
        "PrepareReturned", "PermitSpent", "CallbackEntered", "CallbackPulse",
        "CallbackLeaving", "CallbackJoined", "ResourceDestroyed", "PartialDestroyed",
        "ResourceLoaded", "StorageDestroyed", "ObserverDestroyed", "ContextDestroyed",
        "BackendStart", "WorkerDispatched", "WorkReturned", "EpilogueReleased",
        "WorkerJoined", "ReceiptPublished", "BackendReaped", "SessionTerminal",
        "SessionLastAccess", "SessionReaped", "SessionDestroying", "OwnerTick",
        "OwnerPublish", "Forbidden"
    };
    static_assert(sizeof(names) / sizeof(names[0]) == static_cast<unsigned>(E::Last),
                  "trace names must match the event enumeration");
    return names[static_cast<unsigned>(event)];
}
// Fixed capacity, independently published slots. Concurrent diagnostics never
// inspect an unpublished slot or the worker-owned fixed Outcome.
class Ledger
{
public:
    enum { Capacity = 2048 };
    struct Slot
    {
        std::atomic<bool> ready;
        E event;
        std::thread::id thread;
        Slot() : ready(false), event(E::Forbidden) {}
    };
    Ledger() : _next(0) {}
    void add(E event) noexcept
    {
        const unsigned i = _next.fetch_add(1, std::memory_order_relaxed);
        if (i >= Capacity) std::_Exit(2);
        _slots[i].event = event;
        _slots[i].thread = std::this_thread::get_id();
        _slots[i].ready.store(true, std::memory_order_release);
    }
    unsigned count(E event) const noexcept
    {
        unsigned n = 0;
        const unsigned end = _next.load(std::memory_order_acquire);
        for (unsigned i = 0; i < end && i < Capacity; ++i)
            if (_slots[i].ready.load(std::memory_order_acquire) && _slots[i].event == event) ++n;
        return n;
    }
    unsigned first(E event) const noexcept
    {
        const unsigned end = _next.load(std::memory_order_acquire);
        for (unsigned i = 0; i < end && i < Capacity; ++i)
            if (_slots[i].ready.load(std::memory_order_acquire) && _slots[i].event == event) return i;
        return Capacity;
    }
    bool allOn(E event, std::thread::id id) const noexcept
    {
        const unsigned end = _next.load(std::memory_order_acquire);
        for (unsigned i = 0; i < end && i < Capacity; ++i)
            if (_slots[i].ready.load(std::memory_order_acquire) &&
                _slots[i].event == event && _slots[i].thread != id) return false;
        return true;
    }
    void dump() const noexcept
    {
        const unsigned end = _next.load(std::memory_order_acquire);
        for (unsigned i = 0; i < end && i < Capacity; ++i)
            if (_slots[i].ready.load(std::memory_order_acquire))
                std::fprintf(stderr, "AUDIO_TRACE %u %s\n", i, name(_slots[i].event));
    }
private:
    std::array<Slot, Capacity> _slots;
    std::atomic<unsigned> _next;
};
Ledger *failureLedger = nullptr;
[[noreturn]] void fail(const char *why) noexcept
{
    std::fprintf(stderr, "LOADING_AUDIO_HANDOFF_UNIT FAIL: %s\n", why);
    if (failureLedger) failureLedger->dump();
    std::fflush(stderr);
    // Never unwind an active session, callback or retained resources on failure.
    std::_Exit(1);
}
void require(bool condition, const char *why) noexcept { if (!condition) fail(why); }
class Gate
{
public:
    Gate() : _open(false) {}
    void release() noexcept { _open.store(true, std::memory_order_release); }
    bool open() const noexcept { return _open.load(std::memory_order_acquire); }
    void wait() const noexcept { while (!open()) std::this_thread::yield(); }
private:
    std::atomic<bool> _open;
};
enum class Driver { Dummy, Missing, Other, Empty, Unterminated, ForeignPointer, DummyPrefix };
struct Configuration
{
    bool initialized;
    Driver driver;
    int openCount, frequency, channels;
    Uint16 format;
    int haltChannelsResult, haltMusicResult;
    bool abortIfHalt, holdPause, holdHaltMusic, faultHaltChannels;
    Configuration() : initialized(true), driver(Driver::Dummy), openCount(1),
        frequency(48001), channels(2), format(MIX_DEFAULT_FORMAT),
        haltChannelsResult(0), haltMusicResult(0), abortIfHalt(false),
        holdPause(false), holdHaltMusic(false), faultHaltChannels(false) {}
};
struct World
{
    Ledger ledger;
    const Configuration config;
    const std::thread::id owner;
    std::mutex callbackBoundary;
    Gate lockEntered, pauseEntered, pauseRelease, haltMusicEntered, haltMusicRelease;
    std::atomic<bool> paused, postmixAttached, musicAttached;
    H *reenter;
    explicit World(Configuration c = Configuration()) : config(c),
        owner(std::this_thread::get_id()), paused(false), postmixAttached(true),
        musicAttached(true), reenter(nullptr) { failureLedger = &ledger; }
    ~World() noexcept { failureLedger = nullptr; }
};
World *world = nullptr;
thread_local unsigned outerAudioLockDepth = 0;
thread_local const H *resourcePermitScope = nullptr;
thread_local const Permit *resourcePermit = nullptr;
void workerLeaf(E event) noexcept
{
    require(world && std::this_thread::get_id() != world->owner, "mutation must be worker-only");
    require(outerAudioLockDepth == 0, "mixer or Adlib called inside outer drain lock");
    world->ledger.add(event);
}
[[noreturn]] void forbidden(const char *operation) noexcept
{
    if (world) world->ledger.add(E::Forbidden);
    fail(operation);
}
H::ObserverDisposition absentObserver() noexcept
{
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    return H::ObserverDisposition::Absent;
#else
    return H::ObserverDisposition::NotBuilt;
#endif
}
H::ObserverDisposition initialContextObserver() noexcept
{
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    return H::ObserverDisposition::NotReset;
#else
    return H::ObserverDisposition::NotBuilt;
#endif
}
H::ObserverDisposition retainedObserver() noexcept
{
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    return H::ObserverDisposition::DisabledRetained;
#else
    return H::ObserverDisposition::NotBuilt;
#endif
}
void noMutation(const Ledger &ledger) noexcept
{
    const E effects[] = {E::Pause, E::LockEnter, E::LockAcquired, E::Unlock,
        E::Postmix, E::MusicHook, E::HaltChannels, E::HaltMusic, E::Adlib,
        E::ResourceDestroyed, E::PartialDestroyed, E::ResourceLoaded, E::Forbidden};
    for (E event : effects) require(ledger.count(event) == 0, "rejection performed mutation");
}
void admittedOrder(const Ledger &ledger) noexcept
{
    const E order[] = {E::Query, E::Pause, E::LockEnter, E::LockAcquired, E::Unlock,
        E::Postmix, E::MusicHook, E::HaltChannels, E::HaltMusic,
#ifndef __NO_MUSIC
        E::Adlib,
#endif
        E::PrepareReturned};
    unsigned prior = 0; bool first = true;
    for (E event : order)
    {
        require(ledger.count(event) == 1, "admitted native count mismatch");
        const unsigned at = ledger.first(event);
        require(first || prior < at, "admitted native order mismatch");
        prior = at; first = false;
    }
#ifdef __NO_MUSIC
    require(ledger.count(E::Adlib) == 0, "no-music profile called Adlib");
#endif
    require(ledger.count(E::Forbidden) == 0, "forbidden admitted operation");
}
void exactSpec(const H::Outcome &outcome, const Configuration &c) noexcept
{
    require(outcome.actual.openCount == c.openCount && outcome.actual.frequency == c.frequency &&
        outcome.actual.format == c.format && outcome.actual.channels == c.channels,
        "actual query metadata was replaced or lost");
}
void ready(const H::Outcome &outcome, const Configuration &c,
           H::ObserverDisposition observer = absentObserver()) noexcept
{
    require(outcome.disposition == H::Disposition::ResourceReady &&
        outcome.reason == H::Reason::None &&
        outcome.lastCompletedPhase == H::Phase::ResourceReady, "fixed outcome not ready");
    require(outcome.observer == observer, "observer disposition mismatch");
    exactSpec(outcome, c);
}
struct Resource
{
    Ledger &ledger;
    const bool partial;
    Resource(Ledger &l, bool p = false) : ledger(l), partial(p) {}
    ~Resource() noexcept
    {
        require(resourcePermitScope && resourcePermit && resourcePermit->validFor(*resourcePermitScope),
                "resource destruction lacks the actual live permit");
        require(world->paused.load(std::memory_order_acquire), "resource destruction while unpaused");
        require(!world->postmixAttached.load(std::memory_order_acquire) &&
                !world->musicAttached.load(std::memory_order_acquire), "resource destroyed before hook detach");
        ledger.add(partial ? E::PartialDestroyed : E::ResourceDestroyed);
    }
};
struct Dependencies
{
    Ledger &ledger;
    std::unique_ptr<Resource> oldResource, partial;
    explicit Dependencies(Ledger &l) : ledger(l), oldResource(new Resource(l)) {}
    ~Dependencies() noexcept
    {
        require(!oldResource && !partial, "storage destruction implicitly cleaned audio resources");
        ledger.add(E::StorageDestroyed);
    }
};
class ModeledCallback
{
public:
    ModeledCallback(World &w, std::shared_ptr<Dependencies> dependencies) :
        _world(w), _dependencies(std::move(dependencies)), _pulse(0), _ack(0),
        _joined(false), _thread([this] { run(); }) {}
    ~ModeledCallback() noexcept { require(_joined, "callback was not explicitly retired"); }
    void waitEntered() const noexcept { _entered.wait(); }
    void pulse() noexcept
    {
        const unsigned next = _pulse.fetch_add(1, std::memory_order_release) + 1;
        while (_ack.load(std::memory_order_acquire) != next) std::this_thread::yield();
    }
    void releaseAndJoin() noexcept
    {
        _release.release();
        _thread.join(); // Harness orchestration only; no policy operation waits.
        _world.ledger.add(E::CallbackJoined);
        _joined = true;
        _dependencies.reset();
    }
private:
    void run() noexcept
    {
        std::unique_lock<std::mutex> lock(_world.callbackBoundary);
        _world.ledger.add(E::CallbackEntered);
        _entered.release();
        while (!_release.open())
        {
            const unsigned command = _pulse.load(std::memory_order_acquire);
            if (command != _ack.load(std::memory_order_relaxed))
            {
                require(bool(_dependencies->oldResource), "live callback resource was reclaimed");
                _world.ledger.add(E::CallbackPulse);
                _ack.store(command, std::memory_order_release);
            }
            std::this_thread::yield();
        }
        _world.ledger.add(E::CallbackLeaving);
    }
    World &_world;
    std::shared_ptr<Dependencies> _dependencies;
    Gate _entered, _release;
    std::atomic<unsigned> _pulse, _ack;
    bool _joined;
    std::thread _thread;
};
enum class BackendMode { Normal, NoThread, SetupFailure };
// One immutable binding and one actual worker. Only the independent controller
// owns/joins the worker thread; the owner sees a separately published receipt.
// There are no invented native tokens and no owner join/wait in start/poll.
class ReceiptBackend : public LoaderThreadBackend
{
public:
    Gate launch, workReturned, epilogue, joined, releaseReceipt, receiptPublished;
    ReceiptBackend(Ledger &l, BackendMode mode = BackendMode::Normal) :
        _ledger(l), _mode(mode), _command(false), _shutdown(false), _receipt(false),
        _started(false), _outstanding(false), _reaped(false), _entry(nullptr),
        _context(nullptr), _controller([this] { control(); }) {}
    ~ReceiptBackend() noexcept override
    {
        require(!_outstanding, "backend destroyed before checked reap");
        _shutdown.store(true, std::memory_order_release);
        _controller.join();
    }
    StartResult start(Entry entry, void *context) noexcept override
    {
        if (_started) return StartResult{StartKind::InvariantError, 90};
        _started = true;
        _ledger.add(E::BackendStart);
        if (_mode == BackendMode::NoThread) return StartResult{StartKind::NoThread, 11};
        _entry = entry; _context = context; _outstanding = true;
        _command.store(true, std::memory_order_release);
        return StartResult{StartKind::Started, 0};
    }
    ReapResult pollReap() noexcept override
    {
        if (!_outstanding || _reaped) return ReapResult{ReapKind::Error, 91};
        if (!_receipt.load(std::memory_order_acquire)) return ReapResult{ReapKind::Busy, 0};
        _outstanding = false; _reaped = true;
        _ledger.add(E::BackendReaped);
        return ReapResult{ReapKind::Reaped, 0};
    }
    void finishPhysicalWorker() noexcept { epilogue.release(); joined.wait(); }
    void publishReceipt() noexcept { releaseReceipt.release(); receiptPublished.wait(); }
private:
    void control() noexcept
    {
        while (!_command.load(std::memory_order_acquire))
        {
            if (_shutdown.load(std::memory_order_acquire)) return;
            std::this_thread::yield();
        }
        const Entry entry = _entry;
        void *const context = _context;
        try
        {
            std::thread worker([this, entry, context] {
                launch.wait();
                _ledger.add(E::WorkerDispatched);
                entry(context, _mode == BackendMode::SetupFailure ? 47 : 0);
                _ledger.add(E::WorkReturned);
                workReturned.release();
                epilogue.wait();
                _ledger.add(E::EpilogueReleased);
            });
            worker.join();
            _ledger.add(E::WorkerJoined);
            joined.release();
            releaseReceipt.wait();
            _ledger.add(E::ReceiptPublished);
            _receipt.store(true, std::memory_order_release);
            receiptPublished.release();
        }
        catch (...) { fail("fixture controller create/join failed"); }
    }
    Ledger &_ledger;
    const BackendMode _mode;
    std::atomic<bool> _command, _shutdown, _receipt;
    bool _started, _outstanding, _reaped;
    Entry _entry;
    void *_context;
    std::thread _controller;
};
enum class WorkMode { Replace, CancelAtCheckpoint, StandardThrow, UnknownThrow, AllocationThrow, Cleanup };
struct ResourceFailure : std::exception
{
    const char *what() const noexcept override
    { return "deterministic resource exception with diagnostic detail longer than the small-string buffer"; }
};
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
struct OwnedDisabledObserver
{
    // Destruction order is observer first, then the receipt. Both belong to the
    // real retained Attempt, rather than a borrowed alias or no-op shared_ptr.
    struct DestructionReceipt
    {
        Ledger &ledger;
        explicit DestructionReceipt(Ledger &l) : ledger(l) {}
        ~DestructionReceipt() noexcept
        {
            require(world && std::this_thread::get_id() == world->owner,
                    "observer storage destroyed on worker");
            ledger.add(E::ObserverDestroyed);
        }
    } destructionReceipt;
    OpenXcom::Observer::MixedAudioObserver observer;
    explicit OwnedDisabledObserver(Ledger &l) : destructionReceipt(l), observer(nullptr) {}
};
#endif
struct Attempt
{
    Ledger &ledger;
    const std::shared_ptr<Dependencies> dependencies;
    const WorkMode mode;
    const bool detailFailure;
    std::atomic<unsigned> copyFailures, calls;
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    OwnedDisabledObserver ownedObserver;
#endif
    H handoff;
    H::Outcome outcome; // Worker-owned record, read by owner only after checked reap.
    Gate entered, permitLive, releaseResourceWork;
    const bool holdResourceWork;
    Attempt(Ledger &l, std::shared_ptr<Dependencies> d, WorkMode m,
            bool failDetail = false, unsigned failCopy = 0, bool holdWork = false) :
        ledger(l), dependencies(std::move(d)), mode(m), detailFailure(failDetail),
        copyFailures(failCopy), calls(0),
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
        ownedObserver(l), handoff(&ownedObserver.observer),
#endif
        outcome{H::Disposition::Unattempted, H::Phase::OwnerPrepared, H::Reason::None,
                {0, 0, 0, 0}, initialContextObserver()}, holdResourceWork(holdWork)
    { handoff.sealForWorker(); }
    ~Attempt() noexcept { ledger.add(E::ContextDestroyed); }
    static LoaderSession::WorkResult work(void *opaque, const LoaderSession::StopToken &stop)
    {
        Attempt &a = *static_cast<Attempt *>(opaque);
        a.calls.fetch_add(1, std::memory_order_relaxed);
        a.entered.release();
        auto prepared = a.handoff.prepare();
        a.outcome = prepared.outcome;
        a.ledger.add(E::PrepareReturned);
        require(prepared.permit.validFor(a.handoff), "expected admitted context-bound permit");
        require(prepared.outcome.disposition == H::Disposition::ResourceReady, "permit without ready outcome");
        a.permitLive.release(); // Scheduling evidence only, never owner permission.
        if (a.holdResourceWork) a.releaseResourceWork.wait();
        if (stop.requested() && a.mode == WorkMode::CancelAtCheckpoint)
        {
            prepared.permit.invalidate();
            a.ledger.add(E::PermitSpent);
            return LoaderSession::WorkResult::Cancelled;
        }
        if (a.mode == WorkMode::StandardThrow || a.mode == WorkMode::UnknownThrow)
        {
            a.dependencies->partial.reset(new Resource(a.ledger, true));
            if (a.mode == WorkMode::StandardThrow) throw ResourceFailure();
            throw 17;
        }
        if (a.mode == WorkMode::AllocationThrow) throw std::bad_alloc();
        resourcePermitScope = &a.handoff;
        resourcePermit = &prepared.permit;
        a.dependencies->oldResource.reset();
        a.dependencies->partial.reset();
        resourcePermit = nullptr;
        resourcePermitScope = nullptr;
        if (a.mode == WorkMode::Replace) a.ledger.add(E::ResourceLoaded);
        prepared.permit.invalidate();
        a.ledger.add(E::PermitSpent);
        return LoaderSession::WorkResult::Success;
    }
    static void trace(void *opaque, LoaderSessionTestAccess::Trace event) noexcept
    {
        Attempt &a = *static_cast<Attempt *>(opaque);
        typedef LoaderSessionTestAccess::Trace T;
        if (event == T::WorkTerminal) a.ledger.add(E::SessionTerminal);
        if (event == T::LastCallbackAccess) a.ledger.add(E::SessionLastAccess);
        if (event == T::BackendReaped) a.ledger.add(E::SessionReaped);
        if (event == T::Destroying) a.ledger.add(E::SessionDestroying);
    }
    static bool failDetail(void *opaque) noexcept { return static_cast<Attempt *>(opaque)->detailFailure; }
    static bool failCopy(void *opaque) noexcept
    {
        auto &n = static_cast<Attempt *>(opaque)->copyFailures;
        unsigned old = n.load(std::memory_order_relaxed);
        while (old && !n.compare_exchange_weak(old, old - 1, std::memory_order_relaxed)) {}
        return old != 0;
    }
};
std::shared_ptr<LoaderSession> makeSession(std::shared_ptr<ReceiptBackend> backend,
                                         std::shared_ptr<Attempt> attempt)
{
    return LoaderSessionTestAccess::make(backend, &Attempt::work, attempt,
        attempt.get(), &Attempt::trace, &Attempt::failDetail, &Attempt::failCopy);
}
LoadingUiPolicy::Config policyConfig() noexcept
{
    LoadingUiPolicy::Config config;
    config.maximumEventsPerTick = 4;
    return config;
}
void reserveAndStart(LoadingUiPolicy &policy, LoaderSession::BeginResult expected) noexcept
{
    require(policy.reserveView(false) == LoadingUiPolicy::Admission::Accepted, "policy reserve failed");
    require(policy.startPrepared() == expected, "policy start disposition mismatch");
}
void boundedTicks(LoadingUiPolicy &policy, unsigned iterations = 8) noexcept
{
    const LoadingUiPolicy::EventValue batch[] = {
        LoadingUiPolicy::EventValue(LoadingUiPolicy::EventKind::Focus, 0, 0, 0),
        LoadingUiPolicy::EventValue(LoadingUiPolicy::EventKind::Minimize),
        LoadingUiPolicy::EventValue(LoadingUiPolicy::EventKind::SuppressedInput),
        LoadingUiPolicy::EventValue(LoadingUiPolicy::EventKind::Resize, 640, 400),
        LoadingUiPolicy::EventValue(LoadingUiPolicy::EventKind::Expose)
    };
    for (unsigned i = 0; i < iterations; ++i)
    {
        require(policy.remainingEventBudget() == 4, "event budget not reset");
        require(policy.recordBatch(batch, 5) == 4, "owner exceeded bounded event prefix");
        require(!policy.record(batch[4]), "exhausted budget accepted ordinary event");
        require(policy.tick() == LoadingUiPolicy::Decision::Wait, "owner published before receipt");
        world->ledger.add(E::OwnerTick);
    }
}
// Separate fixture-only attempt. This does not close SDL, establish final audio
// shutdown, or demonstrate an implemented Game fallback after a failed start.
void cleanup(std::shared_ptr<Dependencies> dependencies, Ledger &ledger)
{
    auto attempt = std::make_shared<Attempt>(ledger, dependencies, WorkMode::Cleanup);
    auto backend = std::make_shared<ReceiptBackend>(ledger);
    auto session = makeSession(backend, attempt);
    require(session->begin() == LoaderSession::BeginResult::Started, "cleanup start failed");
    backend->launch.release();
    backend->workReturned.wait();
    require(!session->quiescent(), "cleanup terminal was treated as retirement");
    backend->finishPhysicalWorker();
    require(session->poll() == LoaderSession::PollResult::Running, "join without receipt retired cleanup");
    backend->publishReceipt();
    require(session->poll() == LoaderSession::PollResult::Ready && session->quiescent(), "cleanup not checked reaped");
    LoaderSession::Result result;
    require(session->copyResult(result) && result.terminal == LoaderSession::Terminal::Success,
            "separate cleanup failed");
    ready(attempt->outcome, world->config, retainedObserver());
}

void ownerCaptureAndRejectionCases()
{
    struct SnapshotCase { bool initialized; Driver driver; H::Reason reason; };
    const SnapshotCase snapshots[] = {
        {false, Driver::Dummy, H::Reason::AudioSubsystemUnavailable},
        {true, Driver::Missing, H::Reason::DriverNotDummy},
        {true, Driver::Other, H::Reason::DriverNotDummy},
        {true, Driver::Empty, H::Reason::DriverNotDummy},
        {true, Driver::Unterminated, H::Reason::DriverNotDummy},
        {true, Driver::ForeignPointer, H::Reason::DriverNotDummy},
        {true, Driver::DummyPrefix, H::Reason::DriverNotDummy}
    };
    for (const auto &row : snapshots)
    {
        Configuration c; c.initialized = row.initialized; c.driver = row.driver;
        World w(c); world = &w;
        H handoff;
        require(w.ledger.count(E::WasInit) == 1 &&
                w.ledger.count(E::Driver) == (c.initialized ? 1u : 0u),
                "owner capture did not use only initialized audio-domain queries");
        require(w.ledger.count(E::Query) == 0, "owner capture queried mixer");
        noMutation(w.ledger);
        handoff.sealForWorker();
        H::Outcome outcome{};
        std::thread worker([&] {
            auto result = handoff.prepare(); outcome = result.outcome;
            require(!result.permit.validFor(handoff), "snapshot rejection minted permit");
        });
        worker.join();
        require(outcome.disposition == H::Disposition::RejectedRetained &&
                outcome.reason == row.reason &&
                outcome.lastCompletedPhase == H::Phase::WorkerReserved,
                "snapshot rejection phase/reason mismatch");
        require(outcome.actual.openCount == 0 && outcome.actual.frequency == 0 &&
                outcome.actual.format == 0 && outcome.actual.channels == 0,
                "unqueried snapshot acquired spec metadata");
        require(outcome.observer == absentObserver(), "snapshot observer disposition mismatch");
        require(w.ledger.count(E::Query) == 0, "rejected snapshot reached mixer query");
        require(w.ledger.allOn(E::WasInit, w.owner) && w.ledger.allOn(E::Driver, w.owner),
                "snapshot recaptured from worker");
        noMutation(w.ledger);
    }
    const int counts[] = {0, 2, 3, INT_MAX, -1};
    for (int count : counts)
    {
        Configuration c; c.openCount = count; c.abortIfHalt = true;
        OpenXcom::Options::mute = count == 0 || count > 2;
        World w(c); world = &w; H handoff; handoff.sealForWorker();
        H::Outcome outcome{};
        std::thread worker([&] {
            auto result = handoff.prepare(); outcome = result.outcome;
            require(!result.permit.validFor(handoff), "open-count rejection minted permit");
        });
        worker.join();
        const auto reason = count == 0 ? H::Reason::NoMixer :
            (count > 1 ? H::Reason::MultipleMixerOpens : H::Reason::InvalidMixerState);
        require(outcome.disposition == H::Disposition::RejectedRetained &&
                outcome.lastCompletedPhase == H::Phase::Preflight && outcome.reason == reason,
                "open-count rejection phase/reason mismatch");
        exactSpec(outcome, c);
        require(outcome.observer == absentObserver(), "open-count observer mismatch");
        require(w.ledger.count(E::Query) == 1, "open-count query count mismatch");
        noMutation(w.ledger);
    }
    for (unsigned i = 0; i != 6; ++i)
    {
        Configuration c; c.abortIfHalt = true;
        if (i == 0) c.frequency = 0;
        if (i == 1) c.frequency = -1;
        if (i == 2) c.channels = 0;
        if (i == 3) c.channels = 1;
        if (i == 4) c.channels = 4;
        if (i == 5) c.format = AUDIO_U8;
        World w(c); world = &w; H handoff; handoff.sealForWorker();
        H::Outcome outcome{};
        std::thread worker([&] {
            auto result = handoff.prepare(); outcome = result.outcome;
            require(!result.permit.validFor(handoff), "invalid spec minted permit");
        });
        worker.join();
        require(outcome.disposition == H::Disposition::RejectedRetained &&
                outcome.reason == H::Reason::InvalidMixerState &&
                outcome.lastCompletedPhase == H::Phase::Preflight,
                "invalid spec rejection mismatch");
        exactSpec(outcome, c);
        require(w.ledger.count(E::Query) == 1, "invalid spec query count mismatch");
        noMutation(w.ledger);
    }
}
void admittedAndPermitCases()
{
    for (unsigned i = 0; i != 4; ++i)
    {
        Configuration c; if (i & 2) c.format = AUDIO_S8;
        const bool muted = (i & 1) != 0;
        OpenXcom::Options::mute = muted;
        OpenXcom::Options::audioSampleRate = 12345;
        World w(c); world = &w;
        std::unique_ptr<H> owned(new H), otherOwned(new H);
        H &handoff = *owned, &other = *otherOwned;
        require(w.ledger.count(E::Query) == 0, "construction queried mutable mixer state");
        noMutation(w.ledger);
        handoff.sealForWorker();
        H::Outcome outcome{};
        std::thread worker([&] {
            auto prepared = handoff.prepare(); outcome = prepared.outcome;
            w.ledger.add(E::PrepareReturned);
            require(prepared.permit.validFor(handoff) && !prepared.permit.validFor(other),
                    "permit was not exact-context bound");
            Permit moved(std::move(prepared.permit));
            require(!prepared.permit.validFor(handoff) && moved.validFor(handoff),
                    "permit move constructor duplicated or lost permission");
            prepared.permit = std::move(moved);
            require(!moved.validFor(handoff) && prepared.permit.validFor(handoff),
                    "permit move assignment duplicated or lost permission");
            Permit &same = prepared.permit;
            same = std::move(same);
            require(same.validFor(handoff), "self-move destroyed live permit");
            same.invalidate(); same.invalidate();
            require(!same.validFor(handoff) && !same.validFor(other),
                    "invalidated permit retained permission");
            w.ledger.add(E::PermitSpent);
        });
        worker.join();
        owned.reset(); otherOwned.reset(); // Check destructor effects before count assertions.
        ready(outcome, c);
        admittedOrder(w.ledger);
        require(w.ledger.count(E::WasInit) == 2 && w.ledger.count(E::Driver) == 2 &&
                w.ledger.allOn(E::WasInit, w.owner) && w.ledger.allOn(E::Driver, w.owner),
                "worker recaptured owner facts");
        require(OpenXcom::Options::mute == muted &&
                OpenXcom::Options::audioSampleRate == 12345 &&
                outcome.actual.frequency != OpenXcom::Options::audioSampleRate,
                "mute shortcut or requested-spec substitution");
        require(w.paused.load() && !w.postmixAttached.load() && !w.musicAttached.load(),
                "admitted or destroyed helper restored playback");
    }
}
void partialFailureCases()
{
    for (unsigned music = 0; music != 2; ++music)
    {
        Configuration c;
        if (music) c.haltMusicResult = -1; else c.haltChannelsResult = 7;
        World w(c); world = &w; H handoff; handoff.sealForWorker();
        H::Outcome outcome{};
        std::thread worker([&] {
            auto prepared = handoff.prepare(); outcome = prepared.outcome;
            require(!prepared.permit.validFor(handoff), "partial halt failure minted permit");
        });
        worker.join();
        require(outcome.disposition == H::Disposition::FailedRetained &&
                outcome.reason == (music ? H::Reason::HaltMusicFailed : H::Reason::HaltChannelsFailed) &&
                outcome.lastCompletedPhase == (music ? H::Phase::ChannelsHalted : H::Phase::HooksDetached),
                "partial halt failure lost exact progress");
        exactSpec(outcome, c);
        const E prefix[] = {E::Query, E::Pause, E::LockEnter, E::LockAcquired, E::Unlock,
            E::Postmix, E::MusicHook, E::HaltChannels};
        unsigned previous = 0; bool first = true;
        for (E event : prefix)
        {
            require(w.ledger.count(event) == 1, "partial failure prefix count mismatch");
            require(first || previous < w.ledger.first(event), "partial failure prefix order mismatch");
            previous = w.ledger.first(event); first = false;
        }
        require(w.ledger.count(E::HaltMusic) == music &&
                (!music || previous < w.ledger.first(E::HaltMusic)),
                "music halt ran after channel failure or lost ordering");
        require(w.ledger.count(E::Adlib) == 0 && w.ledger.count(E::ResourceDestroyed) == 0 &&
                w.ledger.count(E::Forbidden) == 0, "partial failure advanced or cleaned resources");
        require(w.paused.load() && !w.postmixAttached.load() && !w.musicAttached.load(),
                "partial failure rolled back hooks or pause");
        require(outcome.observer == absentObserver(), "partial failure observer metadata mismatch");
    }
}
void heldCallbackCase(bool quitDuringDrain, bool earlyQuit)
{
    World w; world = &w;
    auto dependencies = std::make_shared<Dependencies>(w.ledger);
    ModeledCallback callback(w, dependencies); callback.waitEntered();
    auto attempt = std::make_shared<Attempt>(w.ledger, dependencies,
        quitDuringDrain ? WorkMode::CancelAtCheckpoint : WorkMode::Replace, false, 0, true);
    auto backend = std::make_shared<ReceiptBackend>(w.ledger);
    auto session = makeSession(backend, attempt);
    std::unique_ptr<LoadingUiPolicy> policyOwner(new LoadingUiPolicy(session, policyConfig()));
    LoadingUiPolicy &policy = *policyOwner;
    reserveAndStart(policy, LoaderSession::BeginResult::Started);
    if (earlyQuit) require(policy.record(LoadingUiPolicy::EventValue(LoadingUiPolicy::EventKind::Quit)),
                           "early Quit was not accepted");
    backend->launch.release();
    w.lockEntered.wait();
    require(w.ledger.count(E::LockAcquired) == 0 && w.ledger.count(E::Postmix) == 0 &&
            !session->terminalPublished(), "held callback did not hold actual drain boundary");
    require(w.ledger.count(E::ResourceDestroyed) == 0 && w.ledger.count(E::StorageDestroyed) == 0 &&
            w.ledger.count(E::ContextDestroyed) == 0 && w.ledger.count(E::ObserverDestroyed) == 0,
            "callback-held dependencies were destroyed");
    if (quitDuringDrain) require(policy.record(LoadingUiPolicy::EventValue(LoadingUiPolicy::EventKind::Quit)),
                                 "drain Quit was not accepted");
    require(policy.tick() == LoadingUiPolicy::Decision::Wait, "drain Quit retired active worker");
    boundedTicks(policy);
    callback.pulse();
    callback.releaseAndJoin();
    attempt->permitLive.wait();
    require(!session->quiescent() && !session->terminalPublished() &&
            w.ledger.count(E::ResourceDestroyed) == 0 &&
            w.ledger.count(E::ObserverDestroyed) == 0, "permit was confused with retirement");
    boundedTicks(policy);
    attempt->releaseResourceWork.release();
    backend->workReturned.wait();
    require(session->terminalPublished() && !session->quiescent() &&
            w.ledger.count(E::WorkerJoined) == 0, "terminal publication retired held epilogue");
    require(w.ledger.count(E::StorageDestroyed) == 0 && w.ledger.count(E::ContextDestroyed) == 0 &&
            w.ledger.count(E::ObserverDestroyed) == 0 && w.ledger.count(E::OwnerPublish) == 0,
            "terminal signal released context or owner publication");
    boundedTicks(policy);
    backend->finishPhysicalWorker();
    require(policy.tick() == LoadingUiPolicy::Decision::Wait && !session->quiescent(),
            "actual join without backend receipt retired session");
    require(w.ledger.count(E::ContextDestroyed) == 0 && w.ledger.count(E::ObserverDestroyed) == 0,
            "receipt-held observer/context was destroyed");
    backend->publishReceipt();
    const auto decision = policy.tick();
    require(session->quiescent(), "policy did not establish checked quiescence");
    ready(attempt->outcome, w.config, retainedObserver());
    admittedOrder(w.ledger);
    require(w.ledger.first(E::CallbackLeaving) < w.ledger.first(E::LockAcquired) &&
            w.ledger.first(E::WorkerJoined) < w.ledger.first(E::ReceiptPublished) &&
            w.ledger.first(E::ReceiptPublished) < w.ledger.first(E::SessionReaped),
            "distinct callback/join/receipt boundaries collapsed");
    require(w.ledger.count(E::ObserverDestroyed) == 0, "observer lost before owner release");
    if (quitDuringDrain)
    {
        require(decision == LoadingUiPolicy::Decision::FinishQuit && policy.view().closeRequested,
                "Quit did not suppress owner success");
        require(w.ledger.count(E::ResourceLoaded) == 0 && w.ledger.count(E::ResourceDestroyed) == 0,
                "cancellation mutated retained old resources");
        policyOwner.reset();
        LoaderSession::Result result;
        require(session->copyResult(result) && result.terminal == LoaderSession::Terminal::Cancelled,
                "cancelled worker result was lost");
        cleanup(dependencies, w.ledger);
    }
    else
    {
        require(decision == LoadingUiPolicy::Decision::PublishSuccess, "reaped success not reserved");
        require(w.ledger.first(E::HaltMusic) < w.ledger.first(E::ResourceDestroyed),
                "resource mutation preceded playback halt");
        w.ledger.add(E::OwnerPublish);
        require(w.ledger.first(E::PermitSpent) < w.ledger.first(E::OwnerPublish) &&
                w.ledger.first(E::SessionReaped) < w.ledger.first(E::OwnerPublish),
                "owner publication preceded spent permit and checked reap");
        require(policy.transitionCompleted(), "success transition was not completed");
    }
    policyOwner.reset(); session.reset(); attempt.reset();
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    require(w.ledger.count(E::ObserverDestroyed) == (quitDuringDrain ? 2u : 1u) &&
            w.ledger.first(E::SessionReaped) < w.ledger.first(E::ObserverDestroyed),
            "real observer destruction did not follow checked reap");
#else
    require(w.ledger.count(E::ObserverDestroyed) == 0, "observer-OFF acquired observer storage");
#endif
}
void heldNativeLeafCase()
{
    Configuration c; c.holdHaltMusic = true;
    World w(c); world = &w;
    auto dependencies = std::make_shared<Dependencies>(w.ledger);
    auto attempt = std::make_shared<Attempt>(w.ledger, dependencies, WorkMode::Cleanup);
    auto backend = std::make_shared<ReceiptBackend>(w.ledger);
    auto session = makeSession(backend, attempt);
    LoadingUiPolicy policy(session, policyConfig());
    reserveAndStart(policy, LoaderSession::BeginResult::Started);
    backend->launch.release(); w.haltMusicEntered.wait();
    require(!session->terminalPublished() && !session->quiescent() &&
            w.ledger.count(E::PrepareReturned) == 0 && w.ledger.count(E::Adlib) == 0 &&
            w.ledger.count(E::ResourceDestroyed) == 0, "held native halt advanced prelude");
    boundedTicks(policy);
    w.haltMusicRelease.release(); backend->workReturned.wait();
    backend->finishPhysicalWorker(); backend->publishReceipt();
    require(policy.tick() == LoadingUiPolicy::Decision::PublishSuccess && session->quiescent(),
            "released native leaf did not complete through checked reap");
    ready(attempt->outcome, c, retainedObserver());
    require(policy.transitionCompleted(), "native-leaf transition failed");
}
void exceptionCase(WorkMode mode, bool detailFailure, bool copyFailure)
{
    World w; world = &w;
    auto dependencies = std::make_shared<Dependencies>(w.ledger);
    auto attempt = std::make_shared<Attempt>(w.ledger, dependencies, mode,
                                           detailFailure, copyFailure ? 1u : 0u);
    std::weak_ptr<Attempt> retained(attempt);
    auto backend = std::make_shared<ReceiptBackend>(w.ledger);
    auto session = makeSession(backend, attempt);
    std::unique_ptr<LoadingUiPolicy> policy(new LoadingUiPolicy(session, policyConfig()));
    reserveAndStart(*policy, LoaderSession::BeginResult::Started);
    attempt.reset(); // Session, not this test's local shared_ptr, retains the context.
    backend->launch.release(); backend->workReturned.wait();
    require(!retained.expired() && session->terminalPublished() && !session->quiescent(),
            "exception lost complete context or retired epilogue");
    require(w.ledger.count(E::ResourceDestroyed) == 0 && w.ledger.count(E::PartialDestroyed) == 0 &&
            w.ledger.count(E::ContextDestroyed) == 0 && w.ledger.count(E::ObserverDestroyed) == 0 &&
            w.ledger.count(E::OwnerPublish) == 0, "exception reclaimed retained dependencies");
    boundedTicks(*policy);
    backend->finishPhysicalWorker();
    require(policy->tick() == LoadingUiPolicy::Decision::Wait, "exception join bypassed receipt");
    backend->publishReceipt();
    const auto decision = policy->tick();
    require(session->quiescent(), "exception was not checked reaped");
    attempt = retained.lock(); require(bool(attempt), "session failed strong context retention");
    ready(attempt->outcome, w.config, retainedObserver());
    const auto reason = mode == WorkMode::UnknownThrow ? LoaderSession::Reason::UnknownException :
        (detailFailure ? LoaderSession::Reason::ErrorDetailsUnavailable : LoaderSession::Reason::StandardException);
    if (copyFailure)
    {
        require(decision == LoadingUiPolicy::Decision::FinishQuit &&
                policy->view().fixedFailure == LoadingUiPolicy::FixedFailure::ResultCopy &&
                !policy->result(), "copy allocation failure did not close with fixed failure");
        policy.reset();
        LoaderSession::Result retry;
        require(session->copyResult(retry) && retry.terminal == LoaderSession::Terminal::Failure &&
                retry.reason == reason, "copy allocation failure consumed or corrupted session result");
    }
    else
    {
        const auto *result = policy->result();
        require(decision == LoadingUiPolicy::Decision::ShowFailure && result &&
                result->terminal == LoaderSession::Terminal::Failure && result->reason == reason &&
                result->detailsAvailable == (mode != WorkMode::UnknownThrow && !detailFailure),
                "resource exception/detail disposition mismatch");
    }
    require(w.ledger.count(E::PartialDestroyed) == 0 && w.ledger.count(E::ResourceLoaded) == 0,
            "exception implicitly destroyed partial resources or published success");
    cleanup(dependencies, w.ledger);
}
void copyAllocationAtomicityCase()
{
    World w; world = &w;
    auto dependencies = std::make_shared<Dependencies>(w.ledger);
    auto attempt = std::make_shared<Attempt>(w.ledger, dependencies, WorkMode::StandardThrow, false, 1);
    auto backend = std::make_shared<ReceiptBackend>(w.ledger);
    auto session = makeSession(backend, attempt);
    require(session->begin() == LoaderSession::BeginResult::Started, "atomic-copy start failed");
    backend->launch.release(); backend->workReturned.wait();
    LoaderSession::Result output;
    output.attempt = 99; output.terminal = LoaderSession::Terminal::Cancelled;
    output.reason = LoaderSession::Reason::NoThread; output.nativeError = 73;
    output.detailsAvailable = true; output.details = "caller-owned sentinel";
    require(!session->copyResult(output), "terminal alone exposed copyable result");
    backend->finishPhysicalWorker();
    require(session->poll() == LoaderSession::PollResult::Running, "join bypassed copy receipt");
    backend->publishReceipt();
    require(session->poll() == LoaderSession::PollResult::Ready && session->quiescent(),
            "atomic-copy session not reaped");
    ready(attempt->outcome, w.config, retainedObserver());
    bool threw = false;
    try { (void)session->copyResult(output); } catch (const std::bad_alloc &) { threw = true; }
    require(threw && output.attempt == 99 && output.terminal == LoaderSession::Terminal::Cancelled &&
            output.reason == LoaderSession::Reason::NoThread && output.nativeError == 73 &&
            output.detailsAvailable && output.details == "caller-owned sentinel",
            "copy failure modified caller output");
    require(w.ledger.count(E::ResourceDestroyed) == 0 && w.ledger.count(E::PartialDestroyed) == 0 &&
            w.ledger.count(E::OwnerPublish) == 0, "copy failure reclaimed or published resources");
    ready(attempt->outcome, w.config, retainedObserver());
    require(session->copyResult(output) && output.attempt == 1 &&
            output.terminal == LoaderSession::Terminal::Failure &&
            output.reason == LoaderSession::Reason::StandardException && output.detailsAvailable,
            "copy retry did not retrieve unconsumed result");
    require(!session->copyResult(output), "result consumed more than once");
    cleanup(dependencies, w.ledger);
}
enum class InitialFailure { Preparing, NoThread, SetupFailure };
void neverEnteredCase(InitialFailure first, BackendMode second)
{
    World w; world = &w;
    auto dependencies = std::make_shared<Dependencies>(w.ledger);
    ModeledCallback callback(w, dependencies); callback.waitEntered();
    auto attempt = std::make_shared<Attempt>(w.ledger, dependencies, WorkMode::Cleanup);
    auto backend = std::make_shared<ReceiptBackend>(w.ledger,
        first == InitialFailure::NoThread ? BackendMode::NoThread : BackendMode::SetupFailure);
    auto session = makeSession(backend, attempt);
    {
        LoadingUiPolicy policy(session, policyConfig());
        require(policy.reserveView(false) == LoadingUiPolicy::Admission::Accepted, "failure reserve failed");
        if (first == InitialFailure::Preparing) policy.preparationFailed();
        else
        {
            require(policy.startPrepared() == (first == InitialFailure::NoThread ?
                    LoaderSession::BeginResult::StartFailed : LoaderSession::BeginResult::Started),
                    "initial failure start mismatch");
            if (first == InitialFailure::SetupFailure)
            {
                backend->launch.release(); backend->workReturned.wait();
                boundedTicks(policy);
                backend->finishPhysicalWorker(); backend->publishReceipt();
            }
            require(policy.record(LoadingUiPolicy::EventValue(LoadingUiPolicy::EventKind::Quit)),
                    "initial failure Quit not accepted");
        }
        require(policy.tick() == LoadingUiPolicy::Decision::FinishQuit && session->quiescent(),
                "never-entered session not quiescent for retained owner pump");
        require(attempt->calls.load() == 0 && w.ledger.count(E::Query) == 0,
                "NoThread/setup/preparation entered audio helper");
        noMutation(w.ledger);
        callback.pulse();
        require(w.ledger.count(E::ResourceDestroyed) == 0 && w.ledger.count(E::StorageDestroyed) == 0 &&
                w.ledger.count(E::ObserverDestroyed) == 0, "never-entered session lost callback dependencies");
    }
    LoaderSession::Result initial;
    if (first == InitialFailure::Preparing)
        require(!session->copyResult(initial), "preparation failure invented worker result");
    else
        require(session->copyResult(initial) && initial.terminal == LoaderSession::Terminal::Failure &&
                initial.reason == (first == InitialFailure::NoThread ? LoaderSession::Reason::NoThread :
                                                                      LoaderSession::Reason::ThreadSetupFailure) &&
                initial.nativeError == (first == InitialFailure::NoThread ? 11 : 47),
                "never-entered reason/native error mismatch");

    // A new retained event-pump context does not borrow the first handoff. A
    // second failed admission still leaves the actual callback/resource alive.
    auto retry = std::make_shared<Attempt>(w.ledger, dependencies, WorkMode::Cleanup);
    auto retryBackend = std::make_shared<ReceiptBackend>(w.ledger, second);
    auto retrySession = makeSession(retryBackend, retry);
    {
        LoadingUiPolicy pump(retrySession, policyConfig());
        reserveAndStart(pump, second == BackendMode::NoThread ?
            LoaderSession::BeginResult::StartFailed : LoaderSession::BeginResult::Started);
        if (second == BackendMode::Normal)
        {
            retryBackend->launch.release(); w.lockEntered.wait();
            boundedTicks(pump); callback.pulse(); callback.releaseAndJoin();
            retryBackend->workReturned.wait();
            retryBackend->finishPhysicalWorker(); retryBackend->publishReceipt();
            require(pump.tick() == LoadingUiPolicy::Decision::PublishSuccess && retrySession->quiescent(),
                    "separate cleanup attempt did not reap");
            ready(retry->outcome, w.config, retainedObserver());
            require(pump.transitionCompleted(), "cleanup transition failed");
        }
        else
        {
            if (second == BackendMode::SetupFailure)
            {
                retryBackend->launch.release(); retryBackend->workReturned.wait();
                boundedTicks(pump);
                retryBackend->finishPhysicalWorker(); retryBackend->publishReceipt();
            }
            require(pump.record(LoadingUiPolicy::EventValue(LoadingUiPolicy::EventKind::Quit)),
                    "cleanup admission failure Quit not accepted");
            require(pump.tick() == LoadingUiPolicy::Decision::FinishQuit && retrySession->quiescent(),
                    "failed cleanup admission not quiescent");
            require(retry->calls.load() == 0 && w.ledger.count(E::Query) == 0,
                    "failed cleanup admission inferred audio quiescence");
            noMutation(w.ledger); callback.pulse();
            auto finalAttempt = std::make_shared<Attempt>(w.ledger, dependencies, WorkMode::Cleanup);
            auto finalBackend = std::make_shared<ReceiptBackend>(w.ledger);
            auto finalSession = makeSession(finalBackend, finalAttempt);
            require(finalSession->begin() == LoaderSession::BeginResult::Started, "final cleanup start failed");
            finalBackend->launch.release(); w.lockEntered.wait();
            require(!finalSession->quiescent() && w.ledger.count(E::ResourceDestroyed) == 0,
                    "final cleanup inferred drained callback");
            callback.releaseAndJoin();
            finalBackend->workReturned.wait();
            finalBackend->finishPhysicalWorker(); finalBackend->publishReceipt();
            require(finalSession->poll() == LoaderSession::PollResult::Ready && finalSession->quiescent(),
                    "final fixture cleanup not reaped");
            LoaderSession::Result result;
            require(finalSession->copyResult(result) && result.terminal == LoaderSession::Terminal::Success,
                    "final fixture cleanup failed");
            ready(finalAttempt->outcome, w.config, retainedObserver());
        }
    }
    if (second != BackendMode::Normal)
    {
        LoaderSession::Result failedCleanup;
        require(retrySession->copyResult(failedCleanup) &&
                failedCleanup.terminal == LoaderSession::Terminal::Failure &&
                failedCleanup.reason == (second == BackendMode::NoThread ?
                    LoaderSession::Reason::NoThread : LoaderSession::Reason::ThreadSetupFailure) &&
                failedCleanup.nativeError == (second == BackendMode::NoThread ? 11 : 47),
                "cleanup admission failure metadata lost");
    }
    require(w.ledger.count(E::ResourceDestroyed) == 1 && w.ledger.count(E::ResourceLoaded) == 0 &&
            w.ledger.count(E::OwnerPublish) == 0,
            "never-entered path published or duplicated resource cleanup");
}

std::array<unsigned, static_cast<unsigned>(E::Last)> expectedEffects{};
std::atomic<bool> expectedDeath(false);
// Only the intended violating thread may certify the fail-stop oracle. A
// thread-construction/join/orchestration failure must remain unexpected.
thread_local bool expectedTargetThreadArmed = false;
void expect(E event, unsigned count = 1) noexcept
{ expectedEffects[static_cast<unsigned>(event)] = count; }
[[noreturn]] void terminated() noexcept
{
    // Unarmed orchestration failures may arrive after stack unwinding. Never
    // inspect the possibly expired World/ledger on that path.
    if (!expectedTargetThreadArmed)
    {
        std::fprintf(stderr, "UNEXPECTED_TERMINATION exit=1\n");
        std::fflush(stderr);
        std::_Exit(1);
    }
    bool valid = expectedDeath.load(std::memory_order_acquire) && world;
    const E effects[] = {E::Query, E::Pause, E::LockEnter, E::LockAcquired, E::Unlock,
        E::Postmix, E::MusicHook, E::HaltChannels, E::HaltMusic, E::Adlib,
        E::PrepareReturned, E::ResourceDestroyed, E::PartialDestroyed, E::ResourceLoaded,
        E::ContextDestroyed, E::StorageDestroyed, E::ObserverDestroyed, E::Forbidden};
    if (valid)
        for (E event : effects)
            if (world->ledger.count(event) != expectedEffects[static_cast<unsigned>(event)]) valid = false;
    if (world) world->ledger.dump();
    std::fprintf(stderr, "%s exit=%d\n",
        valid ? "EXPECTED_AUDIO_FAIL_STOP" : "UNEXPECTED_TERMINATION", valid ? 86 : 1);
    std::fflush(stderr);
    std::_Exit(valid ? 86 : 1);
}
// These modes run as separate bounded subprocesses in a later authorized harness.
// None catches a native signal. The native-fault case is an explicit leaf
// std::terminate(), not evidence that the helper can recover a native crash.
void deathCase(const char *mode)
{
    Configuration c;
    c.holdPause = std::strcmp(mode, "--death-concurrent") == 0;
    c.faultHaltChannels = std::strcmp(mode, "--death-native-fault") == 0;
    World w(c); world = &w; H handoff;
    if (std::strcmp(mode, "--death-unsealed") != 0) handoff.sealForWorker();
    expectedDeath.store(true, std::memory_order_release);
    if (std::strcmp(mode, "--death-owner") == 0)
    {
        expectedTargetThreadArmed = true;
        (void)handoff.prepare();
        expectedTargetThreadArmed = false;
        fail("owner prepare returned");
    }
    if (std::strcmp(mode, "--death-wrong-seal") == 0)
    {
        std::thread worker([&] {
            expectedTargetThreadArmed = true; handoff.sealForWorker();
            expectedTargetThreadArmed = false;
        }); worker.join();
        fail("wrong-thread seal returned");
    }
    if (std::strcmp(mode, "--death-unsealed") == 0)
    {
        std::thread worker([&] {
            expectedTargetThreadArmed = true; (void)handoff.prepare();
            expectedTargetThreadArmed = false;
        }); worker.join();
        fail("unsealed prepare returned");
    }
    if (std::strcmp(mode, "--death-reentrant") == 0)
    {
        expect(E::Query); w.reenter = &handoff;
        std::thread worker([&] { (void)handoff.prepare(); }); worker.join();
        fail("reentrant prepare returned");
    }
    if (std::strcmp(mode, "--death-concurrent") == 0)
    {
        expect(E::Query); expect(E::Pause);
        std::thread first([&] { (void)handoff.prepare(); });
        w.pauseEntered.wait();
        std::thread second([&] {
            expectedTargetThreadArmed = true; (void)handoff.prepare();
            expectedTargetThreadArmed = false;
        });
        second.join(); first.join();
        fail("concurrent prepare returned");
    }
    const bool repeat = std::strcmp(mode, "--death-repeat") == 0;
    const bool nativeFault = std::strcmp(mode, "--death-native-fault") == 0;
    if (repeat || nativeFault)
    {
        const E prefix[] = {E::Query, E::Pause, E::LockEnter, E::LockAcquired, E::Unlock,
            E::Postmix, E::MusicHook, E::HaltChannels};
        for (E event : prefix) expect(event);
        if (repeat)
        {
            expect(E::HaltMusic); expect(E::PrepareReturned);
#ifndef __NO_MUSIC
            expect(E::Adlib);
#endif
        }
        std::thread worker([&] {
            auto prepared = handoff.prepare();
            w.ledger.add(E::PrepareReturned);
            require(prepared.permit.validFor(handoff), "first one-shot attempt failed");
            if (repeat) expectedTargetThreadArmed = true;
            (void)handoff.prepare();
            expectedTargetThreadArmed = false;
        });
        worker.join();
        fail("repeat/native-fault prepare returned");
    }
    fail("unknown death mode");
}
} // namespace

extern "C"
{
Uint32 SDLCALL __wrap_SDL_WasInit(Uint32 flags)
{
    require(world && std::this_thread::get_id() == world->owner && flags == SDL_INIT_AUDIO,
            "capture must query owner audio initialization only");
    world->ledger.add(E::WasInit);
    return world->config.initialized ? SDL_INIT_AUDIO : 0;
}
char *SDLCALL __wrap_SDL_AudioDriverName(char *buffer, int length)
{
    require(world && std::this_thread::get_id() == world->owner && buffer && length > 6,
            "driver capture contract mismatch");
    world->ledger.add(E::Driver);
    switch (world->config.driver)
    {
    case Driver::Missing: return nullptr;
    case Driver::ForeignPointer:
    {
        static char other[] = "dummy"; return other;
    }
    case Driver::Unterminated: std::memset(buffer, 'x', static_cast<std::size_t>(length)); break;
    case Driver::Empty: buffer[0] = '\0'; break;
    case Driver::Other: std::strcpy(buffer, "alsa"); break;
    case Driver::DummyPrefix:
        std::memset(buffer, 'x', static_cast<std::size_t>(length));
        std::memcpy(buffer, "dummy", 5); buffer[length - 1] = '\0'; break;
    case Driver::Dummy: std::strcpy(buffer, "dummy"); break;
    }
    return buffer;
}
int SDLCALL __wrap_Mix_QuerySpec(int *frequency, Uint16 *format, int *channels)
{
    workerLeaf(E::Query);
    require(frequency && format && channels && *frequency == 0 && *format == 0 && *channels == 0,
            "query did not receive zero-initialized fixed outputs");
    if (world->reenter)
    {
        expectedTargetThreadArmed = true;
        (void)world->reenter->prepare();
        expectedTargetThreadArmed = false;
        fail("nested reentrant prepare returned");
    }
    // Intentionally synthetic even for count <= 0; see the opening qualification.
    *frequency = world->config.frequency; *format = world->config.format;
    *channels = world->config.channels;
    return world->config.openCount;
}
void SDLCALL __wrap_SDL_PauseAudio(int pause)
{
    workerLeaf(E::Pause); require(pause == 1, "worker attempted resume");
    world->paused.store(true, std::memory_order_release);
    world->pauseEntered.release();
    if (world->config.holdPause) world->pauseRelease.wait();
}
void SDLCALL __wrap_SDL_LockAudio()
{
    workerLeaf(E::LockEnter);
    require(world->paused.load(std::memory_order_acquire), "drain did not follow pause");
    world->lockEntered.release();
    world->callbackBoundary.lock();
    ++outerAudioLockDepth;
    world->ledger.add(E::LockAcquired);
}
void SDLCALL __wrap_SDL_UnlockAudio()
{
    require(world && std::this_thread::get_id() != world->owner && outerAudioLockDepth == 1,
            "drain unlock identity/depth mismatch");
    --outerAudioLockDepth;
    world->callbackBoundary.unlock();
    world->ledger.add(E::Unlock);
}
void SDLCALL __wrap_Mix_SetPostMix(void (SDLCALL *callback)(void *, Uint8 *, int), void *context)
{
    workerLeaf(E::Postmix);
    require(!callback && !context && world->ledger.count(E::Unlock) != 0,
            "postmix detach did not follow empty drain");
    world->postmixAttached.store(false, std::memory_order_release);
}
void SDLCALL __wrap_Mix_HookMusic(void (SDLCALL *callback)(void *, Uint8 *, int), void *context)
{
    workerLeaf(E::MusicHook);
    require(!callback && !context && !world->postmixAttached.load(std::memory_order_acquire),
            "music detach did not follow postmix detach");
    world->musicAttached.store(false, std::memory_order_release);
}
int SDLCALL __wrap_Mix_HaltChannel(int channel)
{
    workerLeaf(E::HaltChannels);
    require(!world->config.abortIfHalt && channel == -1, "closed/rejected mixer was halted");
    if (world->config.faultHaltChannels)
    {
        expectedTargetThreadArmed = true;
        std::terminate(); // Noreturn: this exact native-fault leaf is the target.
    }
    return world->config.haltChannelsResult;
}
int SDLCALL __wrap_Mix_HaltMusic()
{
    workerLeaf(E::HaltMusic);
    require(!world->config.abortIfHalt, "closed/rejected music was halted");
    world->haltMusicEntered.release();
    if (world->config.holdHaltMusic) world->haltMusicRelease.wait();
    return world->config.haltMusicResult;
}
#ifndef __NO_MUSIC
void __real__Z9func_mutev();
void __wrap__Z9func_mutev()
{
    workerLeaf(E::Adlib);
    require(opl[0] == nullptr && opl[1] == nullptr, "fixture Adlib state is not legitimate both-null");
    __real__Z9func_mutev();
    require(!func_is_music_playing() && func_get_polyphony() == 0,
            "actual Adlib mute did not clear playback/polyphony");
}
#endif
SDL_audiostatus SDLCALL __wrap_SDL_GetAudioStatus() { forbidden("SDL_GetAudioStatus"); }
void SDLCALL __wrap_SDL_Quit() { forbidden("SDL_Quit"); }
void SDLCALL __wrap_SDL_QuitSubSystem(Uint32) { forbidden("SDL_QuitSubSystem"); }
int SDLCALL __wrap_Mix_OpenAudio(int, Uint16, int, int) { forbidden("Mix_OpenAudio"); }
void SDLCALL __wrap_Mix_CloseAudio() { forbidden("Mix_CloseAudio"); }
void SDLCALL __wrap_Mix_FreeChunk(Mix_Chunk *) { forbidden("Mix_FreeChunk"); }
void SDLCALL __wrap_Mix_FreeMusic(Mix_Music *) { forbidden("Mix_FreeMusic"); }
void __wrap__ZN8OpenXcom5Music4stopEv() { forbidden("Music::stop"); }
void __wrap__ZN8OpenXcom5Sound4stopEv() { forbidden("Sound::stop"); }
} // extern "C"

int main(int argc, char **argv)
{
    std::set_terminate(&terminated);
    if (argc == 2) { deathCase(argv[1]); return 1; }
    require(argc == 1, "expected no argument or one exact death mode");
    ownerCaptureAndRejectionCases();
    admittedAndPermitCases();
    partialFailureCases();
    heldCallbackCase(false, false);
    heldCallbackCase(true, false);
    heldCallbackCase(true, true);
    heldNativeLeafCase();
    exceptionCase(WorkMode::StandardThrow, false, false);
    exceptionCase(WorkMode::UnknownThrow, false, false);
    exceptionCase(WorkMode::AllocationThrow, false, false);
    exceptionCase(WorkMode::StandardThrow, true, false);
    exceptionCase(WorkMode::StandardThrow, false, true);
    exceptionCase(WorkMode::StandardThrow, true, true);
    copyAllocationAtomicityCase();
    const InitialFailure initial[] = {InitialFailure::Preparing, InitialFailure::NoThread,
                                      InitialFailure::SetupFailure};
    const BackendMode retry[] = {BackendMode::Normal, BackendMode::NoThread, BackendMode::SetupFailure};
    for (auto first : initial) for (auto second : retry) neverEnteredCase(first, second);
    world = nullptr;
    std::puts("LOADING_AUDIO_HANDOFF_UNIT PASS conditional_leaf_and_session_evidence_only");
    return 0;
}

