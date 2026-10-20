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
LoaderSession::Result sentinel()
{
    LoaderSession::Result result;
    result.attempt = 999;
    result.terminal = LoaderSession::Terminal::Cancelled;
    result.reason = LoaderSession::Reason::UnknownException;
    result.nativeError = 123;
    result.detailsAvailable = true;
    result.details = "unchanged caller output";
    return result;
}
bool isSentinel(const LoaderSession::Result &result)
{
    return result.attempt == 999 && result.terminal == LoaderSession::Terminal::Cancelled &&
        result.reason == LoaderSession::Reason::UnknownException && result.nativeError == 123 &&
        result.detailsAvailable && result.details == "unchanged caller output";
}
void heartbeat(LoaderSession &session, Ledger &ledger, unsigned generation, bool closing)
{
    unsigned ticks = 0;
    unsigned widthIntent = 0;
    bool focusIntent = false;
    const std::uint64_t attempt = session.attempt();
    LoaderSession::Result output = sentinel();
    for (unsigned i = 0; i < 64; ++i)
    {
        check(session.poll() == LoaderSession::PollResult::Running, "held worker/epilogue stays Running");
        check(session.begin() == (closing ? LoaderSession::BeginResult::Closed :
                                  LoaderSession::BeginResult::Busy), "active admission rejects begin");
        if (closing) session.requestClose();
        check(!session.copyResult(output) && isSentinel(output), "no result read before receipt");
        check(session.attempt() == attempt && !session.quiescent(), "Busy preserves attempt and storage");
        ++ticks;
        widthIntent = 640 + i;
        focusIntent = (i % 2) == 0;
        ledger.add("UiHeartbeat", generation);
        ledger.add("ResizeIntent", generation, widthIntent);
        ledger.add("FocusIntent", generation, focusIntent ? 1U : 0U);
    }
    check(ticks == 64 && widthIntent == 703 && !focusIntent, "fixed synthetic owner work completed");
}
void assertOrder(const Ledger &ledger, unsigned generation)
{
    const char *const sequence[] = {"Reserved", "WorkerEntry", "WorkTerminal", "LastCallbackAccess",
        "CallbackReturned", "EpilogueGate", "EpilogueReleased", "JoinReturned", "ReceiptPublished",
        "BackendReapedReturned", "SessionObservedReaped", "ResultCopied", "ResultConsumed"};
    unsigned previous = 0;
    for (unsigned i = 0; i < sizeof(sequence) / sizeof(sequence[0]); ++i)
    {
        const unsigned current = ledger.first(sequence[i], generation);
        check(current < Ledger::Capacity, "required lifecycle event is recorded");
        if (i != 0) check(previous < current, "publication, epilogue, join, reap, copy, consume order");
        previous = current;
    }
}

void emptyClosed(Ledger &ledger)
{
    ledger.add("CASE_EmptyClosed");
    std::shared_ptr<ControlledBackend> backend(new ControlledBackend(ledger));
    std::shared_ptr<WorkState> work(new WorkState(ledger));
    std::unique_ptr<LoaderSession> session = makeSession(backend, work);
    LoaderSession::Result output = sentinel();
    check(session->poll() == LoaderSession::PollResult::Idle, "empty poll is Idle");
    check(!session->copyResult(output) && isSentinel(output), "empty result absent");
    check(session->attempt() == 0 && session->quiescent(), "empty session has no attempt");
    session->requestClose();
    session->requestClose();
    check(session->admissionClosed() && session->poll() == LoaderSession::PollResult::Closed,
          "idempotent empty close");
    check(session->begin() == LoaderSession::BeginResult::Closed, "closed empty begin rejected");
    check(!session->copyResult(output) && isSentinel(output), "never-started closed result absent");
    check(backend->starts() == 0 && backend->polls() == 0 && work->calls.load() == 0,
          "empty and closed have zero backend/work calls");
}
struct UiToken
{
    Ledger &ledger;
    explicit UiToken(Ledger &events) : ledger(events) {}
    ~UiToken() noexcept { ledger.add("SyntheticUiDestroyed"); }
};

