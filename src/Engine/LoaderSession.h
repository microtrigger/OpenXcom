#pragma once
// Backend-neutral responsive-loader infrastructure. Not yet used by Game.
// Copyright 2026 OpenXcom contributors. GPL-3.0-or-later; see LICENSE.txt and LICENSE-NOTICES.md.
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace OpenXcom
{
/** A backend owns one callback and its entire epilogue until checked Reaped.
 * Both operations MUST be noexcept and nonwaiting on worker progress. Started
 * retains exactly one token and invokes entry exactly once. Entry receives zero
 * for successful setup or a positive native setup error; negative is fatal.
 * NoThread proves no outstanding token/callback/worker at return, not that no
 * kernel task ever existed. Any uncertain ownership is InvariantError.
 * Reaped requires an independent actual thread-retirement receipt, including
 * epilogues/TLS; terminal publication is insufficient. Error forbids retries.
 * Started, Busy and Reaped carry nativeError == 0; errors retain diagnostics.
 * This interface supplies no native backend or platform retirement proof.
 */
class LoaderThreadBackend
{
public:
    typedef void (*Entry)(void *, int setupError);
    enum class StartKind { Started, NoThread, InvariantError };
    enum class ReapKind { Busy, Reaped, Error };
    struct StartResult { StartKind kind; int nativeError; };
    struct ReapResult { ReapKind kind; int nativeError; };
    LoaderThreadBackend() = default;
    virtual ~LoaderThreadBackend() noexcept = default;
    virtual StartResult start(Entry entry, void *context) noexcept = 0;
    virtual ReapResult pollReap() noexcept = 0;
    LoaderThreadBackend(const LoaderThreadBackend &) = delete;
    LoaderThreadBackend &operator=(const LoaderThreadBackend &) = delete;
};

/** Nonwaiting single-process-domain admission. A denied lease cannot release
 * another lease. This does not establish Game member-construction ordering.
 */
class LoaderProcessLease
{
public:
    LoaderProcessLease() noexcept;
    ~LoaderProcessLease() noexcept;
    bool acquired() const noexcept;
    void release() noexcept;
    LoaderProcessLease(const LoaderProcessLease &) = delete;
    LoaderProcessLease &operator=(const LoaderProcessLease &) = delete;
private:
    const std::thread::id _owner;
    bool _held;
};

class LoaderSession
{
public:
    class StopToken
    {
    public:
        bool requested() const noexcept { return _stop.load(std::memory_order_acquire); }
    private:
        friend class LoaderSession;
        explicit StopToken(const std::atomic<bool> &stop) noexcept : _stop(stop) {}
        const std::atomic<bool> &_stop;
    };
    enum class WorkResult { Success, Cancelled };
    typedef WorkResult (*Work)(void *, const StopToken &);
    enum class BeginResult { Started, Busy, Closed, StartFailed, Quarantined };
    enum class PollResult { Idle, Running, Ready, Closed, Quarantined };
    enum class Terminal { Pending, Success, Failure, Cancelled };
    enum class Reason { None, StandardException, UnknownException,
                        ErrorDetailsUnavailable, NoThread, BackendInvariant,
                        ReapError, InvalidPublication, AttemptOverflow,
                        ThreadSetupFailure };
    struct Result
    {
        std::uint64_t attempt;
        Terminal terminal;
        Reason reason;
        int nativeError;
        bool detailsAvailable;
        std::string details;
        Result() : attempt(0), terminal(Terminal::Pending), reason(Reason::None),
                   nativeError(0), detailsAvailable(false) {}
        void swap(Result &other) noexcept;
    };

    /** Strong lifetime ownership, not borrowed references: the backend and
     * work object stay alive through every outstanding callback/epilogue.
     * context must own the complete work object, never a no-op/borrowed alias.
     * Resources reached by that object remain the integration caller's duty.
     * Neither binding may call back into an owner operation. The stop token
     * cannot escape work. Construction rejects non-lock-free shared atomics.
     */
    LoaderSession(std::shared_ptr<LoaderThreadBackend> backend, Work work,
                  std::shared_ptr<void> context);
    /** Illegal active/Starting/quarantined destruction terminates BEFORE any
     * member/context destruction. No wait, detach, or resource reclamation.
     */
    ~LoaderSession() noexcept;
    BeginResult begin() noexcept;
    void requestClose() noexcept;
    PollResult poll() noexcept;
    /** False for Idle, Active, Quarantined or consumed/never-started Closed.
     * Copy into temporary, then noexcept swap into output, then consume.
     * Allocation/injected copy failure leaves both output and session intact.
     */
    bool copyResult(Result &output);
    bool quiescent() const noexcept;
    bool admissionClosed() const noexcept;
    std::uint64_t attempt() const noexcept;
    // Notification only; never grants access to result/detail or resources.
    bool terminalPublished() const noexcept;
    LoaderSession(const LoaderSession &) = delete;
    LoaderSession &operator=(const LoaderSession &) = delete;
private:
    enum class Phase { Idle, Starting, Active, Ready, Closed, Quarantined };
    enum Signal { Pending, Working, Success, Failure, Cancelled };
    class Operation;
    static void entry(void *context, int setupError) noexcept;
    void run(int setupError) noexcept;
    void quarantine(Reason reason, int error) noexcept;
    const std::shared_ptr<LoaderThreadBackend> _backend;
    const Work _work;
    const std::shared_ptr<void> _context;
    const std::thread::id _owner;
    mutable bool _inside;
    Phase _phase;
    bool _closed;
    bool _outstanding;
    bool _hasAttempt;
    bool _consumed;
    std::uint64_t _attempt;
    std::atomic<unsigned> _terminal;
    std::atomic<bool> _stop;
    // Worker-owned until checked reap (or owner-owned proven NoThread).
    Reason _reason;
    int _workerNativeError;
    bool _detailsAvailable;
    std::string _details;
    // Owner-only diagnostics, separate from worker-written result fields.
    Reason _quarantineReason;
    int _nativeError;
#ifdef OPENXCOM_LOADER_LIFETIME_TESTING
    friend struct LoaderSessionTestAccess;
    enum class Trace { Reserved, WorkerEntry, WorkTerminal, LastCallbackAccess,
                       BackendReaped, ResultCopied, ResultConsumed, Destroying };
    // C++11 cannot encode noexcept in pointer typedefs. Hook functions must
    // be noexcept; violating this contract terminates at the noexcept caller.
    typedef void (*TraceHook)(void *, Trace);
    typedef bool (*FailureHook)(void *);
    struct Hooks { void *context; TraceHook trace; FailureHook failDetail; FailureHook failCopy; };
    const Hooks _hooks;
    LoaderSession(std::shared_ptr<LoaderThreadBackend>, Work, std::shared_ptr<void>, Hooks);
    void trace(Trace event) const noexcept;
#endif
};
}
