// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2026 OpenXcom contributors. See LICENSE.txt and LICENSE-NOTICES.md.
// NEW RECONSTRUCTION, 2026-10-15: source-only candidate based on retained author
// text and reviewed r2 contracts. This is not byte-identical recovery and no
// historical compile/test success applies to this new source. No command here
// authorizes compilation or execution.
//
// A03 (observer ON), A06 and A07 link the real LoadingAudioHandoff.cpp,
// MixedAudioObserver.cpp, DiagnosticProfile.cpp and MixedAudioSlot.h. No copied
// handoff algorithm, private-field access, test friend or production dispatch.
// The fixture's joined std::thread is an actual harness retirement boundary;
// it does NOT establish event-pump responsiveness or implement a Game backend.
// Those are separate real-LoaderSession cases in loading_audio_handoff_unit.
//
// Prospective argv: loading-audio-observer-unit (no arguments), once normally
// and once with __NO_MUSIC. Both require OPENXCOM_LOADING_AUDIO_HANDOFF and
// OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER. Music-enabled linkage also needs
// actual Engine/Adlib/adlplayer.cpp and Engine/Adlib/fmopl.cpp (and libm).
// Link the real Engine/Options.cpp too; no replacement Options globals or
// diagnostic implementation is supplied. Function/data sections and linker GC
// may discard unused option/file-loading paths. OptionInfo/ModInfo have no
// user-declared destructors; their empty global containers use inline standard
// destruction. No extra production TU is source-indicated for that path, but
// object/link closure must confirm this without unresolved-symbol suppression.
//
// Required ELF --wrap names (C unless explicitly mangled):
// SDL_WasInit, SDL_AudioDriverName, SDL_PauseAudio, SDL_LockAudio,
// SDL_UnlockAudio, SDL_GetAudioStatus, Mix_Linked_Version, Mix_QuerySpec,
// Mix_SetPostMix, Mix_HookMusic, Mix_HaltChannel, Mix_HaltMusic, Mix_OpenAudio,
// Mix_CloseAudio, fcntl, close, open, read, send, recv,
// _ZN8OpenXcom8Observer26diagnosticProfileQualifiedEv,
// and, unless __NO_MUSIC, _Z9func_mutev.
// Compile without LTO or fortification/large-file remapping which could rename
// the POSIX leaves. Exact providers, symbols, flags, finite process/output caps
// and full commands still require independent object/command review/release.
//
// Deadline/clock limitation is deliberate: no stable clock leaf is admitted
// by this reconstruction. It neither sleeps 30 seconds nor changes private
// clocks. A06's unchanged original _started assignment and 30-second absolute
// deadline, and A07's lack of a worker clock call, remain SOURCE-CONTRACT evidence
// only. Actual deadline execution is separately deferred. A host pause consuming
// a real observer deadline is failure/inconclusive, never permission to extend it.

#include "../../src/Engine/LoadingAudioHandoff.h"
#include "../../src/Observer/MixedAudioObserver.h"
#include "../../src/Observer/DiagnosticProfile.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Logger.h"
#include <SDL_mixer.h>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>
#ifndef __NO_MUSIC
#include "../../src/Engine/Adlib/fmopl.h"
extern FM_OPL *opl[2]; // Actual adlplayer.cpp definition, never overwritten.
#endif
#if !defined(OPENXCOM_LOADING_AUDIO_HANDOFF) || !defined(OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER)
#error "This fixture requires the helper and observer ON profiles"
#endif
#if !defined(__linux__) || !defined(__GLIBC__)
#error "This fixture proposes only the reviewed Linux/glibc ELF profile"
#endif