void earlyReuseAndLifetime(Ledger &ledger)
{
    ledger.add("CASE_EarlyReuseLifetime");
    std::shared_ptr<ControlledBackend> backend(new ControlledBackend(ledger,
        BackendMode::Normal, BackendMode::Normal, true));
    std::shared_ptr<WorkState> work(new WorkState(ledger));
    std::weak_ptr<ControlledBackend> backendLife = backend;
    std::weak_ptr<WorkState> workLife = work;
    ControlledBackend *const rawBackend = backend.get();
    WorkState *const rawWork = work.get();
    std::unique_ptr<LoaderSession> session = makeSession(backend, work);
    std::unique_ptr<UiToken> ui(new UiToken(ledger));
    std::atomic<bool> earlyObserved(false);
    std::thread coordinator([&ledger, rawBackend, &earlyObserved]() {
        rawBackend->cycle(0).callbackReturned.wait();
        earlyObserved.store(!rawBackend->cycle(0).startReturned.load(std::memory_order_acquire),
                            std::memory_order_release);
        ledger.add("CallbackBeforeStartReturn", 1);
        rawBackend->cycle(0).startReturnGate.release();
        ledger.add("CoordinatorReturned");
    });
    ledger.add("CoordinatorSpawned");
    check(session->begin() == LoaderSession::BeginResult::Started, "early callback start accepted");
    coordinator.join();
    ledger.add("CoordinatorJoined");
    check(earlyObserved.load(std::memory_order_acquire), "callback completed before start returned");
    check(session->terminalPublished() && !session->quiescent(), "terminal notification is not retirement");
    const unsigned firstToken = rawBackend->retainedToken();
    check(firstToken != 0 && rawBackend->returnedTokens() == 1, "actual returned token retained once");
    ui.reset();
    backend.reset();
    work.reset();
    check(!backendLife.expired() && !workLife.expired(), "session owns bindings after UI/external owner deletion");
    heartbeat(*session, ledger, 1, false);
    check(rawBackend->starts() == 1 && rawBackend->retainedToken() == firstToken && rawBackend->reaps() == 0,
          "Busy does not create, reset or consume a token");
    rawBackend->cycle(0).epilogue.release();
    rawBackend->waitReceipt(1);
    check(session->poll() == LoaderSession::PollResult::Ready, "Ready requires controller join receipt");
    check(session->begin() == LoaderSession::BeginResult::Busy, "Ready unconsumed blocks begin");
    LoaderSession::Result first;
    check(session->copyResult(first) && first.attempt == 1 &&
          first.terminal == LoaderSession::Terminal::Success, "first result consumed once");
    check(!session->copyResult(first) && session->poll() == LoaderSession::PollResult::Idle,
          "consumed result cannot repeat");
    check(session->begin() == LoaderSession::BeginResult::Started && session->attempt() == 2,
          "reused session reserves fresh attempt");
    rawWork->terminal[1].wait();
    check(session->poll() == LoaderSession::PollResult::Running && rawBackend->retainedToken() != firstToken,
          "old receipt cannot reap a new token");
    check(session->begin() == LoaderSession::BeginResult::Busy && session->attempt() == 2,
          "new terminal-before-reap remains Busy");
    rawBackend->cycle(1).epilogue.release();
    rawBackend->waitReceipt(2);
    check(session->poll() == LoaderSession::PollResult::Ready, "second actual join becomes Ready");
    LoaderSession::Result second;
    check(session->copyResult(second) && second.attempt == 2 && !second.detailsAvailable &&
          second.reason == LoaderSession::Reason::None, "new result is independently reset");
    check(first.attempt == 1 && first.terminal == LoaderSession::Terminal::Success,
          "new publication does not overwrite previous caller output");
    check(rawBackend->callbacks() == 2 && rawBackend->joins() == 2 && rawBackend->reaps() == 2 &&
          rawBackend->returnedTokens() == 2 && rawBackend->retainedToken() == 0,
          "two tokens have exactly two actual joins and consumptions");
    assertOrder(ledger, 1);
    assertOrder(ledger, 2);
    session.reset();
    check(workLife.expired() && backendLife.expired(), "bindings reclaimed only after legal session destruction");
}

