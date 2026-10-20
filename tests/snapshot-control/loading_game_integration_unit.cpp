// SPDX-License-Identifier: GPL-3.0-or-later
// SOURCE ONLY until separately released compile/link/run review. No native
// SDL device, private asset, provider or converter qualification is claimed.
//
// Link actual application objects with function/data sections and gc-sections,
// rename ONLY src/main.cpp's main to openxcomResponsiveApplicationMain, and keep
// actual Game/LoaderSession/LoaderThreadLinux/LoadingUiPolicy/LoadingAudioHandoff,
// StartState/VideoState/FlcPlayer, Mod/Language/State and resource lifetimes.
// OPENXCOM_LOADING_GAME_FIXTURE_SUPPORT_ONLY removes only this file's main/cases
// so the video fixture can share the exact same native-leaf model.
//
// Enumerated model boundary:
//  * SDL initialization/window/cursor/event/clock/device leaves, mixer handles,
//    audio mutex/hook/open/close/volume/playback, software conversion, and
//    LoaderSdlTlsCleanup, explicitly armed owner operator-new failures, and
//    one worker-local converted-storage allocation fault armed by the CVT leaf.
//    Heap SDL surfaces/blits/palettes remain real software
//    SDL calls; no SDL_Init or audio/window driver is entered.
//  * FileMap lookups and Options init/persistence/discovery, Language::loadFile,
//    Mod::loadAll replace filesystem/private assets only. Actual Mod owns real
//    synthetic Font/Palette/Surface/Music/SoundSet/RuleInterface/RuleVideo objects.
//    Provider cases call real Mod::loadExtraResources/Mod::loadMusic/GMCatFile
//    using generated empty-font YAML and bounded synthetic CAT bytes.
//  * An optional owner poll hook supplies deterministic full-Game-run input.
//    It must return no queued event on a turn that directly reserves loading.
//  * Mod/Language C1/D1, State::setGamePtr and State::resetAll wrappers forward
//    to the original bodies and instrument them. They are NOT fake constructors.
//  * No Game orchestration, policy, session, callback or video method is wrapped.
//  * All C++ leaf aliases below must resolve to exactly one actual object symbol
//    in the later command review. Do not allow unresolved-symbol suppression.
//
// Leaf counters dynamically detect only their enumerated access boundaries.
// Direct Options-global reads and unwrapped observer methods require the paired
// source contract; this fixture does not pretend a leaf counter observes them.
// Preparation allocation testing covers six early adoption points, not every
// allocator in legacy UI/decoder constructors. Exhaustive OOM is not claimed.
// This fixture intentionally fails rather than unwinding a live Game/session.
// A test deadline is an outer failure, never a retirement or cleanup receipt.
// Provider cases must be crossed with mixed observer OFF/ON and __NO_MUSIC
// OFF/ON at later command review. PROFILE output identifies the compiled axis;
// no observer runtime descriptor is supplied or observer capability substituted.
// Retained-process cases intentionally exit with live adopted owners and report
// contexts_retained=1; they are not graceful-close or provider-safety passes.
// Every case requires OPENXCOM_LOADING_FIXTURE_DIRECTORY naming its separately
// monitored, precreated empty mode0700 root; no fallback or cleanup removal.
// Generated assets use exclusive direct-child mode0600 creation and fixed caps.
// Command review must bind that root to the run namespace, prohibit concurrent
// writers/renames, and monitor fixture.log growth as well as stdout/stderr.
#include "loading_game_integration_fixture.h"
#include "../../src/Engine/LoaderThreadLinux.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/ModInfo.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Palette.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Engine/Screen.h"
#include "../../src/Engine/Music.h"
#include "../../src/Engine/AdlibMusic.h"
#include "../../src/Engine/Exception.h"
#include "../../src/Engine/Sound.h"
#include "../../src/Engine/SoundSet.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Action.h"
#include "../../src/Mod/Mod.h"
#include "../../src/Mod/ExtraStrings.h"
#include "../../src/Mod/RuleInterface.h"
#include "../../src/Mod/RuleMusic.h"
#include "../../src/Mod/RuleVideo.h"
#include "../../src/Menu/MainMenuState.h"
#include "../../src/Menu/OptionsBaseState.h"
#include "../../src/Menu/ModListState.h"
#include "../../src/Menu/ErrorMessageState.h"
#include "../../src/Savegame/SavedGame.h"
#include <SDL_mixer.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <dirent.h>
#include <fcntl.h>
#include <exception>
#include <map>
#include <locale>
#include <mutex>
#include <new>
#include <set>
#include <stdexcept>
#include <thread>
#include <utility>
#include <pthread.h>
#include <sched.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef OPENXCOM_LOADER_LIFETIME_TESTING
#error Actual immutable session failure hooks are required by this fixture.
#endif

namespace OpenXcom
{
struct LoaderSessionTestAccess
{
    static std::shared_ptr<LoaderSession> make(std::shared_ptr<LoaderThreadBackend> backend,
        LoaderSession::Work work, std::shared_ptr<void> context);
};
// Declared as a nonshipping friend in the scoped Mod header. Only the isolated
// synthetic asset leaf calls these; production classification/lifetime code is
// never skipped and every installed object has its normal owning container.
struct LoadingGameResourceAccess
{
    static void populate(Mod &);
    static void populateCatalogProbe(Mod &);
    static const std::string &playingMusic(const Mod &mod) { return mod._playingMusic; }
    static Music *randomMusic(const Mod &mod, const std::string &name) { return mod.getRandomMusic(name); }
    static void nullMusic(Mod &mod, const std::string &id)
    { LoadingGameFixture::require(mod._musics.emplace(id, nullptr).second, "duplicate null music key"); }
    static void music(Mod &mod, const std::string &id, std::unique_ptr<Music> music)
    {
        LoadingGameFixture::require(!mod._musics.count(id), "duplicate synthetic music key");
        mod._musics.emplace(id, music.get()); music.release();
    }
    static Sound *sound(Mod &mod, const std::string &set, unsigned id)
    {
        auto entry = mod._sounds.find(set);
        if (entry == mod._sounds.end())
        {
            std::unique_ptr<SoundSet> sounds(new SoundSet());
            entry = mod._sounds.emplace(set, sounds.get()).first; sounds.release();
        }
        return entry->second->addSound(id);
    }
};
}

