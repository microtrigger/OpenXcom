// SPDX-License-Identifier: GPL-3.0-or-later
// Policy-only fixture: link actual LoadingUiPolicy.cpp and LoaderSession.cpp.
// The accepted session fixture leaves below are byte-for-byte retained.
// No Game, SDL, window, native backend or latency evidence is claimed.
#include "../../src/Engine/LoadingUiPolicy.h"
#include <limits>
// SPDX-License-Identifier: GPL-3.0-or-later
// Link this translation unit with the actual src/Engine/LoaderSession.cpp,
// both with OPENXCOM_LOADER_LIFETIME_TESTING. No implementation is included.
// This is a conditional backend fixture, not a native backend or Game test.
// No command in this file supplies an execution authorization.
#include "../../src/Engine/LoaderSession.h"
#ifndef OPENXCOM_LOADER_LIFETIME_TESTING
#error "The lifetime fixture requires the nonshipping testing definition"
#endif
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>

namespace OpenXcom
{
// The friend only names hook constants and installs immutable construction
// hooks. It cannot read/write session state, tokens, results or retirement.
struct LoaderSessionTestAccess
{
    typedef LoaderSession::Trace Trace;
    typedef void (*TraceHook)(void *, Trace);
    typedef bool (*FailureHook)(void *);
    static std::unique_ptr<LoaderSession> make(
        std::shared_ptr<LoaderThreadBackend> backend, LoaderSession::Work work,
        std::shared_ptr<void> context, void *hookContext, TraceHook trace,
        FailureHook detail, FailureHook copy)
    {
        const LoaderSession::Hooks hooks = {hookContext, trace, detail, copy};
        return std::unique_ptr<LoaderSession>(new LoaderSession(
            std::move(backend), work, std::move(context), hooks));
    }
};
}