void closeHeldWorker(Ledger &ledger, WorkMode mode)
{
    ledger.add(mode == WorkMode::IgnoreStop ? "CASE_IgnoreStop" : "CASE_CooperativeClose");
    std::shared_ptr<ControlledBackend> backend(new ControlledBackend(ledger));
    std::shared_ptr<WorkState> work(new WorkState(ledger, mode, true));
    std::unique_ptr<LoaderSession> session = makeSession(backend, work);
    check(session->begin() == LoaderSession::BeginResult::Started, "held body starts");
    work->entered[0].wait();
    session->requestClose();
    session->requestClose();
    check(session->admissionClosed() && !session->terminalPublished(), "close immediately stops admission");
    heartbeat(*session, ledger, 1, true);
    check(work->calls.load() == 1 && backend->reaps() == 0 && backend->retainedToken() != 0,
          "unreleased work keeps token and storage despite stop");
    work->bodyGate[0].release();
    work->terminal[0].wait();
    check(work->stopSeen.load() == 1 && session->terminalPublished(), "worker observes close at checkpoint");
    check(session->poll() == LoaderSession::PollResult::Running, "close still requires epilogue retirement");
    LoaderSession::Result output = sentinel();
    check(!session->copyResult(output) && isSentinel(output), "closed active result unreadable");
    backend->cycle(0).epilogue.release();
    backend->waitReceipt(1);
    check(session->poll() == LoaderSession::PollResult::Closed && session->quiescent(),
          "close reaches quiescence only after checked reap");
    check(session->copyResult(output) && output.terminal ==
          (mode == WorkMode::IgnoreStop ? LoaderSession::Terminal::Success : LoaderSession::Terminal::Cancelled),
          "stop-ignoring and cooperative outcomes remain distinct");
    check(!session->copyResult(output) && session->begin() == LoaderSession::BeginResult::Closed,
          "consumed closed session never reopens");
    check(backend->starts() == 1 && backend->reaps() == 1, "repeated close has one start and one reap");
}

void noThread(Ledger &ledger, bool afterSuccess)
{
    ledger.add(afterSuccess ? "CASE_NoThreadAfterSuccess" : "CASE_NoThread");
    std::shared_ptr<ControlledBackend> backend(new ControlledBackend(ledger,
        afterSuccess ? BackendMode::Normal : BackendMode::NoThread,
        afterSuccess ? BackendMode::NoThread : BackendMode::Normal));
    std::shared_ptr<WorkState> work(new WorkState(ledger));
    std::unique_ptr<LoaderSession> session = makeSession(backend, work);
    LoaderSession::Result earlier;
    unsigned priorToken = 0;
    if (afterSuccess)
    {
        check(session->begin() == LoaderSession::BeginResult::Started, "pre-NoThread success starts");
        priorToken = backend->retainedToken();
        backend->cycle(0).epilogue.release();
        backend->waitReceipt(1);
        check(session->poll() == LoaderSession::PollResult::Ready && session->copyResult(earlier),
              "previous token reaped and result consumed");
    }
    const unsigned callbacks = backend->callbacks();
    const unsigned polls = backend->polls();
    const unsigned joins = backend->joins();
    check(session->begin() == LoaderSession::BeginResult::StartFailed, "proven NoThread reports StartFailed");
    check(session->poll() == LoaderSession::PollResult::Ready && session->quiescent(), "NoThread is a ready failure");
    check(session->begin() == LoaderSession::BeginResult::Busy, "NoThread failure must be consumed");
    LoaderSession::Result output;
    check(session->copyResult(output) && output.terminal == LoaderSession::Terminal::Failure &&
          output.reason == LoaderSession::Reason::NoThread && output.nativeError == 11 &&
          output.attempt == (afterSuccess ? 2U : 1U), "NoThread result semantics");
    check(backend->callbacks() == callbacks && backend->polls() == polls && backend->joins() == joins &&
          backend->retainedToken() == 0 && work->calls.load() == callbacks,
          "NoThread adds no callback/reap and leaves no outstanding token or synchronous fallback");
    if (afterSuccess)
        check(priorToken != 0 && earlier.attempt == 1 && earlier.terminal == LoaderSession::Terminal::Success &&
              backend->returnedTokens() == 1, "NoThread cannot overwrite old returned token/result");
    else
    {
        check(session->begin() == LoaderSession::BeginResult::Started && session->attempt() == 2,
              "consumed NoThread permits a fresh real attempt");
        check(backend->retainedToken() != 0, "successful attempt after NoThread owns its own token");
        backend->cycle(1).epilogue.release();
        backend->waitReceipt(2);
        LoaderSession::Result success;
        check(session->poll() == LoaderSession::PollResult::Ready && session->copyResult(success) &&
              success.attempt == 2 && success.terminal == LoaderSession::Terminal::Success &&
              success.reason == LoaderSession::Reason::None && !success.detailsAvailable,
              "success after NoThread resets terminal and diagnostics");
        check(output.attempt == 1 && output.reason == LoaderSession::Reason::NoThread &&
              backend->callbacks() == 1 && backend->joins() == 1 && backend->reaps() == 1 &&
              backend->returnedTokens() == 1 && work->calls.load() == 1,
              "NoThread result remains intact and only the later real attempt runs work");
    }
}

