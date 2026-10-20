#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Nonshipping fixture interface. This is NOT an alternate Game scheduler.
// The accessors are defined in actual Game.cpp after its opaque core types.
#if !defined(OPENXCOM_RESPONSIVE_LOADING) || !defined(OPENXCOM_RESPONSIVE_TESTING)
#error The Game integration fixture requires the responsive nonshipping profile.
#endif
#include "../../src/Engine/Game.h"
#include "../../src/Engine/LoaderSession.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <set>
#include <vector>

namespace OpenXcom
{
struct LoadingGameIntegrationAccess
{
    using BackendFactory = std::shared_ptr<LoaderThreadBackend> (*)();
    using SessionFactory = std::shared_ptr<LoaderSession> (*)(std::shared_ptr<LoaderThreadBackend>, LoaderSession::Work, std::shared_ptr<void>);
    static void setBackendFactory(Game &, BackendFactory);
    static void setSessionFactory(Game &, SessionFactory);
    static void setTrace(Game &, void (*)(const char *));
    static bool commitBoundary(Game &) noexcept;
    static void tick(Game &) noexcept;
    static bool videoTick(Game &) noexcept;
    static bool hasOperation(const Game &);
    static bool workerOwnsEpoch(const Game &);
    static bool finalClosed(const Game &);
    static bool hasStandby(const Game &);
    static std::shared_ptr<const LoaderSession> primarySession(const Game &);
    static std::shared_ptr<const LoaderSession> cleanupSession(const Game &);
    static State *topState(Game &);
    static std::size_t deferredCount(const Game &);
    static SDL_Surface *terminalFrameDestination(Game &);
    static void callbackDepth(Game &, unsigned);
    // Test setup only: a previously deferred State, never a manufactured epoch.
    static void enqueueDeferred(Game &game, State *state) { game._deleted.push_back(state); }
};
}

