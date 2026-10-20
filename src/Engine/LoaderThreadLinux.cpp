// Copyright 2026 OpenXcom contributors. GPL-3.0-or-later; see LICENSE.txt and LICENSE-NOTICES.md.
#include "LoaderThreadLinux.h"
#include <cerrno>
#include <exception>
#include <unistd.h>

#if !defined(__linux__) || !defined(__GLIBC__) || !defined(_GNU_SOURCE)
#error "LoaderThreadLinux requires Linux/glibc and the private _GNU_SOURCE build selection"
#endif

namespace OpenXcom
{
namespace
{
[[noreturn]] void backendInvariantFailure() noexcept { std::terminate(); }
}

class LoaderThreadLinux::OwnerOperation
{
public:
    explicit OwnerOperation(const LoaderThreadLinux &backend) noexcept : _backend(backend)
    {
        // A fork-inherited pthread token is never queried. Immutable guards
        // precede all owner-mutable fields, including the reentry flag.
        if (getpid() != _backend._process) backendInvariantFailure();
        if (!pthread_equal(pthread_self(), _backend._owner)) backendInvariantFailure();
        if (_backend._inside) backendInvariantFailure();
        _backend._inside = true;
    }
    ~OwnerOperation() noexcept { _backend._inside = false; }
private:
    const LoaderThreadLinux &_backend;
};

LoaderThreadLinux::LoaderThreadLinux() noexcept
#ifdef OPENXCOM_LOADER_LINUX_TESTING
    : LoaderThreadLinux(Options{nullptr, nullptr, nullptr, SIG_BLOCK, 0,
                               {false, false, false, false}, {0, 0, 0, 0}}) {}
LoaderThreadLinux::LoaderThreadLinux(Options options) noexcept
#endif
    : _process(getpid()), _owner(pthread_self()), _inside(false),
      _phase(Phase::Empty), _hasThread(false), _entry(nullptr), _context(nullptr),
      _ownerDiagnostic{NativeOperation::None, 0, NativeOperation::None, 0},
      _workerDiagnostic{NativeOperation::None, 0, NativeOperation::None, 0},
      _workerReadable(true)
#ifdef OPENXCOM_LOADER_LINUX_TESTING
      , _options(options), _testAttempt(0)
#endif
{
#ifdef OPENXCOM_LOADER_LINUX_TESTING
    if (_options.guardCount > 4) backendInvariantFailure();
#endif
}

LoaderThreadLinux::~LoaderThreadLinux() noexcept
{
    OwnerOperation operation(*this);
    // Never wait, detach, discard an owned handle or reclaim uncertain state.
    if (_phase != Phase::Empty || _hasThread) backendInvariantFailure();
}

LoaderThreadBackend::StartResult LoaderThreadLinux::failStart(
    NativeOperation operation, int code, pthread_attr_t *initializedAttr) noexcept
{
    _ownerDiagnostic = {operation, code, NativeOperation::None, 0};
    if (initializedAttr)
    {
        const int cleanup = pthread_attr_destroy(initializedAttr);
        if (cleanup != 0)
        {
            _ownerDiagnostic.secondaryOperation = NativeOperation::AttrDestroy;
            _ownerDiagnostic.secondaryCode = cleanup;
            _workerReadable = true; // No callback outstanding, attrs uncertain.
            _phase = Phase::Poisoned;
            return {StartKind::InvariantError, code};
        }
    }
    _entry = nullptr;
    _context = nullptr;
    _workerReadable = true;
    _phase = Phase::Empty;
    // Post-return assertion: no outstanding thread/callback/token. A general
    // create error need not imply that no transient kernel task ever existed.
    return {StartKind::NoThread, code};
}

LoaderThreadBackend::StartResult LoaderThreadLinux::start(Entry entry, void *context) noexcept
{
    OwnerOperation operation(*this);
    if (_phase != Phase::Empty || _hasThread) backendInvariantFailure();
#ifdef OPENXCOM_LOADER_LINUX_TESTING
    if (_options.guardCount && _testAttempt >= _options.guardCount) backendInvariantFailure();
    const unsigned testAttempt = _testAttempt;
    if (_options.guardCount) ++_testAttempt;
#endif
    if (!entry || !context)
    {
        _ownerDiagnostic = {NativeOperation::Binding, EINVAL, NativeOperation::None, 0};
        _phase = Phase::Poisoned;
        return {StartKind::InvariantError, EINVAL};
    }
    _phase = Phase::Creating;
    _entry = entry;
    _context = context;
    _ownerDiagnostic = {NativeOperation::None, 0, NativeOperation::None, 0};
    _workerDiagnostic = {NativeOperation::None, 0, NativeOperation::None, 0};
    _workerReadable = false;

    if (sigemptyset(&_mask) != 0)
    {
        const int error = errno;
        return failStart(NativeOperation::SignalEmpty, error, nullptr);
    }
    const int blocked[] = {SIGHUP, SIGINT, SIGQUIT, SIGPIPE, SIGALRM, SIGTERM,
                           SIGCHLD, SIGWINCH, SIGVTALRM, SIGPROF};
    for (unsigned i = 0; i < sizeof(blocked) / sizeof(blocked[0]); ++i)
    {
        if (sigaddset(&_mask, blocked[i]) != 0)
        {
            const int error = errno;
            return failStart(NativeOperation::SignalAdd, error, nullptr);
        }
    }
    pthread_attr_t attr;
    int error = pthread_attr_init(&attr);
    if (error != 0) return failStart(NativeOperation::AttrInit, error, nullptr);
    error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    if (error != 0) return failStart(NativeOperation::AttrSetDetach, error, &attr);
    int detachState;
    error = pthread_attr_getdetachstate(&attr, &detachState);
    if (error != 0) return failStart(NativeOperation::AttrGetDetach, error, &attr);
    if (detachState != PTHREAD_CREATE_JOINABLE)
        return failStart(NativeOperation::AttrGetDetach, EPROTO, &attr);
    error = pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
    if (error != 0) return failStart(NativeOperation::AttrSetInherit, error, &attr);
    int inheritState;
    error = pthread_attr_getinheritsched(&attr, &inheritState);
    if (error != 0) return failStart(NativeOperation::AttrGetInherit, error, &attr);
    if (inheritState != PTHREAD_INHERIT_SCHED)
        return failStart(NativeOperation::AttrGetInherit, EPROTO, &attr);
#ifdef OPENXCOM_LOADER_LINUX_TESTING
    if (_options.guardCount && _options.guardEnabled[testAttempt])
    {
        error = pthread_attr_setguardsize(&attr, _options.guardSize[testAttempt]);
        trace(Trace::AttrGuardSize, error);
        if (error != 0) return failStart(NativeOperation::AttrSetGuard, error, &attr);
    }
#endif
    // A failed pthread_create may write an invalid output. Never read that
    // output on failure. The worker may finish before this call returns.
    pthread_t created;
    const int createdCode = pthread_create(&created, &attr, &LoaderThreadLinux::threadMain, this);
    if (createdCode != 0)
    {
#ifdef OPENXCOM_LOADER_LINUX_TESTING
        trace(Trace::NativeCreate, createdCode);
#endif
        return failStart(NativeOperation::Create, createdCode, &attr);
    }
    _thread = created;
    _hasThread = true;
    _phase = Phase::Owned;
#ifdef OPENXCOM_LOADER_LINUX_TESTING
    trace(Trace::NativeCreate, 0);
    trace(Trace::HandleInstalled, 0);
#endif
    const int cleanup = pthread_attr_destroy(&attr);
    if (cleanup != 0)
    {
        _ownerDiagnostic = {NativeOperation::AttrDestroy, cleanup, NativeOperation::None, 0};
        _phase = Phase::Poisoned; // Live handle/context retained, never NoThread.
        return {StartKind::InvariantError, cleanup};
    }
#ifdef OPENXCOM_LOADER_LINUX_TESTING
    if (_options.afterCreate) _options.afterCreate(_options.context);
#endif
    return {StartKind::Started, 0};
}

void *LoaderThreadLinux::threadMain(void *context) noexcept
{
    LoaderThreadLinux &backend = *static_cast<LoaderThreadLinux *>(context);
    // Only stable pre-create binding/mask/options and worker-only diagnostics
    // are used here. No returned handle or owner phase may be observed.
#ifdef OPENXCOM_LOADER_LINUX_TESTING
    backend.trace(Trace::WorkerEntered, 0);
#endif
    int setupError = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
#ifdef OPENXCOM_LOADER_LINUX_TESTING
    backend.trace(Trace::CancelState, setupError);
#endif
    if (setupError != 0)
        backend._workerDiagnostic = {NativeOperation::CancelState, setupError, NativeOperation::None, 0};
    else
    {
        int how = SIG_BLOCK;
#ifdef OPENXCOM_LOADER_LINUX_TESTING
        how = backend._options.sigmaskHow;
#endif
        setupError = pthread_sigmask(how, &backend._mask, nullptr);
#ifdef OPENXCOM_LOADER_LINUX_TESTING
        backend.trace(Trace::SignalMask, setupError);
#endif
        if (setupError != 0)
            backend._workerDiagnostic = {NativeOperation::SignalMask, setupError, NativeOperation::None, 0};
    }
    backend._entry(backend._context, setupError);
#ifdef OPENXCOM_LOADER_LINUX_TESTING
    backend.trace(Trace::CallbackReturned, setupError);
    backend.trace(Trace::SdlCleanupEntered, 0);
#endif
    OpenXcom_LoaderSdlTlsCleanup();
#ifdef OPENXCOM_LOADER_LINUX_TESTING
    backend.trace(Trace::SdlCleanupReturned, 0);
    backend.trace(Trace::WrapperReturning, 0);
#endif
    // Real C++ TLS, pthread-key and libc epilogues follow this return.
    return nullptr;
}

LoaderThreadBackend::ReapResult LoaderThreadLinux::pollReap() noexcept
{
    OwnerOperation operation(*this);
    if (_phase != Phase::Owned || !_hasThread) backendInvariantFailure();
    void *returned;
    const int error = pthread_tryjoin_np(_thread, &returned);
    if (error == 0)
    {
        // Native success CONSUMED the token. Revoke authority immediately,
        // before tracing, callbacks or checking any wrapper-return invariant.
        _hasThread = false;
        _workerReadable = true;
        _entry = nullptr;
        _context = nullptr;
        _phase = Phase::Empty;
#ifdef OPENXCOM_LOADER_LINUX_TESTING
        trace(Trace::NativeTryJoin, error);
        trace(Trace::OwnershipCleared, 0);
#endif
        if (returned != nullptr)
        {
            _ownerDiagnostic = {NativeOperation::WrapperResult, EPROTO, NativeOperation::None, 0};
            _phase = Phase::Poisoned; // Context quarantine, no native ownership.
            return {ReapKind::Error, EPROTO};
        }
        return {ReapKind::Reaped, 0};
    }
#ifdef OPENXCOM_LOADER_LINUX_TESTING
    trace(Trace::NativeTryJoin, error);
#endif
    if (error == EBUSY) return {ReapKind::Busy, 0};
    _ownerDiagnostic = {NativeOperation::TryJoin, error, NativeOperation::None, 0};
    _phase = Phase::Poisoned;
    // Keep sole handle and binding on uncertainty, and forbid further polls.
    return {ReapKind::Error, error};
}

LoaderThreadLinux::Diagnostics LoaderThreadLinux::diagnostics() const noexcept
{
    OwnerOperation operation(*this);
    Diagnostics result{_ownerDiagnostic,
                       {NativeOperation::None, 0, NativeOperation::None, 0}, !_workerReadable};
    if (_workerReadable) result.worker = _workerDiagnostic;
    return result;
}

#ifdef OPENXCOM_LOADER_LINUX_TESTING
void LoaderThreadLinux::trace(Trace event, int rawCode) const noexcept
{
    if (_options.trace) _options.trace(_options.context, event, rawCode);
}
#endif
}