namespace LoadingGameFixture
{
using namespace OpenXcom;
using Access = LoadingGameIntegrationAccess;
namespace
{
struct TraceSlot
{
    std::atomic<bool> ready;
    const char *name;
    TraceSlot() : ready(false), name(nullptr) {}
};
std::array<TraceSlot, 32768> ledger;
std::atomic<unsigned> ledgerCount(0);
std::unique_ptr<NativeModel> native;
std::thread::id owner;
Game *activeGame = nullptr;
std::deque<SDL_Event> events;
std::vector<std::shared_ptr<BackendControl>> controls;
std::deque<BackendControl::Fault> nextFaults;
std::map<std::string, std::string> fileAliases;
std::set<std::string> soundFolder;
std::mutex audioMutex;
thread_local bool audioLocked = false;
thread_local BackendControl *currentBackend = nullptr;
pthread_key_t tlsKey;
bool tlsKeyReady = false;
SDL_Surface *physical = nullptr;
SDL_Cursor cursorObject = {};
SDL_Cursor *currentCursor = &cursorObject;
void (*musicHook)(void *, Uint8 *, int) = nullptr;
void *musicHookData = nullptr;
std::map<Mix_Music *, Mix_MusicType> musicHandles;
struct MusicProvider { Mix_MusicType type; bool fail; };
std::map<std::string, MusicProvider> musicProviders;
Mix_MusicType memoryMusicType = MUS_WAV;
enum class CatalogProbe { None, Enums, Supported, Empty, Blank, Null, Fallback, Gm, GmMalformed, Adlib };
CatalogProbe catalogProbe = CatalogProbe::None;
struct MusicTypeCase { const char *id; Mix_MusicType type; bool supported; };
// Expectations are a literal table, never computed with the production switch.
const MusicTypeCase musicTypeCases[] = {
    {"WAV", MUS_WAV, true}, {"MOD", MUS_MOD, true}, {"OGG", MUS_OGG, true},
    {"MP3", MUS_MP3, true}, {"MP3_MAD", MUS_MP3_MAD, true},
    {"FLAC", MUS_FLAC, true}, {"MODPLUG", MUS_MODPLUG, true},
    {"MID", MUS_MID, false}, {"CMD", MUS_CMD, false}, {"NONE", MUS_NONE, false},
    {"UNKNOWN", static_cast<Mix_MusicType>(15), false}
};
std::map<Mix_Chunk *, std::unique_ptr<unsigned char[]>> chunkHandles;
std::atomic<unsigned> liveBackends(0);
std::atomic<unsigned> liveCallbacks(0);
bool optionsCreated = false;
bool autoCloseAtReservation = false;
bool videoMusic = false, videoSounds = false;
bool introConfigured = false, introUfo = false;
std::vector<std::string> introVideos, introTracks;
std::string activeMaster = "fixture-master";
void (*ownerEventHook)(Game &) = nullptr;
thread_local bool videoStorageAllocationArmed = false;
int fixtureDirectoryFd = -1;
std::set<std::string> fixtureCreatedFiles;
std::size_t fixtureCreatedBytes = 0;
const std::size_t fixtureFileCap = 8u * 1024u * 1024u;
const std::size_t fixtureTotalCap = 16u * 1024u * 1024u;
const std::size_t fixtureFileCountCap = 32;

void assertOwner(const char *message)
{
    require(std::this_thread::get_id() == owner, message);
}
void assertWorker(const char *message)
{
    if (std::this_thread::get_id() == owner)
    { ++model().activeOwnerViolations; fail(message); }
}
void ownerResourceAccess(const char *message)
{
    if (std::this_thread::get_id() == owner && activeGame && Access::workerOwnsEpoch(*activeGame))
    { ++model().activeOwnerViolations; fail(message); }
}
void activeTickLeaf()
{
    if (activeGame && Access::hasOperation(*activeGame) && Access::workerOwnsEpoch(*activeGame))
        require(Logger::suppressed(), "owner tick did not suppress logging");
}
void tlsDestructor(void *opaque)
{
    auto *control = static_cast<BackendControl *>(opaque);
    ++control->tlsEntered; trace("tls-enter");
    control->tls.wait();
    trace("tls-exit"); ++control->tlsExited;
}
class ForwardingBackend final : public LoaderThreadBackend
{
    std::shared_ptr<BackendControl> control;
    LoaderThreadLinux real;
    Entry entryFunction;
    void *entryContext;
    bool injectSetup;
    static void entry(void *opaque, int error)
    {
        auto &self = *static_cast<ForwardingBackend *>(opaque);
        currentBackend = self.control.get();
        require(pthread_setspecific(tlsKey, currentBackend) == 0, "install real pthread TLS destructor");
        ++self.control->entries; trace("worker-entry");
        self.control->entry.wait();
        self.entryFunction(self.entryContext, self.injectSetup ? EIO : error);
        ++self.control->terminalReturns; trace("session-entry-return");
    }
public:
    explicit ForwardingBackend(std::shared_ptr<BackendControl> c) : control(std::move(c)), entryFunction(nullptr), entryContext(nullptr), injectSetup(false)
    { ++liveBackends; }
    ~ForwardingBackend() override { --liveBackends; }
    StartResult start(Entry fn, void *context) noexcept override
    {
        assertOwner("backend start outside owner");
        ++control->starts; trace("start-prepared");
        if (control->fault == BackendControl::Fault::NoThreadOnce)
        {
            control->fault = BackendControl::Fault::None;
            trace("native-no-thread"); return {StartKind::NoThread, EAGAIN};
        }
        injectSetup = control->fault == BackendControl::Fault::SetupFailureOnce;
        control->fault = BackendControl::Fault::None;
        entryFunction = fn; entryContext = context;
        return real.start(&ForwardingBackend::entry, this);
    }
    ReapResult pollReap() noexcept override
    {
        assertOwner("backend poll outside owner"); activeTickLeaf(); ++control->polls;
        const ReapResult result = real.pollReap();
        if (result.kind == ReapKind::Reaped)
        { ++control->actualReaps; trace("actual-reap"); }
        return result;
    }
};
std::shared_ptr<LoaderThreadBackend> backendFactory()
{
    BackendControl::Fault fault = BackendControl::Fault::None;
    if (!nextFaults.empty()) { fault = nextFaults.front(); nextFaults.pop_front(); }
    std::shared_ptr<BackendControl> control(new BackendControl(fault));
    controls.push_back(control);
    return std::make_shared<ForwardingBackend>(control);
}
bool copyFailure(void *) noexcept
{
    if (model().failCopyOnce.exchange(false))
    { ++model().copyFailures; trace("copy-failure"); return true; }
    return false;
}
}

NativeModel::NativeModel() :
    openCount(0), frequency(22050), channels(2), audioStatus(SDL_AUDIO_STOPPED), format(AUDIO_S16SYS),
    opens(0), closes(0), pauses(0), unpauses(0), locks(0), drains(0), hooks(0), unhooks(0),
    postmixChanges(0), musicPlays(0), musicHalts(0), channelHalts(0), musicLoads(0), memoryMusicLoads(0), musicTypeQueries(0), lastLoadedMusic(0), lastQueriedMusic(0), currentMusic(0), flips(0), blits(0), physicalPaletteChanges(0), polls(0), delays(0),
    windowResets(0), sdlQuits(0), saves(0), maps(0), updates(0), languageLoads(0), resourceLoads(0),
    modConstructed(0), modDestroyed(0), languageConstructed(0), languageDestroyed(0),
    activeOwnerViolations(0), workerSubsystemViolations(0), genericTailCalls(0), workEntered(0), copyFailures(0), callbackInvocations(0), allocationFailures(0), failAllocationAfter(-1),
    failFlip(false), failBlit(false), failOpen(false), residualOpenFailure(false), failLanguage(false),
    failLoad(false), throwUnknownLoad(false), failCopyOnce(false), failClose(false), failBuildConversion(false), failConversion(false), failVideoStorageAllocation(false), failHaltMusic(false),
    clock(1000), focus(SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS), eventBudget(64) {}
BackendControl::BackendControl(Fault f) : starts(0), polls(0), actualReaps(0), entries(0), terminalReturns(0), epilogueEntered(0), tlsEntered(0), tlsExited(0), fault(f) {}
NativeModel &model() { require(bool(native), "fixture must be reset first"); return *native; }
[[noreturn]] void fail(const char *message) noexcept
{
    std::fprintf(stderr, "GAME_INTEGRATION FAIL: %s\n", message);
    dumpTrace(); std::fflush(nullptr); std::_Exit(1);
}
void require(bool value, const char *message) noexcept { if (!value) fail(message); }
std::uint64_t monotonicNanoseconds() noexcept
{
    timespec time = {}; require(clock_gettime(CLOCK_MONOTONIC, &time) == 0, "monotonic clock");
    return std::uint64_t(time.tv_sec) * 1000000000ULL + time.tv_nsec;
}
void Gate::wait() const noexcept
{
    const auto deadline = monotonicNanoseconds() + 10000000000ULL;
    while (!open()) { require(monotonicNanoseconds() < deadline, "independent gate watchdog"); sched_yield(); }
}
void trace(const char *name) noexcept
{
    const unsigned index = ledgerCount.fetch_add(1);
    require(index < ledger.size(), "trace bound"); ledger[index].name = name; ledger[index].ready.store(true, std::memory_order_release);
    if (autoCloseAtReservation && std::strcmp(name, "reservation") == 0) enqueueQuit();
}
std::size_t tracePosition(const char *name) noexcept
{
    const unsigned count = std::min<unsigned>(ledgerCount.load(), ledger.size());
    for (unsigned i = 0; i != count; ++i)
        if (ledger[i].ready.load(std::memory_order_acquire) && !std::strcmp(ledger[i].name, name)) return i;
    return ledger.size();
}
bool saw(const char *name) noexcept { return tracePosition(name) != ledger.size(); }
void clearTrace() noexcept
{
    require(!liveCallbacks.load(), "cannot clear live callback trace");
    for (auto &slot : ledger) slot.ready.store(false); ledgerCount.store(0);
}
void dumpTrace() noexcept
{
    const unsigned count = std::min<unsigned>(ledgerCount.load(), ledger.size());
    for (unsigned i = 0; i != count; ++i)
        std::fprintf(stderr, "TRACE %u %s\n", i, ledger[i].ready.load(std::memory_order_acquire) ? ledger[i].name : "unpublished");
}
void waitFor(const std::atomic<unsigned> &value, unsigned minimum, const char *message)
{
    const auto deadline = monotonicNanoseconds() + 10000000000ULL;
    while (value.load(std::memory_order_acquire) < minimum)
    { require(monotonicNanoseconds() < deadline, message); sched_yield(); }
}
namespace
{
bool safeFixtureBasename(const std::string &name)
{
    if (name.empty() || name.size() > 96 || name == "." || name == "..") return false;
    for (unsigned char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
    return true;
}
void acquireFixtureDirectory()
{
    require(fixtureDirectoryFd < 0, "one fixture reset/root per process");
    const char *supplied = std::getenv("OPENXCOM_LOADING_FIXTURE_DIRECTORY");
    require(supplied != nullptr, "required OPENXCOM_LOADING_FIXTURE_DIRECTORY missing; no fallback directory");
    const std::size_t length = strnlen(supplied, 4097);
    require(length > 1 && length <= 4096 && supplied[0] == '/' && supplied[length - 1] != '/', "absolute bounded normalized fixture directory required");
    const std::string path(supplied, length);
    int current = open("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    require(current >= 0, "open filesystem root for no-follow fixture validation");
    for (std::size_t at = 1; at < path.size();)
    {
        const std::size_t end = path.find('/', at);
        const std::string component = path.substr(at, end == std::string::npos ? std::string::npos : end - at);
        require(!component.empty() && component != "." && component != ".." && component.size() <= 255,
            "fixture directory forbids empty, dot and parent components");
        const int next = openat(current, component.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        require(next >= 0, "every fixture directory component must preexist and not be a symlink");
        require(close(current) == 0, "close fixture path traversal descriptor"); current = next;
        if (end == std::string::npos) break;
        at = end + 1;
    }
    struct stat metadata = {};
    require(fstat(current, &metadata) == 0 && S_ISDIR(metadata.st_mode) && metadata.st_uid == geteuid() &&
        (metadata.st_mode & 07777) == 0700, "fixture root must be owned directory with exact mode0700");
    const int scan = dup(current); require(scan >= 0, "duplicate verified fixture directory");
    DIR *directory = fdopendir(scan); require(directory != nullptr, "inspect exact initial fixture emptiness");
    errno = 0;
    while (dirent *entry = readdir(directory))
    {
        require(!std::strcmp(entry->d_name, ".") || !std::strcmp(entry->d_name, ".."), "fixture root must be empty before this command");
        errno = 0;
    }
    require(errno == 0 && closedir(directory) == 0, "finish fixture directory inspection");
    fixtureDirectoryFd = current; model().temporaryRoot = path;
    fixtureCreatedFiles.clear(); fixtureCreatedBytes = 0;
}
std::string fixtureBasenameFromPath(const std::string &path)
{
    const std::string prefix = model().temporaryRoot + "/";
    require(path.compare(0, prefix.size(), prefix) == 0, "synthetic path outside explicit monitored fixture root");
    const std::string name = path.substr(prefix.size());
    require(safeFixtureBasename(name), "synthetic asset must be a bounded direct-child basename"); return name;
}
bool checkedFixtureExists(const std::string &path)
{
    const std::string name = fixtureBasenameFromPath(path);
    struct stat metadata = {};
    if (fstatat(fixtureDirectoryFd, name.c_str(), &metadata, AT_SYMLINK_NOFOLLOW) != 0)
    { require(errno == ENOENT, "unexpected fixture path inspection failure"); return false; }
    require(S_ISREG(metadata.st_mode) && metadata.st_uid == geteuid() && metadata.st_nlink == 1 &&
        (metadata.st_mode & 07777) == 0600 && metadata.st_size >= 0 && std::uint64_t(metadata.st_size) <= fixtureFileCap &&
        fixtureCreatedFiles.count(name) == 1, "fixture file must be this command's bounded owned regular nonlinked file");
    return true;
}
}
std::string fixturePath(const std::string &basename)
{
    require(fixtureDirectoryFd >= 0 && safeFixtureBasename(basename), "verified root and bounded synthetic basename required");
    return model().temporaryRoot + "/" + basename;
}
std::string createFixtureFile(const std::string &basename, const void *bytes, std::size_t size)
{
    assertOwner("synthetic file creation must be on owner"); ownerResourceAccess("synthetic file creation during worker ownership");
    const std::string path = fixturePath(basename);
    require((bytes || size == 0) && size <= fixtureFileCap && size <= fixtureTotalCap - fixtureCreatedBytes &&
        fixtureCreatedFiles.size() < fixtureFileCountCap, "synthetic file count/byte bound");
    const int file = openat(fixtureDirectoryFd, basename.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    require(file >= 0, "synthetic file must be absent; exclusive no-follow creation failed");
    const unsigned char *next = static_cast<const unsigned char *>(bytes); std::size_t left = size;
    while (left)
    {
        const ssize_t written = write(file, next, left);
        if (written < 0 && errno == EINTR) continue;
        require(written > 0, "write bounded synthetic file"); next += written; left -= std::size_t(written);
    }
    struct stat metadata = {};
    require(fstat(file, &metadata) == 0 && S_ISREG(metadata.st_mode) && metadata.st_uid == geteuid() && metadata.st_nlink == 1 &&
        (metadata.st_mode & 07777) == 0600 && std::uint64_t(metadata.st_size) == size, "new synthetic file exact metadata/length");
    require(close(file) == 0, "close complete synthetic file");
    require(fixtureCreatedFiles.insert(basename).second, "synthetic file name reused"); fixtureCreatedBytes += size;
    return path;
}
void reset()
{
    require(!activeGame && !liveBackends.load() && !liveCallbacks.load(), "reset before actual retirement");
    require(!physical && musicHandles.empty() && chunkHandles.empty(), "leaked native/model resource");
    controls.clear(); nextFaults.clear(); events.clear(); musicProviders.clear(); memoryMusicType = MUS_WAV; catalogProbe = CatalogProbe::None; videoMusic = videoSounds = autoCloseAtReservation = false; fileAliases.clear(); soundFolder.clear(); clearTrace();
    owner = std::this_thread::get_id(); native.reset(new NativeModel());
    introConfigured = introUfo = false; introVideos.clear(); introTracks.clear(); activeMaster = "fixture-master"; ownerEventHook = nullptr;
    if (!tlsKeyReady) { require(pthread_key_create(&tlsKey, &tlsDestructor) == 0, "pthread key"); tlsKeyReady = true; }
    if (!optionsCreated) { Options::create(); optionsCreated = true; }
    Options::resetDefault(false);
    Options::mods.clear(); Options::language = "en-US";
    Options::displayWidth = Options::newDisplayWidth = Options::baseXResolution = 320;
    Options::displayHeight = Options::newDisplayHeight = Options::baseYResolution = 200;
    Options::baseXGeoscape = Options::baseXBattlescape = 320;
    Options::baseYGeoscape = Options::baseYBattlescape = 200;
    Options::audioSampleRate = 22050; Options::audioBitDepth = 16; Options::audioChunkSize = 1024;
    Options::useOpenGL = Options::useHQXFilter = Options::useXBRZFilter = Options::useScaleFilter = false;
    Options::asyncBlit = false; Options::playIntro = false; Options::allowResize = true;
    Options::fpsCounter = false; Options::reload = false;
    acquireFixtureDirectory();
    Logger::logFile() = createFixtureFile("fixture.log", nullptr, 0);
}
void bind(Game &game)
{
    require(!activeGame || activeGame == &game, "only one fixture Game"); activeGame = &game;
    Access::setBackendFactory(game, &backendFactory);
    Access::setSessionFactory(game, &LoaderSessionTestAccess::make);
    Access::setTrace(game, &trace);
}
std::unique_ptr<Game> makeGame()
{
    Logger::ScopedSuppression suppress;
    std::unique_ptr<Game> game(new Game("responsive integration modeled fixture"));
    State::setGamePtr(game.get()); // forwarding wrapper calls bind after actual setter
    require(activeGame == game.get(), "State::setGamePtr wrapper binding is missing");
    return game;
}
void setNextBackendFault(BackendControl::Fault value) { nextFaults.push_back(value); }
std::vector<std::shared_ptr<BackendControl>> backends() { return controls; }
void enqueue(const SDL_Event &event) { assertOwner("enqueue is owner-only"); require(events.size() < 1024, "bounded fixture event input"); events.push_back(event); }
void enqueueQuit() { SDL_Event event = {}; event.type = SDL_QUIT; enqueue(event); }
void enqueueKey(SDLKey key) { SDL_Event event = {}; event.type = SDL_KEYDOWN; event.key.keysym.sym = key; enqueue(event); }
std::size_t queuedEvents() { return events.size(); }
void tick(Game &game)
{
    if (Access::commitBoundary(game))
    {
        if (Access::hasOperation(game)) Access::tick(game);
        else Access::videoTick(game);
    }
    else Access::videoTick(game);
    sched_yield();
}
void loadToPublished(Game &game)
{
    const std::size_t first = controls.size();
    require(game.reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "actual startup admission");
    const auto deadline = monotonicNanoseconds() + 10000000000ULL;
    while (Access::hasOperation(game))
    { require(monotonicNanoseconds() < deadline, "startup publication deadline"); tick(game); }
    require(Access::hasStandby(game) && game.getMod() && game.getLanguage(), "real epoch published");
    require(!game.isQuitting(), "load unexpectedly closed");
    require(controls[first]->actualReaps.load() == 1, "publication without actual Linux reap");
}
void releaseAllGates() noexcept
{
    model().loadGate.release();
    for (auto &control : controls) { control->entry.release(); control->epilogue.release(); control->tls.release(); }
}
void finish(Game &game)
{
    releaseAllGates(); game.quit();
    const auto deadline = monotonicNanoseconds() + 10000000000ULL;
    while (!Access::finalClosed(game))
    { require(monotonicNanoseconds() < deadline, "final cleanup deadline"); tick(game); }
    require(game.isQuitting() && model().openCount.load() == 0, "final close independent device proof");
    require(model().activeOwnerViolations.load() == 0 && model().workerSubsystemViolations.load() == 0, "owner/worker exclusion");
}
Mod *publishedMod(Game &game) { return game.getMod(); }
void provideMusic(Mod *mod, const std::string &id, std::unique_ptr<Music> music)
{ require(mod && music, "owned synthetic music"); ownerResourceAccess("provide music during worker"); LoadingGameResourceAccess::music(*mod, id, std::move(music)); }
void provideSound(Mod *mod, const std::string &set, unsigned id)
{
    require(mod, "owned synthetic sound"); ownerResourceAccess("provide sound during worker");
    static const unsigned char pcm[] = {0, 0, 0, 0};
    LoadingGameResourceAccess::sound(*mod, set, id)->load(pcm, sizeof(pcm));
}
void aliasFile(const std::string &name, const std::string &path)
{
    ownerResourceAccess("alias configuration during worker ownership");
    require(checkedFixtureExists(path), "file alias requires already-created bounded synthetic file"); fileAliases[name] = path;
}
void setSoundFolder(const std::set<std::string> &files) { soundFolder = files; }
void configureVideoAssets(bool music, bool ufoSounds)
{ require(controls.empty(), "configure synthetic assets before loading"); videoMusic = music; videoSounds = ufoSounds; }
void configureIntro(const std::vector<std::string> &videos, const std::vector<std::string> &tracks, bool ufo)
{
    require(controls.empty(), "configure intro before actual startup reserve");
    introVideos = videos; introTracks = tracks; introUfo = ufo; introConfigured = true;
    activeMaster = "fixture-before-intro"; Options::playIntro = true;
}
void setPollHook(void (*hook)(Game &)) { assertOwner("owner event hook setup"); ownerEventHook = hook; }
void configureMusicProvider(const std::string &path, int type, bool failLoad)
{
    ownerResourceAccess("configure music provider during worker epoch");
    musicProviders[path] = MusicProvider{static_cast<Mix_MusicType>(type), failLoad};
}
void configureMemoryMusicProvider(int type)
{ ownerResourceAccess("configure memory decoder during worker epoch"); memoryMusicType = static_cast<Mix_MusicType>(type); }
void invokeMusicCallback(Uint8 *destination, int length)
{
    require(musicHook != nullptr, "no installed modeled callback");
    std::lock_guard<std::mutex> lock(audioMutex);
    ++model().callbackInvocations; musicHook(musicHookData, destination, length);
}
struct CallbackHold::Impl
{
    Gate gate;
    std::atomic<unsigned> entered;
    std::thread worker;
    Impl() : gate(false), entered(0), worker([this] {
        ++liveCallbacks; audioMutex.lock(); entered.store(1, std::memory_order_release);
        trace("callback-holds-mutex"); gate.wait();
        if (musicHook)
        {
            Uint8 output[8] = {}; ++model().callbackInvocations;
            trace("actual-audio-callback-enter"); musicHook(musicHookData, output, sizeof output); trace("actual-audio-callback-return");
        }
        trace("callback-released-mutex"); audioMutex.unlock(); --liveCallbacks;
    }) { waitFor(entered, 1, "independent callback entry"); }
    ~Impl() { gate.release(); worker.join(); }
};
CallbackHold::CallbackHold() : _impl(new Impl()) {}
CallbackHold::~CallbackHold() { release(); }
void CallbackHold::release() { _impl.reset(); }
}

namespace OpenXcom
{
std::shared_ptr<LoaderSession> LoaderSessionTestAccess::make(std::shared_ptr<LoaderThreadBackend> backend,
    LoaderSession::Work work, std::shared_ptr<void> context)
{
    const LoaderSession::Hooks hooks = {nullptr, nullptr, nullptr, &LoadingGameFixture::copyFailure};
    return std::shared_ptr<LoaderSession>(new LoaderSession(std::move(backend), work, std::move(context), hooks));
}
void LoadingGameResourceAccess::populate(Mod &mod)
{
    for (const char *id : {"FONT_BIG", "FONT_SMALL"})
    {
        std::unique_ptr<Font> font(new Font()); font->loadTerminal();
        mod._fonts.emplace(id, font.get()); font.release();
    }
    SDL_Color colors[256] = {};
    for (unsigned i = 0; i < 256; ++i) colors[i] = SDL_Color{Uint8(i), Uint8(i), Uint8(i), 0};
    for (const char *id : {"PAL_GEOSCAPE", "BACKPALS.DAT"})
    {
        std::unique_ptr<Palette> palette(new Palette()); palette->setColors(colors, 256);
        mod._palettes.emplace(id, palette.get()); palette.release();
    }
    // Exact menu labels for the synthetic en-US catalog. Identity values keep
    // the previous fallback text while exercising real Language::loadRule and
    // getString, without manufacturing expected logger diagnostics.
    std::unique_ptr<ExtraStrings> labels(new ExtraStrings());
    for (const char *id : {"STR_NEW_GAME", "STR_NEW_BATTLE", "STR_LOAD_SAVED_GAME", "STR_OPTIONS", "STR_MODS", "STR_QUIT",
        "STR_OPENXCOM", "STR_YES", "STR_NO", "STR_BASE_GAME", "STR_OK", "STR_CANCEL"})
        LoadingGameFixture::require(labels->getStrings()->emplace(id, id).second, "unique synthetic language key");
    LoadingGameFixture::require(labels->getStrings()->size()==12&&mod._extraStrings.emplace("en-US", labels.get()).second,
        "exact owned synthetic language catalog");
    labels.release(); // the real Mod destructor owns the catalog
    std::unique_ptr<Surface> surface(new Surface(320, 200)); surface->setPalette(colors);
    mod._surfaces.emplace("BACK01.SCR", surface.get()); surface.release();
    for (const char *id : {"mainMenu", "optionsMenu", "modsMenu"})
    {
        std::unique_ptr<RuleInterface> rule(new RuleInterface(id));
        YAML::Node value; value["palette"] = "PAL_GEOSCAPE"; rule->load(value);
        mod._interfaces.emplace(id, rule.get()); rule.release();
    }
#ifndef __NO_MUSIC
    // A real loaded object in the real catalog, classified through the actual
    // Music virtual method and exact modeled nonnull Mix_Music handle.
    if (!Options::mute && (LoadingGameFixture::catalogProbe == LoadingGameFixture::CatalogProbe::None ||
        LoadingGameFixture::catalogProbe == LoadingGameFixture::CatalogProbe::Adlib))
    {
    std::unique_ptr<Music> music(new Music()); music->load("fixture-catalog.wav");
    LoadingGameResourceAccess::music(mod, "FIXTURE_CATALOG", std::move(music));
    if (LoadingGameFixture::videoMusic) for (const char *id : {"TRACK", "GMINTRO1", "GMINTRO2", "GMINTRO3"})
    {
        std::unique_ptr<Music> track(new Music()); track->load("fixture-video.wav");
        LoadingGameResourceAccess::music(mod, id, std::move(track));
    }
    }
#endif
    if (LoadingGameFixture::introConfigured)
    {
        std::unique_ptr<RuleVideo> video(new RuleVideo("intro")); YAML::Node config;
        config["videos"] = LoadingGameFixture::introVideos; config["audioTracks"] = LoadingGameFixture::introTracks;
        config["useUfoAudioSequence"] = LoadingGameFixture::introUfo; video->load(config);
        mod._videos.emplace("intro", video.get()); video.release();
    }
    if (LoadingGameFixture::videoSounds && !Options::mute)
    {
        static const unsigned char pcm[] = {0, 0, 0, 0};
        for (const char *set : {"INTRO.CAT", "SAMPLE3.CAT"}) for (unsigned id = 0; id != 64; ++id)
            LoadingGameResourceAccess::sound(mod, set, id)->load(pcm, sizeof(pcm));
    }
}
void LoadingGameResourceAccess::populateCatalogProbe(Mod &mod)
{
    using namespace LoadingGameFixture;
    if (catalogProbe == CatalogProbe::None) return;
    require(mod._playingMusic.empty(), "new actual Mod playback label");
    if (catalogProbe == CatalogProbe::Enums || catalogProbe == CatalogProbe::Supported)
    {
        const unsigned plays = model().musicPlays.load(), halts = model().musicHalts.load();
        const unsigned hooks = model().hooks.load(), unhooks = model().unhooks.load();
        bool unsupported = false;
        for (const auto &entry : musicTypeCases)
        {
            if (catalogProbe == CatalogProbe::Supported && !entry.supported) continue;
            std::unique_ptr<Music> value(new Music());
            const unsigned loads = model().musicLoads.load(), queries = model().musicTypeQueries.load();
            value->load(std::string("enum-") + entry.id + ".mid"); // deliberately misleading extension
            Music *base = value.get();
#ifndef __NO_MUSIC
            const bool expected = entry.supported;
            const std::uintptr_t handle = model().lastLoadedMusic.load();
            require(model().musicLoads.load() == loads + 1 && handle != 0, "actual load produced exact modeled handle");
#else
            const bool expected = false;
            require(model().musicLoads.load() == loads, "music-disabled load is a native-free no-op");
#endif
            require(base->isSoftwareMixedAudio() == expected, "actual Music virtual enum classification");
#ifndef __NO_MUSIC
            require(model().musicTypeQueries.load() == queries + 1 && model().lastQueriedMusic.load() == handle,
                "classifier queried exact nonnull loaded handle");
#else
            require(model().musicTypeQueries.load() == queries, "music-disabled classifier must not query a provider");
            if (catalogProbe == CatalogProbe::Supported) continue; // no loaded native catalog in this profile
#endif
            music(mod, entry.id, std::move(value)); unsupported = unsupported || !expected;
            require(mod.responsiveMusicCatalog() == (unsupported ? Mod::ResponsiveMusicCatalog::Unsupported : Mod::ResponsiveMusicCatalog::SoftwareMixed),
                "actual whole catalog classification after each adopted member");
        }
        require(model().musicPlays.load() == plays && model().musicHalts.load() == halts &&
            model().hooks.load() == hooks && model().unhooks.load() == unhooks, "classification is playback-side-effect free");
    }
    else if (catalogProbe == CatalogProbe::Blank)
    {
        std::unique_ptr<Music> blank(new Music());
        const unsigned queries = model().musicTypeQueries.load();
        require(!blank->isSoftwareMixedAudio() && model().musicTypeQueries.load() == queries, "blank Music rejects without querying current track");
        music(mod, "BLANK", std::move(blank));
    }
    else if (catalogProbe == CatalogProbe::Null) nullMusic(mod, "NULL_MEMBER");
    else if (catalogProbe == CatalogProbe::Fallback || catalogProbe == CatalogProbe::Gm || catalogProbe == CatalogProbe::GmMalformed)
    {
        const char *key = catalogProbe == CatalogProbe::Fallback ? "FALLBACK" : "GM_TRACK";
        std::unique_ptr<RuleMusic> rule(new RuleMusic(key)); YAML::Node config;
        config["name"] = "fixture-track"; config["catPos"] = 0; rule->load(config);
        require(mod._musicDefs.emplace(key, rule.get()).second, "unique actual music rule"); rule.release();
        // Invoke the actual priority loop, actual Mod::loadMusic, and actual
        // GMCatFile::loadMIDI where selected. Only FileMap/decoder leaves model
        // filenames and native handles. Font YAML is a generated empty list.
        mod.loadExtraResources();
#ifndef __NO_MUSIC
        require(mod._musics.count(key) == 1 && mod._musics.at(key), "actual preferred-format/GM loader adopted an object");
        if (catalogProbe == CatalogProbe::Fallback)
            require(model().musicLoads.load() == 2 && mod._musics.at(key)->isSoftwareMixedAudio(), "actual failed preferred load falls back to modeled software handle");
        else
        {
            require(model().memoryMusicLoads.load() == (catalogProbe == CatalogProbe::Gm ? 1u : 0u), "actual GM conversion versus malformed blank result");
            require(!mod._musics.at(key)->isSoftwareMixedAudio(), "actual GM native MIDI or malformed blank rejects");
        }
#else
        require(mod._musics.empty() && model().musicLoads.load() == 0 && model().memoryMusicLoads.load() == 0,
            "music-disabled actual priority loader stays native-free and empty");
#endif
    }
    else if (catalogProbe == CatalogProbe::Adlib)
    {
        const unsigned queries = model().musicTypeQueries.load();
        std::unique_ptr<Music> loaded(new AdlibMusic()), empty(new AdlibMusic());
        // Classification does not imply valid playback data. Only nonempty
        // ownership is needed here; successful decoder playback is not claimed.
        std::unique_ptr<char[]> bytes(new char[32]()); bytes[0] = 0x7f;
        loaded->load(bytes.get(), 32); bytes.release();
        require(loaded->isSoftwareMixedAudio() && empty->isSoftwareMixedAudio(), "actual loaded/unloaded Adlib virtual override, including observer OFF");
        require(model().musicTypeQueries.load() == queries, "custom Adlib capability never queries a null mixer handle");
        music(mod, "ADLIB_LOADED", std::move(loaded)); music(mod, "ADLIB_EMPTY", std::move(empty));
    }
    const bool rejected = catalogProbe == CatalogProbe::Enums || catalogProbe == CatalogProbe::Blank || catalogProbe == CatalogProbe::Null
#ifndef __NO_MUSIC
        || catalogProbe == CatalogProbe::Gm || catalogProbe == CatalogProbe::GmMalformed
#endif
        ;
    if (rejected) require(mod.responsiveMusicCatalog() == Mod::ResponsiveMusicCatalog::Unsupported && !mod.isMusicCatalogSoftwareMixed(), "actual rejected catalog is not vacuously qualified");
    else if (mod._musics.empty()) require(mod.responsiveMusicCatalog() == Mod::ResponsiveMusicCatalog::Empty && mod.isMusicCatalogSoftwareMixed(), "empty catalog distinguished from software capability");
    else require(mod.responsiveMusicCatalog() == Mod::ResponsiveMusicCatalog::SoftwareMixed, "all actual loaded catalog members qualified");
    trace("provider-catalog-oracles-complete");
}
}

// C++ leaf forwarding aliases. These simple lifetime signatures are pinned in
// the link manifest; the original bodies remain in their original objects.
extern "C" {
void ox_real_mod_ctor(OpenXcom::Mod *) asm("__real__ZN8OpenXcom3ModC1Ev");
void ox_real_mod_dtor(OpenXcom::Mod *) asm("__real__ZN8OpenXcom3ModD1Ev");
void ox_real_language_ctor(OpenXcom::Language *) asm("__real__ZN8OpenXcom8LanguageC1Ev");
void ox_real_language_dtor(OpenXcom::Language *) asm("__real__ZN8OpenXcom8LanguageD1Ev");
void ox_real_set_game(OpenXcom::Game *) asm("__real__ZN8OpenXcom5State10setGamePtrEPNS_4GameE");
void ox_real_reset_all(OpenXcom::State *) asm("__real__ZN8OpenXcom5State8resetAllEv");
}
using namespace LoadingGameFixture;
using namespace OpenXcom;
namespace Options = OpenXcom::Options;
extern "C" void ox_fixture_mod_ctor(Mod *mod)
{
    trace("mod-constructor-enter"); ox_real_mod_ctor(mod); ++model().modConstructed; trace("mod-constructed");
}
extern "C" void ox_fixture_mod_dtor(Mod *mod)
{
    assertWorker("Mod destroyed on owner in responsive domain");
    require((model().openCount.load() == 1 && model().audioStatus.load() == SDL_AUDIO_PAUSED) ||
            (model().openCount.load() == 0 && Options::mute), "Mod native/provider lifetime domain");
    trace("mod-destructor-enter"); ox_real_mod_dtor(mod); ++model().modDestroyed; trace("mod-destroyed");
}
extern "C" void ox_fixture_language_ctor(Language *language)
{ ox_real_language_ctor(language); ++model().languageConstructed; trace("language-constructed"); }
extern "C" void ox_fixture_language_dtor(Language *language)
{ trace("language-destructor-enter"); ox_real_language_dtor(language); ++model().languageDestroyed; trace("language-destroyed"); }
extern "C" void ox_fixture_set_game(Game *game)
{ ox_real_set_game(game); if (game) bind(*game); }
extern "C" void ox_fixture_reset_all(State *state)
{
    ++model().genericTailCalls;
    require(!activeGame || !LoadingGameIntegrationAccess::hasOperation(*activeGame), "generic resetAll after reservation");
    ox_real_reset_all(state);
}
using Rulesets = std::vector<std::pair<std::string, std::vector<std::string>>>;
extern "C" void ox_fixture_load_all(Mod *mod, const Rulesets &)
{
    assertWorker("asset load entered owner"); ++model().resourceLoads; ++model().workEntered;
    trace("asset-load-entry"); model().loadGate.wait();
    LoadingGameResourceAccess::populate(*mod);
    LoadingGameResourceAccess::populateCatalogProbe(*mod);
    if (model().throwUnknownLoad.load())
    {
#ifndef __NO_MUSIC
        std::unique_ptr<Music> unsupported(new Music()); unsupported->load("fixture-unsupported.cmd");
        LoadingGameResourceAccess::music(*mod, "UNSUPPORTED_PARTIAL", std::move(unsupported));
#endif
        throw 17; // adopted unsupported provider throws BEFORE catalog qualification
    }
    if (model().failLoad.load()) throw std::runtime_error("modeled resource leaf failure after ownership adoption");
    trace("asset-load-return");
}
extern "C" void ox_fixture_language_file(Language *, const std::string &)
{
    assertWorker("language filesystem leaf on owner"); ++model().languageLoads;
    if (model().failLanguage.load()) throw std::runtime_error("modeled language file failure");
}
extern "C" bool ox_fixture_options_init(int, char **) { return true; }
extern "C" bool ox_fixture_options_save(bool, const std::string &)
{ ownerResourceAccess("options save on active owner"); ++model().saves; trace("options-save"); return true; }
extern "C" void ox_fixture_options_map()
{ assertWorker("resource mapping on active owner"); ++model().maps; trace("resource-map"); }
extern "C" void ox_fixture_options_update()
{ assertWorker("mod update on active owner"); ++model().updates; if (introConfigured) activeMaster = "fixture-master"; }
extern "C" void ox_fixture_options_refresh() { ownerResourceAccess("owner refresh during operation"); }
extern "C" std::string ox_fixture_active_master()
{ ownerResourceAccess("Options active master during operation"); return activeMaster; }
extern "C" std::vector<const ModInfo *> ox_fixture_active_mods() { return {}; }
extern "C" ModInfo ox_fixture_mod_info(const std::string &) { return ModInfo("fixture-master"); }
extern "C" const std::map<std::string, ModInfo> &ox_fixture_mod_infos()
{ static const std::map<std::string, ModInfo> empty; return empty; }
extern "C" const Rulesets &ox_fixture_rulesets()
{ assertWorker("FileMap rulesets accessed by active owner"); static const Rulesets empty; return empty; }
extern "C" const std::string &ox_fixture_file_path(const std::string &path)
{
    ownerResourceAccess("FileMap path during operation");
    const auto found = fileAliases.find(path);
    if (found != fileAliases.end()) { require(checkedFixtureExists(found->second), "aliased synthetic file disappeared"); return found->second; }
    if (!path.empty() && path[0] == '/') (void)checkedFixtureExists(path);
    return path;
}
extern "C" const std::set<std::string> &ox_fixture_folder(const std::string &path)
{
    ownerResourceAccess("FileMap folder during operation");
    static const std::set<std::string> empty;
    return path == "SOUND" || path == "sound" ? soundFolder : empty;
}
extern "C" std::string ox_fixture_search_data(const std::string &path)
{
    // Discovery is modeled; no lookup may escape into real game/private data.
    require(path.size() <= 80 && !path.empty() && path[0] != '/', "bounded logical data key");
    std::string name = "missing-" + path; std::replace(name.begin(), name.end(), '/', '_');
    return fixturePath(name);
}
extern "C" bool ox_fixture_file_exists(const std::string &path)
{ return !path.empty() && path[0] == '/' ? checkedFixtureExists(path) : false; }
extern "C" void ox_fixture_window_icon(int, const std::string &) {}
extern "C" void ox_fixture_error_dialog() {}

// Locale enumeration is an external-command discovery leaf. These en-US
// fixtures retain the initial C locale and never discover installed locales.
extern "C" void ox_fixture_utf8_locale()
{
    assertOwner("locale discovery on worker");
    ownerResourceAccess("locale discovery during resource transfer");
    const char *lang = std::getenv("LANG"), *all = std::getenv("LC_ALL");
    require(lang && all && !std::strcmp(lang, "C") && !std::strcmp(all, "C") &&
            Options::language == "en-US" && std::locale().name() == "C", "exact modeled C locale");
    trace("locale-discovery-modeled-C");
}
// No selected case may execute a command, including a shell-availability probe.
// This link boundary covers application references, not DSO-internal calls.
extern "C" int __wrap_system(const char *)
{
    fail("external command route is outside the fixture scope");
}

namespace LoadingGameFixture
{
const CxxLeafBinding cxxLeafBindings[] = {
    {"OpenXcom::Mod::Mod() [complete object constructor]", "ox_fixture_mod_ctor"},
    {"OpenXcom::Mod::~Mod() [complete object destructor]", "ox_fixture_mod_dtor"},
    {"OpenXcom::Language::Language() [complete object constructor]", "ox_fixture_language_ctor"},
    {"OpenXcom::Language::~Language() [complete object destructor]", "ox_fixture_language_dtor"},
    {"OpenXcom::State::setGamePtr(OpenXcom::Game*)", "ox_fixture_set_game"},
    {"OpenXcom::State::resetAll()", "ox_fixture_reset_all"},
    {"OpenXcom::Mod::loadAll(Rulesets const&)", "ox_fixture_load_all"},
    {"OpenXcom::Language::loadFile(std::string const&)", "ox_fixture_language_file"},
    {"OpenXcom::Options::init(int, char**)", "ox_fixture_options_init"},
    {"OpenXcom::Options::save(bool, std::string const&)", "ox_fixture_options_save"},
    {"OpenXcom::Options::mapResources()", "ox_fixture_options_map"},
    {"OpenXcom::Options::updateMods()", "ox_fixture_options_update"},
    {"OpenXcom::Options::refreshMods()", "ox_fixture_options_refresh"},
    {"OpenXcom::Options::getActiveMaster()", "ox_fixture_active_master"},
    {"OpenXcom::Options::getActiveMods()", "ox_fixture_active_mods"},
    {"OpenXcom::Options::getModInfo(std::string const&)", "ox_fixture_mod_info"},
    {"OpenXcom::Options::getModInfos()", "ox_fixture_mod_infos"},
    {"OpenXcom::FileMap::getRulesets()", "ox_fixture_rulesets"},
    {"OpenXcom::FileMap::getFilePath(std::string const&)", "ox_fixture_file_path"},
    {"OpenXcom::FileMap::getVFolderContents(std::string const&)", "ox_fixture_folder"},
    {"OpenXcom::CrossPlatform::searchDataFile(std::string const&)", "ox_fixture_search_data"},
    {"OpenXcom::CrossPlatform::fileExists(std::string const&)", "ox_fixture_file_exists"},
    {"OpenXcom::CrossPlatform::setWindowIcon(int, std::string const&)", "ox_fixture_window_icon"},
    {"OpenXcom::CrossPlatform::getErrorDialog()", "ox_fixture_error_dialog"},
    {"OpenXcom::Unicode::getUtf8Locale()", "ox_fixture_utf8_locale"}
};
const std::size_t cxxLeafBindingCount = sizeof(cxxLeafBindings) / sizeof(cxxLeafBindings[0]);
}

// Native C leaf model. Every __wrap_ name below is a mandatory --wrap binding.
// Pure heap SDL surface/RW helpers remain in the reviewed software SDL library.
extern "C" char *__wrap_SDL_GetError() { static char message[] = "modeled SDL leaf error"; return message; }
extern "C" int __wrap_SDL_Init(Uint32) { assertOwner("SDL init on worker"); return 0; }
extern "C" int __wrap_SDL_InitSubSystem(Uint32) { assertOwner("SDL init subsystem on worker"); return 0; }
extern "C" Uint32 __wrap_SDL_WasInit(Uint32 flags) { return flags & (SDL_INIT_AUDIO | SDL_INIT_VIDEO); }
extern "C" void __wrap_SDL_QuitSubSystem(Uint32)
{ ++model().workerSubsystemViolations; fail("unexpected SDL subsystem transition"); }
extern "C" void __wrap_SDL_Quit()
{
    assertOwner("SDL_Quit on worker");
    require(model().openCount.load() == 0 && model().modConstructed.load() == model().modDestroyed.load(),
        "SDL_Quit before independent device/resource closure");
    for (const auto &control : controls)
        require(control->actualReaps.load() == control->entries.load(), "SDL_Quit before actual native reap");
    ++model().sdlQuits; trace("owner-sdl-quit");
    SDL_FreeSurface(physical); physical = nullptr; activeGame = nullptr;
}
extern "C" char *__wrap_SDL_AudioDriverName(char *buffer, int length)
{ const char value[] = "dummy"; require(length >= int(sizeof(value)), "driver buffer"); std::memcpy(buffer, value, sizeof(value)); return buffer; }
extern "C" SDL_Surface *__wrap_SDL_SetVideoMode(int width, int height, int bpp, Uint32)
{
    ownerResourceAccess("window reset during audio ownership"); ++model().windowResets;
    require(width > 0 && width <= 1920 && height > 0 && height <= 1200, "bounded model display");
    SDL_Surface *next = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, bpp, 0, 0, 0, 0);
    require(next, "heap SDL display surface"); SDL_FreeSurface(physical); physical = next; return physical;
}
extern "C" SDL_Surface *__wrap_SDL_GetVideoSurface() { return physical; }
extern "C" SDL_GrabMode __wrap_SDL_WM_GrabInput(SDL_GrabMode mode) { ownerResourceAccess("grab during operation"); return mode; }
extern "C" void __wrap_SDL_WM_SetCaption(const char *, const char *) {}
extern "C" int __wrap_SDL_EnableUNICODE(int enable) { return enable; }
extern "C" int __wrap_SDL_ShowCursor(int show) { return show; }
extern "C" SDL_Cursor *__wrap_SDL_CreateCursor(Uint8 *, Uint8 *, int, int, int, int) { return &cursorObject; }
extern "C" void __wrap_SDL_SetCursor(SDL_Cursor *cursor) { currentCursor = cursor; }
extern "C" SDL_Cursor *__wrap_SDL_GetCursor() { return currentCursor; }
extern "C" void __wrap_SDL_FreeCursor(SDL_Cursor *) {}
extern "C" Uint8 __wrap_SDL_GetAppState() { return model().focus.load(); }
extern "C" Uint32 __wrap_SDL_GetTicks() { return model().clock.load(); }
extern "C" void __wrap_SDL_Delay(Uint32 milliseconds)
{
    assertOwner("SDL delay from worker"); require(milliseconds <= 1, "blocking legacy delay reached responsive model");
    ++model().delays; model().clock.fetch_add(milliseconds); sched_yield();
}
extern "C" int __wrap_SDL_PollEvent(SDL_Event *event)
{
    assertOwner("event pump from worker");
    if (ownerEventHook && activeGame) ownerEventHook(*activeGame);
    activeTickLeaf(); ++model().polls;
    if (events.empty()) return 0;
    *event = events.front(); events.pop_front(); return 1;
}
extern "C" Uint8 __wrap_SDL_GetMouseState(int *x, int *y)
{
    require(!activeGame || !LoadingGameIntegrationAccess::hasOperation(*activeGame), "mouse callback after reservation");
    if (x) *x = 0; if (y) *y = 0; return 0;
}
extern "C" int __wrap_SDL_Flip(SDL_Surface *)
{ assertOwner("flip on worker"); ++model().flips; return model().failFlip.load() ? -1 : 0; }
// Observe physical presentation separately from logical decoder palettes.
// The original software SDL implementation still owns every palette update.
extern "C" int __real_SDL_SetColors(SDL_Surface *, SDL_Color *, int, int);
extern "C" int __wrap_SDL_SetColors(SDL_Surface *surface, SDL_Color *colors, int first, int count)
{
    if (physical && surface == physical) ++model().physicalPaletteChanges;
    return __real_SDL_SetColors(surface, colors, first, count);
}
extern "C" int __real_SDL_UpperBlit(SDL_Surface *, SDL_Rect *, SDL_Surface *, SDL_Rect *);
extern "C" int __wrap_SDL_UpperBlit(SDL_Surface *source, SDL_Rect *src, SDL_Surface *target, SDL_Rect *dst)
{
    ++model().blits;
    if (model().failBlit.load() && std::this_thread::get_id() == owner && activeGame && LoadingGameIntegrationAccess::workerOwnsEpoch(*activeGame)) return -1;
    return __real_SDL_UpperBlit(source, src, target, dst);
}
extern "C" int __wrap_Mix_QuerySpec(int *frequency, Uint16 *format, int *channels)
{
    if (frequency) *frequency = model().frequency.load();
    if (format) *format = Uint16(model().format.load());
    if (channels) *channels = model().channels.load(); return model().openCount.load();
}
extern "C" int __wrap_Mix_OpenAudio(int frequency, Uint16 format, int channels, int)
{
    assertWorker("owner audio open"); require(model().openCount.load() == 0, "open from unproved zero");
    ++model().opens; trace("native-open");
    if (model().failOpen.load())
    {
        if (model().residualOpenFailure.load()) { model().openCount = 1; model().audioStatus = SDL_AUDIO_PLAYING; }
        return -1;
    }
    model().frequency = frequency; model().format = format; model().channels = channels;
    model().openCount = 1; model().audioStatus = SDL_AUDIO_PLAYING; // deliberate auto-unpause
    return 0;
}
extern "C" void __wrap_Mix_CloseAudio()
{
    assertWorker("owner audio close"); require(!audioLocked, "close under mutex");
    require(model().openCount.load() == 1 && model().audioStatus.load() == SDL_AUDIO_PAUSED, "close without single paused device");
    ++model().closes; trace("native-close");
    if (!model().failClose.load()) { model().openCount = 0; model().audioStatus = SDL_AUDIO_STOPPED; }
}
extern "C" SDL_audiostatus __wrap_SDL_GetAudioStatus()
{ assertWorker("owner audio status/lock query"); return SDL_audiostatus(model().audioStatus.load()); }
extern "C" void __wrap_SDL_PauseAudio(int pause)
{
    if (pause) { assertWorker("owner pause/drain fallback"); ++model().pauses; trace("pause"); model().audioStatus = SDL_AUDIO_PAUSED; }
    else { ownerResourceAccess("unpause before epoch/actual reap publication"); ++model().unpauses; trace("unpause"); model().audioStatus = SDL_AUDIO_PLAYING; }
}
extern "C" void __wrap_SDL_LockAudio()
{
    assertWorker("owner audio drain"); ++model().locks; trace("drain-enter");
    audioMutex.lock(); audioLocked = true;
}
extern "C" void __wrap_SDL_UnlockAudio()
{
    require(audioLocked, "unlock without matching audio lock"); audioLocked = false; audioMutex.unlock();
    ++model().drains; trace("drain-complete");
}
extern "C" void __wrap_Mix_SetPostMix(void (*)(void *, Uint8 *, int), void *)
{ require(!audioLocked, "mixer setter under drain mutex"); ++model().postmixChanges; }
extern "C" void __wrap_Mix_HookMusic(void (*hook)(void *, Uint8 *, int), void *data)
{
    require(!audioLocked, "hook setter under drain mutex");
    ownerResourceAccess("owner hook change during operation");
    musicHook = hook; musicHookData = data;
    if (hook) { ++model().hooks; trace("hook-install"); } else { ++model().unhooks; trace("hook-detach"); }
}
extern "C" int __wrap_Mix_HaltChannel(int)
{ require(!audioLocked, "halt channel under drain mutex"); ownerResourceAccess("owner halt during operation"); ++model().channelHalts; trace("channels-halted"); return 0; }
extern "C" int __wrap_Mix_HaltMusic()
{ require(!audioLocked, "halt music under drain mutex"); ownerResourceAccess("owner halt music during operation"); ++model().musicHalts; if (model().failHaltMusic.exchange(false)) { trace("native-halt-music-failed"); return -1; } model().currentMusic = 0; trace("music-halted"); return 0; }
extern "C" int __wrap_Mix_AllocateChannels(int count) { assertWorker("owner channel allocate"); return count; }
extern "C" int __wrap_Mix_ReserveChannels(int count) { assertWorker("owner channel reserve"); return count; }
extern "C" int __wrap_Mix_GroupChannels(int from, int to, int) { assertWorker("owner channel group"); return to - from + 1; }
// The shared fixture owns the mixer channel model; querying an unrelated real
// unopened mixer would cross that boundary. Its reviewed UI callers request
// group zero, and the modeled channels are immediately available after play.
extern "C" int __wrap_Mix_GroupAvailable(int group)
{
    assertOwner("UI channel query on worker");
    ownerResourceAccess("UI channel query during resource transfer");
    require(group == 0, "unreviewed UI channel group");
    trace("ui-channel-available");
    return 0;
}
extern "C" int __wrap_Mix_Volume(int, int volume) { ownerResourceAccess("owner volume during operation"); return volume; }
extern "C" int __wrap_Mix_VolumeMusic(int volume) { ownerResourceAccess("owner music volume during operation"); return volume < 0 ? 64 : volume; }
namespace
{
Mix_Music *makeMusicHandle(Mix_MusicType type)
{
    auto *music = reinterpret_cast<Mix_Music *>(new unsigned char[1]);
    require(musicHandles.emplace(music, type).second, "duplicate music handle");
    model().lastLoadedMusic = reinterpret_cast<std::uintptr_t>(music); return music;
}
}
extern "C" Mix_Music *__wrap_Mix_LoadMUS(const char *filename)
{
    ownerResourceAccess("music decoder invoked during worker epoch"); ++model().musicLoads;
    const auto provider = musicProviders.find(filename);
    if (provider != musicProviders.end())
    {
        if (provider->second.fail) { model().lastLoadedMusic = 0; trace("native-music-load-failed"); return nullptr; }
        return makeMusicHandle(provider->second.type);
    }
    // Legacy shared-video fixtures keep their WAV default. Explicit provider
    // cases always configure an exact path, including misleading extensions.
    return makeMusicHandle(std::strstr(filename, "unsupported") ? MUS_CMD : MUS_WAV);
}
extern "C" Mix_Music *__wrap_Mix_LoadMUS_RW(SDL_RWops *rw)
{
    ownerResourceAccess("memory music decoder during worker epoch"); require(rw, "actual memory RW source");
    ++model().memoryMusicLoads; return makeMusicHandle(memoryMusicType);
}
extern "C" Mix_MusicType __wrap_Mix_GetMusicType(const Mix_Music *music)
{
    require(music != nullptr, "capability queried current track rather than actual handle");
    ++model().musicTypeQueries; model().lastQueriedMusic = reinterpret_cast<std::uintptr_t>(music);
    auto found = musicHandles.find(const_cast<Mix_Music *>(music)); require(found != musicHandles.end(), "unknown native music handle"); return found->second;
}
extern "C" void __wrap_Mix_FreeMusic(Mix_Music *music)
{
    if (!music) return;
    require(model().openCount.load() == 1 && model().audioStatus.load() == SDL_AUDIO_PAUSED, "provider/free before drain or after close");
    require(musicHandles.erase(music) == 1, "music double free"); delete[] reinterpret_cast<unsigned char *>(music); trace("native-music-free");
}
extern "C" int __wrap_Mix_PlayMusic(Mix_Music *music, int)
{ ownerResourceAccess("music playback before reap"); require(musicHandles.count(music), "play unknown handle"); ++model().musicPlays; model().currentMusic = reinterpret_cast<std::uintptr_t>(music); trace("music-play"); return 0; }
extern "C" int __wrap_Mix_PlayingMusic() { return 0; }
extern "C" int __wrap_Mix_FadeOutChannel(int, int)
{ ownerResourceAccess("owner fade during operation"); trace("channel-fade"); return 1; }
extern "C" int __wrap_Mix_FadeOutMusic(int)
{ ownerResourceAccess("owner music fade during operation"); trace("music-fade"); return 1; }
extern "C" Mix_Chunk *__wrap_Mix_LoadWAV_RW(SDL_RWops *rw, int freeSource)
{
    if (freeSource) SDL_FreeRW(rw);
    std::unique_ptr<Mix_Chunk> chunk(new Mix_Chunk());
    std::unique_ptr<unsigned char[]> bytes(new unsigned char[4]());
    chunk->allocated = 0; chunk->abuf = bytes.get(); chunk->alen = 4; chunk->volume = MIX_MAX_VOLUME;
    chunkHandles.emplace(chunk.get(), std::move(bytes)); return chunk.release();
}
extern "C" void __wrap_Mix_FreeChunk(Mix_Chunk *chunk)
{ if (chunk) { require(chunkHandles.erase(chunk) == 1, "chunk double free"); delete chunk; } }
extern "C" int __wrap_Mix_PlayChannelTimed(int channel, Mix_Chunk *chunk, int, int)
{ ownerResourceAccess("sound playback before reap"); require(chunkHandles.count(chunk), "play unknown chunk"); trace("sound-play"); return channel < 0 ? 0 : channel; }
extern "C" int __wrap_Mix_Playing(int) { return 0; }
extern "C" int __wrap_Mix_SetPosition(int, Sint16, Uint8) { return 1; }
extern "C" void __wrap_OpenXcom_LoaderSdlTlsCleanup() noexcept
{
    require(currentBackend != nullptr, "real Linux wrapper bridge without context");
    ++currentBackend->epilogueEntered; trace("native-epilogue-enter"); currentBackend->epilogue.wait(); trace("native-epilogue-exit");
    // The independently registered real pthread TLS destructor runs AFTER this
    // returns and AFTER the real LoaderThreadLinux thread wrapper returns.
}

namespace
{
thread_local int conversionSourceRate = 0, conversionDestinationRate = 0;
thread_local Uint16 conversionDestination = 0;
}
extern "C" int __wrap_SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 sourceFormat, Uint8 sourceChannels,
    int sourceRate, Uint16 destinationFormat, Uint8 destinationChannels, int destinationRate)
{
    assertWorker("conversion on owner");
    if (model().failBuildConversion.load()) return -1;
    require(sourceFormat == AUDIO_S16SYS && sourceChannels == 1 && destinationChannels == 2,
        "explicit modeled conversion input domain");
    require(sourceRate > 0 && destinationRate > 0 && (destinationFormat == AUDIO_S16SYS || destinationFormat == AUDIO_S8),
        "explicit modeled conversion output domain");
    conversionSourceRate = sourceRate; conversionDestinationRate = destinationRate; conversionDestination = destinationFormat;
    std::memset(cvt, 0, sizeof(*cvt));
    cvt->src_format = sourceFormat; cvt->dst_format = destinationFormat;
    cvt->len_ratio = double(destinationRate) / sourceRate * (destinationFormat == AUDIO_S16SYS ? 2.0 : 1.0);
    cvt->len_mult = int(cvt->len_ratio) + 3; cvt->needed = 1;
    require(!videoStorageAllocationArmed, "unconsumed video allocation fault");
    videoStorageAllocationArmed = model().failVideoStorageAllocation.exchange(false); return 1;
}
extern "C" int __wrap_SDL_ConvertAudio(SDL_AudioCVT *cvt)
{
    assertWorker("conversion on owner"); if (model().failConversion.load()) return -1;
    require(cvt && cvt->buf && cvt->len > 0 && !(cvt->len % 2), "bounded model PCM input");
    const std::size_t frames = std::size_t(cvt->len) / 2;
    const std::size_t outputFrames = (frames * std::size_t(conversionDestinationRate) + conversionSourceRate - 1) / conversionSourceRate;
    const unsigned frameBytes = conversionDestination == AUDIO_S16SYS ? 4 : 2;
    require(outputFrames <= std::size_t(cvt->len) * cvt->len_mult / frameBytes, "modeled converter capacity");
    std::vector<Uint8> source(cvt->buf, cvt->buf + cvt->len);
    for (std::size_t frame = 0; frame != outputFrames; ++frame)
    {
        const auto index = std::min(frames - 1, frame * std::size_t(conversionSourceRate) / conversionDestinationRate);
        Sint16 sample; std::memcpy(&sample, source.data() + index * 2, 2);
        if (frameBytes == 4)
        { std::memcpy(cvt->buf + frame * 4, &sample, 2); std::memcpy(cvt->buf + frame * 4 + 2, &sample, 2); }
        else { cvt->buf[frame * 2] = cvt->buf[frame * 2 + 1] = Uint8(Sint8(sample / 256)); }
    }
    cvt->len_cvt = int(outputFrames * frameBytes); return 0;
}

// Explicit owner-allocation fault leaf. Wrapping new is inert unless an exact
// case arms a finite countdown; worker/resource allocation remains untouched.
extern "C" void *__real__Znwm(std::size_t);
extern "C" void *__real__Znam(std::size_t);
namespace {
bool ownerAllocationFailure()
{
    if (!native) return false;
    if (std::this_thread::get_id() != owner)
    {
        if (!videoStorageAllocationArmed) return false;
        videoStorageAllocationArmed = false; ++model().allocationFailures;
        trace("video-storage-allocation-failure"); return true;
    }
    int remaining = model().failAllocationAfter.load();
    if (remaining < 0) return false;
    if (remaining == 0)
    { model().failAllocationAfter = -1; ++model().allocationFailures; trace("owner-allocation-failure"); return true; }
    model().failAllocationAfter = remaining - 1; return false;
}
}
extern "C" void *__wrap__Znwm(std::size_t bytes)
{ if (ownerAllocationFailure()) throw std::bad_alloc(); return __real__Znwm(bytes); }
extern "C" void *__wrap__Znam(std::size_t bytes)
{ if (ownerAllocationFailure()) throw std::bad_alloc(); return __real__Znam(bytes); }

#ifndef OPENXCOM_LOADING_GAME_FIXTURE_SUPPORT_ONLY
// This reference and the caller invocations below retain the real object code
// after gc-sections. The separate symbol/source contract must verify owners.
int openxcomResponsiveApplicationMain(int argc, char **argv);
namespace
{
using Access = LoadingGameIntegrationAccess;
template<typename Predicate>
void spinUntil(Game &game, Predicate predicate, const char *message)
{
    const auto deadline = monotonicNanoseconds() + 10000000000ULL;
    while (!predicate()) { require(monotonicNanoseconds() < deadline, message); tick(game); }
}
void complete(std::unique_ptr<Game> &game, const char *name)
{
    if (!Access::finalClosed(*game)) finish(*game);
    require(Access::deferredCount(*game) == 0, "final deferred-state queue");
    game.reset();
    require(model().sdlQuits.load() == 1 && model().modConstructed.load() == model().modDestroyed.load(), "final real destructor ledger");
    require(model().languageConstructed.load() == model().languageDestroyed.load(), "real Language lifetime balance");
    std::printf("CASE %s COMPLETE actual_reap=1 device_closed=1 contexts_destroyed=1\n", name);
}
class DeferredProbe final : public State
{
    Game &game;
public:
    explicit DeferredProbe(Game &g) : game(g) {}
    ~DeferredProbe() override
    {
        require(!Access::workerOwnsEpoch(game), "old borrower deleted after resource transfer");
        require(game.getLanguage() != nullptr, "old Language must outlive all deferred states");
        for (auto &control : backends()) require(control->starts.load() == 0, "worker began before old State destructor returned");
        trace("old-state-destructor");
    }
};
void startupBoundary()
{
    auto game = makeGame();
    Access::enqueueDeferred(*game, new DeferredProbe(*game));
    model().loadGate.hold();
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "reserve actual startup");
    require(!Access::workerOwnsEpoch(*game) && backends()[0]->starts.load() == 0, "reserve must not start a worker");
    trace("test-initiator-returned");
    tick(*game); waitFor(model().workEntered, 1, "worker entered resource leaf");
    require(Access::deferredCount(*game) == 0, "deferred queue fully drained before worker");
    require(tracePosition("reservation") < tracePosition("test-initiator-returned") &&
            tracePosition("test-initiator-returned") < tracePosition("commit") &&
            tracePosition("commit") < tracePosition("old-state-destructor") &&
            tracePosition("old-state-destructor") < tracePosition("deleted-drained") &&
            tracePosition("deleted-drained") < tracePosition("transfer-seal") &&
            tracePosition("transfer-seal") < tracePosition("start-prepared") &&
            tracePosition("start-prepared") < tracePosition("asset-load-entry"), "exact real boundary trace");
    complete(game, "startup-boundary");
}
void terminalAndReap()
{
    auto game = makeGame(); model().loadGate.hold();
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "reserve startup");
    auto control = backends()[0]; control->epilogue.hold(); control->tls.hold();
    tick(*game); waitFor(model().workEntered, 1, "body independent gate");
    auto session = Access::primarySession(*game);
    require(!session->terminalPublished(), "work-body hold cannot be terminal");
    model().loadGate.release(); waitFor(control->epilogueEntered, 1, "native epilogue gate");
    require(session->terminalPublished() && !session->quiescent(), "terminal publication is not thread reap");
    for (unsigned i = 0; i != 8; ++i) tick(*game);
    require(Access::workerOwnsEpoch(*game) && !model().unpauses.load() && control->actualReaps.load() == 0, "epilogue retains resources");
    control->epilogue.release(); waitFor(control->tlsEntered, 1, "actual pthread TLS destructor gate");
    for (unsigned i = 0; i != 8; ++i) tick(*game);
    require(!session->quiescent() && !model().unpauses.load() && !model().modDestroyed.load(), "TLS retention independent of terminal");
    control->tls.release(); spinUntil(*game, [&] { return !Access::hasOperation(*game); }, "publish after checked Linux reap");
    require(control->tlsExited.load() == 1 && control->actualReaps.load() == 1 && model().unpauses.load() == 1,
        "publication requires actual TLS completion/checked reap");
    session.reset(); complete(game, "terminal-epilogue-tls-reap");
}
void closeBeatsSuccess()
{
    auto game = makeGame();
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "reserve startup");
    auto control = backends()[0]; control->tls.hold(); tick(*game); waitFor(control->tlsEntered, 1, "terminal before close");
    require(Access::primarySession(*game)->terminalPublished(), "successful worker terminal exists");
    const unsigned unpauses = model().unpauses.load(); enqueueQuit(); control->tls.release();
    spinUntil(*game, [&] { return Access::finalClosed(*game); }, "same-tick close final cleanup");
    require(model().unpauses.load() == unpauses, "close must beat same-tick success publication");
    require(model().modDestroyed.load() == 1 && model().closes.load() == 1, "cleanup retired unexposed successful epoch");
    complete(game, "close-beats-success");
}
void boundedPump(bool failRender)
{
    auto game = makeGame(); model().loadGate.hold();
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "reserve startup");
    tick(*game); waitFor(model().workEntered, 1, "held worker");
    auto control = backends()[0];
    if (!failRender)
    {
        model().focus = 0;
        SDL_Event focus = {}; focus.type = SDL_ACTIVEEVENT; focus.active.gain = 0;
        focus.active.state = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS; enqueue(focus);
    }
    else
    {
        require((model().focus.load() & SDL_APPACTIVE) != 0, "render-failure case must remain active");
        model().failFlip = true;
    }
    for (unsigned i = 0; i != (failRender ? 100u : 99u); ++i) enqueueKey(SDLK_F12);
    const unsigned polls = control->polls.load(), nativePolls = model().polls.load();
    const unsigned resets = model().windowResets.load(), frees = model().modDestroyed.load();
    const unsigned flips = model().flips.load(), blits = model().blits.load(), palettes = model().physicalPaletteChanges.load();
    tick(*game);
    require(queuedEvents() == 36 && model().polls.load() - nativePolls == 64, "bounded event budget exactly64 including actual focus event");
    require(control->polls.load() == polls + 1, "exactly one backend poll despite focus/render state");
    require(model().windowResets.load() == resets && model().modDestroyed.load() == frees, "no active resource/display mutation");
    if (failRender) require(model().flips.load() == flips + 1 && model().blits.load() > blits, "active render failure actually attempted presentation");
    else require(model().flips.load() == flips && model().blits.load() == blits && model().physicalPaletteChanges.load() == palettes,
        "actual minimized focus event suppresses flip/blit/physical palette without stopping poll");
    const unsigned afterFailureFlips = model().flips.load(), afterFailureBlits = model().blits.load(), afterFailurePalettes = model().physicalPaletteChanges.load();
    tick(*game); require(queuedEvents() == 0, "suppressed input consumed, not replayed");
    require(control->polls.load() == polls + 2, "continued retirement pump after focus loss/render failure");
    require(model().flips.load() == afterFailureFlips && model().blits.load() == afterFailureBlits && model().physicalPaletteChanges.load() == afterFailurePalettes,
        "minimized or failed presentation stays suppressed on later retirement tick");
    model().failFlip = false; complete(game, failRender ? "render-failure-pump" : "focus-bounded-pump");
}
void loadFailure(bool unknown)
{
    auto game = makeGame();
    // Language failure follows successful actual catalog qualification, so the
    // partial objects are safe to retire with the existing provider proof.
    model().failLanguage = true;
    if (unknown) model().throwUnknownLoad = true;
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "reserve failure case");
    if (!unknown)
    {
        spinUntil(*game, [&] { return Access::finalClosed(*game); }, "load failure cleanup");
        require(model().modConstructed.load() == 1 && model().modDestroyed.load() == 1 && model().unpauses.load() == 0,
            "actual partial Mod adopted and destroyed only in cleanup");
        complete(game, "language-failure-cleanup");
    }
    else
    {
        spinUntil(*game, [&] { return backends()[1]->actualReaps.load() == 1; }, "unknown native failure retained after cleanup refusal");
        require(Access::workerOwnsEpoch(*game) && !Access::finalClosed(*game) && model().modDestroyed.load() == 0 && model().closes.load() == 0,
            "unknown entered load cannot fabricate provider destruction proof");
        const unsigned starts = backends()[1]->starts.load(); enqueueKey(SDLK_r); enqueueQuit(); tick(*game);
        require(backends()[1]->starts.load() == starts, "entered retained cleanup cannot retry");
        std::puts("CASE unknown-load-retained COMPLETE actual_reap=1 device_closed=0 contexts_retained=1");
        std::fflush(nullptr); std::_Exit(0); // explicit retained-process outcome, not a graceful cleanup pass
    }
}
void cleanupRetry(bool setupFailure)
{
    auto game = makeGame();
    setNextBackendFault(BackendControl::Fault::None);
    setNextBackendFault(setupFailure ? BackendControl::Fault::SetupFailureOnce : BackendControl::Fault::NoThreadOnce);
    model().failLanguage = true;
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "reserve retry case");
    auto cleanup = backends()[1];
    if (setupFailure) cleanup->tls.hold();
    spinUntil(*game, [&] { return cleanup->starts.load() == 1; }, "cleanup independent first start");
    if (setupFailure)
    {
        waitFor(cleanup->tlsEntered, 1, "setup worker real TLS");
        enqueueKey(SDLK_r); enqueueQuit(); tick(*game);
        require(cleanup->starts.load() == 1 && cleanup->actualReaps.load() == 0, "cannot retry unreaped setup worker");
        cleanup->tls.release();
    }
    auto session = Access::cleanupSession(*game);
    spinUntil(*game, [&] { return session->quiescent(); }, "cleanup failed start consumed only after actual reap");
    tick(*game);
    require(!session->admissionClosed(), "outer quit must not close retry session admission");
    require(model().modDestroyed.load() == 0 && model().closes.load() == 0, "NoThread/setup does not close live device");
    enqueueQuit(); tick(*game); require(cleanup->starts.load() == 1, "repeated close is not implicit retry");
    enqueueKey(SDLK_r); tick(*game);
    require(cleanup->starts.load() == 2, "explicit eligible retry starts same open real session");
    spinUntil(*game, [&] { return Access::finalClosed(*game); }, "retry close proof and actual reap");
    require(cleanup->actualReaps.load() == (setupFailure ? 2u : 1u), "exact real reaps for retry attempts");
    session.reset(); complete(game, setupFailure ? "setup-reap-retry" : "no-thread-retry");
}
void copyFailureRetains()
{
    auto game = makeGame();
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "reserve copy case");
    auto primary = backends()[0]; primary->tls.hold(); tick(*game); waitFor(primary->tlsEntered, 1, "copy terminal TLS gate");
    backends()[1]->entry.hold(); model().failCopyOnce = true; primary->tls.release();
    spinUntil(*game, [&] { return model().copyFailures.load() == 1; }, "real LoaderSession copy injection");
    require(model().modDestroyed.load() == 0 && model().unpauses.load() == 0, "result copy failure cannot expose/destroy retained context");
    complete(game, "result-copy-failure");
}