void setupFailureAndReuse(Ledger &ledger)
{
    ledger.add("CASE_ConditionalSetupFailureReuse");
    std::shared_ptr<ControlledBackend> backend(new ControlledBackend(ledger, BackendMode::SetupFailure));
    std::shared_ptr<WorkState> work(new WorkState(ledger));
    std::unique_ptr<LoaderSession> session = makeSession(backend, work);
    LoaderSession::Result output = sentinel();
    check(session->begin() == LoaderSession::BeginResult::Started, "setup failure still has a created worker");
    backend->cycle(0).epilogueEntered.wait();
    const unsigned token = backend->retainedToken();
    check(session->terminalPublished() && !session->quiescent() && token != 0,
          "setup failure publication does not retire the worker");
    check(backend->callbacks() == 1 && work->calls.load() == 0 && backend->joins() == 0,
          "setup failure invokes one entry and skips ordinary Work");
    heartbeat(*session, ledger, 1, false);
    check(!session->copyResult(output) && isSentinel(output) && backend->retainedToken() == token,
          "setup diagnostics and token remain inaccessible before actual join receipt");
    backend->cycle(0).epilogue.release();
    backend->waitReceipt(1);
    check(session->poll() == LoaderSession::PollResult::Ready && session->quiescent(),
          "setup failure is Ready only after checked reap");
    check(session->begin() == LoaderSession::BeginResult::Busy, "setup failure still requires consumption");
    check(session->copyResult(output) && output.attempt == 1 &&
          output.terminal == LoaderSession::Terminal::Failure &&
          output.reason == LoaderSession::Reason::ThreadSetupFailure && output.nativeError == 47 &&
          !output.detailsAvailable && output.details.empty(), "fixed setup failure retains its native error");
    check(!session->copyResult(output) && backend->joins() == 1 && backend->reaps() == 1,
          "setup failure is consumed once after one actual join and checked reap");
    check(session->begin() == LoaderSession::BeginResult::Started && session->attempt() == 2,
          "consumed setup failure permits fresh admission");
    backend->cycle(1).epilogueEntered.wait();
    check(session->poll() == LoaderSession::PollResult::Running && backend->retainedToken() != token,
          "reused session retains a fresh token until its own receipt");
    backend->cycle(1).epilogue.release();
    backend->waitReceipt(2);
    LoaderSession::Result success;
    check(session->poll() == LoaderSession::PollResult::Ready && session->copyResult(success) &&
          success.attempt == 2 && success.terminal == LoaderSession::Terminal::Success &&
          success.reason == LoaderSession::Reason::None && success.nativeError == 0 &&
          !success.detailsAvailable && success.details.empty(), "reuse clears worker setup diagnostics");
    check(output.attempt == 1 && output.reason == LoaderSession::Reason::ThreadSetupFailure &&
          output.nativeError == 47 && backend->callbacks() == 2 && work->calls.load() == 1 &&
          backend->joins() == 2 && backend->reaps() == 2 && backend->retainedToken() == 0,
          "setup failure remains intact while only the later ordinary attempt runs Work");
}