namespace
{
using OpenXcom::LoadingAudioHandoff;
using OpenXcom::Observer::MixedAudioObserver;
using OpenXcom::Observer::MixedAudioSlot;
typedef LoadingAudioHandoff Handoff;
typedef Handoff::ObserverDisposition ObserverDisposition;
typedef void (SDLCALL *Postmix)(void *, Uint8 *, int);
const std::thread::id owner = std::this_thread::get_id();
thread_local bool preparing = false;
thread_local unsigned outerLockDepth = 0;
std::mutex callbackBoundary;
unsigned checks = 0;

[[noreturn]] void fail(const char *message) noexcept
{
    std::fprintf(stderr, "LOADING_AUDIO_OBSERVER_UNIT FAIL: %s\n", message);
    std::_Exit(1); // Never unwind a live worker/context after a failed assertion.
}
void check(bool value, const char *message) noexcept
{
    if (!value) fail(message);
    // This counter is touched serially: owner, one joined callback, one joined
    // handoff worker. No two fixture assertion writers exist concurrently.
    ++checks;
}
void requireOwner(const char *message) noexcept
{
    check(!preparing && std::this_thread::get_id() == owner, message);
}
void requireWorker(const char *message) noexcept
{
    check(preparing && std::this_thread::get_id() != owner, message);
}
enum class Event { Query, Pause, Lock, Unlock, PostmixDetached, ObserverClosed,
                   MusicDetached, ChannelsHalted, MusicHalted, AdlibMute, Permit };
struct Model
{
    Event trace[32];
    unsigned traceSize, ownerCapture, driverCapture, attaches, detaches;
    unsigned ownedCloseCalls, sends, receives, tokenOpens, tokenReads, diagnostics;
    unsigned callbackCalls;
    int inheritedFd, ownedFd, openCount;
    bool inheritedConsumed, paused, drained, closeReturnsEintr;
    Postmix callback;
    void *callbackContext;
    MixedAudioSlot *slot;
    Model() : traceSize(0), ownerCapture(0), driverCapture(0), attaches(0), detaches(0),
        ownedCloseCalls(0), sends(0), receives(0), tokenOpens(0), tokenReads(0),
        diagnostics(0), callbackCalls(0), inheritedFd(-1), ownedFd(-1), openCount(1),
        inheritedConsumed(false), paused(false), drained(false), closeReturnsEintr(false),
        callback(nullptr), callbackContext(nullptr), slot(nullptr) {}
    void append(Event event) noexcept
    {
        check(traceSize < sizeof(trace) / sizeof(trace[0]), "fixed trace overflow");
        trace[traceSize++] = event;
    }
} model;
const SDL_version linkedVersion = {1, 2, 12};

// A real connected local socket is consumed by the production observer. Only
// its peer remains harness-owned; neither path retries an ambiguous close.
struct SocketPair
{
    int peer, inherited;
    char descriptor[32];
    explicit SocketPair(bool enabled) : peer(-1), inherited(-1), descriptor{}
    {
        requireOwner("socket creation must be owner-only");
        if (!enabled) return;
        int pair[2];
        check(::socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) == 0, "socketpair");
        check(pair[0] >= 3 && pair[1] >= 3, "harness requires ordinary open stdio");
        peer = pair[0]; inherited = pair[1];
        model.inheritedFd = inherited;
        std::snprintf(descriptor, sizeof(descriptor), "%d", inherited);
    }
    ~SocketPair()
    {
        requireOwner("socket destruction before worker retirement");
        if (peer >= 0) check(::close(peer) == 0, "peer close");
        if (inherited >= 0 && !model.inheritedConsumed)
            check(::close(inherited) == 0, "unconsumed inherited close");
    }
};
struct Context
{
    SocketPair transport;
    MixedAudioObserver observer;
    Handoff handoff;
    Handoff::Outcome outcome;
    bool permitSpent;
    explicit Context(bool enabled = true, bool present = true) :
        transport(enabled), observer(enabled ? transport.descriptor : nullptr),
        handoff(present ? &observer : nullptr), outcome{}, permitSpent(false)
    {
        check(model.ownerCapture == 1 && model.driverCapture == 1,
              "owner snapshot captured exactly once");
        check(model.traceSize == 0, "capture must not perform audio mutation/query");
        if (enabled)
            check(observer.enabled() && model.inheritedConsumed && model.ownedFd >= 0,
                  "production observer owns its duplicated descriptor");
        else check(!observer.enabled(), "null descriptor starts disabled");
    }
};
void expectNoBytes(int peer, bool eof)
{
    requireOwner("peer observation before checked worker retirement");
    char bytes[102400];
    errno = 0;
    const ssize_t count = ::recv(peer, bytes, sizeof(bytes), MSG_DONTWAIT);
    if (eof) check(count == 0, "terminal observer must yield EOF without packets");
    else check(count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK),
               "live bootstrap must yield EAGAIN, not EOF/hello/PCM");
}
std::string receiveHello(Context &context)
{
    char bytes[128];
    const ssize_t count = ::recv(context.transport.peer, bytes, sizeof(bytes), MSG_DONTWAIT);
    const std::string prefix = "OPENXCOM-MIXED-AUDIO/1 ";
    check(count == static_cast<ssize_t>(prefix.size() + 64), "exact hello length");
    const std::string hello(bytes, static_cast<std::size_t>(count));
    check(hello.compare(0, prefix.size(), prefix) == 0, "hello protocol prefix");
    const std::string token = hello.substr(prefix.size());
    check(token.find_first_not_of("0123456789abcdef") == std::string::npos,
          "production tokens are hex");
    check(model.callback && model.slot && model.slot->state() == MixedAudioSlot::IDLE,
          "real postmix hook starts with real IDLE slot");
    return token;
}
std::string hello(Context &context)
{
    requireOwner("catalog and hello on owner only");
    context.observer.publishCatalogSupported(true);
    context.observer.poll();
    return receiveHello(context);
}
void arm(Context &context, const std::string &token)
{
    requireOwner("admission on owner only");
    const std::string request = token + std::string(32, 'c');
    check(request.size() == 96, "request uses actual received epoch/admission");
    check(::send(context.transport.peer, request.data(), request.size(), MSG_NOSIGNAL) == 96,
          "send legitimate owner protocol request");
    context.observer.poll();
    check(context.observer.enabled() && model.slot && model.slot->state() == MixedAudioSlot::ARMED,
          "only real protocol arms the real slot");
    expectNoBytes(context.transport.peer, false);
}
void callbackTo(bool ready)
{
    // Run the actual captured production postmix function under the same modeled
    // mutex as the SDL_LockAudio leaf. This is not actual SDL driver evidence.
    std::thread callback([ready]() {
        std::lock_guard<std::mutex> ownership(callbackBoundary);
        check(std::this_thread::get_id() != owner && !model.paused && model.callback,
              "callback requires owned, unpaused boundary and registered hook");
        Uint8 bytes[MixedAudioSlot::Bytes];
        std::memset(bytes, 0x5a, sizeof(bytes));
        model.callback(model.callbackContext, bytes, sizeof(bytes));
        ++model.callbackCalls;
        check(model.slot->state() == MixedAudioSlot::CAPTURING,
              "first post-admission callback is skipped");
        if (ready)
        {
            model.callback(model.callbackContext, bytes, sizeof(bytes));
            ++model.callbackCalls;
            check(model.slot->state() == MixedAudioSlot::READY, "real slot captured full frame count");
            check(model.slot->readyData()[0] == 0x5a &&
                  model.slot->readyData()[MixedAudioSlot::Bytes - 1] == 0x5a,
                  "actual callback copied modeled PCM");
        }
    });
    callback.join(); // Harness only: checked actual callback/TLS retirement.
}
void expectTrace(bool workerClosed)
{
    const Event expected[] = { Event::Query, Event::Pause, Event::Lock,
        Event::Unlock, Event::PostmixDetached, Event::ObserverClosed,
        Event::MusicDetached, Event::ChannelsHalted, Event::MusicHalted,
#ifndef __NO_MUSIC
        Event::AdlibMute,
#endif
        Event::Permit };
    unsigned actual = 0;
    for (unsigned i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
    {
        if (expected[i] == Event::ObserverClosed && !workerClosed) continue;
        check(actual < model.traceSize && model.trace[actual++] == expected[i],
              "exact production handoff/observer-close order");
    }
    check(actual == model.traceSize, "no extra native/permit events");
}
void prepareAndJoin(const std::shared_ptr<Context> &context, ObserverDisposition expected,
                    bool workerClosed)
{
    requireOwner("handoff sealing on owner");
    const unsigned sends = model.sends, receives = model.receives;
    const unsigned opens = model.tokenOpens, reads = model.tokenReads;
    const unsigned diagnostics = model.diagnostics, callbacks = model.callbackCalls;
    const unsigned detaches = model.detaches, closes = model.ownedCloseCalls;
    context->handoff.sealForWorker();
    // Strongly capture the COMPLETE context, not a borrowed observer alias.
    std::thread worker([context]() {
        preparing = true;
        Handoff::PrepareResult result = context->handoff.prepare();
        check(result.outcome.disposition == Handoff::Disposition::ResourceReady &&
              result.outcome.reason == Handoff::Reason::None &&
              result.outcome.lastCompletedPhase == Handoff::Phase::ResourceReady,
              "admitted helper reaches fixed ResourceReady outcome");
        check(result.permit.validFor(context->handoff), "permit bound to exact owned context");
        model.append(Event::Permit);
        Handoff::ResourceMutationPermit moved(std::move(result.permit));
        check(!result.permit.validFor(context->handoff) && moved.validFor(context->handoff),
              "moving permit invalidates source");
        moved.invalidate();
        check(!moved.validFor(context->handoff), "permit spent before owner observer publication");
        context->outcome = result.outcome;
        context->permitSpent = true;
        preparing = false;
    });
    // No owner observer/audio/resource/outcome sampling occurs until join returns.
    // This blocking harness join is NOT proposed as a production owner path.
    worker.join();
    check(context->permitSpent && context->outcome.observer == expected,
          "read fixed outcome only after actual joined retirement");
    check(context->outcome.actual.openCount == 1 && context->outcome.actual.frequency == 22050 &&
          context->outcome.actual.format == AUDIO_S16LSB && context->outcome.actual.channels == 2,
          "actual queried spec preserved");
    check(model.paused && model.drained && !model.callback && !model.callbackContext,
          "continuous pause plus detached hook retained");
    check(model.detaches == detaches + 1 && model.ownedCloseCalls == closes + (workerClosed ? 1u : 0u),
          "exactly one global detach and expected owned-descriptor close");
    check(model.sends == sends && model.receives == receives && model.tokenOpens == opens &&
          model.tokenReads == reads && model.diagnostics == diagnostics && model.callbackCalls == callbacks,
          "worker performs no protocol, token, qualification or owner callback work");
    expectTrace(workerClosed);
}
void neverRevives(Context &context, bool hasPeer)
{
    check(!context.observer.enabled(), "terminal/disabled observer remains disabled");
    const unsigned closes = model.ownedCloseCalls, attaches = model.attaches;
    const unsigned sends = model.sends, opens = model.tokenOpens;
    const unsigned diagnostics = model.diagnostics;
    for (bool supported : {true, false, true})
    {
        context.observer.publishCatalogSupported(supported);
        context.observer.poll();
        check(!context.observer.enabled() && !model.callback, "owner publication cannot revive closed observer");
    }
    context.observer.beforeAudioReset(true);
    context.observer.close();
    check(model.ownedCloseCalls == closes && model.attaches == attaches && model.sends == sends &&
          model.tokenOpens == opens && model.diagnostics == diagnostics,
          "later owner APIs do not retry close, qualify, tokenize, attach or send");
    if (model.slot)
        check(model.slot->state() == MixedAudioSlot::CLOSED && !model.slot->arm(),
              "real CLOSED slot is terminal and cannot rearm");
    if (hasPeer) expectNoBytes(context.transport.peer, true);
}
} // namespace