namespace
{
using OpenXcom::LoaderProcessLease;
using OpenXcom::LoaderSession;
using OpenXcom::LoaderSessionTestAccess;
using OpenXcom::LoaderThreadBackend;

// Preallocated, append-only ledger. A slot is read only after its independent
// release/acquire publication. Fatal-process dumps are safe with live writers;
// an unfinished slot is printed as such, never treated as a retirement receipt.
class Ledger
{
public:
    enum { Capacity = 8192 };
    struct Slot
    {
        std::atomic<bool> ready;
        const char *event;
        unsigned generation;
        unsigned token;
        Slot() : ready(false), event(nullptr), generation(0), token(0) {}
    };
    Ledger() : _next(0) {}
    void add(const char *event, unsigned generation = 0, unsigned token = 0) noexcept
    {
        const unsigned index = _next.fetch_add(1, std::memory_order_relaxed);
        if (index >= Capacity) std::_Exit(2);
        Slot &slot = _slots[index];
        slot.event = event;
        slot.generation = generation;
        slot.token = token;
        slot.ready.store(true, std::memory_order_release);
    }
    unsigned count(const char *event) const noexcept
    {
        unsigned result = 0;
        const unsigned size = _next.load(std::memory_order_acquire);
        for (unsigned i = 0; i < size && i < Capacity; ++i)
            if (_slots[i].ready.load(std::memory_order_acquire) &&
                std::strcmp(_slots[i].event, event) == 0) ++result;
        return result;
    }
    unsigned first(const char *event, unsigned generation) const noexcept
    {
        const unsigned size = _next.load(std::memory_order_acquire);
        for (unsigned i = 0; i < size && i < Capacity; ++i)
            if (_slots[i].ready.load(std::memory_order_acquire) &&
                _slots[i].generation == generation &&
                std::strcmp(_slots[i].event, event) == 0) return i;
        return Capacity;
    }
    bool complete() const noexcept
    {
        const unsigned size = _next.load(std::memory_order_acquire);
        if (size > Capacity) return false;
        for (unsigned i = 0; i < size; ++i)
            if (!_slots[i].ready.load(std::memory_order_acquire)) return false;
        return true;
    }
    void dump() const noexcept
    {
        const unsigned size = _next.load(std::memory_order_acquire);
        for (unsigned i = 0; i < size && i < Capacity; ++i)
        {
            const Slot &slot = _slots[i];
            if (slot.ready.load(std::memory_order_acquire))
                std::printf("LEDGER %u %s attempt=%u token=%u\n", i,
                            slot.event, slot.generation, slot.token);
            else std::printf("LEDGER %u UNFINISHED_SLOT\n", i);
        }
        std::fflush(stdout);
    }
private:
    std::array<Slot, Capacity> _slots;
    std::atomic<unsigned> _next;
};

Ledger *processLedger = nullptr;
std::atomic<bool> expectedFatal(false);
const char *expectedFatalMarker = nullptr;
unsigned expectedFatalStarts = 0;
unsigned checks = 0;

[[noreturn]] void fail(const char *message) noexcept
{
    std::fprintf(stderr, "LOADER_LIFETIME_UNIT FAIL: %s\n", message);
    if (processLedger) processLedger->dump();
    std::fflush(stderr);
    // Never unwind an unreaped session to turn a failing test into reclamation.
    std::_Exit(1);
}
void check(bool value, const char *message)
{
    ++checks;
    if (!value) fail(message);
}
[[noreturn]] void terminated() noexcept
{
    const bool expected = expectedFatal.load(std::memory_order_acquire);
    bool preserved = processLedger && processLedger->complete() &&
        processLedger->count("ContextDestroyed") == 0 &&
        processLedger->count("SessionDestroying") == 0 &&
        expectedFatalMarker && processLedger->count(expectedFatalMarker) == 1 &&
        processLedger->count("BackendStart") == expectedFatalStarts &&
        processLedger->count("BackendPoll") == 0;
    if (preserved && std::strcmp(expectedFatalMarker, "DeliberateDuplicateCallback") == 0)
        preserved = processLedger->count("CallbackDispatched") == 2 &&
            processLedger->count("WorkEntered") == 1 &&
            processLedger->count("WorkerEntry") == 1 &&
            processLedger->count("WorkTerminal") == 1 &&
            processLedger->count("JoinReturned") == 0;
    if (processLedger) processLedger->dump();
    std::fprintf(stderr, "%s exit=%d\n",
        expected && preserved ? "EXPECTED_FATAL_INVARIANT" : "UNEXPECTED_TERMINATION",
        expected && preserved ? 86 : 1);
    std::fflush(stderr);
    std::_Exit(expected && preserved ? 86 : 1);
}

// A one-shot, nonallocating fixture latch. There are no sleeps or duration
// assertions. Waiting is permitted only in fixture orchestration, work/epilogue,
// or the explicitly documented start-return scheduling hold, never owner poll.
class Latch
{
public:
    Latch() : _open(false) {}
    void release() noexcept { _open.store(true, std::memory_order_release); }
    bool open() const noexcept { return _open.load(std::memory_order_acquire); }
    void wait() const noexcept { while (!open()) std::this_thread::yield(); }
private:
    std::atomic<bool> _open;
};

enum class WorkMode { Success, CancelAtCheckpoint, IgnoreStop, StandardThrow, UnknownThrow };
struct WorkState
{
    Ledger &ledger;
    const WorkMode mode;
    const bool injectDetailFailure;
    std::atomic<unsigned> copyFailures;
    std::atomic<unsigned> calls;
    std::atomic<unsigned> generation;
    std::array<Latch, 4> bodyGate;
    std::array<Latch, 4> entered;
    std::array<Latch, 4> terminal;
    std::array<Latch, 4> lastAccess;
    std::atomic<unsigned> stopSeen;
    WorkState(Ledger &events, WorkMode behavior = WorkMode::Success,
              bool holdBody = false, bool failDetail = false, unsigned failCopy = 0) :
        ledger(events), mode(behavior), injectDetailFailure(failDetail),
        copyFailures(failCopy), calls(0), generation(0), stopSeen(0)
    {
        if (!holdBody) for (unsigned i = 0; i < bodyGate.size(); ++i) bodyGate[i].release();
    }
    ~WorkState() noexcept { ledger.add("ContextDestroyed"); }
    static LoaderSession::WorkResult work(void *opaque, const LoaderSession::StopToken &stop)
    {
        WorkState &state = *static_cast<WorkState *>(opaque);
        const unsigned index = state.calls.fetch_add(1, std::memory_order_relaxed);
        const unsigned generation = state.generation.load(std::memory_order_relaxed);
        if (index >= state.bodyGate.size()) fail("work plan exhausted");
        state.ledger.add("WorkEntered", generation);
        state.entered[index].release();
        state.bodyGate[index].wait();
        state.ledger.add("WorkGateReleased", generation);
        if (stop.requested())
        {
            state.stopSeen.fetch_add(1, std::memory_order_relaxed);
            state.ledger.add("StopObserved", generation);
            if (state.mode == WorkMode::CancelAtCheckpoint)
                return LoaderSession::WorkResult::Cancelled;
        }
        if (state.mode == WorkMode::StandardThrow)
            throw std::runtime_error("deterministic loader failure");
        if (state.mode == WorkMode::UnknownThrow) throw 17;
        return LoaderSession::WorkResult::Success;
    }
    static void trace(void *opaque, LoaderSessionTestAccess::Trace event) noexcept
    {
        WorkState &state = *static_cast<WorkState *>(opaque);
        typedef LoaderSessionTestAccess::Trace Trace;
        if (event == Trace::Reserved)
            state.generation.fetch_add(1, std::memory_order_relaxed);
        const unsigned generation = state.generation.load(std::memory_order_relaxed);
        const char *name = nullptr;
        switch (event)
        {
        case Trace::Reserved: name = "Reserved"; break;
        case Trace::WorkerEntry: name = "WorkerEntry"; break;
        case Trace::WorkTerminal: name = "WorkTerminal"; break;
        case Trace::LastCallbackAccess: name = "LastCallbackAccess"; break;
        case Trace::BackendReaped: name = "SessionObservedReaped"; break;
        case Trace::ResultCopied: name = "ResultCopied"; break;
        case Trace::ResultConsumed: name = "ResultConsumed"; break;
        case Trace::Destroying: name = "SessionDestroying"; break;
        }
        state.ledger.add(name, generation);
        if (generation > 0 && generation <= state.terminal.size())
        {
            if (event == Trace::WorkTerminal) state.terminal[generation - 1].release();
            if (event == Trace::LastCallbackAccess) state.lastAccess[generation - 1].release();
        }
    }
    static bool failDetail(void *opaque) noexcept
    {
        return static_cast<WorkState *>(opaque)->injectDetailFailure;
    }
    static bool failCopy(void *opaque) noexcept
    {
        WorkState &state = *static_cast<WorkState *>(opaque);
        unsigned remaining = state.copyFailures.load(std::memory_order_relaxed);
        while (remaining != 0)
            if (state.copyFailures.compare_exchange_weak(remaining, remaining - 1,
                    std::memory_order_relaxed)) return true;
        return false;
    }
};

enum class BackendMode
{
    Normal, NoThread, StartInvariant, StartedError, ReapError, BusyError,
    SkipCallback, DuplicateCallback, NoThreadAfterCallback, Reentrant, SetupFailure
};
struct ReentryTarget { LoaderSession *session; ReentryTarget() : session(nullptr) {} };

class ControlledBackend : public LoaderThreadBackend
{
public:
    struct Cycle
    {
        Latch startReturnGate;
        Latch workerLaunch;
        Latch callbackReturned;
        Latch epilogueEntered;
        Latch epilogue;
        std::atomic<bool> startReturned;
        unsigned token;
        Cycle() : startReturned(false), token(0) {}
    };
    ControlledBackend(Ledger &events, BackendMode first = BackendMode::Normal,
                      BackendMode second = BackendMode::Normal,
                      bool holdStartReturn = false, ReentryTarget *reentry = nullptr) :
        _ledger(events), _modes{{first, second, BackendMode::Normal, BackendMode::Normal}},
        _holdStartReturn(holdStartReturn), _reentry(reentry), _command(0), _receipt(0),
        _shutdown(false), _controllerFailed(false), _callbacks(0), _joins(0),
        _starts(0), _polls(0), _reaps(0), _returnedTokens(0), _retainedToken(0),
        _entry(nullptr), _context(nullptr), _finished(false),
        _controller(&ControlledBackend::control, this)
    {
        _ledger.add("ControllerSpawned");
    }
    ~ControlledBackend() noexcept override
    {
        finishController();
        _ledger.add("BackendDestroyed");
    }
    StartResult start(Entry entry, void *context) noexcept override
    {
        ++_starts;
        _ledger.add("BackendStart", _starts);
        if (_starts > _cycles.size() || _retainedToken != 0)
            return StartResult{StartKind::InvariantError, 90};
        const BackendMode mode = _modes[_starts - 1];
        if (mode == BackendMode::Reentrant)
        {
            if (!_reentry || !_reentry->session) fail("missing deliberate reentry target");
            _ledger.add("DeliberateReentrantPoll", _starts);
            _reentry->session->poll();
            fail("reentrant owner operation returned");
        }
        if (mode == BackendMode::NoThread)
        {
            _cycles[_starts - 1].startReturned.store(true, std::memory_order_release);
            _ledger.add("NoThreadReturned", _starts);
            return StartResult{StartKind::NoThread, 11};
        }
        // A token is backend-owned before the controller may launch the worker.
        // It is never an OS handle and never reaches a native thread API.
        Cycle &cycle = _cycles[_starts - 1];
        cycle.token = 1000 + _starts;
        _retainedToken = cycle.token;
        _entry = entry;
        _context = context;
        _ledger.add("TokenRetained", _starts, cycle.token);
        _command.store(_starts, std::memory_order_release);
        if (_holdStartReturn && _starts == 1)
        {
            // Scheduling seam only: an independent coordinator releases this
            // gate. It models delayed return, not a permissible production wait.
            _ledger.add("StartReturnHeld", _starts, cycle.token);
            cycle.startReturnGate.wait();
        }
        ++_returnedTokens;
        _ledger.add("StartReturnedToken", _starts, cycle.token);
        cycle.startReturned.store(true, std::memory_order_release);
        if (mode == BackendMode::StartInvariant)
            return StartResult{StartKind::InvariantError, 21};
        if (mode == BackendMode::StartedError)
            return StartResult{StartKind::Started, 22};
        if (mode == BackendMode::NoThreadAfterCallback)
            return StartResult{StartKind::NoThread, 11};
        return StartResult{StartKind::Started, 0};
    }
    ReapResult pollReap() noexcept override
    {
        ++_polls;
        _ledger.add("BackendPoll", _starts, _retainedToken);
        const BackendMode mode = _modes[_starts - 1];
        if (mode == BackendMode::ReapError || _controllerFailed.load(std::memory_order_acquire))
            return ReapResult{ReapKind::Error, 31};
        if (mode == BackendMode::BusyError) return ReapResult{ReapKind::Busy, 32};
        // The owner reads a receipt; it never joins, waits, touches the worker's
        // thread object, or treats callback/terminal completion as retirement.
        if (_receipt.load(std::memory_order_acquire) != _starts)
            return ReapResult{ReapKind::Busy, 0};
        if (_retainedToken == 0) return ReapResult{ReapKind::Error, 33};
        ++_reaps;
        _ledger.add("BackendReapedReturned", _starts, _retainedToken);
        _retainedToken = 0;
        return ReapResult{ReapKind::Reaped, 0};
    }
    void waitReceipt(unsigned generation) const noexcept
    {
        while (_receipt.load(std::memory_order_acquire) != generation)
        {
            if (_controllerFailed.load(std::memory_order_acquire)) fail("fixture worker creation failed");
            std::this_thread::yield();
        }
    }
    void finishController() noexcept
    {
        if (_finished) return;
        if (_receipt.load(std::memory_order_acquire) != _command.load(std::memory_order_acquire))
            fail("attempt to finish controller before its actual worker join receipt");
        _shutdown.store(true, std::memory_order_release);
        _controller.join();
        _ledger.add("ControllerJoined");
        _finished = true;
    }
    Cycle &cycle(unsigned index) noexcept { return _cycles[index]; }
    unsigned starts() const noexcept { return _starts; }
    unsigned polls() const noexcept { return _polls; }
    unsigned reaps() const noexcept { return _reaps; }
    unsigned returnedTokens() const noexcept { return _returnedTokens; }
    unsigned retainedToken() const noexcept { return _retainedToken; }
    unsigned callbacks() const noexcept { return _callbacks.load(std::memory_order_acquire); }
    unsigned joins() const noexcept { return _joins.load(std::memory_order_acquire); }
private:
    void control() noexcept
    {
        unsigned completed = 0;
        while (!_shutdown.load(std::memory_order_acquire))
        {
            const unsigned command = _command.load(std::memory_order_acquire);
            if (command == completed) { std::this_thread::yield(); continue; }
            const unsigned index = command - 1;
            const Entry entry = _entry;
            void *const context = _context;
            const BackendMode mode = _modes[index];
            try
            {
                std::thread worker([this, command, index, entry, context, mode]() {
                    _cycles[index].workerLaunch.wait();
                    if (mode != BackendMode::SkipCallback)
                    {
                        _callbacks.fetch_add(1, std::memory_order_release);
                        _ledger.add("CallbackDispatched", command, _cycles[index].token);
                        // Conditional session coverage only: no native setup
                        // call was made or observed by this controlled backend.
                        entry(context, mode == BackendMode::SetupFailure ? 47 : 0);
                        _ledger.add("CallbackReturned", command, _cycles[index].token);
                        _cycles[index].callbackReturned.release();
                    }
                    else _ledger.add("DeliberatelySkippedCallback", command, _cycles[index].token);
                    _ledger.add("EpilogueGate", command, _cycles[index].token);
                    _cycles[index].epilogueEntered.release();
                    _cycles[index].epilogue.wait();
                    _ledger.add("EpilogueReleased", command, _cycles[index].token);
                    if (mode == BackendMode::DuplicateCallback)
                    {
                        _callbacks.fetch_add(1, std::memory_order_release);
                        _ledger.add("CallbackDispatched", command, _cycles[index].token);
                        _ledger.add("DeliberateDuplicateCallback", command, _cycles[index].token);
                        entry(context, 0);
                        fail("duplicate callback returned");
                    }
                });
                _ledger.add("WorkerSpawned", command, _cycles[index].token);
                _cycles[index].workerLaunch.release();
                worker.join(); // Only the independent controller joins a worker.
                _joins.fetch_add(1, std::memory_order_release);
                _ledger.add("JoinReturned", command, _cycles[index].token);
                // Ledger publication precedes receipt publication, so a receipt
                // acquire also establishes the complete join evidence.
                _ledger.add("ReceiptPublished", command, _cycles[index].token);
                _receipt.store(command, std::memory_order_release);
            }
            catch (...)
            {
                _ledger.add("ControllerFailure", command);
                _controllerFailed.store(true, std::memory_order_release);
                return;
            }
            completed = command;
        }
        _ledger.add("ControllerReturned");
    }
    Ledger &_ledger;
    const std::array<BackendMode, 4> _modes;
    const bool _holdStartReturn;
    ReentryTarget *const _reentry;
    std::array<Cycle, 4> _cycles;
    std::atomic<unsigned> _command;
    std::atomic<unsigned> _receipt;
    std::atomic<bool> _shutdown;
    std::atomic<bool> _controllerFailed;
    std::atomic<unsigned> _callbacks;
    std::atomic<unsigned> _joins;
    unsigned _starts;
    unsigned _polls;
    unsigned _reaps;
    unsigned _returnedTokens;
    unsigned _retainedToken;
    Entry _entry;
    void *_context;
    bool _finished;
    std::thread _controller;
};

std::unique_ptr<LoaderSession> makeSession(const std::shared_ptr<ControlledBackend> &backend,
                                         const std::shared_ptr<WorkState> &work)
{
    return LoaderSessionTestAccess::make(backend, &WorkState::work, work,
        work.get(), &WorkState::trace, &WorkState::failDetail, &WorkState::failCopy);
}
// END ACCEPTED SESSION FIXTURE LEAVES. No policy implementation is copied.
using OpenXcom::LoadingUiPolicy;
typedef LoadingUiPolicy Policy;
typedef Policy::EventValue Event;
typedef Policy::EventKind Kind;
typedef Policy::Decision Decision;
typedef Policy::Admission Admission;
typedef LoaderSession::BeginResult Begin;

struct Fixture
{
    std::shared_ptr<ControlledBackend> backend;
    std::shared_ptr<WorkState> work;
    std::shared_ptr<LoaderSession> session;
    Fixture(Ledger &ledger, BackendMode backendMode = BackendMode::Normal,
            WorkMode workMode = WorkMode::Success, bool heldBody = false,
            bool detailFailure = false, unsigned copyFailures = 0,
            bool early = false) :
        backend(new ControlledBackend(ledger, backendMode, BackendMode::Normal, early)),
        work(new WorkState(ledger, workMode, heldBody, detailFailure, copyFailures)),
        session(makeSession(backend, work)) {}
    void retire(unsigned index = 0)
    {
        work->bodyGate[index].release();
        backend->cycle(index).epilogue.release();
        backend->waitReceipt(index + 1);
    }
};
void start(Policy &policy)
{
    check(policy.reserveView(false) == Admission::Accepted, "reserve is accepted");
    check(policy.startPrepared() == Begin::Started, "real session start accepted");
}
void heldTicks(Policy &policy, Fixture &f, Ledger &ledger, unsigned count, bool closing)
{
    const unsigned before = f.backend->polls();
    const unsigned copied = ledger.count("ResultCopied");
    for (unsigned i = 0; i < count; ++i)
    {
        const Event events[] = {Event(Kind::Resize, 640 + static_cast<int>(i), 480),
            Event(Kind::Focus, 0, 0, 0), Event(Kind::Minimize),
            Event(Kind::SuppressedInput), Event(Kind::Expose)};
        check(policy.recordBatch(events, 5) == 5, "bounded event prefix consumed");
        check(policy.tick() == Decision::Wait, "held body/epilogue has no result decision");
        check(policy.reserveView(false) == (closing ? Admission::Closed : Admission::Busy),
              "held policy rejects admission without another begin");
        check(policy.result() == nullptr && !f.session->quiescent(), "no detail access before actual retirement");
        check(policy.view().focus == 0, "minimized/unfocused policy still ticks");
        ledger.add("ProductionPolicyTick", 1);
        if (policy.takeRepaint()) ledger.add("RepaintIntentConsumed", 1);
    }
    check(f.backend->polls() == before + count, "exactly one real backend poll per held tick");
    check(ledger.count("ResultCopied") == copied, "held ticks never copy result details");
}

void constructionAndPreparation(Ledger &ledger)
{
    ledger.add("CASE_ConstructionPreparation");
    Fixture f(ledger);
    Policy::Config config;
    config.maximumEventsPerTick = 0;
    bool rejected = false;
    try { Policy invalid(f.session, config); }
    catch (const std::invalid_argument &) { rejected = true; }
    check(rejected && f.backend->starts() == 0, "invalid bounded config has no begin");
    Policy policy(f.session, Policy::Config());
    check(policy.tick() == Decision::Wait, "Idle poll is a nonterminal wait");
    check(policy.reserveView(true) == Admission::LiveSave, "live-save intent rejected before preparation");
    check(policy.view().phase == Policy::Phase::Idle && f.session->attempt() == 0,
          "live-save rejection preserves idle and session attempt");
    check(policy.reserveView(false) == Admission::Accepted, "fresh preparing reservation");
    check(policy.reserveView(false) == Admission::Busy && policy.reserveView(true) == Admission::Busy,
          "Preparing rejects repeated admissions before any caller preparation");
    check(policy.tick() == Decision::Wait && f.backend->starts() == 0, "Preparing tick cannot begin");
    // Synthetic caller allocation failure, not a claim of actual Game fault injection.
    try { throw std::bad_alloc(); }
    catch (const std::bad_alloc &) { policy.preparationFailed(); }
    check(policy.startPrepared() == Begin::Closed, "failed preparation cannot fall through to begin");
    check(policy.tick() == Decision::FinishQuit && policy.result() == nullptr,
          "failed Preparing closes without requiring nonexistent result");
    check(policy.view().fixedFailure == Policy::FixedFailure::Preparation &&
          f.backend->starts() == 0 && f.work->calls.load() == 0, "allocation failure has explicit fixed disposition");
}
void closeBeforeBegin(Ledger &ledger, bool preparing)
{
    ledger.add(preparing ? "CASE_ClosePreparing" : "CASE_CloseIdle");
    Fixture f(ledger);
    Policy policy(f.session, Policy::Config());
    if (preparing) check(policy.reserveView(false) == Admission::Accepted, "reserve before close");
    check(policy.record(Event(Kind::Quit)), "quit intent immediately accepted");
    check(policy.startPrepared() == Begin::Closed && f.session->admissionClosed(), "close is immediate and permanent");
    check(policy.tick() == Decision::FinishQuit && policy.tick() == Decision::FinishQuit,
          "never-started Closed finishes idempotently");
    check(policy.result() == nullptr && f.backend->starts() == 0 && f.backend->polls() == 0,
          "Closed without result is not a copy failure");
}
void eventIntents(Ledger &ledger)
{
    ledger.add("CASE_EventIntents");
    Fixture f(ledger);
    Policy::Config config;
    config.maximumEventsPerTick = 4;
    config.maximumWidth = 1024; config.maximumHeight = 768;
    Policy policy(f.session, config);
    check(policy.reserveView(false) == Admission::Accepted, "event-only preparing reservation");
    policy.takeRepaint();
    check(policy.record(Event(Kind::StartupResize, 999, 999)) && !policy.view().resizePending,
          "exactly first tagged startup resize suppressed");
    const Event initial[] = {Event(Kind::Resize, 800, 600),
        Event(Kind::StartupResize, std::numeric_limits<int>::max(), std::numeric_limits<int>::min()),
        Event(Kind::Focus, 0, 0, 0xff), Event(Kind::Resize, 900, 700)};
    check(policy.recordBatch(initial, 4) == 3 && policy.remainingEventBudget() == 0,
          "only remaining bounded prefix is consumed");
    check(!policy.record(Event(Kind::Resize, 900, 700)), "unconsumed event remains caller-owned after budget");
    Policy::View view = policy.view();
    check(view.resizePending && view.width == 1024 && view.height == 200 && view.focus == 7,
          "last admitted dimensions clamped and focus mask bounded");
    check(policy.tick() == Decision::Wait && policy.remainingEventBudget() == 4, "tick resets bounded budget");
    check(policy.recordBatch(initial + 3, 1) == 1 && policy.view().width == 900 && policy.view().height == 700,
          "deferred event consumed next iteration and latest resize wins");
    policy.record(Event(Kind::Minimize));
    check(policy.view().focus == 3, "minimize clears active bit only");
    policy.record(Event(Kind::Restore));
    check(policy.view().focus == 7, "restore preserves latest focus bits");
    policy.record(Event(Kind::SuppressedInput));
    check(policy.tick() == Decision::Wait, "suppressed ordinary input has no dispatch decision");
    policy.takeRepaint();
    policy.record(Event(Kind::KeyDown));
    check(!policy.view().closeRequested && !policy.takeRepaint(), "ordinary key is consumed without replay or close");
    policy.record(Event(Kind::Expose));
    check(policy.takeRepaint() && !policy.takeRepaint(), "expose produces one bounded repaint intent");
    policy.record(Event(Kind::Focus, 0, 0, 0));
    policy.record(Event(Kind::SuppressedInput));
    check(policy.remainingEventBudget() == 0 && policy.record(Event(Kind::Quit)), "direct quit survives budget exhaustion");
    check(f.session->admissionClosed() && policy.tick() == Decision::FinishQuit, "event close never needs a result");
    check(f.backend->starts() == 0, "intent tests do not launch work");
}
void disabledResize(Ledger &ledger)
{
    ledger.add("CASE_DisabledResize");
    Fixture f(ledger);
    Policy::Config config; config.allowResize = false;
    Policy policy(f.session, config);
    policy.record(Event(Kind::StartupResize, 800, 600));
    policy.record(Event(Kind::Resize, 900, 700));
    check(!policy.view().resizePending, "disabled resize is consumed without stored mutation");
    policy.record(Event(Kind::Quit));
    check(policy.tick() == Decision::FinishQuit, "disabled-resize idle closes");
}
void earlySuccessAndReuse(Ledger &ledger)
{
    ledger.add("CASE_EarlySuccessReuse");
    Fixture f(ledger, BackendMode::Normal, WorkMode::Success, false, false, 0, true);
    Policy policy(f.session, Policy::Config());
    check(policy.reserveView(false) == Admission::Accepted, "early completion reservation");
    std::atomic<bool> early(false);
    std::thread coordinator([&f, &ledger, &early]() {
        f.backend->cycle(0).callbackReturned.wait();
        early.store(!f.backend->cycle(0).startReturned.load(std::memory_order_acquire), std::memory_order_release);
        ledger.add("CallbackBeforeStartReturn", 1);
        f.backend->cycle(0).startReturnGate.release();
        ledger.add("CoordinatorReturned");
    });
    ledger.add("CoordinatorSpawned");
    check(policy.startPrepared() == Begin::Started, "early callback cannot erase ownership");
    coordinator.join(); ledger.add("CoordinatorJoined");
    check(early.load() && f.session->terminalPublished() && !f.session->quiescent(),
          "callback completed before begin returned but actual epilogue is retained");
    check(policy.startPrepared() == Begin::Busy, "repeated start never calls begin again");
    heldTicks(policy, f, ledger, 32, false);
    f.retire();
    check(policy.tick() == Decision::PublishSuccess && policy.result() &&
          policy.result()->terminal == LoaderSession::Terminal::Success, "success copied after real join and reap");
    check(f.session->poll() == LoaderSession::PollResult::Idle, "copy consumed the real session result");
    const unsigned copied = ledger.count("ResultCopied");
    check(policy.tick() == Decision::PublishSuccess && ledger.count("ResultCopied") == copied,
          "reserved transition does not copy again");
    check(policy.reserveView(false) == Admission::Busy && policy.startPrepared() == Begin::Busy,
          "consumed successful session remains UI-reserved until transition completes");
    check(policy.transitionCompleted() && !policy.transitionCompleted(), "exactly one successful transition acknowledgment");
    check(policy.reserveView(false) == Admission::Accepted && policy.result() == nullptr,
          "exactly one fresh admission after navigation; stale result hidden");
    policy.record(Event(Kind::StartupResize, 720, 540));
    // The first tagged startup resize was not delivered in the first attempt.
    check(!policy.view().resizePending, "startup suppression is tied to first tagged event, not attempt");
    check(policy.startPrepared() == Begin::Started && f.session->attempt() == 2, "second real attempt starts");
    f.retire(1);
    check(policy.tick() == Decision::PublishSuccess && policy.result()->attempt == 2, "second independently retired result");
    check(f.backend->starts() == 2 && f.backend->joins() == 2 && f.backend->reaps() == 2,
          "two attempts use two actual join receipts");
    policy.record(Event(Kind::Quit));
    check(!policy.transitionCompleted() && policy.tick() == Decision::FinishQuit,
          "quit after consumed success finishes Closed without another result");
}
void heldClose(Ledger &ledger, WorkMode mode, bool renderingFailure)
{
    ledger.add(renderingFailure ? "CASE_RenderFailureHeld" :
               (mode == WorkMode::IgnoreStop ? "CASE_IgnoreStopHeld" : "CASE_CancelHeld"));
    Fixture f(ledger, BackendMode::Normal, mode, true);
    Policy policy(f.session, Policy::Config());
    start(policy); f.work->entered[0].wait();
    if (renderingFailure) policy.renderingFailed();
    else policy.record(Event(Kind::Quit));
    check(f.session->admissionClosed(), "close immediately reaches real stop token");
    heldTicks(policy, f, ledger, 32, true);
    if (renderingFailure)
        check(!policy.view().renderAllowed && !policy.takeRepaint() &&
              policy.view().fixedFailure == Policy::FixedFailure::Rendering,
              "render failure keeps event-only closing without repaint revival");
    f.work->bodyGate[0].release();
    f.backend->cycle(0).callbackReturned.wait();
    check(f.work->stopSeen.load() == 1, "real work observed policy close");
    heldTicks(policy, f, ledger, 16, true);
    f.retire();
    const unsigned copied = ledger.count("ResultCopied");
    check(policy.tick() == Decision::FinishQuit && policy.result() == nullptr &&
          ledger.count("ResultCopied") == copied, "closing retires without diagnostic allocation or success");
}
void quitBeatsReady(Ledger &ledger)
{
    ledger.add("CASE_QuitBeatsReady");
    Fixture f(ledger);
    Policy policy(f.session, Policy::Config()); start(policy); f.retire();
    const unsigned copied = ledger.count("ResultCopied");
    policy.record(Event(Kind::Quit));
    check(policy.tick() == Decision::FinishQuit && policy.result() == nullptr &&
          ledger.count("ResultCopied") == copied, "observed quit dominates available same-tick success");
}
void failedResult(Ledger &ledger, BackendMode backendMode, WorkMode workMode,
                  bool failDetail = false, bool failCopy = false)
{
    ledger.add(backendMode == BackendMode::NoThread ? "CASE_NoThread" :
        backendMode == BackendMode::SetupFailure ? "CASE_SetupFailure" :
        failCopy ? "CASE_CopyFailure" : failDetail ? "CASE_DetailFailure" :
        workMode == WorkMode::UnknownThrow ? "CASE_UnknownFailure" : "CASE_StandardFailure");
    Fixture f(ledger, backendMode, workMode, false, failDetail, failCopy ? 1 : 0);
    Policy policy(f.session, Policy::Config());
    check(policy.reserveView(false) == Admission::Accepted, "failure reservation");
    const Begin begun = policy.startPrepared();
    check(begun == (backendMode == BackendMode::NoThread ? Begin::StartFailed : Begin::Started),
          "actual begin disposition propagated");
    if (backendMode != BackendMode::NoThread) f.retire();
    const unsigned copied = ledger.count("ResultCopied");
    if (failCopy)
    {
        check(policy.tick() == Decision::FinishQuit && policy.result() == nullptr &&
              policy.view().fixedFailure == Policy::FixedFailure::ResultCopy,
              "copy allocation failure closes with fixed reason after actual retirement");
        check(ledger.count("ResultCopied") == copied && policy.tick() == Decision::FinishQuit &&
              f.work->copyFailures.load() == 0, "failed copy neither consumes nor retries diagnostics");
        check(policy.reserveView(false) == Admission::Closed, "copy failure never implicitly reopens admission");
        return;
    }
    check(policy.tick() == Decision::ShowFailure && policy.result() &&
          policy.result()->terminal == LoaderSession::Terminal::Failure, "failed result is terminal-owned failure only");
    const LoaderSession::Reason expected = backendMode == BackendMode::NoThread ? LoaderSession::Reason::NoThread :
        backendMode == BackendMode::SetupFailure ? LoaderSession::Reason::ThreadSetupFailure :
        failDetail ? LoaderSession::Reason::ErrorDetailsUnavailable :
        workMode == WorkMode::UnknownThrow ? LoaderSession::Reason::UnknownException : LoaderSession::Reason::StandardException;
    check(policy.result()->reason == expected, "real failure reason retained");
    if (backendMode == BackendMode::NoThread || backendMode == BackendMode::SetupFailure)
        check(f.work->calls.load() == 0, "NoThread/setup failure never runs work");
    if (backendMode == BackendMode::NoThread) check(f.backend->polls() == 0, "NoThread needs no backend reap");
    if (backendMode == BackendMode::SetupFailure) check(policy.result()->nativeError == 47, "setup diagnostics copied only after reap");
    check(policy.tick() == Decision::ShowFailure && policy.reserveView(false) == Admission::Busy,
          "failure display holds admission without implicit new session");
    policy.record(Event(Kind::KeyDown));
    check(policy.tick() == Decision::FinishQuit, "error-screen key closes without partial-resource access");
}
void cancelledResult(Ledger &ledger)
{
    ledger.add("CASE_CancelledResult");
    // Work returns Cancelled independently of a UI quit; this is a work leaf.
    struct CancelledWork
    {
        static LoaderSession::WorkResult run(void *, const LoaderSession::StopToken &)
        { return LoaderSession::WorkResult::Cancelled; }
    };
    std::shared_ptr<ControlledBackend> backend(new ControlledBackend(ledger));
    std::shared_ptr<WorkState> work(new WorkState(ledger));
    std::shared_ptr<LoaderSession> session(LoaderSessionTestAccess::make(backend, &CancelledWork::run,
        work, work.get(), &WorkState::trace, &WorkState::failDetail, &WorkState::failCopy));
    Policy policy(session, Policy::Config()); start(policy);
    backend->cycle(0).epilogue.release(); backend->waitReceipt(1);
    check(policy.tick() == Decision::ShowFailure && policy.result()->terminal == LoaderSession::Terminal::Cancelled,
          "Cancelled never becomes success navigation");
    policy.record(Event(Kind::KeyDown)); check(policy.tick() == Decision::FinishQuit, "cancelled error closes");
}
void transitionFailure(Ledger &ledger)
{
    ledger.add("CASE_TransitionFailure");
    Fixture f(ledger); Policy policy(f.session, Policy::Config());
    start(policy); f.retire();
    check(policy.tick() == Decision::PublishSuccess, "success reserved before failed navigation");
    try { throw std::bad_alloc(); }
    catch (...) { policy.transitionFailed(); }
    check(!policy.transitionCompleted() && policy.reserveView(false) == Admission::Closed &&
          policy.tick() == Decision::FinishQuit && policy.view().fixedFailure == Policy::FixedFailure::Transition,
          "failed navigation closes consumed session without accidental readmission");
}
void unexpectedOwnerUse(Ledger &ledger, unsigned mode)
{
    ledger.add(mode == 0 ? "CASE_UnexpectedBusy" : mode == 1 ? "CASE_UnexpectedClosed" : "CASE_UnexpectedConsumed");
    Fixture f(ledger, BackendMode::Normal, WorkMode::Success, true);
    Policy policy(f.session, Policy::Config());
    check(policy.reserveView(false) == Admission::Accepted, "fault-adapter reservation");
    // Deliberately violate sole-owner-operation contract through the real API.
    // This tests defensive dispositions; it does not bless external callers.
    if (mode == 1)
    {
        f.session->requestClose();
        check(policy.startPrepared() == Begin::Closed && policy.tick() == Decision::FinishQuit,
              "unexpected actual Closed needs no result");
        return;
    }
    if (mode == 0)
    {
        check(f.session->begin() == Begin::Started, "external test-only begin");
        check(policy.startPrepared() == Begin::Busy, "actual Busy forwarded and closes without reset");
        heldTicks(policy, f, ledger, 4, true); f.retire();
        check(policy.tick() == Decision::FinishQuit && f.backend->starts() == 1,
              "unexpected Busy retains and retires the original real worker");
        return;
    }
    check(policy.startPrepared() == Begin::Started, "missing-result case starts"); f.retire();
    check(f.session->poll() == LoaderSession::PollResult::Ready, "external fault observes actual Ready");
    LoaderSession::Result consumed;
    check(f.session->copyResult(consumed), "external fault consumes real result");
    check(policy.tick() == Decision::FinishQuit && policy.view().fixedFailure == Policy::FixedFailure::MissingResult,
          "unexpected Idle after consumed result never invents success");
}
void ownedSessionLifetime(Ledger &ledger)
{
    ledger.add("CASE_OwnedSessionLifetime");
    Fixture f(ledger);
    std::weak_ptr<LoaderSession> weak = f.session;
    std::unique_ptr<Policy> policy(new Policy(f.session, Policy::Config()));
    f.session.reset();
    check(!weak.expired(), "policy strongly owns its actual session");
    policy->record(Event(Kind::Quit));
    check(policy->tick() == Decision::FinishQuit, "retained unused session closes");
    policy.reset();
    check(weak.expired(), "quiescent policy destruction releases session");
}
void normal(Ledger &ledger)
{
    constructionAndPreparation(ledger);
    closeBeforeBegin(ledger, false); closeBeforeBegin(ledger, true);
    eventIntents(ledger); disabledResize(ledger); earlySuccessAndReuse(ledger);
    heldClose(ledger, WorkMode::CancelAtCheckpoint, false);
    heldClose(ledger, WorkMode::IgnoreStop, false);
    heldClose(ledger, WorkMode::IgnoreStop, true);
    quitBeatsReady(ledger);
    failedResult(ledger, BackendMode::NoThread, WorkMode::Success);
    failedResult(ledger, BackendMode::SetupFailure, WorkMode::Success);
    failedResult(ledger, BackendMode::Normal, WorkMode::StandardThrow);
    failedResult(ledger, BackendMode::Normal, WorkMode::UnknownThrow);
    failedResult(ledger, BackendMode::Normal, WorkMode::StandardThrow, true);
    failedResult(ledger, BackendMode::Normal, WorkMode::StandardThrow, false, true);
    cancelledResult(ledger); transitionFailure(ledger);
    unexpectedOwnerUse(ledger, 0); unexpectedOwnerUse(ledger, 1); unexpectedOwnerUse(ledger, 2);
    ownedSessionLifetime(ledger);
}

struct PolicyReentry
{
    Ledger &ledger;
    Policy *policy;
    explicit PolicyReentry(Ledger &events) : ledger(events), policy(nullptr) {}
    static void trace(void *opaque, LoaderSessionTestAccess::Trace event) noexcept
    {
        PolicyReentry &target = *static_cast<PolicyReentry *>(opaque);
        if (event == LoaderSessionTestAccess::Trace::Reserved)
        {
            target.ledger.add("PolicyReentrantView");
            target.policy->view();
        }
    }
};
[[noreturn]] void retainedPolicy(Policy &policy, Fixture &f, Ledger &ledger)
{
    const unsigned polls = f.backend->polls();
    const unsigned token = f.backend->retainedToken();
    const unsigned copies = ledger.count("ResultCopied");
    for (unsigned i = 0; i < 32; ++i)
    {
        policy.record(Event(Kind::Resize, 640 + i, 480));
        policy.record(Event(Kind::Quit));
        check(policy.tick() == Decision::RetainQuarantine && policy.startPrepared() == Begin::Quarantined,
              "quarantine is retained, event-only and never retries session polling");
        check(policy.reserveView(false) == Admission::Closed && policy.result() == nullptr,
              "quarantined policy exposes no result or reuse");
        ledger.add("ProductionQuarantineTick", 1);
    }
    check(f.backend->polls() == polls && f.backend->retainedToken() == token &&
          ledger.count("ResultCopied") == copies && !f.session->quiescent(),
          "quarantine does not infer retirement or clear retained backend state");
    f.retire(); f.backend->finishController();
    check(ledger.count("ContextDestroyed") == 0 && ledger.count("SessionDestroying") == 0,
          "even independent fixture join does not normalize a quarantined session");
    ledger.add("RetainedPolicyProcessExit"); ledger.dump();
    std::fprintf(stderr, "EXPECTED_RETAINED_POLICY_QUARANTINE exit=87\n");
    std::fflush(stderr); std::_Exit(87);
}
void negative(const char *mode, Ledger &ledger)
{
    ledger.add(mode);
    std::printf("NEGATIVE_MODE %s\n", mode); std::fflush(stdout);
    const bool wrong = std::strcmp(mode, "--negative-wrong-owner") == 0;
    const bool reentry = std::strcmp(mode, "--negative-reentrant") == 0;
    const bool active = std::strcmp(mode, "--negative-destructor-active") == 0;
    const bool destroyedQuarantine = std::strcmp(mode, "--negative-destructor-quarantined") == 0;
    const bool startError = std::strcmp(mode, "--negative-quarantine-start") == 0;
    const bool reapError = std::strcmp(mode, "--negative-quarantine-reap") == 0;
    const bool invalidTerminal = std::strcmp(mode, "--negative-quarantine-invalid-terminal") == 0;
    if (!wrong && !reentry && !active && !destroyedQuarantine && !startError && !reapError && !invalidTerminal)
        fail("unknown policy negative mode");
    Fixture f(ledger, startError || destroyedQuarantine ? BackendMode::StartInvariant :
        reapError ? BackendMode::ReapError : invalidTerminal ? BackendMode::SkipCallback : BackendMode::Normal,
        WorkMode::Success, true);
    PolicyReentry target(ledger);
    if (reentry)
    {
        // Replace only a never-started session with immutable accepted trace leaf.
        // Its destruction would contaminate the fatal pre-destruction oracle, so
        // retain it too until this deliberately fail-stopped process exits.
        static std::shared_ptr<LoaderSession> unused;
        unused = f.session;
        f.session = std::shared_ptr<LoaderSession>(LoaderSessionTestAccess::make(f.backend, &WorkState::work,
            f.work, &target, &PolicyReentry::trace, nullptr, nullptr));
    }
    std::unique_ptr<Policy> policy(new Policy(f.session, Policy::Config())); target.policy = policy.get();
    expectedFatalStarts = wrong || reentry ? 0 : 1;
    expectedFatalMarker = wrong ? "PolicyForeignView" : reentry ? "PolicyReentrantView" :
        active ? "PolicyActiveDestruction" : "PolicyQuarantinedDestruction";
    expectedFatal.store(wrong || reentry || active || destroyedQuarantine, std::memory_order_release);
    if (wrong)
    {
        std::thread foreign([&policy, &ledger]() { ledger.add("PolicyForeignView"); policy->view(); });
        foreign.join(); fail("foreign policy operation returned");
    }
    check(policy->reserveView(false) == Admission::Accepted, "negative reservation");
    const Begin begun = policy->startPrepared();
    if (reentry) fail("reentrant policy operation returned");
    if (active || destroyedQuarantine)
    {
        check(begun == (active ? Begin::Started : Begin::Quarantined), "negative destruction disposition");
        f.work->entered[0].wait();
        ledger.add(expectedFatalMarker); policy.reset(); fail("unsafe policy destruction returned");
    }
    if (startError) check(begun == Begin::Quarantined, "ambiguous start quarantines production policy");
    else
    {
        check(begun == Begin::Started, "negative reap case starts");
        if (invalidTerminal) f.retire();
        check(policy->tick() == Decision::RetainQuarantine, "reap error or invalid publication quarantines");
    }
    retainedPolicy(*policy, f, ledger);
}
}