void exceptionCase(Ledger &ledger, WorkMode mode, bool detailFailure, bool copyFailure)
{
    ledger.add(copyFailure ? "CASE_CopyFailure" : detailFailure ? "CASE_DetailFailure" :
               mode == WorkMode::UnknownThrow ? "CASE_UnknownException" : "CASE_StandardException");
    std::shared_ptr<ControlledBackend> backend(new ControlledBackend(ledger));
    std::shared_ptr<WorkState> work(new WorkState(ledger, mode, false, detailFailure, copyFailure ? 1U : 0U));
    std::unique_ptr<LoaderSession> session = makeSession(backend, work);
    check(session->begin() == LoaderSession::BeginResult::Started, "throwing work starts");
    work->terminal[0].wait();
    LoaderSession::Result output = sentinel();
    check(session->terminalPublished() && !session->copyResult(output) && isSentinel(output),
          "failure notification does not permit pre-reap details");
    check(session->poll() == LoaderSession::PollResult::Running, "exception epilogue still holds Busy");
    backend->cycle(0).epilogue.release();
    backend->waitReceipt(1);
    check(session->poll() == LoaderSession::PollResult::Ready, "exception requires successful reap");
    const unsigned polls = backend->polls();
    const bool closeReady = mode == WorkMode::StandardThrow && !detailFailure && !copyFailure;
    if (closeReady)
    {
        session->requestClose();
        session->requestClose();
        check(session->poll() == LoaderSession::PollResult::Closed && session->quiescent() &&
              session->admissionClosed() && session->attempt() == 1 &&
              session->begin() == LoaderSession::BeginResult::Closed,
              "reaped Ready can close without consuming its completed result");
        check(backend->polls() == polls, "Ready-to-Closed transition never repeats backend reap");
    }
    if (copyFailure)
    {
        bool caught = false;
        try { session->copyResult(output); }
        catch (const std::bad_alloc &) { caught = true; }
        check(caught && isSentinel(output), "injected owner copy throw preserves output");
        check(session->begin() == LoaderSession::BeginResult::Busy && session->attempt() == 1 &&
              session->poll() == LoaderSession::PollResult::Ready && backend->polls() == polls,
              "copy throw preserves unconsumed result without duplicate reap");
    }
    check(session->copyResult(output) && output.terminal == LoaderSession::Terminal::Failure,
          "production exception envelope publishes Failure");
    const LoaderSession::Reason expected = detailFailure ? LoaderSession::Reason::ErrorDetailsUnavailable :
        mode == WorkMode::UnknownThrow ? LoaderSession::Reason::UnknownException : LoaderSession::Reason::StandardException;
    check(output.reason == expected, "exception reason is exact");
    if (mode == WorkMode::StandardThrow && !detailFailure)
        check(output.detailsAvailable && output.details == "deterministic loader failure", "standard exception detail copied");
    else check(!output.detailsAvailable && output.details.empty(), "fallback detail is unavailable");
    check(!session->copyResult(output) && backend->reaps() == 1 && backend->callbacks() == 1,
          "failure consumed once after one callback/reap");
    if (closeReady)
        check(session->poll() == LoaderSession::PollResult::Closed &&
              session->begin() == LoaderSession::BeginResult::Closed,
              "closing an unconsumed Ready result preserves permanent closed admission after copy");
}

void processLease(Ledger &ledger)
{
    ledger.add("CASE_ProcessLease");
    unsigned bodies = 0;
    LoaderProcessLease first;
    check(first.acquired(), "first process lease admitted");
    if (first.acquired()) ++bodies;
    LoaderProcessLease denied;
    check(!denied.acquired(), "second lease denied before body work");
    if (denied.acquired()) ++bodies;
    denied.release();
    LoaderProcessLease stillDenied;
    check(!stillDenied.acquired() && bodies == 1, "denied lease cannot release another owner");
    first.release();
    first.release();
    LoaderProcessLease next;
    check(next.acquired(), "explicit holder release permits next lease");
    stillDenied.release();
    denied.release();
    LoaderProcessLease deniedAgain;
    check(!deniedAgain.acquired(), "old denied leases cannot clear current holder");
    next.release();
    LoaderProcessLease finalLease;
    check(finalLease.acquired(), "lease bookkeeping remains reusable");
}