namespace LoadingGameFixture
{
[[noreturn]] void fail(const char *) noexcept;
void require(bool, const char *) noexcept;
std::uint64_t monotonicNanoseconds() noexcept;
class Gate
{
    std::atomic<bool> _open;
public:
    explicit Gate(bool open = true) noexcept : _open(open) {}
    void hold() noexcept { _open.store(false, std::memory_order_release); }
    void release() noexcept { _open.store(true, std::memory_order_release); }
    bool open() const noexcept { return _open.load(std::memory_order_acquire); }
    void wait() const noexcept;
};

// A forwarding backend. Each successful start invokes the REAL Linux backend;
// only NoThread/setup-error native-leaf fault injection is modeled. Terminal,
// epilogue and actual pthread-key TLS gates are independent of pollReap.
struct BackendControl
{
    enum class Fault { None, NoThreadOnce, SetupFailureOnce };
    Gate entry, epilogue, tls;
    std::atomic<unsigned> starts, polls, actualReaps, entries, terminalReturns;
    std::atomic<unsigned> epilogueEntered, tlsEntered, tlsExited;
    Fault fault;
    explicit BackendControl(Fault = Fault::None);
};

struct NativeModel
{
    std::atomic<int> openCount, frequency, channels, audioStatus;
    std::atomic<unsigned> format;
    std::atomic<unsigned> opens, closes, pauses, unpauses, locks, drains;
    std::atomic<unsigned> hooks, unhooks, postmixChanges, musicPlays, musicHalts, channelHalts;
    std::atomic<unsigned> musicLoads, memoryMusicLoads, musicTypeQueries;
    std::atomic<std::uintptr_t> lastLoadedMusic, lastQueriedMusic, currentMusic;
    std::atomic<unsigned> flips, blits, physicalPaletteChanges, polls, delays, windowResets, sdlQuits;
    std::atomic<unsigned> saves, maps, updates, languageLoads, resourceLoads;
    std::atomic<unsigned> modConstructed, modDestroyed, languageConstructed, languageDestroyed;
    std::atomic<unsigned> activeOwnerViolations, workerSubsystemViolations, genericTailCalls;
    std::atomic<unsigned> workEntered, copyFailures, callbackInvocations, allocationFailures;
    std::atomic<int> failAllocationAfter;
    std::atomic<bool> failFlip, failBlit, failOpen, residualOpenFailure, failLanguage;
    std::atomic<bool> failLoad, throwUnknownLoad, failCopyOnce, failClose, failBuildConversion, failConversion, failVideoStorageAllocation, failHaltMusic;
    std::atomic<Uint32> clock;
    std::atomic<Uint8> focus;
    Gate loadGate;
    unsigned eventBudget;
    std::string temporaryRoot;
    NativeModel();
};

NativeModel &model();
// One case per process. Caller must supply OPENXCOM_LOADING_FIXTURE_DIRECTORY:
// an existing, empty, absolute, non-symlink directory owned by the effective UID
// with exact mode0700 inside the reviewed monitored run namespace. No fallback.
void reset(); // also requires every previous Game/backend and callback retired
// Direct-child basenames only; no traversal/subdirectories. Creation requires
// absence (O_EXCL/O_NOFOLLOW), mode0600, <=8MiB/file, <=16MiB and32files/case.
// These are creation caps; later Logger append growth is a run-monitor limit.
std::string fixturePath(const std::string &basename);
std::string createFixtureFile(const std::string &basename, const void *bytes, std::size_t size);
std::unique_ptr<OpenXcom::Game> makeGame();
void bind(OpenXcom::Game &); // installs immutable backend/session factories only
void setNextBackendFault(BackendControl::Fault);
std::vector<std::shared_ptr<BackendControl>> backends();
void enqueue(const SDL_Event &);
void enqueueQuit();
void enqueueKey(SDLKey);
std::size_t queuedEvents();
void tick(OpenXcom::Game &); // calls actual boundary then actual operation/video method
void loadToPublished(OpenXcom::Game &);
void finish(OpenXcom::Game &); // request actual Game::quit, pump until exact finalClosed
void trace(const char *) noexcept;
bool saw(const char *) noexcept;
std::size_t tracePosition(const char *) noexcept;
void clearTrace() noexcept;
void dumpTrace() noexcept;
void releaseAllGates() noexcept;
void waitFor(const std::atomic<unsigned> &, unsigned minimum, const char *);

// Creates an independent callback-thread borrower of the SAME modeled audio
// mutex used by SDL_LockAudio. It is never an owner-thread drain substitute.
class CallbackHold
{
    struct Impl;
    std::unique_ptr<Impl> _impl;
public:
    CallbackHold();
    ~CallbackHold();
    void release();
    CallbackHold(const CallbackHold &) = delete;
    CallbackHold &operator=(const CallbackHold &) = delete;
};

// Synthetic assets belong to the actual owning maps of one real Mod and are
// destroyed by the actual Mod destructor. They model asset-loading leaves only.
// Actual Music/Sound/Adlib methods must NOT be replaced for video.
OpenXcom::Mod *publishedMod(OpenXcom::Game &);
void provideMusic(OpenXcom::Mod *, const std::string &, std::unique_ptr<OpenXcom::Music>);
void provideSound(OpenXcom::Mod *, const std::string &set, unsigned id);
void aliasFile(const std::string &name, const std::string &path);
void setSoundFolder(const std::set<std::string> &);
void configureVideoAssets(bool music, bool ufoSounds);
void configureIntro(const std::vector<std::string> &videos, const std::vector<std::string> &tracks, bool ufo);
// Event-source leaf hook only. Allows a fixture to enqueue input while actual
// Game::run owns init/handle/think; it must not replace any orchestration method.
void setPollHook(void (*)(OpenXcom::Game &));
// Explicit native decoder results, independent of the name/extension. Configure
// only before a load or while the owner has a published, idle epoch.
void configureMusicProvider(const std::string &path, int type, bool failLoad = false);
void configureMemoryMusicProvider(int type);
void invokeMusicCallback(Uint8 *destination, int length);

// Link manifests enumerate wrapper aliases without relying on an invented C++
// ABI spelling. Later command review MUST resolve each demangled C++ signature
// to exactly one object symbol and alias __wrap_<that symbol> to this C name.
// Missing wrappers cause independent count/ownership oracles to fail.
struct CxxLeafBinding { const char *signature; const char *wrapper; };
extern const CxxLeafBinding cxxLeafBindings[];
extern const std::size_t cxxLeafBindingCount;
}