int main(int argc, char **argv)
{
    Ledger ledger; processLedger = &ledger; std::set_terminate(&terminated);
    if (argc > 2) return 2;
    if (argc == 2 && std::strcmp(argv[1], "--normal") != 0)
    {
        negative(argv[1], ledger); fail("negative policy mode returned");
    }
    try
    {
        std::thread owner([&ledger]() {
            try { normal(ledger); ledger.add("OwnerReturned"); }
            catch (...) { fail("unexpected policy suite exception"); }
        });
        ledger.add("OwnerSpawned"); owner.join(); ledger.add("OwnerJoined");
        check(ledger.count("WorkerSpawned") == ledger.count("JoinReturned") &&
              ledger.count("JoinReturned") == ledger.count("ReceiptPublished") &&
              ledger.count("ReceiptPublished") == ledger.count("BackendReapedReturned"),
              "all normal real fixture workers joined and receipts consumed");
        check(ledger.count("ControllerSpawned") == ledger.count("ControllerJoined") &&
              ledger.count("CoordinatorSpawned") == ledger.count("CoordinatorJoined") &&
              ledger.count("ControllerFailure") == 0, "all fixture orchestration finished without failure");
        check(ledger.count("SessionDestroying") == ledger.count("ContextDestroyed") &&
              ledger.count("ContextDestroyed") == ledger.count("BackendDestroyed"),
              "all normal owned contexts reclaimed after real session quiescence");
        check(ledger.count("OwnerReturned") == 1 && ledger.count("OwnerJoined") == 1 && ledger.complete(),
              "complete ledger and actual owner join precede PASS");
        ledger.dump();
        std::printf("LOADING_UI_POLICY_UNIT PASS %u checks; production policy only, no Game responsiveness claim\n", checks);
        return 0;
    }
    catch (...) { fail("policy fixture orchestration failed"); }
}