class CallbackOptions final : public OptionsBaseState
{
    unsigned mode;
    bool fired;
    void reserve()
    {
        require(!fired, "callback invoked twice"); fired = true;
        trace("test-callback-enter"); btnOkClick(nullptr);
        require(Access::hasOperation(*activeGame) && !Access::workerOwnsEpoch(*activeGame), "callback only reserves");
        require(backends().back()->starts.load() == 0, "cleanup worker unexpectedly started in callback");
        trace("test-callback-return");
    }
public:
    explicit CallbackOptions(unsigned value) : OptionsBaseState(OPT_MENU), mode(value), fired(false) {}
    void init() override { if (mode == 0) reserve(); }
    void handle(Action *) override { if (mode == 1 && !fired) reserve(); }
    void think() override { if (mode == 2 && !fired) reserve(); }
    void blit() override { require(!fired, "generic blit after reservation"); }
    ~CallbackOptions() override { trace("initiating-state-deleted"); }
};
void actualCaller(unsigned callbackMode, bool mods)
{
    auto game = makeGame(); loadToPublished(*game);
    game->setState(new MainMenuState());
    Options::reload = true;
    const unsigned saves = model().saves.load();
    clearTrace();
    if (mods)
    {
        auto *state = new ModListState(); game->pushState(state);
        state->btnOkClick(nullptr); trace("test-callback-return");
        require(Access::hasOperation(*game) && !Access::workerOwnsEpoch(*game), "actual ModList caller reservation");
        enqueueQuit(); game->run();
    }
    else
    {
        autoCloseAtReservation = true;
        game->pushState(new CallbackOptions(callbackMode)); game->run(); autoCloseAtReservation = false;
    }
    require(Access::finalClosed(*game), "real run loop reaches proof-backed close");
    require(tracePosition("reservation") < tracePosition("test-callback-return") &&
            tracePosition("test-callback-return") < tracePosition("commit") &&
            tracePosition("deleted-drained") < tracePosition("start-prepared"), "actual menu callback boundary ordering");
    require(model().saves.load() >= saves, "save counter monotonic");
    complete(game, mods ? "actual-mod-list-caller" : callbackMode == 0 ? "actual-options-init" : callbackMode == 1 ? "actual-options-handle" : "actual-options-think");
}
void callbackDrain()
{
    auto game = makeGame(); loadToPublished(*game); game->setState(new MainMenuState());
    auto *options = new OptionsBaseState(OPT_MENU); game->pushState(options); Options::reload = true;
    CallbackHold callback;
    const unsigned drains = model().drains.load(), destroyed = model().modDestroyed.load();
    options->btnOkClick(nullptr); tick(*game);
    const auto deadline = monotonicNanoseconds() + 10000000000ULL;
    while (model().locks.load() <= drains)
    { require(monotonicNanoseconds() < deadline, "worker entered same callback mutex"); tick(*game); }
    const auto primary = backends()[2];
    require(model().drains.load() == drains && model().modDestroyed.load() == destroyed, "pause alone cannot free old Mod");
    const unsigned polls = primary->polls.load(); enqueueKey(SDLK_F12); tick(*game);
    require(primary->polls.load() == polls + 1, "owner pumps independently while worker waits on callback mutex");
    callback.release();
    spinUntil(*game, [&] { return !Access::hasOperation(*game); }, "reload after actual callback drain/reap");
    require(model().modDestroyed.load() == destroyed + 1 && model().opens.load() == 1 && model().closes.load() == 0,
        "reload retires real old Mod without changing known mixer generation");
    complete(game, "independent-callback-drain");
}
void admission()
{
    auto game = makeGame();
    const unsigned saves = model().saves.load(), maps = model().maps.load(), resets = model().windowResets.load();
    Options::audioChunkSize = 63;
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::InvalidDeviceState, "invalid requested plan rejects");
    require(Options::audioChunkSize == 63, "staged bad option unchanged"); Options::audioChunkSize = 1024;
    for (int count : {-1, 2})
    {
        model().openCount = count;
        require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::InvalidDeviceState, "invalid actual open count rejects");
    }
    model().openCount = 1;
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::UnknownAudioGeneration, "positive count does not certify generation");
    model().openCount = 0;
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::OptionsMenu, nullptr) == Game::LoadingAdmission::UnsupportedProfile, "wrong origin rejects before mutation");
    require(model().saves.load() == saves && model().maps.load() == maps && model().windowResets.load() == resets && backends().empty(), "rejected admission side effect counters unchanged");
    loadToPublished(*game);
    game->setState(new MainMenuState()); auto *options = new OptionsBaseState(OPT_MENU); game->pushState(options);
    std::unique_ptr<SavedGame> save(new SavedGame()); game->setSavedGame(save.get());
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::OptionsMenu, options) == Game::LoadingAdmission::LiveSave, "actual live save rejects reload");
    game->setSavedGame(nullptr); // ordinary setter deletes the owned SavedGame
    save.release();
    Options::reload = true; options->btnOkClick(nullptr);
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::OptionsMenu, options) == Game::LoadingAdmission::Busy, "double reservation rejects busy");
    complete(game, "admission-no-mutation");
}
void preparationAllocation()
{
    auto game = makeGame();
    const unsigned languages = model().languageConstructed.load() - model().languageDestroyed.load();
    for (int point : {0, 1, 2, 3, 4, 5})
    {
        model().failAllocationAfter = point;
        const auto admitted = game->reserveResponsiveLoad(Game::LoadingOrigin::Startup);
        model().failAllocationAfter = -1;
        require(admitted == Game::LoadingAdmission::PreparationFailure, "target preparation allocation rejected");
        require(!Access::hasOperation(*game) && !Access::workerOwnsEpoch(*game), "failed preparation retains initial ownership");
        require(game->getLanguage() && !game->getMod(), "failed preparation preserves bootstrap Language");
        for (auto &control : backends()) require(control->starts.load() == 0, "failed preparation never begins session");
        require(model().languageConstructed.load() - model().languageDestroyed.load() == languages, "failed preparation lifetime balance");
    }
    require(model().allocationFailures.load() == 6, "exact modeled allocator leaf hits");
    loadToPublished(*game); complete(game, "preparation-allocation");
}
void navigationAllocation()
{
    auto game = makeGame();
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "reserve navigation failure");
    auto primary = backends()[0]; primary->tls.hold(); tick(*game); waitFor(primary->tlsEntered, 1, "navigation waits actual reap");
    primary->tls.release();
    model().failAllocationAfter = 0;
    spinUntil(*game, [&] { return model().allocationFailures.load() == 1; }, "actual navigation allocation leaf");
    model().failAllocationAfter = -1;
    require(model().unpauses.load() == 0 && Access::workerOwnsEpoch(*game), "failed navigation cannot expose or unpause epoch");
    complete(game, "navigation-allocation");
}
void cleanupCloseBeforeReap()
{
    auto game = makeGame();
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "reserve close-reap distinction");
    auto cleanup = backends()[1]; cleanup->tls.hold();
    model().failLanguage = true;
    spinUntil(*game, [&] { return cleanup->tlsEntered.load() == 1; }, "cleanup actual TLS gate");
    require(model().closes.load() == 1 && model().openCount.load() == 0 && model().modDestroyed.load() == 1,
        "worker recorded independent native close/resource receipt");
    require(Access::cleanupSession(*game)->terminalPublished() && !Access::cleanupSession(*game)->quiescent(),
        "closed device and terminal still not actual retirement");
    for (unsigned i = 0; i != 8; ++i) tick(*game);
    require(!Access::finalClosed(*game) && model().sdlQuits.load() == 0, "native close alone cannot finalize SDL");
    cleanup->tls.release(); complete(game, "cleanup-close-before-reap");
}
void primaryNoThread(bool reload)
{
    auto game = makeGame();
    if (reload) { loadToPublished(*game); game->setState(new MainMenuState()); }
    setNextBackendFault(BackendControl::Fault::NoThreadOnce);
    if (reload)
    {
        auto *options = new OptionsBaseState(OPT_MENU); game->pushState(options); Options::reload = true;
        CallbackHold callback; const unsigned destroyed = model().modDestroyed.load();
        const unsigned drains = model().drains.load(); options->btnOkClick(nullptr); tick(*game);
        spinUntil(*game, [&] { return model().locks.load() > drains; }, "cleanup started independent of NoThread primary");
        require(model().modDestroyed.load() == destroyed && model().openCount.load() == 1, "NoThread does not retire preexisting callback");
        callback.release();
    }
    else { require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "initial NoThread admission"); tick(*game); }
    complete(game, reload ? "primary-no-thread-reload" : "primary-no-thread-initial");
}
void rejectedCaller(bool mods)
{
    auto game = makeGame(); loadToPublished(*game); game->setState(new MainMenuState());
    State *caller;
    if (mods) { auto *state = new ModListState(); game->pushState(state); caller = state; }
    else { auto *state = new OptionsBaseState(OPT_MENU); game->pushState(state); caller = state; }
    Options::reload = true; Options::audioChunkSize = 63;
    const unsigned saves = model().saves.load(), maps = model().maps.load(), resets = model().windowResets.load();
    const unsigned opens = model().opens.load(), closes = model().closes.load();
    if (mods) static_cast<ModListState *>(caller)->btnOkClick(nullptr);
    else static_cast<OptionsBaseState *>(caller)->btnOkClick(nullptr);
    require(!Access::hasOperation(*game) && Options::audioChunkSize == 63 && Options::reload, "rejection preserves staged edits");
    require(dynamic_cast<ErrorMessageState *>(Access::topState(*game)) != nullptr, "actual originating menu gets usable rejection state");
    require(model().saves.load() == saves && model().maps.load() == maps && model().windowResets.load() == resets &&
        model().opens.load() == opens && model().closes.load() == closes, "real rejected caller side effects unchanged");
    game->popState(); require(Access::topState(*game) == caller, "rejection returns to same originating menu");
    Options::audioChunkSize = 1024;
    complete(game, mods ? "mod-list-rejection" : "options-rejection");
}
void configureCatalogProbe(CatalogProbe probe)
{
    catalogProbe = probe;
    for (const auto &entry : musicTypeCases)
        configureMusicProvider(std::string("enum-") + entry.id + ".mid", entry.type);
    if (probe == CatalogProbe::Fallback || probe == CatalogProbe::Gm || probe == CatalogProbe::GmMalformed)
    {
        const char fonts[] = "fonts: []\n";
        const std::string fontPath = createFixtureFile("fonts.yml", fonts, sizeof(fonts) - 1);
        aliasFile("Language/", fontPath);
        if (probe == CatalogProbe::Fallback)
        {
            Options::preferredMusic = MUSIC_MIDI; setSoundFolder({"fixture-track.mid", "fixture-track.ogg"});
            configureMusicProvider("SOUND/fixture-track.mid", MUS_MID, true);
            // Extension OGG, actual decoder WAV: production must query handle.
            configureMusicProvider("SOUND/fixture-track.ogg", MUS_WAV);
        }
        else
        {
            Options::preferredMusic = MUSIC_GM; setSoundFolder({"gm.cat"});
            std::string catPath;
            // One CAT record, offset8, payload length3 (tempo120/no subs/no
            // tracks) or malformed length1. No private or arbitrary file data.
            const unsigned char valid[] = {8,0,0,0,3,0,0,0,120,0,0};
            const unsigned char malformed[] = {8,0,0,0,1,0,0,0,120};
            if (probe == CatalogProbe::Gm) catPath = createFixtureFile("synthetic-gm.cat", valid, sizeof valid);
            else catPath = createFixtureFile("synthetic-gm.cat", malformed, sizeof malformed);
            aliasFile("SOUND/gm.cat", catPath); configureMemoryMusicProvider(MUS_MID);
        }
    }
}
[[noreturn]] void retainedCatalogOutcome(Game &game, const char *name)
{
    spinUntil(game, [&] { return backends()[1]->actualReaps.load() == 1; }, "actual catalog refusal cleanup reaped");
    require(backends()[0]->actualReaps.load() == 1 && saw("provider-catalog-oracles-complete"), "actual classification and primary native retirement");
    require(Access::hasOperation(game) && !Access::hasStandby(game) && Access::workerOwnsEpoch(game) && !Access::finalClosed(game),
        "unsupported catalog blocks publication/navigation and remains owned");
    require(model().unpauses.load() == 0 && model().musicPlays.load() == 0 && model().hooks.load() == 0 &&
        model().modDestroyed.load() == 0 && model().closes.load() == 0 && !saw("native-music-free"),
        "unsupported adopted catalog cannot authorize unpause/play/free/close");
    std::printf("CASE %s COMPLETE actual_reap=1 device_closed=0 contexts_retained=1\n", name);
    std::fflush(nullptr); std::_Exit(0); // explicit retained-process result; never native provider acceptance
}
void providerCatalog(CatalogProbe probe, const char *name)
{
    configureCatalogProbe(probe); auto game = makeGame();
    bool rejected = probe == CatalogProbe::Enums || probe == CatalogProbe::Null || probe == CatalogProbe::Blank;
#ifndef __NO_MUSIC
    rejected = rejected || probe == CatalogProbe::Gm || probe == CatalogProbe::GmMalformed;
#endif
    if (rejected)
    {
        require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "actual catalog test startup reserve");
        retainedCatalogOutcome(*game, name);
    }
    loadToPublished(*game); require(saw("provider-catalog-oracles-complete"), "actual resource-leaf catalog assertions executed");
    Mod *mod = game->getMod();
    if (probe == CatalogProbe::Empty)
    {
        require(mod->responsiveMusicCatalog() == Mod::ResponsiveMusicCatalog::Empty, "empty catalog published without fabricated track capability");
        Music *sentinel = LoadingGameResourceAccess::randomMusic(*mod, "missing-track"); require(sentinel && !sentinel->isSoftwareMixedAudio(), "actual mute sentinel is blank and unsupported");
        const unsigned plays = model().musicPlays.load(), halts = model().musicHalts.load(), hooks = model().hooks.load(), unhooks = model().unhooks.load();
        const std::string label = LoadingGameResourceAccess::playingMusic(*mod);
        sentinel->play(); mod->playMusic("missing-track");
        Options::mute = true; require(mod->getMusic("anything") == sentinel, "mute accessor returns actual sentinel");
        sentinel->play(); mod->playMusic("anything", 1); Options::mute = false;
        require(model().musicPlays.load() == plays && model().musicHalts.load() == halts && model().hooks.load() == hooks &&
            model().unhooks.load() == unhooks && LoadingGameResourceAccess::playingMusic(*mod) == label,
            "blank/mute/missing sentinel remains explicit no-op, not a playing label");
    }
    complete(game, name);
}
struct PlaybackSnapshot
{
    unsigned plays, halts, hooks, unhooks;
    std::uintptr_t current;
    std::string label;
    explicit PlaybackSnapshot(const Mod &mod) : plays(model().musicPlays.load()), halts(model().musicHalts.load()),
        hooks(model().hooks.load()), unhooks(model().unhooks.load()), current(model().currentMusic.load()), label(LoadingGameResourceAccess::playingMusic(mod)) {}
    void unchanged(const Mod &mod) const
    {
        require(model().musicPlays.load() == plays && model().musicHalts.load() == halts && model().hooks.load() == hooks &&
            model().unhooks.load() == unhooks && model().currentMusic.load() == current && LoadingGameResourceAccess::playingMusic(mod) == label,
            "rejected play changed prior native playback, hook, stop count or actual Mod label");
    }
};
template<typename Request>
void expectMusicRefusal(Request request, bool expected)
{
    bool rejected = false;
    try { request(); } catch (const Exception &) { rejected = true; }
    require(rejected == expected, "actual caller has wrong explicit music-rejection outcome");
}
void providerPreplay()
{
    configureCatalogProbe(CatalogProbe::Adlib); auto game = makeGame(); loadToPublished(*game);
    Mod *mod = game->getMod();
#ifndef __NO_MUSIC
    mod->playMusic("FIXTURE_CATALOG");
    require(model().currentMusic.load() != 0 && model().musicPlays.load() == 1, "real previous software playback is active");
#else
    // The disabled play body is an intentional no-op; a true Adlib override is
    // still separate from ordinary blank Music. No native playback is claimed.
    mod->playMusic("ADLIB_EMPTY"); require(model().musicPlays.load() == 0, "music-disabled playback makes no native call");
#endif
    const PlaybackSnapshot before(*mod);
    Music *emptyAdlib = mod->getMusic("ADLIB_EMPTY");
    require(emptyAdlib->isSoftwareMixedAudio(), "actual unloaded Adlib override visible without observer");
    expectMusicRefusal([&] { emptyAdlib->play(); },
#ifndef __NO_MUSIC
        true
#else
        false
#endif
    ); before.unchanged(*mod);
#ifndef __NO_MUSIC
    Music *loadedAdlib = mod->getMusic("ADLIB_LOADED");
    const int rate = Options::audioSampleRate; Options::audioSampleRate = 44100;
    expectMusicRefusal([&] { loadedAdlib->play(); }, true); before.unchanged(*mod); Options::audioSampleRate = rate;
    model().frequency = 44100;
    expectMusicRefusal([&] { loadedAdlib->play(); }, true); before.unchanged(*mod); model().frequency = rate;
#endif
    // Deliberately inject adopted members into the real map AFTER a supported
    // publication. This tests per-call bypass guards, not catalog publication.
    // Such owners are retained at process exit; no destructor/close is granted.
    for (const auto &entry : musicTypeCases) if (!entry.supported)
    {
        const std::string path = std::string("direct-") + entry.id + ".wav";
        configureMusicProvider(path, entry.type);
        std::unique_ptr<Music> candidate(new Music()); candidate->load(path); Music *raw = candidate.get();
        const std::string name = std::string("BAD_") + entry.id;
        provideMusic(mod, name + "1", std::move(candidate));
        expectMusicRefusal([&] { raw->play(); },
#ifndef __NO_MUSIC
            true
#else
            false
#endif
        ); before.unchanged(*mod);
        expectMusicRefusal([&] { mod->playMusic(name, 1); }, true); before.unchanged(*mod);
        expectMusicRefusal([&] { mod->playMusic(name); }, true); before.unchanged(*mod);
    }
    require(mod->responsiveMusicCatalog() == Mod::ResponsiveMusicCatalog::Unsupported, "injected mixed catalog remains honestly unsupported");
    require(model().closes.load() == 0 && model().modDestroyed.load() == 0 && !Access::hasOperation(*game), "direct guard changes no lifecycle ownership");
    std::puts("CASE provider-preplay-retained COMPLETE actual_reap=1 device_closed=0 contexts_retained=1 injected_map=1");
    std::fflush(nullptr); std::_Exit(0);
}
std::size_t traceAfter(const char *name, std::size_t start)
{
    const unsigned count = std::min<unsigned>(ledgerCount.load(), ledger.size());
    for (std::size_t i = start; i < count; ++i)
        if (ledger[i].ready.load(std::memory_order_acquire) && !std::strcmp(ledger[i].name, name)) return i;
    return ledger.size();
}
void reloadChunk(bool changed)
{
    auto game = makeGame(); loadToPublished(*game); game->setState(new MainMenuState());
    auto *options = new OptionsBaseState(OPT_MENU); game->pushState(options);
    const auto generation = game->responsiveAudioGeneration();
    const unsigned opens = model().opens.load(), closes = model().closes.load(), drains = model().drains.load();
    Options::audioChunkSize = 63;
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::OptionsMenu, options) == Game::LoadingAdmission::InvalidDeviceState &&
        game->responsiveAudioGeneration() == generation && model().opens.load() == opens && model().closes.load() == closes,
        "rejected chunk request preserves published generation and device");
    Options::audioChunkSize = changed ? 2048 : 1024; Options::reload = true; clearTrace(); model().loadGate.hold();
    options->btnOkClick(nullptr);
    require(game->responsiveAudioGeneration() == generation && model().opens.load() == opens && model().closes.load() == closes,
        "reservation does not overwrite prior chunk provenance");
    tick(*game); waitFor(model().workEntered, 2, "actual menu reload reaches new resource leaf");
    require(model().modDestroyed.load() == 1 && model().audioStatus.load() == SDL_AUDIO_PAUSED, "old Mod destroyed in paused owned device domain");
    require(model().opens.load() == opens + (changed ? 1u : 0u) && model().closes.load() == closes + (changed ? 1u : 0u),
        "unchanged chunk reuses device; changed chunk closes/reopens exactly once");
    if (changed)
    {
        const auto destroyed = tracePosition("mod-destroyed"), closed = tracePosition("native-close"), opened = tracePosition("native-open");
        const auto drained = traceAfter("drain-complete", opened + 1), constructed = tracePosition("mod-constructed");
        require(destroyed < closed && closed < opened && opened < drained && drained < constructed && constructed < ledger.size(),
            "changed chunk ordering: old free, close, open, fresh drain, new real Mod");
        require(model().drains.load() >= drains + 2, "changed chunk requires both old and fresh drains");
    }
    else require(!saw("native-open") && !saw("native-close"), "unchanged chunk no device replacement");
    model().loadGate.release(); spinUntil(*game, [&] { return !Access::hasOperation(*game); }, "menu reload publication after native reap");
    require(game->responsiveAudioGeneration() == generation + (changed ? 1u : 0u), "only completed reopen advances generation");
    complete(game, changed ? "reload-changed-chunk" : "reload-unchanged-chunk");
}
// The process handler may run on any thread, but only the thread executing the
// immediately adjacent target can be armed. Unarmed failure never calls fail(),
// dumpTrace(), model(), or any potentially destroyed fixture context.
thread_local const char *deathTarget = nullptr;
[[noreturn]] void expectedTermination() noexcept
{
    if (!deathTarget)
    {
        std::fputs("GAME_INTEGRATION UNARMED_TERMINATION\n", stderr);
        std::fflush(stderr); std::_Exit(87);
    }
    const char *target = deathTarget; deathTarget = nullptr;
    std::fprintf(stderr, "GAME_INTEGRATION EXPECTED_FAIL_STOP %s\n", target);
    std::fflush(stderr); std::_Exit(86);
}
class DeathTargetScope
{
    std::terminate_handler previous;
public:
    explicit DeathTargetScope(const char *target) : previous(std::set_terminate(&expectedTermination))
    { require(!deathTarget, "nested death target"); deathTarget = target; }
    void disarm() noexcept { deathTarget = nullptr; }
    ~DeathTargetScope() { disarm(); std::set_terminate(previous); }
};
void deathInvariant(bool callbackDepth)
{
    auto game = makeGame(); model().loadGate.hold();
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup) == Game::LoadingAdmission::Accepted, "death case genuine reserve");
    if (callbackDepth)
    {
        // An actual reserved operation exists. Only simulate a still-active
        // initiating callback depth immediately before the invariant target.
        Access::callbackDepth(*game, 1);
        DeathTargetScope target("callback-depth-boundary");
        Access::commitBoundary(*game); target.disarm();
    }
    else
    {
        tick(*game); waitFor(model().workEntered, 1, "death case actual active worker");
        DeathTargetScope target("owner-resource-access");
        (void)game->getMod(); target.disarm();
    }
    fail("target invariant returned instead of fail-stop");
}
void actualMain()
{
    autoCloseAtReservation = true;
    char argument[] = "modeled-responsive-main"; char *argv[] = {argument, nullptr};
    const int result = openxcomResponsiveApplicationMain(1, argv);
    require(result == EXIT_SUCCESS && model().sdlQuits.load() == 1 && saw("reservation") && saw("actual-reap"), "actual main startup/run/destructor route");
    require(!activeGame && model().openCount.load() == 0, "actual main closed safely");
    std::puts("CASE actual-main COMPLETE actual_reap=1 device_closed=1 contexts_destroyed=1");
}
}
int main(int argc, char **argv)
{
    if (argc != 2) { std::fprintf(stderr, "one reviewed case name required\n"); return 2; }
    reset();
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    std::fputs("PROFILE observer=ON ", stdout);
#else
    std::fputs("PROFILE observer=OFF ", stdout);
#endif
#ifdef __NO_MUSIC
    std::puts("music=OFF");
#else
    std::puts("music=ON");
#endif
    const std::string name = argv[1];
    try
    {
        if (name == "startup-boundary") startupBoundary();
        else if (name == "terminal-epilogue-tls-reap") terminalAndReap();
        else if (name == "close-beats-success") closeBeatsSuccess();
        else if (name == "focus-bounded-pump") boundedPump(false);
        else if (name == "render-failure-pump") boundedPump(true);
        else if (name == "language-failure-cleanup") loadFailure(false);
        else if (name == "unknown-load-retained") loadFailure(true);
        else if (name == "no-thread-retry") cleanupRetry(false);
        else if (name == "setup-reap-retry") cleanupRetry(true);
        else if (name == "result-copy-failure") copyFailureRetains();
        else if (name == "actual-options-init") actualCaller(0, false);
        else if (name == "actual-options-handle") actualCaller(1, false);
        else if (name == "actual-options-think") actualCaller(2, false);
        else if (name == "actual-mod-list-caller") actualCaller(0, true);
        else if (name == "independent-callback-drain") callbackDrain();
        else if (name == "admission-no-mutation") admission();
        else if (name == "actual-main") actualMain();
        else if (name == "death-callback-depth") deathInvariant(true);
        else if (name == "death-owner-resource") deathInvariant(false);
        else if (name == "options-rejection") rejectedCaller(false);
        else if (name == "mod-list-rejection") rejectedCaller(true);
        else if (name == "preparation-allocation") preparationAllocation();
        else if (name == "navigation-allocation") navigationAllocation();
        else if (name == "cleanup-close-before-reap") cleanupCloseBeforeReap();
        else if (name == "primary-no-thread-initial") primaryNoThread(false);
        else if (name == "primary-no-thread-reload") primaryNoThread(true);
        else if (name == "provider-enum-table-retained") providerCatalog(CatalogProbe::Enums, argv[1]);
        else if (name == "provider-supported-catalog") providerCatalog(CatalogProbe::Supported, argv[1]);
        else if (name == "provider-empty-catalog") providerCatalog(CatalogProbe::Empty, argv[1]);
        else if (name == "provider-null-catalog-retained") providerCatalog(CatalogProbe::Null, argv[1]);
        else if (name == "provider-blank-catalog-retained") providerCatalog(CatalogProbe::Blank, argv[1]);
        else if (name == "provider-preferred-fallback") providerCatalog(CatalogProbe::Fallback, argv[1]);
        else if (name == "provider-gm-memory") providerCatalog(CatalogProbe::Gm, argv[1]);
        else if (name == "provider-gm-malformed") providerCatalog(CatalogProbe::GmMalformed, argv[1]);
        else if (name == "provider-preplay-retained") providerPreplay();
        else if (name == "reload-changed-chunk") reloadChunk(true);
        else if (name == "reload-unchanged-chunk") reloadChunk(false);
        else { std::fprintf(stderr, "unknown case: %s\n", argv[1]); return 2; }
    }
    catch (...) { fail("unexpected fixture/orchestration exception; no target-invariant success"); }
    return 0;
}
#endif
