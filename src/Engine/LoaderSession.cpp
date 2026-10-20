// Copyright 2026 OpenXcom contributors. GPL-3.0-or-later; see LICENSE.txt and LICENSE-NOTICES.md.
#include "LoaderSession.h"
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace OpenXcom
{
namespace
{
std::atomic_flag domainHeld = ATOMIC_FLAG_INIT;
[[noreturn]] void invariantFailure() noexcept { std::terminate(); }
}

LoaderProcessLease::LoaderProcessLease() noexcept :
    _owner(std::this_thread::get_id()), _held(!domainHeld.test_and_set(std::memory_order_acquire)) {}
LoaderProcessLease::~LoaderProcessLease() noexcept { release(); }
bool LoaderProcessLease::acquired() const noexcept
{
    if (std::this_thread::get_id() != _owner) invariantFailure();
    return _held;
}
void LoaderProcessLease::release() noexcept
{
    if (std::this_thread::get_id() != _owner) invariantFailure();
    if (_held) { _held = false; domainHeld.clear(std::memory_order_release); }
}

class LoaderSession::Operation
{
public:
    explicit Operation(const LoaderSession &session) noexcept : _session(session)
    {
        // Owner is immutable: reject foreign access before touching owner state.
        if (std::this_thread::get_id() != _session._owner) invariantFailure();
        if (_session._inside) invariantFailure();
        _session._inside = true;
    }
    ~Operation() noexcept { _session._inside = false; }
private:
    const LoaderSession &_session;
};

void LoaderSession::Result::swap(Result &other) noexcept
{
    using std::swap;
    swap(attempt, other.attempt); swap(terminal, other.terminal);
    swap(reason, other.reason); swap(nativeError, other.nativeError);
    swap(detailsAvailable, other.detailsAvailable); details.swap(other.details);
}

LoaderSession::LoaderSession(std::shared_ptr<LoaderThreadBackend> backend, Work work,
                             std::shared_ptr<void> context)
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
    : LoaderSession(std::move(backend), work, std::move(context), Hooks{nullptr, nullptr, nullptr, nullptr}) {}
LoaderSession::LoaderSession(std::shared_ptr<LoaderThreadBackend> backend, Work work,
                             std::shared_ptr<void> context, Hooks hooks)
#endif
    : _backend(std::move(backend)), _work(work), _context(std::move(context)),
      _owner(std::this_thread::get_id()), _inside(false), _phase(Phase::Idle),
      _closed(false), _outstanding(false), _hasAttempt(false), _consumed(true),
      _attempt(0), _terminal(Pending), _stop(false), _reason(Reason::None),
      _workerNativeError(0), _detailsAvailable(false),
      _quarantineReason(Reason::None), _nativeError(0)
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
      , _hooks(hooks)
#endif
{
    if (!_backend || !_work || !_context) throw std::invalid_argument("LoaderSession requires owned bindings");
    if (!_terminal.is_lock_free() || !_stop.is_lock_free())
        throw std::runtime_error("LoaderSession requires lock-free terminal and stop atomics");
}

LoaderSession::~LoaderSession() noexcept
{
    Operation operation(*this);
    if (_outstanding || _phase == Phase::Starting || _phase == Phase::Active || _phase == Phase::Quarantined)
        invariantFailure();
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
    trace(Trace::Destroying);
#endif
}

LoaderSession::BeginResult LoaderSession::begin() noexcept
{
    Operation operation(*this);
    if (_phase == Phase::Quarantined) return BeginResult::Quarantined;
    if (_closed) return BeginResult::Closed;
    if (_phase != Phase::Idle || !_consumed) return BeginResult::Busy;
    // Check before reservation/reset. Overflow never wraps into a reused ID.
    if (_attempt == std::numeric_limits<std::uint64_t>::max())
    {
        quarantine(Reason::AttemptOverflow, 0);
        return BeginResult::Quarantined;
    }
    ++_attempt;
    _phase = Phase::Starting;
    _hasAttempt = true;
    _consumed = false;
    _details.clear();
    _detailsAvailable = false;
    _reason = Reason::None;
    _workerNativeError = 0;
    _nativeError = 0;
    _stop.store(false, std::memory_order_release);
    _terminal.store(Pending, std::memory_order_release);
    // Treat ownership as outstanding BEFORE the potentially early callback.
    _outstanding = true;
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
    trace(Trace::Reserved);
#endif
    const LoaderThreadBackend::StartResult started = _backend->start(&LoaderSession::entry, this);
    if (started.kind == LoaderThreadBackend::StartKind::Started && started.nativeError == 0)
    {
        _phase = Phase::Active;
        return BeginResult::Started;
    }
    if (started.kind == LoaderThreadBackend::StartKind::NoThread &&
        _terminal.load(std::memory_order_acquire) == Pending)
    {
        // Only proven no outstanding worker/callback at return permits reclamation.
        _outstanding = false;
        _reason = Reason::NoThread;
        _nativeError = started.nativeError;
        _terminal.store(Failure, std::memory_order_release);
        _phase = Phase::Ready;
        return BeginResult::StartFailed;
    }
    quarantine(Reason::BackendInvariant, started.nativeError);
    return BeginResult::Quarantined;
}

void LoaderSession::quarantine(Reason reason, int error) noexcept
{
    _closed = true;
    _stop.store(true, std::memory_order_release);
    _quarantineReason = reason;
    _nativeError = error;
    _phase = Phase::Quarantined;
    // Do not clear ownership, worker detail, native token or retained bindings.
}

void LoaderSession::requestClose() noexcept
{
    Operation operation(*this);
    _closed = true;
    _stop.store(true, std::memory_order_release);
    if (_phase == Phase::Idle || _phase == Phase::Ready) _phase = Phase::Closed;
}

LoaderSession::PollResult LoaderSession::poll() noexcept
{
    Operation operation(*this);
    if (_phase == Phase::Quarantined) return PollResult::Quarantined;
    if (_phase == Phase::Idle) return PollResult::Idle;
    if (_phase == Phase::Closed) return PollResult::Closed;
    if (_phase == Phase::Ready) return PollResult::Ready;
    if (_phase != Phase::Active || !_outstanding) invariantFailure();
    const LoaderThreadBackend::ReapResult reaped = _backend->pollReap();
    if (reaped.kind == LoaderThreadBackend::ReapKind::Busy && reaped.nativeError == 0)
        return PollResult::Running;
    if (reaped.kind != LoaderThreadBackend::ReapKind::Reaped || reaped.nativeError != 0)
    {
        quarantine(Reason::ReapError, reaped.nativeError);
        return PollResult::Quarantined;
    }
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
    trace(Trace::BackendReaped);
#endif
    const unsigned terminal = _terminal.load(std::memory_order_acquire);
    if (terminal != Success && terminal != Failure && terminal != Cancelled)
    {
        quarantine(Reason::InvalidPublication, 0);
        return PollResult::Quarantined;
    }
    _outstanding = false;
    _phase = _closed ? Phase::Closed : Phase::Ready;
    return _closed ? PollResult::Closed : PollResult::Ready;
}

bool LoaderSession::copyResult(Result &output)
{
    Operation operation(*this);
    if ((_phase != Phase::Ready && _phase != Phase::Closed) || _outstanding || !_hasAttempt || _consumed)
        return false;
    Result copy;
    copy.attempt = _attempt;
    const unsigned terminal = _terminal.load(std::memory_order_acquire);
    if (terminal == Success) copy.terminal = Terminal::Success;
    else if (terminal == Failure) copy.terminal = Terminal::Failure;
    else if (terminal == Cancelled) copy.terminal = Terminal::Cancelled;
    else invariantFailure();
    copy.reason = _reason;
    copy.nativeError = _reason == Reason::ThreadSetupFailure ? _workerNativeError : _nativeError;
    copy.detailsAvailable = _detailsAvailable;
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
    if (_hooks.failCopy && _hooks.failCopy(_hooks.context)) throw std::bad_alloc();
#endif
    if (_detailsAvailable) copy.details = _details;
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
    trace(Trace::ResultCopied);
#endif
    output.swap(copy);
    _consumed = true;
    _phase = _closed ? Phase::Closed : Phase::Idle;
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
    trace(Trace::ResultConsumed);
#endif
    return true;
}

bool LoaderSession::quiescent() const noexcept
{
    Operation operation(*this);
    return !_outstanding && _phase != Phase::Starting && _phase != Phase::Quarantined;
}
bool LoaderSession::admissionClosed() const noexcept { Operation operation(*this); return _closed; }
std::uint64_t LoaderSession::attempt() const noexcept { Operation operation(*this); return _attempt; }
bool LoaderSession::terminalPublished() const noexcept
{
    Operation operation(*this);
    const unsigned terminal = _terminal.load(std::memory_order_acquire);
    return terminal == Success || terminal == Failure || terminal == Cancelled;
}

void LoaderSession::entry(void *context, int setupError) noexcept
{
    static_cast<LoaderSession *>(context)->run(setupError);
}
void LoaderSession::run(int setupError) noexcept
{
    unsigned expected = Pending;
    // Claim before any detail write: a duplicate callback is a fatal backend
    // violation, never a second writer or a second successful publication.
    if (!_terminal.compare_exchange_strong(expected, Working, std::memory_order_acq_rel)) invariantFailure();
    if (setupError < 0) invariantFailure();
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
    trace(Trace::WorkerEntry);
#endif
    unsigned terminal = Failure;
    if (setupError != 0)
    {
        // A created worker still claims/publishes exactly once. Its setup
        // diagnostics are not read by the owner until checked retirement.
        _workerNativeError = setupError;
        _reason = Reason::ThreadSetupFailure;
    }
    else try
    {
        const StopToken stop(_stop);
        const WorkResult result = _work(_context.get(), stop);
        if (result == WorkResult::Success) terminal = Success;
        else if (result == WorkResult::Cancelled) terminal = Cancelled;
        else invariantFailure();
    }
    catch (const std::exception &error)
    {
        _reason = Reason::StandardException;
        try
        {
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
            if (_hooks.failDetail && _hooks.failDetail(_hooks.context)) throw std::bad_alloc();
#endif
            _details = error.what();
            _detailsAvailable = true;
        }
        catch (...)
        {
            _reason = Reason::ErrorDetailsUnavailable;
            _detailsAvailable = false;
        }
    }
    catch (...) { _reason = Reason::UnknownException; _detailsAvailable = false; }
    expected = Working;
    if (!_terminal.compare_exchange_strong(expected, terminal, std::memory_order_release, std::memory_order_relaxed))
        invariantFailure();
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
    trace(Trace::WorkTerminal);
    trace(Trace::LastCallbackAccess);
#endif
}
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
void LoaderSession::trace(Trace event) const noexcept
{
    if (_hooks.trace) _hooks.trace(_hooks.context, event);
}
#endif
}
