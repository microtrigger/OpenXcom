// SPDX-License-Identifier: GPL-3.0-or-later
// Original timing calibration contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
// Same-process/same-thread descriptive calibration, never an equivalence verdict.
#include "RenderPairGate.h"
#include "../../src/Battlescape/BattlescapeState.h"
#include "../../src/Battlescape/Map.h"
#include "../../src/Battlescape/Camera.h"
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/RNG.h"
#include "../../src/Engine/Screen.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/Cursor.h"
#include "../../src/Interface/FpsCounter.h"
#include "../../src/Mod/Armor.h"
#include "../../src/Mod/Mod.h"
#include "../../src/Mod/Unit.h"
#include "../../src/Observer/PresentedCursorSurface.h"
#include "../../src/Savegame/BattleUnit.h"
#include "../../src/Savegame/SavedBattleGame.h"
#include "../../src/Savegame/SavedGame.h"
#include "../../src/Savegame/Tile.h"
#include <SDL_mixer.h>
#include <yaml-cpp/yaml.h>
#include <array>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace OpenXcom { namespace RenderPairGate {
namespace {
using Clock = std::chrono::steady_clock;
bool enabled = false, running = false, pendingSetup = false, completed = false, recording = false;
bool mutant = false;
int peer = -1, recordFd = -1, pairIndex = 0, leg = 0, totalPairs = 0;
BattlescapeState *state = 0;
SavedBattleGame *currentBattle = 0;
BattleUnit *occupant = 0;
const Position hover(20, 22, 0), playerPosition(20, 20, 0), parked(2, 2, 0);
std::string epoch, admission, challenge;
std::set<std::string> randomTokens;
Clock::time_point runStart, lastResult;
bool haveResult = false, haveSeed = false;
uint64_t initialSeed = 0;
enum Kind { WARMUP, AA, AB };
struct Pair { Kind kind; bool reversed; };
std::array<Pair, 56> schedule;
struct Sample { Clock::time_point t0, flip, t1; };
std::array<Sample, 112> samples; // Preallocated before any timed request.
struct Frame {
    SDL_Surface *surface = 0;
    const Tile *tile = 0;
    int width = 0, height = 0, bw = 0, bh = 0, phase = -1;
    int cropX = 0, cropY = 0, cropWidth = 0, cropHeight = 0;
    int cropScreenWidth = 0, cropScreenHeight = 0;
    unsigned int visits = 0, flips = 0, captures = 0;
    bool allowed = false;
    std::vector<unsigned char> rgb;
    std::string request, response;
};
std::array<Frame, 2> frames;
Frame &frame() { return frames[leg]; }
Sample &sample() { return samples[pairIndex * 2 + leg]; }
char currentLabel() { return (leg == int(schedule[pairIndex].reversed)) ? 'A' : 'B'; }
const char *kindName(Kind kind) { return kind == AA ? "aa" : kind == AB ? "ab" : "warmup"; }
void require(bool condition, const char *reason)
{
    if (!condition) {
        std::cerr << "TIMING_CALIBRATION INCOMPLETE " << reason << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
void watchdog()
{
    require(Clock::now() - runStart < std::chrono::seconds(mutant ? 90 : 120), "watchdog");
}
void drainFocusEvents()
{
    // Outside timing. Do not let focus events queued during spacing (including
    // the last sample) disappear behind a successful completion marker.
    SDL_Event event;
    while (SDL_PollEvent(&event)) timingEvent(event);
}
void writeRecord(const std::string &record)
{
    require(write(recordFd, record.data(), record.size()) == static_cast<ssize_t>(record.size()), "record-write");
}
void recordSample()
{
    const Sample &s = sample();
    const auto ns = [](Clock::time_point t) { return std::chrono::duration_cast<std::chrono::nanoseconds>(t - runStart).count(); };
    std::ostringstream out;
    out << "{\"type\":\"sample\",\"index\":" << pairIndex * 2 + leg
        << ",\"pair\":" << pairIndex << ",\"kind\":\"" << kindName(schedule[pairIndex].kind)
        << "\",\"order\":\"" << (schedule[pairIndex].reversed ? "BA" : "AB")
        << "\",\"label\":\"" << currentLabel() << "\",\"t0_ns\":" << ns(s.t0)
        << ",\"flip_ns\":" << ns(s.flip) << ",\"t1_ns\":" << ns(s.t1) << "}\n";
    writeRecord(out.str());
}
bool token(const std::string &value)
{
    return value.size() == 32 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}
void fresh(const std::string &value)
{
    require(token(value) && randomTokens.insert(value).second, "transport-token-uniqueness");
}
std::string randomChallenge()
{
    unsigned char bytes[16];
    const int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    require(fd >= 0, "challenge-source");
    const ssize_t count = read(fd, bytes, sizeof(bytes));
    close(fd);
    require(count == sizeof(bytes), "challenge-read");
    std::string result;
    const char *hex = "0123456789abcdef";
    for (unsigned char b : bytes) { result += hex[b >> 4]; result += hex[b & 15]; }
    fresh(result);
    return result;
}
std::string receiveHello()
{
    char bytes[102401];
    iovec part = {bytes, sizeof(bytes)}; msghdr message = {};
    message.msg_iov = &part; message.msg_iovlen = 1;
    const ssize_t size = recvmsg(peer, &message, MSG_DONTWAIT);
    require(size > 0 && !(message.msg_flags & MSG_TRUNC), "producer-hello-response");
    return std::string(bytes, size);
}
void empty()
{
    char byte;
    const ssize_t count = recv(peer, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
    require(count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK), "producer-order-count-closure");
}
void silent()
{
    int frequency, channels; Uint16 format;
    require(Options::mute && Mix_QuerySpec(&frequency, &format, &channels) == 0 &&
            !(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO), "audio-disabled-profile");
}
struct Bijection {
    std::map<std::string, std::string> values;
    std::string one(const std::string &raw) {
        require(token(raw), "normalizer-token-shape");
        if (!values.count(raw)) {
            std::ostringstream out; out << std::hex << std::setw(32) << std::setfill('0') << values.size() + 1;
            values[raw] = out.str();
        }
        return values[raw];
    }
    std::string packet(const std::string &raw) {
        if (raw.size() == 96 && raw.find_first_not_of("0123456789abcdef") == std::string::npos) {
            std::string result;
            for (size_t offset = 0; offset < 96; offset += 32) result += one(raw.substr(offset, 32));
            return result;
        }
        std::string result = raw;
        for (const char *field : {"epoch", "challenge", "next_token"}) {
            const std::string prefix = std::string("\"") + field + "\":\"";
            const size_t at = result.find(prefix);
            require(at != std::string::npos && result.find(prefix, at + 1) == std::string::npos,
                    "normalizer-field-shape");
            const size_t start = at + prefix.size();
            require(result.size() > start + 32 && result[start + 32] == '"', "normalizer-field-width");
            result.replace(start, 32, one(result.substr(start, 32)));
        }
        require(result.size() == raw.size(), "normalizer-packet-length");
        return result;
    }
};
void copyAndVerifyFrame()
{
    Frame &f = frame();
    require(f.flips == 1 && f.captures == 1 && f.visits == 1 && f.phase == 1 &&
            f.tile == currentBattle->getTile(hover), "frame-instrumentation");
    require(Observer::supportedPresentedSurface(f.surface, f.bw, f.bh, f.allowed) &&
            f.surface->w == 640 && f.surface->h == 400, "supported-focused-presentation");
    f.width = f.surface->w; f.height = f.surface->h;
    require(f.rgb.size() == size_t(f.width * f.height * 3), "preallocated-frame-size");
    require(SDL_LockSurface(f.surface) == 0, "final-surface-lock");
    size_t offset = 0;
    for (int y = 0; y < f.surface->h; ++y) {
        const unsigned char *row = static_cast<const unsigned char *>(f.surface->pixels) + y * f.surface->pitch;
        for (int x = 0; x < f.surface->w; ++x) {
            const SDL_Color &color = f.surface->format->palette->colors[row[x]];
            f.rgb[offset++] = color.r; f.rgb[offset++] = color.g; f.rgb[offset++] = color.b;
        }
    }
    SDL_UnlockSurface(f.surface);
}
void verifyActualCrop(const std::string &response)
{
    const Frame &f = frame();
    Observer::PixelCrop crop;
    crop.screenWidth = f.cropScreenWidth; crop.screenHeight = f.cropScreenHeight;
    crop.x = f.cropX; crop.y = f.cropY; crop.width = f.cropWidth; crop.height = f.cropHeight;
    require(crop.screenWidth == f.width && crop.screenHeight == f.height &&
            crop.x >= 0 && crop.y >= 0 && crop.width > 0 && crop.height > 0 &&
            crop.width <= Observer::CursorCropSize && crop.height <= Observer::CursorCropSize &&
            crop.x + crop.width <= f.width && crop.y + crop.height <= f.height, "actual-crop-geometry");
    for (int y = crop.y; y < crop.y + crop.height; ++y)
        for (int x = crop.x; x < crop.x + crop.width; ++x)
            for (int c = 0; c < 3; ++c) crop.rgb.push_back(f.rgb[(y * f.width + x) * 3 + c]);
    require(Observer::validCrop(crop) &&
            response.find(std::string("\"crop\":") + Observer::serializeCrop(crop)) != std::string::npos,
            "actual-produced-crop");
}
void verifyPair()
{
    const Frame &a = frames[0], &b = frames[1];
    require(a.width == b.width && a.height == b.height && a.rgb == b.rgb, "pair-final-rgb");
    require(a.cropX == b.cropX && a.cropY == b.cropY && a.cropWidth == b.cropWidth &&
            a.cropHeight == b.cropHeight && a.cropScreenWidth == b.cropScreenWidth &&
            a.cropScreenHeight == b.cropScreenHeight, "pair-presentation-geometry");
    Bijection left, right;
    require(left.packet(a.request) == right.packet(b.request) &&
            left.packet(a.response) == right.packet(b.response), "pair-public-transcript");
    writeRecord(std::string("{\"type\":\"pair\",\"pair\":") + std::to_string(pairIndex) + "}\n");
}
} // namespace

void prepare()
{
    const char *option = std::getenv("OPENXCOM_TIMING_CALIBRATION");
    if (!option) return;
    enabled = true;
    require(std::string(option) == "normal" || std::string(option) == "mutant", "profile-mode");
    mutant = std::string(option) == "mutant";
    runStart = Clock::now();
    for (const char *other : {"OPENXCOM_BATTLE_PAIR_GATE", "OPENXCOM_SMOKE_DENSITY_GATE",
                             "OPENXCOM_SMOKE_CLIPPING_GATE", "OPENXCOM_OBSERVER_EFFECTS_GATE",
                             "OPENXCOM_NATURAL_FOV_GATE", "OPENXCOM_BATTLEFIELD_AUDIO_GATE",
                             "OPENXCOM_MIXED_AUDIO_FD", "OPENXCOM_PRESENTATION_FD",
                             "OPENXCOM_EXPERIMENTAL_CURSOR_DIR", "OPENXCOM_EXPERIMENTAL_HUD_DIR"})
        require(!std::getenv(other), "isolated-gate-profile");
    const char *record = std::getenv("OPENXCOM_TIMING_RECORD_FD");
    require(record && *record, "private-record-descriptor");
    char *end = 0; errno = 0;
    long value = std::strtol(record, &end, 10);
    require(!errno && !*end && value >= 3 && value <= INT_MAX, "private-record-descriptor");
    recordFd = static_cast<int>(value);
    struct stat info = {};
    const int flags = fcntl(recordFd, F_GETFL);
    require(fstat(recordFd, &info) == 0 && S_ISREG(info.st_mode) && info.st_uid == geteuid() &&
            (info.st_mode & 0777) == 0600 && flags >= 0 && (flags & O_ACCMODE) == O_WRONLY,
            "private-record-profile");
    totalPairs = mutant ? 20 : 56;
    uint32_t orderState = 0x4f584354u;
    for (int i = 0; i < totalPairs; ++i) {
        orderState ^= orderState << 13; orderState ^= orderState >> 17; orderState ^= orderState << 5;
        schedule[i] = {i < (mutant ? 4 : 8) ? WARMUP : (!mutant && i < 24 ? AA : AB), bool(orderState & 1)};
    }
    for (Frame &f : frames) f.rgb.resize(640 * 400 * 3);
    int sockets[2];
    require(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets) == 0, "internal-socketpair");
    peer = sockets[0];
    const std::string descriptor = std::to_string(sockets[1]);
    require(setenv("OPENXCOM_PRESENTATION_FD", descriptor.c_str(), 1) == 0 &&
            setenv("OPENXCOM_PRESENTATION_VERSION", "2", 1) == 0, "internal-channel-setup");
    Options::displayWidth = 640; Options::displayHeight = 400;
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    Options::fullscreen = false; Options::allowResize = false; Options::asyncBlit = false;
    Options::useOpenGL = false; Options::useScaleFilter = false;
    Options::useHQXFilter = false; Options::useXBRZFilter = false;
    Options::fpsCounter = false; Options::debug = false; Options::debugUi = false;
    Options::traceAI = false; Options::playIntro = false; Options::autosave = false;
    Options::pauseMode = 0;
}
void silence()
{
    if (!enabled) return;
    Mix_HaltChannel(-1); Mix_HaltMusic(); Mix_CloseAudio(); SDL_QuitSubSystem(SDL_INIT_AUDIO);
    Options::mute = true; silent();
}
bool loaded(Game *game)
{
    if (!enabled) return false;
    watchdog(); silent();
    const std::string hello = receiveHello();
    require(hello.size() == 88 && hello.substr(0, 24) == "OPENXCOM-PRESENTATION/2 ", "producer-hello");
    epoch = hello.substr(24, 32); admission = hello.substr(56, 32);
    fresh(epoch); fresh(admission); empty();
    running = true; pendingSetup = true;
    game->setState(new State);
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    game->getScreen()->resetDisplay(false);
    game->getCursor()->setVisible(true); game->getFpsCounter()->setVisible(false);
    return true;
}
bool active() { return running; }
void afterCleanup(Game *game)
{
    if (!running || !pendingSetup) return;
    watchdog();
    // Delete the previous BattlescapeState before destroying its SavedGame.
    pendingSetup = false;
    RNG::setSeed(123456789); std::srand(12345);
    SavedGame *save = new SavedGame;
    SavedBattleGame *battle = new SavedBattleGame; currentBattle = battle;
    save->setBattleGame(battle); game->setSavedGame(save);
    battle->initMap(40, 40, 1); battle->initUtilities(game->getMod());
    battle->setAmbientSound(-1);
    for (int i = 0; i < battle->getMapSizeXYZ(); ++i) battle->getTiles()[i]->setDiscovered(true, 2);
    const std::string master = Options::getActiveMaster();
    require((master == "xcom1" || master == "xcom2") && Options::getActiveMods().size() == 1, "supported-vanilla-master");
    Unit *rules = game->getMod()->getUnit(master == "xcom1" ? "STR_SECTOID_SOLDIER" : "STR_AQUATOID_SOLDIER", true);
    Armor *armor = game->getMod()->getArmor(rules->getArmor(), true);
    require(armor->getSize() == 1 && !armor->getConstantAnimation(), "fixture-unit-profile");
    BattleUnit *player = new BattleUnit(rules, FACTION_PLAYER, 1, armor, game->getMod()->getStatAdjustment(0), 0);
    occupant = new BattleUnit(rules, FACTION_HOSTILE, 2, armor, game->getMod()->getStatAdjustment(0), 0);
    player->setPosition(playerPosition); player->setDirection(0);
    const bool occupied = schedule[pairIndex].kind == AA || currentLabel() == 'A';
    occupant->setPosition(occupied ? hover : parked); occupant->setDirection(0);
    battle->getUnits()->push_back(player); battle->getUnits()->push_back(occupant);
    battle->resetUnitTiles(); battle->setSelectedUnit(player);
    state = new BattlescapeState;
    battle->setBattleState(state); game->setState(state);
}
void beforeFrame(Game *game)
{
    if (!running) return;
    watchdog();
    require(state && game->isState(state) && !pendingSetup, "normal-battlescape-top-state");
    silent();
    SavedBattleGame *battle = currentBattle;
    Map *map = state->getMap();
    require(!occupant->getVisible(), "initial-hidden-visibility");
    map->cacheUnits();
    Position pixel; map->getCamera()->convertMapToScreen(hover, &pixel);
    pixel += map->getCamera()->getMapOffset();
    SDL_Event event = {}; event.type = SDL_MOUSEMOTION;
    event.motion.x = (pixel.x + 16) * 2; event.motion.y = (pixel.y + 24) * 2;
    Action action(&event, game->getScreen()->getXScale(), game->getScreen()->getYScale(), 0, 0);
    game->getCursor()->handle(&action); state->handle(&action);
    state->animate(); // Fresh constructor phase 0 -> fixed measured phase 1.
    Position selected; map->getSelectorPosition(&selected);
    require(selected == hover && map->getCamera()->getViewLevel() == hover.z &&
            map->getCursorType() == CT_NORMAL && !state->getMouseOverIcons() &&
            game->getCursor()->getVisible() && !Options::debug && !battle->getDebugMode(), "actual-hover-cursor-preconditions");
    const bool occupied = schedule[pairIndex].kind == AA || currentLabel() == 'A';
    require(battle->getUnits()->size() == 2 && occupant->getPosition() == (occupied ? hover : parked) &&
            occupant->getTile() == battle->getTile(occupant->getPosition()) &&
            occupant->getTile()->getUnit() == occupant && !occupant->getVisible() &&
            battle->getTile(hover)->getUnit() == (occupied ? occupant : 0), "real-occupancy-visibility-preconditions");
    if (!haveSeed) { initialSeed = RNG::getSeed(); haveSeed = true; }
    require(RNG::getSeed() == initialSeed, "identical-setup-seed");
    empty(); challenge = randomChallenge();
    Frame &f = frame();
    f.request = epoch + admission + challenge; f.response.clear();
    f.surface = 0; f.tile = 0; f.phase = -1; f.visits = 0; f.flips = 0; f.captures = 0;
    sample() = Sample();
    if (haveResult) std::this_thread::sleep_until(lastResult + std::chrono::milliseconds(300));
    watchdog(); drainFocusEvents();
    const Uint8 focus = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
    require((SDL_GetAppState() & focus) == focus, "focus-lost");
    recording = true;
    Sample &s = sample();
    const char *bytes = f.request.data(); const size_t length = f.request.size();
    s.t0 = Clock::now();
    const ssize_t sent = send(peer, bytes, length, MSG_DONTWAIT | MSG_NOSIGNAL);
    // No failing-path logging while the measurement is live; check after t1.
    if (sent != 96) recording = false;
}
void visited(const Tile *tile, int animationPhase)
{
    if (!recording) return;
    Frame &f = frame(); f.tile = tile; f.phase = animationPhase; ++f.visits;
}
void timingFlipped()
{
    if (!recording) return;
    sample().flip = Clock::now(); ++frame().flips;
}
void capture(SDL_Surface *surface, int bw, int bh, bool allowed,
             const Observer::HudFrame &, const Observer::PixelCrop &crop)
{
    if (!recording) return;
    // Fixed metadata only: no vector copy, extraction, parsing or allocation.
    Frame &f = frame(); f.surface = surface; f.bw = bw; f.bh = bh; f.allowed = allowed;
    f.cropX = crop.x; f.cropY = crop.y; f.cropWidth = crop.width; f.cropHeight = crop.height;
    f.cropScreenWidth = crop.screenWidth; f.cropScreenHeight = crop.screenHeight;
    ++f.captures;
}
void timingEvent(const SDL_Event &event)
{
    if (!running) return;
    require(event.type != SDL_QUIT, "window-closed");
    require(event.type != SDL_ACTIVEEVENT || event.active.gain ||
            !(event.active.state & (SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS)), "focus-lost");
}
bool timingInvisibleHoverOccupant()
{
    if (!running || !state || !currentBattle) return false;
    Position selected; state->getMap()->getSelectorPosition(&selected);
    const Tile *tile = currentBattle->getTile(selected);
    const BattleUnit *unit = tile ? tile->getUnit() : 0;
    return unit && !unit->getVisible();
}
void afterFrame(Game *game)
{
    if (!running) return;
    char bytes[102401];
    iovec part = {bytes, sizeof(bytes)}; msghdr message = {};
    message.msg_iov = &part; message.msg_iovlen = 1;
    Sample &s = sample();
    const ssize_t size = recvmsg(peer, &message, MSG_DONTWAIT);
    s.t1 = Clock::now(); // First operation after the single native receive.
    lastResult = s.t1; haveResult = true;
    recordSample(); // Preserve chronology even if a subsequent oracle fails.
    require(recording, "request-send"); recording = false;
    require(s.t0 > runStart && s.flip > s.t0 && s.t1 > s.flip, "nonmonotonic-clock");
    require(size > 0 && !(message.msg_flags & MSG_TRUNC), "producer-response");
    watchdog();
    // The just-presented software surface is unchanged until the next frame.
    copyAndVerifyFrame(); drainFocusEvents();
    Frame &f = frame(); f.response.assign(bytes, size);
    YAML::Node packet;
    try {
        packet = YAML::Load(f.response);
        require(packet.IsMap() && packet.size() == 8 &&
                packet["schema"].as<std::string>() == "experimental-presentation/2" &&
                packet["epoch"].as<std::string>() == epoch && packet["challenge"].as<std::string>() == challenge &&
                packet["semantics"].as<std::string>() == "presented_after_request" &&
                packet["profile"].as<std::string>() == "software-single-buffer-8bit-integer-scale-no-bands",
                "public-response-binding");
        require(packet["hud"].IsMap() && packet["hud"].size() == 4 && packet["crop"].IsMap(), "non-null-hud-and-cursor");
        for (const char *field : {"time_units", "energy", "health", "morale"})
            require(packet["hud"][field].IsScalar() && packet["hud"][field].as<unsigned int>() <= 999, "public-hud-shape");
        admission = packet["next_token"].as<std::string>(); fresh(admission);
    } catch (...) { require(false, "public-response-json"); }
    verifyActualCrop(f.response);
    empty(); silent();
    if (leg == 1) {
        verifyPair(); ++pairIndex; leg = 0;
        if (pairIndex == totalPairs) {
            completed = true; running = false;
            std::cout << "TIMING_CALIBRATION COMPLETE" << std::endl;
            game->quit(); return;
        }
    } else ++leg;
    state = 0; occupant = 0; currentBattle = 0; pendingSetup = true; game->setState(new State);
}
int exitStatus()
{
    if (peer >= 0) { close(peer); peer = -1; }
    if (recordFd >= 0) { close(recordFd); recordFd = -1; }
    return enabled && !completed ? EXIT_FAILURE : EXIT_SUCCESS;
}
} }