// Native leaves have exact declared signatures. Mixer leaves never run with an
// outer SDL lock held; only the instrumented empty drain owns that lock.
extern "C"
{
Uint32 SDLCALL __wrap_SDL_WasInit(Uint32 flags)
{
    requireOwner("SDL_WasInit capture moved to worker");
    check(flags == SDL_INIT_AUDIO, "capture queries only audio domain");
    ++model.ownerCapture; return SDL_INIT_AUDIO;
}
char *SDLCALL __wrap_SDL_AudioDriverName(char *name, int length)
{
    requireOwner("driver capture moved to worker");
    check(name && length >= 6, "bounded driver output buffer");
    ++model.driverCapture; std::memcpy(name, "dummy", 6); return name;
}
void SDLCALL __wrap_SDL_PauseAudio(int pause)
{
    requireWorker("pause only in handoff worker");
    check(pause == 1 && !outerLockDepth && !model.paused, "one pause, never unpause");
    model.paused = true; model.append(Event::Pause);
}
void SDLCALL __wrap_SDL_LockAudio()
{
    requireWorker("audio drain lock only in handoff worker");
    check(!outerLockDepth && model.paused, "drain begins after pause");
    callbackBoundary.lock(); ++outerLockDepth; model.append(Event::Lock);
}
void SDLCALL __wrap_SDL_UnlockAudio()
{
    requireWorker("audio unlock only in handoff worker");
    check(outerLockDepth == 1, "matching drain unlock");
    --outerLockDepth; callbackBoundary.unlock();
    model.drained = true; model.append(Event::Unlock);
}
SDL_audiostatus SDLCALL __wrap_SDL_GetAudioStatus()
{ fail("forbidden SDL_GetAudioStatus"); }
const SDL_version *SDLCALL __wrap_Mix_Linked_Version()
{ requireOwner("worker mixer-version qualification"); return &linkedVersion; }
int SDLCALL __wrap_Mix_QuerySpec(int *frequency, Uint16 *format, int *channels)
{
    check(!outerLockDepth && frequency && format && channels, "query outside outer lock");
    if (preparing)
    {
        check(*frequency == 0 && *format == 0 && *channels == 0, "zeroed actual spec");
        model.append(Event::Query);
    }
    else requireOwner("query on unexpected thread");
    // Retained SDL_mixer leaves output storage untouched when no mixer is open.
    if (model.openCount == 0) return 0;
    *frequency = 22050; *format = AUDIO_S16LSB; *channels = 2; return model.openCount;
}
void SDLCALL __wrap_Mix_SetPostMix(Postmix callback, void *context)
{
    check(!outerLockDepth, "postmix setter must not nest outer SDL lock");
    if (preparing)
    {
        check(!callback && !context && model.drained && model.paused,
              "worker detach only after completed drain");
        model.append(Event::PostmixDetached);
    }
    else requireOwner("owner hook setter on unexpected thread");
    std::lock_guard<std::mutex> ownership(callbackBoundary);
    model.callback = callback; model.callbackContext = context;
    if (callback)
    {
        check(context != nullptr, "actual callback has slot userdata");
        model.slot = static_cast<MixedAudioSlot *>(context); ++model.attaches;
    }
    else { check(!context, "null detach context"); ++model.detaches; }
}
void SDLCALL __wrap_Mix_HookMusic(Postmix callback, void *context)
{
    requireWorker("music hook detach on worker");
    check(!outerLockDepth && !callback && !context && model.drained && !model.callback,
          "music detach follows completed postmix detach outside lock");
    model.append(Event::MusicDetached);
}
int SDLCALL __wrap_Mix_HaltChannel(int channel)
{
    requireWorker("channel halt on worker");
    check(!outerLockDepth && channel == -1, "halt all channels outside outer lock");
    model.append(Event::ChannelsHalted); return 0;
}
int SDLCALL __wrap_Mix_HaltMusic()
{
    requireWorker("music halt on worker");
    check(!outerLockDepth, "music halt outside outer lock");
    model.append(Event::MusicHalted); return 0;
}
int SDLCALL __wrap_Mix_OpenAudio(int, Uint16, int, int)
{ fail("forbidden mixer open"); }
void SDLCALL __wrap_Mix_CloseAudio()
{ fail("forbidden mixer close"); }

int __real_fcntl(int, int, ...);
int __wrap_fcntl(int fd, int command, ...)
{
    requireOwner("worker descriptor manipulation");
    int result = -1;
    if (command == F_GETFL) result = __real_fcntl(fd, command);
    else
    {
        check(command == F_DUPFD_CLOEXEC || command == F_SETFL, "unexpected fcntl command");
        va_list arguments; va_start(arguments, command);
        const int argument = va_arg(arguments, int); va_end(arguments);
        result = __real_fcntl(fd, command, argument);
        if (command == F_DUPFD_CLOEXEC)
        {
            check(fd == model.inheritedFd && model.ownedFd < 0, "one real inherited descriptor duplication");
            model.ownedFd = result;
        }
    }
    return result;
}
int __real_close(int);
int __wrap_close(int fd)
{
    if (fd == model.ownedFd && fd >= 0)
    {
        check(!model.callback && !outerLockDepth, "descriptor cannot close before hook detach");
        if (model.slot) check(model.slot->state() == MixedAudioSlot::CLOSED,
                              "real terminal slot close precedes descriptor release");
        ++model.ownedCloseCalls;
        check(model.ownedCloseCalls == 1, "owned descriptor close attempted only once");
        if (preparing)
        {
            check(model.drained && model.paused, "worker close follows completed drain/detach");
            model.append(Event::ObserverClosed);
        }
        const int result = __real_close(fd);
        check(result == 0, "actual owned descriptor closes successfully in fixture");
        if (model.closeReturnsEintr) { errno = EINTR; return -1; }
        return result;
    }
    check(!preparing, "worker closes an unrelated descriptor/token source");
    if (fd == model.inheritedFd) model.inheritedConsumed = true;
    return __real_close(fd);
}
int __real_open(const char *, int, ...);
int __wrap_open(const char *path, int flags, ...)
{
    requireOwner("worker attempts token generation/file opening");
    check(path && std::strcmp(path, "/dev/urandom") == 0 && !(flags & O_CREAT),
          "only real observer token-source opening is admitted");
    ++model.tokenOpens; return __real_open(path, flags);
}
ssize_t __real_read(int, void *, size_t);
ssize_t __wrap_read(int fd, void *buffer, size_t count)
{
    requireOwner("worker attempts token read");
    ++model.tokenReads; return __real_read(fd, buffer, count);
}
ssize_t __real_send(int, const void *, size_t, int);
ssize_t __wrap_send(int fd, const void *buffer, size_t count, int flags)
{
    requireOwner("worker attempts packet send");
    ++model.sends; return __real_send(fd, buffer, count, flags);
}
ssize_t __real_recv(int, void *, size_t, int);
ssize_t __wrap_recv(int fd, void *buffer, size_t count, int flags)
{
    requireOwner("worker attempts packet receive/peek");
    ++model.receives; return __real_recv(fd, buffer, count, flags);
}
bool __real__ZN8OpenXcom8Observer26diagnosticProfileQualifiedEv();
bool __wrap__ZN8OpenXcom8Observer26diagnosticProfileQualifiedEv()
{
    requireOwner("worker enters main-only diagnostic qualification");
    ++model.diagnostics;
    return __real__ZN8OpenXcom8Observer26diagnosticProfileQualifiedEv();
}
#ifndef __NO_MUSIC
void __real__Z9func_mutev();
void __wrap__Z9func_mutev()
{
    requireWorker("Adlib mute on handoff worker");
    check(!outerLockDepth && opl[0] == nullptr && opl[1] == nullptr,
          "actual Adlib call uses legitimate initialized both-null OPL globals");
    model.append(Event::AdlibMute);
    __real__Z9func_mutev(); // Real native player mutation, never a replacement.
    check(opl[0] == nullptr && opl[1] == nullptr, "mute preserves null OPL ownership");
}
#endif
} // extern C