void normal(Ledger &ledger)
{
    emptyClosed(ledger);
    earlyReuseAndLifetime(ledger);
    closeHeldWorker(ledger, WorkMode::CancelAtCheckpoint);
    closeHeldWorker(ledger, WorkMode::IgnoreStop);
    noThread(ledger, false);
    noThread(ledger, true);
    setupFailureAndReuse(ledger);
    exceptionCase(ledger, WorkMode::StandardThrow, false, false);
    exceptionCase(ledger, WorkMode::UnknownThrow, false, false);
    exceptionCase(ledger, WorkMode::StandardThrow, true, false);
    exceptionCase(ledger, WorkMode::StandardThrow, false, true);
    processLease(ledger);
}

// Negative modes are separate process commands, never normal-suite branches.
// Exit 86 means the expected fail-stop invariant fired before any context or
// session member destruction. Exit 87 means a quarantined object was retained
// and the process deliberately exited without its destructor. These are not
// normal PASS exits. The releasing runner must preserve stdout/stderr/exit and
// apply its watchdog; a timeout, signal or incomplete ledger is never PASS.
[[noreturn]] void retainedQuarantineExit(LoaderSession &session, ControlledBackend &backend,
                                       WorkState &work, Ledger &ledger, bool skipCallback)
{
    const unsigned polls = backend.polls();
    const unsigned token = backend.retainedToken();
    check(skipCallback || token != 0, "ambiguous/error backend still owns its original token");
    LoaderSession::Result output = sentinel();
    for (unsigned i = 0; i < 16; ++i)
    {
        check(session.poll() == LoaderSession::PollResult::Quarantined, "quarantine poll remains quarantined");
        check(session.begin() == LoaderSession::BeginResult::Quarantined, "quarantine rejects new admission");
        session.requestClose();
        check(session.admissionClosed() && !session.quiescent() &&
              !session.copyResult(output) && isSentinel(output), "quarantine retains inaccessible result/storage");
        ledger.add("QuarantineHeartbeat", 1);
    }
    check(backend.polls() == polls && backend.retainedToken() == token,
          "quarantine never retries backend poll or clears its retained token");
    work.bodyGate[0].release();
    backend.cycle(0).epilogue.release();
    backend.waitReceipt(1);
    backend.finishController();
    check(backend.joins() == 1 && backend.callbacks() == (skipCallback ? 0U : 1U),
          "negative quarantine fixture controller/worker joined independently");
    check(ledger.count("ContextDestroyed") == 0 && ledger.count("SessionDestroying") == 0,
          "quarantined session/bindings were never normally destroyed");
    ledger.add("RetainedQuarantineProcessExit");
    ledger.dump();
    std::fprintf(stderr, "EXPECTED_RETAINED_QUARANTINE exit=87\n");
    std::fflush(stderr);
    std::_Exit(87);
}

