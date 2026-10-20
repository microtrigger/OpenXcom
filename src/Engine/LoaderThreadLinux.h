#pragma once
// Unintegrated Linux/glibc responsive-loader backend; not activated by Game.
// Copyright 2026 OpenXcom contributors. GPL-3.0-or-later; see LICENSE.txt and LICENSE-NOTICES.md.
#include "LoaderSession.h"
#include <cstddef>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>

// SDL types deliberately do not cross this project-local ABI.
extern "C" void OpenXcom_LoaderSdlTlsCleanup() noexcept;

namespace OpenXcom
{
/** One owned joinable native thread, retired only by checked tryjoin success.
 * Owner calls do not wait for target-thread progress. Successful native reclaim
 * may take libc allocator/stack-cache locks: this is not a hard-time guarantee.
 * Initialize SDL on the owner before start; keep SDL/DSOs alive through reap.
 * Work must return normally (exceptions caught by LoaderSession), must not use
 * native cancellation/exit, and must not re-enable native cancellation.
 * Later native TLS destructors must not recreate SDL TLS after the bridge.
 */
class LoaderThreadLinux final : public LoaderThreadBackend
{
public:
    enum class NativeOperation
    {
        None, SignalEmpty, SignalAdd, AttrInit, AttrSetDetach, AttrGetDetach,
        AttrSetInherit, AttrGetInherit, AttrSetGuard, AttrDestroy, Create,
        CancelState, SignalMask, TryJoin, WrapperResult, Binding
    };
    struct Diagnostic
    {
        NativeOperation operation;
        int code;
        NativeOperation secondaryOperation;
        int secondaryCode;
    };
    struct Diagnostics { Diagnostic owner; Diagnostic worker; bool workerPending; };

    LoaderThreadLinux() noexcept;
    ~LoaderThreadLinux() noexcept override;
    StartResult start(Entry entry, void *context) noexcept override;
    ReapResult pollReap() noexcept override;
    // Owner only. No worker field is read until actual native retirement, even
    // if the session has already published a terminal value.
    Diagnostics diagnostics() const noexcept;
    LoaderThreadLinux(const LoaderThreadLinux &) = delete;
    LoaderThreadLinux &operator=(const LoaderThreadLinux &) = delete;
    LoaderThreadLinux(LoaderThreadLinux &&) = delete;
    LoaderThreadLinux &operator=(LoaderThreadLinux &&) = delete;

private:
    enum class Phase { Empty, Creating, Owned, Poisoned };
    class OwnerOperation;
    static void *threadMain(void *context) noexcept;
    StartResult failStart(NativeOperation operation, int code, pthread_attr_t *initializedAttr) noexcept;
    const pid_t _process;
    const pthread_t _owner;
    mutable bool _inside;
    Phase _phase;
    bool _hasThread;
    pthread_t _thread; // Unreadable unless _hasThread grants sole ownership.
    Entry _entry;
    void *_context;
    sigset_t _mask;
    Diagnostic _ownerDiagnostic;
    Diagnostic _workerDiagnostic;
    bool _workerReadable; // Owner-only permission granted by no-thread/reap.

#ifdef OPENXCOM_LOADER_LINUX_TESTING
    friend struct LoaderThreadLinuxTestAccess;
    enum class Trace
    {
        AttrGuardSize, NativeCreate, HandleInstalled, WorkerEntered, CancelState,
        SignalMask, CallbackReturned, SdlCleanupEntered, SdlCleanupReturned,
        WrapperReturning, NativeTryJoin, OwnershipCleared
    };
    // C++11 cannot encode noexcept in pointer typedefs. Hooks must be noexcept.
    typedef void (*TraceHook)(void *, Trace, int);
    struct Options
    {
        void *context;
        TraceHook trace;
        void (*afterCreate)(void *);
        int sigmaskHow;
        // An immutable copied argument schedule, never native-result injection.
        // Zero count uses ordinary attrs forever. Otherwise exactly count
        // attempts (maximum four) are permitted; exhaustion is fail-stop.
        unsigned guardCount;
        bool guardEnabled[4];
        std::size_t guardSize[4];
    };
    const Options _options;
    unsigned _testAttempt;
    explicit LoaderThreadLinux(Options options) noexcept;
    void trace(Trace event, int rawCode) const noexcept;
#endif
};
}