int main(int argc, char **)
{
    check(argc == 1, "no runtime/deadline bypass flags are supported");
    // Set the real ordinary globals before diagnostic initialization and before
    // any worker exists. No Options::create, file loading or device setup runs.
    OpenXcom::Options::debug = false;
    OpenXcom::Options::debugUi = false;
    OpenXcom::Options::traceAI = false;
    OpenXcom::Options::verboseLogging = false;
    OpenXcom::Logger::reportingLevel() = OpenXcom::LOG_INFO;
    OpenXcom::Observer::initializeDiagnosticProfile();
    check(OpenXcom::Observer::diagnosticProfileQualified(), "normal diagnostics admitted");
    for (bool muted : {false, true})
    {
        OpenXcom::Options::mute = muted;
        // A02/A06: early rejection must report the present observer as NotReset,
        // preserving its descriptor and already-published bootstrap catalog.
        model = Model();
        {
            const std::shared_ptr<Context> context = std::make_shared<Context>();
            context->observer.publishCatalogSupported(true);
            expectNoBytes(context->transport.peer, false);
            const unsigned sends = model.sends, receives = model.receives;
            const unsigned opens = model.tokenOpens, reads = model.tokenReads;
            const unsigned diagnostics = model.diagnostics, callbacks = model.callbackCalls;
            const unsigned attaches = model.attaches, detaches = model.detaches;
            const unsigned closes = model.ownedCloseCalls;
            const int ownedFd = model.ownedFd;
            model.openCount = 0;
            context->handoff.sealForWorker();
            std::thread worker([context]() {
                preparing = true;
                Handoff::PrepareResult result = context->handoff.prepare();
                check(!result.permit.validFor(context->handoff),
                      "no-mixer rejection grants no mutation permit");
                context->outcome = result.outcome;
                preparing = false;
            });
            // No owner observer/outcome access before actual harness retirement.
            worker.join();
            check(context->outcome.disposition == Handoff::Disposition::RejectedRetained &&
                  context->outcome.reason == Handoff::Reason::NoMixer &&
                  context->outcome.lastCompletedPhase == Handoff::Phase::Preflight &&
                  context->outcome.observer == ObserverDisposition::NotReset,
                  "present observer remains explicitly unexamined/unreset on rejection");
            check(context->outcome.actual.openCount == 0 && context->outcome.actual.frequency == 0 &&
                  context->outcome.actual.format == 0 && context->outcome.actual.channels == 0,
                  "zero-open query preserves zero-initialized actual spec");
            check(model.traceSize == 1 && model.trace[0] == Event::Query &&
                  !model.paused && !model.drained && !model.callback && !model.callbackContext,
                  "no-mixer rejection performs no pause, drain, detach, halt, mute or permit effect");
            check(model.ownedFd == ownedFd && model.ownedCloseCalls == closes &&
                  model.attaches == attaches && model.detaches == detaches &&
                  model.sends == sends && model.receives == receives &&
                  model.tokenOpens == opens && model.tokenReads == reads &&
                  model.diagnostics == diagnostics && model.callbackCalls == callbacks &&
                  model.ownerCapture == 1 && model.driverCapture == 1,
                  "rejected worker leaves observer descriptor, protocol and qualification untouched");
            check(context->observer.enabled(), "rejected bootstrap descriptor remains live");
            expectNoBytes(context->transport.peer, false);
            model.openCount = 1;
            context->observer.poll(); // Intentionally do NOT republish the catalog.
            receiveHello(*context);
            check(model.attaches == attaches + 1, "pre-rejection catalog still enables legitimate hello");
            context->observer.close();
            neverRevives(*context, true);
        }
        check(model.ownedCloseCalls == 1, "rejected bootstrap closes only in later owner cleanup");
        // A06: even a previously published catalog must be invalidated. Bootstrap
        // emits nothing and keeps its descriptor; only reaped owner publication
        // and the legitimate peer exchange may later arm the unclosed real slot.
        model = Model();
        {
            const std::shared_ptr<Context> context = std::make_shared<Context>();
            context->observer.publishCatalogSupported(true);
            prepareAndJoin(context, ObserverDisposition::BootstrapPreserved, false);
            check(context->observer.enabled() && !model.attaches && !model.tokenOpens && !model.sends,
                  "pre-hello reset preserves live descriptor without bootstrap traffic");
            context->observer.poll();
            expectNoBytes(context->transport.peer, false);
            check(!model.attaches && !model.tokenOpens, "reset invalidated formerly supported catalog");
            const std::string token = hello(*context);
            arm(*context, token);
            check(model.paused, "owner protocol did not unpause the admitted mixer");
            context->observer.close();
            neverRevives(*context, true);
        }
        check(model.ownedCloseCalls == 1, "bootstrap eventual owner/destructor close is once-only");
        // A07: actual hello, armed, capturing and ready production states. No
        // pending READY payload may escape during reset or later owner polling.
        for (unsigned stage = 0; stage < 4; ++stage)
        {
            model = Model();
            {
                const std::shared_ptr<Context> context = std::make_shared<Context>();
                const std::string token = hello(*context);
                if (stage >= 1) arm(*context, token);
                if (stage >= 2) callbackTo(stage == 3);
                expectNoBytes(context->transport.peer, false);
                // A returned close error is ambiguous; it must never cause retry.
                model.closeReturnsEintr = (stage == 3);
                prepareAndJoin(context, ObserverDisposition::EpochClosed, true);
                check(model.slot->state() == MixedAudioSlot::CLOSED,
                      "hello/armed/capturing/ready epoch closes terminally");
                neverRevives(*context, true);
            }
            check(model.ownedCloseCalls == 1, "terminal destructor does not close again");
        }
        // Disabled from construction, closed before hello, and already-closed
        // hello epoch are distinct input states, none may regain an epoch.
        for (unsigned stage = 0; stage < 3; ++stage)
        {
            model = Model();
            {
                const std::shared_ptr<Context> context = std::make_shared<Context>(stage != 0);
                if (stage == 2) hello(*context);
                if (stage != 0) context->observer.close();
                prepareAndJoin(context, stage == 2 ? ObserverDisposition::EpochClosed :
                    ObserverDisposition::DisabledRetained, false);
                neverRevives(*context, stage != 0);
            }
            check(model.ownedCloseCalls == (stage ? 1u : 0u), "already-disabled close count unchanged");
        }
        // Observer compiled ON but no observer in the handoff is explicit Absent.
        model = Model();
        {
            const std::shared_ptr<Context> context = std::make_shared<Context>(false, false);
            prepareAndJoin(context, ObserverDisposition::Absent, false);
            neverRevives(*context, false);
        }
    }
    check(checks > 0, "fixture performed checks");
    std::puts("PASS observer-linked A03/A06/A07: real protocol, slot, callback, "
              "once-only close, no revival; modeled native leaves only");
    std::puts("DEFERRED: deadline/worker-clock claims use source evidence; stable clock leaf, "
              "real-time deadline, native dummy/provider runtime and owner responsiveness are unproven here.");
    return 0;
}