void negative(const char *mode, Ledger &ledger)
{
    ledger.add(mode);
    std::printf("NEGATIVE_MODE %s\n", mode);
    std::fflush(stdout);
    if (std::strcmp(mode, "--negative-lease-owner") == 0)
    {
        LoaderProcessLease lease;
        check(lease.acquired(), "negative lease acquired");
        expectedFatalMarker = "ForeignLeaseRelease";
        expectedFatal.store(true, std::memory_order_release);
        std::thread foreign([&lease, &ledger]() { ledger.add("ForeignLeaseRelease"); lease.release(); });
        foreign.join();
        fail("foreign lease operation returned");
    }
    BackendMode backendMode = BackendMode::Normal;
    bool fatal = false;
    bool heldReturn = false;
    bool activeDestruction = false;
    bool quarantinedDestruction = false;
    bool wrongOwner = false;
    if (std::strcmp(mode, "--negative-quarantine-start") == 0) backendMode = BackendMode::StartInvariant;
    else if (std::strcmp(mode, "--negative-quarantine-started-error") == 0) backendMode = BackendMode::StartedError;
    else if (std::strcmp(mode, "--negative-quarantine-reap") == 0) backendMode = BackendMode::ReapError;
    else if (std::strcmp(mode, "--negative-quarantine-busy-error") == 0) backendMode = BackendMode::BusyError;
    else if (std::strcmp(mode, "--negative-quarantine-invalid-terminal") == 0) backendMode = BackendMode::SkipCallback;
    else if (std::strcmp(mode, "--negative-quarantine-no-thread-callback") == 0)
    { backendMode = BackendMode::NoThreadAfterCallback; heldReturn = true; }
    else if (std::strcmp(mode, "--negative-duplicate-callback") == 0)
    { backendMode = BackendMode::DuplicateCallback; fatal = true; }
    else if (std::strcmp(mode, "--negative-reentrant") == 0)
    { backendMode = BackendMode::Reentrant; fatal = true; }
    else if (std::strcmp(mode, "--negative-wrong-owner") == 0)
    { wrongOwner = true; fatal = true; }
    else if (std::strcmp(mode, "--negative-destructor-active") == 0)
    { activeDestruction = true; fatal = true; }
    else if (std::strcmp(mode, "--negative-destructor-quarantined") == 0)
    { backendMode = BackendMode::StartInvariant; quarantinedDestruction = true; fatal = true; }
    else fail("unknown negative mode");

    ReentryTarget reentry;
    std::shared_ptr<ControlledBackend> backend(new ControlledBackend(ledger,
        backendMode, BackendMode::Normal, heldReturn, &reentry));
    std::shared_ptr<WorkState> work(new WorkState(ledger, WorkMode::Success, activeDestruction));
    std::unique_ptr<LoaderSession> session = makeSession(backend, work);
    reentry.session = session.get();
    expectedFatalStarts = wrongOwner ? 0U : 1U;
    if (wrongOwner) expectedFatalMarker = "ForeignBegin";
    else if (activeDestruction) expectedFatalMarker = "DeliberateActiveDestruction";
    else if (quarantinedDestruction) expectedFatalMarker = "DeliberateQuarantinedDestruction";
    else if (backendMode == BackendMode::Reentrant) expectedFatalMarker = "DeliberateReentrantPoll";
    else if (backendMode == BackendMode::DuplicateCallback) expectedFatalMarker = "DeliberateDuplicateCallback";
    expectedFatal.store(fatal, std::memory_order_release);
    if (wrongOwner)
    {
        std::thread foreign([&session, &ledger]() { ledger.add("ForeignBegin"); session->begin(); });
        foreign.join();
        fail("wrong-owner begin returned");
    }
    std::thread coordinator;
    if (heldReturn)
    {
        coordinator = std::thread([&backend, &ledger]() {
            backend->cycle(0).callbackReturned.wait();
            ledger.add("CallbackBeforeFalseNoThreadReturn", 1);
            backend->cycle(0).startReturnGate.release();
            ledger.add("CoordinatorReturned");
        });
        ledger.add("CoordinatorSpawned");
    }
    const LoaderSession::BeginResult began = session->begin();
    if (heldReturn) { coordinator.join(); ledger.add("CoordinatorJoined"); }
    if (activeDestruction)
    {
        work->entered[0].wait();
        ledger.add("DeliberateActiveDestruction");
        session.reset();
        fail("active destruction returned");
    }
    if (quarantinedDestruction)
    {
        check(began == LoaderSession::BeginResult::Quarantined, "negative destructor target quarantined");
        backend->cycle(0).epilogueEntered.wait();
        ledger.add("DeliberateQuarantinedDestruction");
        session.reset();
        fail("quarantined destruction returned");
    }
    if (backendMode == BackendMode::DuplicateCallback)
    {
        check(began == LoaderSession::BeginResult::Started, "duplicate negative first callback starts");
        work->terminal[0].wait();
        backend->cycle(0).epilogue.release();
        backend->waitReceipt(1);
        fail("duplicate callback failed to terminate");
    }
    if (backendMode == BackendMode::SkipCallback)
    {
        check(began == LoaderSession::BeginResult::Started, "invalid-terminal backend starts");
        backend->cycle(0).epilogue.release();
        backend->waitReceipt(1);
        check(session->poll() == LoaderSession::PollResult::Quarantined,
              "Reaped without terminal quarantines instead of fabricating result");
    }
    else if (backendMode == BackendMode::ReapError || backendMode == BackendMode::BusyError)
    {
        check(began == LoaderSession::BeginResult::Started, "reap-error backend starts");
        work->terminal[0].wait();
        check(session->poll() == LoaderSession::PollResult::Quarantined, "reap error quarantines");
    }
    else check(began == LoaderSession::BeginResult::Quarantined, "ambiguous start quarantines");
    retainedQuarantineExit(*session, *backend, *work, ledger, backendMode == BackendMode::SkipCallback);
}

void modes()
{
    std::puts("--normal (or no arguments): expected exit 0, LOADER_LIFETIME_UNIT PASS after all fixture joins");
    std::puts("--negative-quarantine-start: expected exit 87");
    std::puts("--negative-quarantine-started-error: expected exit 87");
    std::puts("--negative-quarantine-reap: expected exit 87");
    std::puts("--negative-quarantine-busy-error: expected exit 87");
    std::puts("--negative-quarantine-invalid-terminal: expected exit 87");
    std::puts("--negative-quarantine-no-thread-callback: expected exit 87");
    std::puts("--negative-wrong-owner: expected exit 86");
    std::puts("--negative-reentrant: expected exit 86");
    std::puts("--negative-destructor-active: expected exit 86");
    std::puts("--negative-destructor-quarantined: expected exit 86");
    std::puts("--negative-duplicate-callback: expected exit 86");
    std::puts("--negative-lease-owner: expected exit 86");
    std::puts("--list-modes: expected exit 0; lists commands without running tests");
}
}

int main(int argc, char **argv)
{
    Ledger ledger;
    processLedger = &ledger;
    std::set_terminate(&terminated);
    if (argc == 2 && std::strcmp(argv[1], "--list-modes") == 0) { modes(); return 0; }
    if (argc > 2) { modes(); return 2; }
    if (argc == 2 && std::strcmp(argv[1], "--normal") != 0)
    {
        negative(argv[1], ledger);
        fail("negative process unexpectedly returned");
    }
    try
    {
        std::thread owner([&ledger]() {
            try { normal(ledger); ledger.add("OwnerReturned"); }
            catch (...) { fail("unexpected normal-suite exception"); }
        });
        ledger.add("OwnerSpawned");
        owner.join();
        ledger.add("OwnerJoined");
        check(ledger.count("WorkerSpawned") == ledger.count("JoinReturned"), "all normal workers actually joined");
        check(ledger.count("CallbackReturned") == ledger.count("JoinReturned") &&
              ledger.count("ReceiptPublished") == ledger.count("JoinReturned"),
              "all normal callback returns have independent actual join receipts");
        check(ledger.count("TokenRetained") == ledger.count("StartReturnedToken") &&
              ledger.count("StartReturnedToken") == ledger.count("BackendReapedReturned") &&
              ledger.count("BackendReapedReturned") == ledger.count("JoinReturned"),
              "each independently returned token is retained and reaped exactly once");
        check(ledger.count("ControllerSpawned") == ledger.count("ControllerJoined"), "all normal controllers joined");
        check(ledger.count("CoordinatorSpawned") == ledger.count("CoordinatorJoined"), "all normal coordinators joined");
        check(ledger.count("OwnerSpawned") == 1 && ledger.count("OwnerJoined") == 1, "normal owner joined before PASS");
        check(ledger.count("ControllerFailure") == 0, "no hidden controller failure");
        check(ledger.count("SessionDestroying") == ledger.count("ContextDestroyed") &&
              ledger.count("ContextDestroyed") == ledger.count("BackendDestroyed"),
              "all normal owned bindings reclaimed only by legal destruction");
        ledger.dump();
        std::printf("LOADER_LIFETIME_UNIT PASS %u checks; conditional fixture only\n", checks);
        return 0;
    }
    catch (...) { fail("fixture orchestration failed"); }
}
