// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
// A finite, default-OFF integration fixture. Actual states, SDL controls, final
// display pixels, ordinary next/previous entries and shared stance ledger are tested.
// Oracle values, resources and captures remain private in memory.
#include "SelectionControlGate.h"
#include "../../src/Battlescape/BattlescapeGame.h"
#include "../../src/Battlescape/BattlescapeState.h"
#include "../../src/Battlescape/BattleState.h"
#include "../../src/Battlescape/Camera.h"
#include "../../src/Battlescape/Map.h"
#include "../../src/Battlescape/Pathfinding.h"
#include "../../src/Battlescape/TileEngine.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/RNG.h"
#include "../../src/Engine/Screen.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/BattlescapeButton.h"
#include "../../src/Interface/Cursor.h"
#include "../../src/Interface/FpsCounter.h"
#include "../../src/Menu/PauseState.h"
#include "../../src/Mod/Armor.h"
#include "../../src/Mod/MapData.h"
#include "../../src/Mod/MapDataSet.h"
#include "../../src/Mod/Mod.h"
#include "../../src/Mod/RuleItem.h"
#include "../../src/Mod/RuleSoldier.h"
#include "../../src/Mod/Unit.h"
#include "../../src/Savegame/Base.h"
#include "../../src/Savegame/BattleItem.h"
#include "../../src/Savegame/BattleUnit.h"
#include "../../src/Savegame/SavedBattleGame.h"
#include "../../src/Savegame/SavedGame.h"
#include "../../src/Savegame/Soldier.h"
#include "../../src/Savegame/Tile.h"
#include <SDL_mixer.h>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sys/socket.h>
#include <typeinfo>
#include <unistd.h>
#include <vector>

namespace OpenXcom { namespace SelectionControlGate {
namespace {
using Clock = std::chrono::steady_clock;
// Three separately eligible soldiers are essential: with only two, next and
// previous can select the same actor even when dispatch direction is wrong.
struct Fixture { char direction; int start, expected, skipMask, deadMask; };
const Fixture fixtures[] = {
    {'n',0,1,0,0}, {'p',0,2,0,0}, // independently different directions
    {'n',2,0,0,0}, {'p',2,1,0,0}, // forward and backward wrap boundary
    {'n',0,2,2,0}, {'p',0,1,4,0}, // native do-not-reselect skips
    {'n',0,0,6,0}, {'p',0,0,6,0}, // one eligible actor: native no-op
    {'n',0,2,0,2}, {'p',0,1,0,4}  // native out-of-play skips
};
enum { PairCount = sizeof(fixtures) / sizeof(fixtures[0]), PairWorlds = PairCount * 2 };
enum Adversary { SELECT_RACE, MANUAL_RACE, OVERLAY_RACE, FOCUS_RACE,
    LOAD_SOURCE, LOAD_TARGET, TU_RACE, RESERVE_RACE, MISSING_CONTEXT,
    CANCEL_CONFIRMATION, STALE_CONFIRMATION,
    PENDING_SELECT_RACE, POST_DISPATCH_UNAVAILABLE, PREPRESENTED_Y, HELD_Y_REPEAT, HELD_MOUSE_RELEASE, LEDGER_REPLAY, LEDGER_STANCE, DISCONNECT_PENDING, AdversaryCount };
enum Stage { WARM, REQUEST_CONTEXT, WAIT_CONTEXT, PREPARE_COMMAND, WAIT_MUTATION,
    WAIT_PENDING, CONFIRM, WAIT_DISPATCH, CHECK_REPLAY, WAIT_REPLAY,
    WAIT_REJECTED, WAIT_CANCELLED, WAIT_MANUAL_PANEL, REQUEST_POST_CONTEXT,
    WAIT_POST_CONTEXT, WAIT_POST_OVERLAY, WAIT_FAILED_POST, WAIT_HELD_INPUT,
    WAIT_BLOCKED_INPUT, WAIT_BLOCKED_STATUS, REQUEST_LEDGER_CONTEXT, WAIT_LEDGER_CONTEXT, FINISHED };
bool enabled = false, running = false, pendingSetup = false, completed = false;
bool moved = false, captured = false, commandSent = false, samePanel = false;
int peer = -1, world = 0, initialTU = 0, twinTU = 0, callbacks = 0, selectionCallbacks = 0, stanceCallbacks = 0;
int contextCount = 0, parityCount = 0, replayStep = 0, frameCount = 0, modalFrames = 0;
Stage stage = WARM;
Game *owner = 0;
BattlescapeState *state = 0;
BattleUnit *player = 0, *twin = 0, *third = 0, *hostile = 0;
BattleUnit *soldiers[3] = {};
int originalTU[3] = {};
bool originalKneel[3] = {};
const Position playerPosition(20,20,0), twinPosition(24,24,0), thirdPosition(28,20,0), hostilePosition(20,16,0);
Clock::time_point started, worldStarted;
std::deque<SDL_Event> inputs;
std::string master, epoch, capability, challenge, commandId, commandPacket;
std::string savedEpoch, savedCapability;
std::set<std::string> tokens;
struct Snapshot { YAML::Node save, runtime; uint64_t rng; };
Snapshot manual, actual, before;
std::vector<unsigned char> contextPixels, manualPanel;
void check(bool ok, int assertion)
{
    if (!ok) {
        // Compile-time source location only; never emit private state or tokens.
        std::cerr << "SELECTION_CONTROL_GATE FAIL assertion-" << assertion << std::endl;
        std::_Exit(EXIT_FAILURE);
    }
}
#define require(ok) check((ok), __LINE__)
bool paired() { return world < PairWorlds; }
int adversary() { return world - PairWorlds; }
bool controlled() { return !paired() || world % 2; }
const Fixture &fixture() { return fixtures[paired() ? world / 2 : 0]; }
bool equal(const YAML::Node &a, const YAML::Node &b)
{
    if (a.Type() != b.Type() || a.Tag() != b.Tag()) return false;
    if (a.IsScalar()) return a.Scalar() == b.Scalar();
    if (a.IsNull() || !a.IsDefined()) return true;
    if (a.size() != b.size()) return false;
    if (a.IsSequence()) {
        for (size_t i = 0; i < a.size(); ++i) if (!equal(a[i], b[i])) return false;
        return true;
    }
    std::map<std::string,YAML::Node> right;
    for (auto i : b) { require(i.first.IsScalar()); require(right.insert({i.first.Scalar(), i.second}).second); }
    for (auto i : a) {
        require(i.first.IsScalar()); auto j = right.find(i.first.Scalar());
        if (j == right.end() || !equal(i.second, j->second)) return false;
    }
    return true;
}
void compare(const Snapshot &a, const Snapshot &b)
{
    require(a.rng == b.rng && equal(a.save, b.save) && equal(a.runtime, b.runtime));
}
int identity(const BattleUnit *unit)
{
    require(!unit || unit == player || unit == twin || unit == third || unit == hostile);
    return unit ? unit->getId() : -1;
}
YAML::Node actionSnapshot(const BattleAction &a)
{
    YAML::Node n;
    n["type"] = int(a.type); n["actor"] = identity(a.actor);
    n["weapon"] = a.weapon ? a.weapon->getId() : -1; n["target"] = a.target;
    n["TU"] = a.TU; n["targeting"] = a.targeting; n["result"] = a.result;
    n["value"] = a.value; n["strafe"] = a.strafe; n["run"] = a.run;
    n["diff"] = a.diff; n["autoShotCounter"] = a.autoShotCounter;
    n["cameraPosition"] = a.cameraPosition; n["desperate"] = a.desperate;
    n["finalFacing"] = a.finalFacing; n["finalAction"] = a.finalAction; n["number"] = a.number;
    for (const Position &p : a.waypoints) n["waypoints"].push_back(p);
    return n;
}
Snapshot snapshot()
{
    SavedBattleGame *battle = owner->getSavedGame()->getSavedBattle();
    Snapshot s; s.rng = RNG::getSeed(); s.save = battle->save();
    YAML::Node &n = s.runtime;
    n["selected"] = identity(battle->getSelectedUnit()); n["busy"] = state->getBattleGame()->isBusy();
    n["panic"] = state->getBattleGame()->getPanicHandled(); n["cursor"] = int(state->getMap()->getCursorType());
    n["camera"] = state->getMap()->getCamera()->getMapOffset();
    n["current"] = actionSnapshot(*state->getBattleGame()->getCurrentAction());
    for (BattleState *entry : state->getBattleGame()->getStates()) {
        require(entry);
        YAML::Node q; q["kind"] = std::string(typeid(*entry).name()); q["action"] = actionSnapshot(entry->getAction());
        n["queue"].push_back(q);
    }
    for (BattleUnit *unit : *battle->getUnits()) {
        YAML::Node u; u["id"] = identity(unit); u["stats"] = *unit->getBaseStats();
        u["status"] = int(unit->getStatus()); u["kneeled"] = unit->isKneeled();
        u["timeUnits"] = unit->getTimeUnits(); u["energy"] = unit->getEnergy();
        u["health"] = unit->getHealth(); u["morale"] = unit->getMorale();
        u["visible"] = unit->getVisible(); u["direction"] = unit->getDirection();
        u["motion"] = unit->getMotionPoints(); u["position"] = unit->getPosition();
        for (BattleUnit *v : *unit->getVisibleUnits()) u["visibleUnits"].push_back(identity(v));
        for (Tile *v : *unit->getVisibleTiles()) u["visibleTiles"].push_back(battle->getTileIndex(v->getPosition()));
        for (BattleUnit *v : unit->getUnitsSpottedThisTurn()) u["spotted"].push_back(identity(v));
        n["units"].push_back(u);
    }
    require(s.rng == RNG::getSeed()); return s;
}
void fieldsAt(int assertion, const YAML::Node &n, std::initializer_list<const char*> names)
{
    check(n.IsMap() && n.size() == names.size(), assertion); std::set<std::string> remaining;
    for (const char *name : names) remaining.insert(name);
    for (auto entry : n) check(entry.first.IsScalar() && remaining.erase(entry.first.Scalar()) == 1, assertion);
    check(remaining.empty(), assertion);
}
// Report the compile-time caller location, never a runtime phase or packet value.
#define fields(...) fieldsAt(__LINE__, __VA_ARGS__)
bool token(const std::string &s) { return s.size() == 32 && s.find_first_not_of("0123456789abcdef") == std::string::npos; }
std::string randomToken()
{
    unsigned char bytes[16]; int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC); require(fd >= 0);
    ssize_t count = read(fd, bytes, sizeof(bytes)); close(fd); require(count == sizeof(bytes));
    std::string s; const char *hex = "0123456789abcdef";
    for (unsigned char b : bytes) { s += hex[b >> 4]; s += hex[b & 15]; }
    require(tokens.insert(s).second); return s;
}
void sendPacket(const std::string &s)
{
    require(peer >= 0 && send(peer, s.data(), s.size(), MSG_DONTWAIT | MSG_NOSIGNAL) == ssize_t(s.size()));
}
bool available()
{
    char b; ssize_t n = recv(peer, &b, 1, MSG_PEEK | MSG_DONTWAIT);
    require(n >= 0 || errno == EAGAIN || errno == EWOULDBLOCK); return n > 0;
}
YAML::Node receive()
{
    char bytes[120000]; iovec part = {bytes, sizeof(bytes)}; msghdr m = {};
    m.msg_iov = &part; m.msg_iovlen = 1; ssize_t n = recvmsg(peer, &m, MSG_DONTWAIT);
    require(n > 0 && !(m.msg_flags & MSG_TRUNC));
    try { return YAML::Load(std::string(bytes, n)); } catch (...) { require(false); }
    return YAML::Node();
}
// Status/2 correlation is transport-only, independent of command identity.
std::string statusQueryNonce(32, '0'), expectedStatusQueryNonce;
void sendStatusQuery()
{
    require(expectedStatusQueryNonce.empty());
    require(statusQueryNonce != std::string(32, 'f'));
    const char *hex = "0123456789abcdef";
    for (int i = 31; i >= 0; --i)
    {
        if (statusQueryNonce[i] == 'f') { statusQueryNonce[i] = '0'; continue; }
        statusQueryNonce[i] = hex[std::string(hex).find(statusQueryNonce[i]) + 1]; break;
    }
    expectedStatusQueryNonce = statusQueryNonce;
    sendPacket("S" + commandId + statusQueryNonce);
}
void status(const YAML::Node &n, const std::string &value)
{
    if (expectedStatusQueryNonce.empty()) require(n["query_nonce"].IsNull());
    else require(n["query_nonce"].IsScalar() && n["query_nonce"].as<std::string>() == expectedStatusQueryNonce);
    expectedStatusQueryNonce.clear();
    fields(n, {"schema", "command_id", "status", "query_nonce"});
    require(n["schema"].as<std::string>() == "experimental-stance-status/2" &&
        n["command_id"].as<std::string>() == commandId && n["status"].as<std::string>() == value);
}
std::vector<unsigned char> panel()
{
    // Independent physical display oracle, sampled only after natural SDL_Flip.
    SDL_Surface *d = SDL_GetVideoSurface();
    require(d && d->w == 640 && d->h == 400 && d->format && d->format->BitsPerPixel == 8 &&
        d->format->BytesPerPixel == 1 && d->format->palette && d->format->palette->ncolors == 256 &&
        !(d->flags & (SDL_OPENGL | SDL_HWSURFACE | SDL_DOUBLEBUF)));
    std::vector<unsigned char> result; result.reserve(320 * 56 * 3);
    require(SDL_LockSurface(d) == 0);
    for (int y = 144; y < 200; ++y) for (int x = 0; x < 320; ++x) {
        const unsigned char *row = static_cast<const unsigned char*>(d->pixels) + (y * 2) * d->pitch;
        const SDL_Color &c = d->format->palette->colors[row[x * 2]];
        for (int sy = 0; sy < 2; ++sy) for (int sx = 0; sx < 2; ++sx) {
            const unsigned char *r = static_cast<const unsigned char*>(d->pixels) + (y * 2 + sy) * d->pitch;
            const SDL_Color &p = d->format->palette->colors[r[x * 2 + sx]];
            require(p.r == c.r && p.g == c.g && p.b == c.b);
        }
        result.push_back(c.r); result.push_back(c.g); result.push_back(c.b);
    }
    SDL_UnlockSurface(d); return result;
}
void context(const YAML::Node &n)
{
    fields(n, {"schema", "challenge", "control_epoch", "capability", "semantics", "panel"});
    require(n["schema"].as<std::string>() == "experimental-stance-context/1" &&
        n["challenge"].as<std::string>() == challenge &&
        n["semantics"].as<std::string>() == "presented_after_request");
    epoch = n["control_epoch"].as<std::string>(); capability = n["capability"].as<std::string>();
    require(token(epoch) && token(capability) && tokens.insert(capability).second);
    fields(n["panel"], {"width", "height", "rgb_hex"});
    require(n["panel"]["width"].as<int>() == 320 && n["panel"]["height"].as<int>() == 56);
    const std::string hex = n["panel"]["rgb_hex"].as<std::string>();
    require(hex.size() == 320 * 56 * 6 && hex.find_first_not_of("0123456789abcdef") == std::string::npos);
    const std::string alphabet = "0123456789abcdef";
    contextPixels = panel();
    for (size_t i = 0; i < contextPixels.size(); ++i)
        require(contextPixels[i] == alphabet.find(hex[i * 2]) * 16 + alphabet.find(hex[i * 2 + 1]));
    ++contextCount;
}
void queue(SDL_Event e) { inputs.push_back(e); require(SDL_PushEvent(&e) == 0); }
void motion(int x = 160, int y = 80)
{
    SDL_Event e = {}; e.type = SDL_MOUSEMOTION; e.motion.which = 255;
    e.motion.x = x * 2; e.motion.y = y * 2; queue(e);
}
void keyEdge(SDLKey code, bool down)
{
    SDL_Event e = {}; e.type = down ? SDL_KEYDOWN : SDL_KEYUP; e.key.which = 255;
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED; e.key.keysym.sym = code; e.key.keysym.mod = KMOD_NONE; queue(e);
}
void key(SDLKey code) { keyEdge(code,true); keyEdge(code,false); }
void mouseEdge(bool down)
{
    SDL_Event e = {}; e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; e.button.which = 255;
    e.button.x = 190; e.button.y = 236; e.button.button = SDL_BUTTON_LEFT;
    e.button.state = down ? SDL_PRESSED : SDL_RELEASED; queue(e);
}
void click(bool ok)
{
    int x = ok ? 95 : 225; motion(x,118);
    SDL_Event e = {}; e.type = SDL_MOUSEBUTTONDOWN; e.button.which = 255;
    e.button.x = x * 2; e.button.y = 236; e.button.button = SDL_BUTTON_LEFT; e.button.state = SDL_PRESSED; queue(e);
    e.type = SDL_MOUSEBUTTONUP; e.button.state = SDL_RELEASED; queue(e);
}
void alternateSurfaceOracle()
{
    // Use the actual native button and virtual Surface interface. TFTD's size
    // override dirties the base, while the selected alternate is already ready.
    // No subclass, replacement widget, game-state mutation or output is used.
    BattlescapeButton button(17,11,0,0);
    button.setTFTDMode(true); button.setWidth(20); button.setHeight(11);
    button.initSurfaces(); button.toggle(true);
    Surface target(32,32,0,0);
    button.blit(&target); // Exercise the real inverted render branch first.
    Surface *shown = &button;
    require(shown->stanceContextVisible());
    button.toggle(false); require(!shown->stanceContextVisible());
    button.blit(&target); require(shown->stanceContextVisible());
    // Resizing to the same native geometry marks only the base pending again.
    button.setWidth(20); button.toggle(true); button.blit(&target);
    require(shown->stanceContextVisible());
    button.setVisible(false); require(!shown->stanceContextVisible()); button.setVisible(true);
    button.setHidden(true); require(!shown->stanceContextVisible()); button.setHidden(false);
    button.getCrop()->w = 1; require(!shown->stanceContextVisible()); button.resetCrop();
    require(shown->stanceContextVisible());
    button.setWidth(21); // Alternate still has width20: actual displayed geometry mismatches.
    require(!shown->stanceContextVisible());
    button.initSurfaces(); require(shown->stanceContextVisible());
}
void quiet()
{
    int frequency, channels; Uint16 format; char driver[64] = {};
    require(Options::mute && Mix_QuerySpec(&frequency, &format, &channels) > 0 &&
        SDL_AudioDriverName(driver, sizeof(driver)) && std::string(driver) == "dummy" &&
        Mix_Playing(-1) == 0 && Mix_PlayingMusic() == 0);
}
bool ready()
{
    SavedBattleGame *b = owner->getSavedGame()->getSavedBattle();
    const Uint8 focus = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
    return owner->isState(state) && (SDL_GetAppState() & focus) == focus &&
        b->getSelectedUnit() == soldiers[fixture().start] && b->getSide() == FACTION_PLAYER && !b->getDebugMode() &&
        state->getBattleGame()->getPanicHandled() && !state->getBattleGame()->isBusy() &&
        !state->getMap()->getProjectile() && state->getMap()->getCursorType() == CT_NORMAL &&
        !b->getUnitsFalling() && state->getBattleGame()->getCurrentAction()->type == BA_NONE;
}
// Watchdog diagnostics retain the fixed compile-time assertion-label contract.
// They run only after expiry in this default-OFF fixture, never in production
// observation/control paths, and do not emit values or relax readiness gates.
void watchdogFailure()
{
    if (stage == WARM) {
        require(moved);
        require(inputs.empty());
        require(owner->isState(state));
        require((SDL_GetAppState() & SDL_APPACTIVE) != 0);
        require((SDL_GetAppState() & SDL_APPINPUTFOCUS) != 0);
        require((SDL_GetAppState() & SDL_APPMOUSEFOCUS) != 0);
        SavedBattleGame *b = owner->getSavedGame()->getSavedBattle();
        require(b->getSelectedUnit() == soldiers[fixture().start]);
        require(b->getSide() == FACTION_PLAYER && !b->getDebugMode());
        require(state->getBattleGame()->getPanicHandled());
        require(!state->getBattleGame()->isBusy());
        require(!state->getMap()->getProjectile());
        require(state->getMap()->getCursorType() == CT_NORMAL);
        require(!b->getUnitsFalling());
        require(state->getBattleGame()->getCurrentAction()->type == BA_NONE);
        require(frameCount >= 2);
    }
    require(stage != WARM);
    require(stage != REQUEST_CONTEXT);
    require(stage != WAIT_CONTEXT);
    require(stage != PREPARE_COMMAND);
    require(stage != WAIT_MUTATION);
    require(stage != WAIT_PENDING);
    require(stage != CONFIRM);
    require(stage != WAIT_DISPATCH);
    require(stage != CHECK_REPLAY);
    require(stage != WAIT_REPLAY);
    require(stage != WAIT_REJECTED);
    require(stage != WAIT_CANCELLED);
    require(stage != WAIT_MANUAL_PANEL);
    require(stage != REQUEST_POST_CONTEXT);
    require(stage != WAIT_POST_CONTEXT);
    require(stage != WAIT_POST_OVERLAY);
    require(stage != WAIT_FAILED_POST);
    require(stage != WAIT_HELD_INPUT);
    require(stage != WAIT_BLOCKED_INPUT);
    require(stage != WAIT_BLOCKED_STATUS);
    require(stage != REQUEST_LEDGER_CONTEXT);
    require(stage != WAIT_LEDGER_CONTEXT);
    require(stage != FINISHED);
    require(false);
}
void replacement(Game *game)
{
    pendingSetup = true; game->setState(new State);
}
void finishWorld()
{
    require(inputs.empty());
    ++world;
    if (world == PairWorlds + AdversaryCount) { running = false; completed = true; owner->quit(); }
    else replacement(owner);
}
bool stanceCommand() { return !paired() && adversary() == LEDGER_STANCE; }
void command()
{
    commandId = randomToken();
    commandPacket = (stanceCommand() ? "D" : "N") + epoch + capability + commandId +
        (stanceCommand() ? std::string("sk") : std::string(1,fixture().direction) + "0");
    require(commandPacket.size() == 99);
    sendPacket(commandPacket); commandSent = true;
}
void rejectedUnchanged()
{
    require(callbacks == ((adversary() == MANUAL_RACE || adversary() == SELECT_RACE) ? 1 : 0));
    if (adversary() == SELECT_RACE) require(samePanel);
    compare(before, snapshot()); finishWorld();
}
void actionOracle()
{
    require(callbacks == 1);
    const bool stance = stanceCommand() || (!paired() && adversary() == MANUAL_RACE);
    if (stance) {
        require(stanceCallbacks == 1 && selectionCallbacks == 0 &&
            owner->getSavedGame()->getSavedBattle()->getSelectedUnit() == player);
    } else {
        require(selectionCallbacks == 1 && stanceCallbacks == 0 &&
            owner->getSavedGame()->getSavedBattle()->getSelectedUnit() == soldiers[fixture().expected]);
        require(state->getBattleGame()->getCurrentAction()->actor == soldiers[fixture().expected]);
    }
    for (int i = 0; i < 3; ++i) {
        require(soldiers[i]->getTimeUnits() == originalTU[i] - (stance && i == 0 ? 4 : 0));
        require(soldiers[i]->isKneeled() == (stance && i == 0 ? true : originalKneel[i]));
    }
    require(hostile->getTimeUnits() == 100 && !hostile->isKneeled() &&
        state->getBattleGame()->getStates().empty());
}
} // namespace

bool active()
{
    const char *e = std::getenv("OPENXCOM_SELECTION_CONTROL_GATE");
    return e && std::string(e) == "1";
}
void prepare()
{
    const char *e = std::getenv("OPENXCOM_SELECTION_CONTROL_GATE"); enabled = e && std::string(e) == "1";
    if (!enabled) return;
    require(!std::getenv("OPENXCOM_STANCE_CONTROL_GATE"));
    require(setenv("OPENXCOM_SELECTION_ENABLED", "1", 1) == 0);
    require(Options::language == "en-US" && !Options::playIntro);
    require(!std::getenv("OPENXCOM_STANCE_FD") && !std::getenv("OPENXCOM_PRESENTATION_FD") &&
        !std::getenv("OPENXCOM_AUDIO_FD") && !std::getenv("OPENXCOM_NATURAL_FOV_GATE") &&
        !std::getenv("OPENXCOM_BATTLEFIELD_AUDIO_GATE") && !std::getenv("OPENXCOM_BATTLE_PAIR_GATE"));
    int sockets[2]; require(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets) == 0); peer = sockets[0];
    const std::string descriptor = std::to_string(sockets[1]);
    require(setenv("OPENXCOM_STANCE_FD", descriptor.c_str(), 1) == 0 && setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    Options::displayWidth = 640; Options::displayHeight = 400;
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    Options::fullscreen = false; Options::allowResize = false; Options::asyncBlit = false;
    Options::useOpenGL = false; Options::useScaleFilter = false; Options::useHQXFilter = false; Options::useXBRZFilter = false;
    Options::fpsCounter = false; Options::debug = false; Options::debugUi = false; Options::traceAI = false;
    Options::playIntro = false; Options::autosave = false; Options::pauseMode = 0;
    Options::battleDragScrollButton = SDL_BUTTON_MIDDLE; Options::strafe = false;
}
void silence()
{
    if (!enabled) return;
    Mix_HaltChannel(-1); Mix_HaltMusic(); Options::mute = true; quiet();
}
bool loaded(Game *game)
{
    if (!enabled) return false;
    owner = game; quiet(); alternateSurfaceOracle();
    require(Options::language == "en-US" && !Options::playIntro);
    YAML::Node hello = receive(); fields(hello, {"schema", "control_epoch"});
    require(hello["schema"].as<std::string>() == "experimental-stance/2");
    epoch = hello["control_epoch"].as<std::string>(); require(token(epoch) && !available());
    running = true; started = Clock::now(); replacement(game);
    Options::baseXResolution = 320; Options::baseYResolution = 200; game->getScreen()->resetDisplay(false);
    game->getCursor()->setVisible(true); game->getFpsCounter()->setVisible(false); return true;
}
void afterCleanup(Game *game)
{
    if (!running || !pendingSetup) return;
    pendingSetup = false; moved = captured = commandSent = samePanel = false; callbacks = selectionCallbacks = stanceCallbacks = frameCount = replayStep = modalFrames = 0;
    stage = WARM; inputs.clear(); worldStarted = Clock::now();
    RNG::setSeed(123456789); std::srand(12345); // Fixture creation only; no gameplay-time reset.
    SavedGame *save = new SavedGame; SavedBattleGame *battle = new SavedBattleGame;
    save->setBattleGame(battle); game->setSavedGame(save);
    battle->initMap(40,40,1); battle->setDepth(0); battle->setAmbientSound(-1);
    Mod *mod = game->getMod(); master = Options::getActiveMaster();
    require((master == "xcom1" || master == "xcom2") && Options::getActiveMods().size() == 1);
    MapDataSet *data = mod->getMapDataSet("BLANKS"); require(data);
    data->loadData(mod->getMCDPatch(data->getName())); battle->getMapDataSets()->push_back(data);
    MapData *floor = data->getObject(1);
    require(floor && floor->getObjectType() == O_FLOOR && !floor->isNoFloor() && floor->getTerrainLevel() == 0);
    for (int i = 0; i < battle->getMapSizeXYZ(); ++i) battle->getTiles()[i]->setMapData(floor,1,0,O_FLOOR);
    Base *base = new Base(mod); save->getBases()->push_back(base);
    RuleSoldier *rule = mod->getSoldier("STR_SOLDIER",true); Armor *armor = mod->getArmor(rule->getArmor(),true);
    for (int i = 0; i < 3; ++i) {
        Soldier *soldier = new Soldier(rule,armor,i+1);
        soldier->setName(paired() ? "SELECTION FIXTURE " + std::to_string(i+1) : "SELECTION FIXTURE");
        soldier->getCurrentStats()->tu = 60; soldier->getCurrentStats()->reactions = 1;
        if (i) {
            YAML::Node clone = base->getSoldiers()->front()->save(); clone["id"] = i+1;
            soldier->load(clone,mod,save);
            if (paired()) soldier->setName("SELECTION FIXTURE " + std::to_string(i+1));
        }
        base->getSoldiers()->push_back(soldier);
        soldiers[i] = new BattleUnit(soldier,0);
        originalTU[i] = paired() ? 40-i*7 : 40;
        originalKneel[i] = paired() && i == 1;
        soldiers[i]->setTimeUnits(originalTU[i]); soldiers[i]->kneel(originalKneel[i]);
        if (fixture().skipMask & (1 << i)) soldiers[i]->dontReselect();
        if (fixture().deadMask & (1 << i)) soldiers[i]->instaKill();
        battle->getUnits()->push_back(soldiers[i]);
    }
    player = soldiers[0]; twin = soldiers[1]; third = soldiers[2];
    Unit *alien = mod->getUnit(master == "xcom1" ? "STR_SECTOID_SOLDIER" : "STR_AQUATOID_SOLDIER",true);
    hostile = new BattleUnit(alien, FACTION_HOSTILE, BattleUnit::MAX_SOLDIER_ID,
        mod->getArmor(alien->getArmor(),true),mod->getStatAdjustment(0),0);
    player->setPosition(playerPosition); player->setDirection(0);
    twin->setPosition(twinPosition); twin->setDirection(0);
    third->setPosition(thirdPosition); third->setDirection(0);
    hostile->setPosition(hostilePosition); hostile->setDirection(4);
    hostile->getBaseStats()->tu = 100; hostile->getBaseStats()->reactions = 100;
    hostile->setTimeUnits(100); battle->getUnits()->push_back(hostile);
    battle->resetUnitTiles(); battle->setSelectedUnit(soldiers[fixture().start]); battle->initUtilities(mod);
    initialTU = originalTU[0]; twinTU = originalTU[1];
    for (int i = 0; i < 3; ++i)
        require(soldiers[i]->isSelectable(FACTION_PLAYER,true,false) ==
            !((fixture().skipMask | fixture().deadMask) & (1 << i)));
    battle->setTUReserved(BA_NONE); battle->setKneelReserved(false);
    battle->getTileEngine()->calculateSunShading(); battle->getTileEngine()->calculateTerrainLighting();
    battle->getTileEngine()->calculateUnitLighting(); battle->getTileEngine()->recalculateFOV();
    state = new BattlescapeState; battle->setBattleState(state); game->setState(state);
}
void beforeEvents(Game *game)
{
    if (!running || pendingSetup) return;
    if (Clock::now() - started >= std::chrono::seconds(180) ||
        Clock::now() - worldStarted >= std::chrono::seconds(15)) watchdogFailure();
    quiet(); require(SDL_GetModState() == KMOD_NONE);
    if (!moved) { motion(); moved = true; return; }
    if (stage == PREPARE_COMMAND) {
        if (paired()) { command(); stage = WAIT_PENDING; }
        else switch (adversary()) {
        case SELECT_RACE: key(Options::keyBattleNextUnit); stage = WAIT_MUTATION; break;
        case MANUAL_RACE: key(Options::keyBattleKneel); stage = WAIT_MUTATION; break;
        case OVERLAY_RACE: key(Options::keyBattleOptions); stage = WAIT_MUTATION; break;
        case FOCUS_RACE: {
            SDL_Event e = {}; e.type = SDL_ACTIVEEVENT; e.active.gain = 0; e.active.state = SDL_APPINPUTFOCUS;
            queue(e); stage = WAIT_MUTATION; break;
        }
        case LOAD_SOURCE: savedEpoch = epoch; savedCapability = capability; finishWorld(); break;
        case TU_RACE: player->setTimeUnits(initialTU - 1); state->updateSoldierInfo(); before = snapshot(); command(); stage = WAIT_REJECTED; break;
        case RESERVE_RACE: state->getBattleGame()->setKneelReserved(true); before = snapshot(); command(); stage = WAIT_REJECTED; break;
        default: before = snapshot(); command(); stage = WAIT_PENDING; break;
        }
    } else if (stage == WAIT_PENDING && available()) {
        status(receive(),"pending_confirmation"); require(!game->isState(state) && callbacks == 0);
        if (!paired() && adversary() == PREPRESENTED_Y) {
            require(modalFrames == 0); key(SDLK_y); stage = WAIT_BLOCKED_INPUT;
        } else stage = CONFIRM;
    } else if (stage == CONFIRM) {
        if (modalFrames == 0) return; // Every valid operator edge follows an actual modal flip.
        if (!paired() && (adversary() == HELD_Y_REPEAT || adversary() == HELD_MOUSE_RELEASE)) {
            if (modalFrames == 0) return;
            if (adversary() == HELD_Y_REPEAT) keyEdge(SDLK_y,true);
            else mouseEdge(false);
            stage = WAIT_BLOCKED_INPUT;
        } else if (!paired() && adversary() == DISCONNECT_PENDING) { close(peer); peer = -1; click(true); stage = WAIT_CANCELLED; }
        else if (!paired() && adversary() == CANCEL_CONFIRMATION) { click(false); stage = WAIT_CANCELLED; }
        else if (!paired() && adversary() == PENDING_SELECT_RACE) {
            game->getSavedGame()->getSavedBattle()->setSelectedUnit(twin);
            state->updateSoldierInfo(); before = snapshot(); click(true); stage = WAIT_CANCELLED;
        } else if (!paired() && adversary() == STALE_CONFIRMATION) {
            // The accepted command's actor becomes invalid while the owned local
            // confirmation is open. A stale OK must not dispatch to anybody.
            player->setTimeUnits(initialTU - 1); before = snapshot(); click(true); stage = WAIT_CANCELLED;
        } else { if (paired() && world == 3) key(SDLK_y); else click(true); stage = WAIT_DISPATCH; }
    } else if (stage == WAIT_DISPATCH && captured && available()) {
        status(receive(),"dispatched");
        if (paired()) {
            compare(manual,actual); ++parityCount;
            motion(); stage = REQUEST_POST_CONTEXT;
        } else {
            actionOracle(); before = actual;
            if (adversary() == POST_DISPATCH_UNAVAILABLE) { key(Options::keyBattleOptions); stage = WAIT_POST_OVERLAY; }
            else stage = CHECK_REPLAY;
        }
    } else if (stage == CHECK_REPLAY) {
        // Replays are sent after local consent and dispatch; no second consent.
        if (replayStep == 0) sendPacket(commandPacket);
        else if (replayStep == 1) sendStatusQuery();
        else if (replayStep == 2) {
            std::string conflict = commandPacket;
            if (stanceCommand()) conflict[98] = 's';
            else conflict[97] = fixture().direction == 'n' ? 'p' : 'n';
            sendPacket(conflict);
        }
        else if (replayStep == 3) { commandId = randomToken(); std::string replay = commandPacket; replay.replace(65,32,commandId); sendPacket(replay); }
        else if (replayStep == 4) {
            // Fresh, current capability makes a cross-action ID conflict an
            // independent ledger check rather than an incidental stale reject.
            motion(); stage = REQUEST_LEDGER_CONTEXT; return;
        }
        else if (replayStep == 5) {
            std::string cross = commandPacket;
            cross[0] = stanceCommand() ? 'N' : 'D';
            cross[97] = stanceCommand() ? 'n' : 's';
            cross[98] = stanceCommand() ? '0' : 'k';
            commandId = randomToken(); cross.replace(65,32,commandId);
            sendPacket(cross); // Original consumed capability, opposite action.
        }
        else { compare(before,snapshot()); finishWorld(); return; }
        stage = WAIT_REPLAY;
    } else if (stage == WAIT_REPLAY && available()) {
        status(receive(),replayStep < 2 ? "dispatched" : "unavailable");
        require(callbacks == 1); compare(before,snapshot()); ++replayStep; stage = CHECK_REPLAY;
    } else if (stage == WAIT_REJECTED && available() && (adversary() != SELECT_RACE || samePanel)) {
        status(receive(),"unavailable"); rejectedUnchanged();
    } else if (stage == WAIT_BLOCKED_INPUT && inputs.empty()) {
        require(callbacks == 0); sendStatusQuery(); stage = WAIT_BLOCKED_STATUS;
    } else if (stage == WAIT_BLOCKED_STATUS && available()) {
        status(receive(),"unavailable"); require(callbacks == 0); compare(before,snapshot());
        if (!game->isState(state)) key(SDLK_ESCAPE);
        if (adversary() == HELD_Y_REPEAT) keyEdge(SDLK_y,false);
        stage = WAIT_CANCELLED;
    } else if (stage == WAIT_CANCELLED && inputs.empty() && game->isState(state)) {
        if (peer >= 0) { if (!available()) return; status(receive(),"unavailable"); }
        require(callbacks == 0); compare(before,snapshot()); finishWorld();
    }
}
bool acceptEvent(const SDL_Event &e)
{
    if (!running) return true;
    if (e.type == SDL_QUIT) return true;
    if (e.type == SDL_ACTIVEEVENT && (inputs.empty() || inputs.front().type != SDL_ACTIVEEVENT)) return true;
    const bool scripted = e.type == SDL_ACTIVEEVENT ||
        (e.type == SDL_MOUSEMOTION && e.motion.which == 255) ||
        ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && e.button.which == 255) ||
        ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && e.key.which == 255);
    if (!scripted) return false;
    require(!inputs.empty() && e.type == inputs.front().type);
    const SDL_Event expected = inputs.front(); inputs.pop_front();
    if (e.type == SDL_MOUSEMOTION) require(e.motion.x == expected.motion.x && e.motion.y == expected.motion.y);
    else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP)
        require(e.button.x == expected.button.x && e.button.y == expected.button.y &&
            e.button.button == expected.button.button && e.button.state == expected.button.state);
    else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
        require(e.key.keysym.sym == expected.key.keysym.sym && e.key.state == expected.key.state && e.key.keysym.mod == KMOD_NONE);
    else require(e.active.state == expected.active.state && e.active.gain == expected.active.gain);
    return true;
}
void afterEvents(Game *game)
{
    if (!running || pendingSetup || !inputs.empty()) return;
    if (stage == WAIT_MUTATION) {
        if (adversary() == SELECT_RACE) {
            require(game->getSavedGame()->getSavedBattle()->getSelectedUnit() == twin && player != twin &&
                player->getName(game->getLanguage()) == twin->getName(game->getLanguage()) &&
                player->getTimeUnits() == twin->getTimeUnits() && player->isKneeled() == twin->isKneeled());
        } else if (adversary() == MANUAL_RACE) require(callbacks == 1 && player->isKneeled());
        else if (adversary() == OVERLAY_RACE) require(!game->isState(state));
        before = snapshot(); command(); stage = WAIT_REJECTED;
    } else if (stage == WAIT_DISPATCH && captured) {
        if (paired() && !controlled()) {
            manual = actual;
            stage = WAIT_MANUAL_PANEL;
        }
        else sendStatusQuery();
    } else if (stage == WAIT_HELD_INPUT) stage = REQUEST_CONTEXT;
    else if (stage == WAIT_CANCELLED && peer >= 0) sendStatusQuery();
    else if (stage == WAIT_POST_OVERLAY) {
        require(!game->isState(state) && callbacks == 1);
        challenge = randomToken(); sendPacket("O" + challenge); stage = WAIT_FAILED_POST;
    }
}
void beforeFrame(Game *, State *)
{
    if (!running || pendingSetup) return;
    if (stage == WARM && moved && inputs.empty() && ready()) {
        ++frameCount;
        if (frameCount < 2) return; // Natural initialization plus one completed frame; no delay oracle.
        if (!paired() && adversary() == HELD_Y_REPEAT) { keyEdge(SDLK_y,true); stage = WAIT_HELD_INPUT; }
        else if (!paired() && adversary() == HELD_MOUSE_RELEASE) { motion(95,118); mouseEdge(true); stage = WAIT_HELD_INPUT; }
        else if (paired() && !controlled()) { key(fixture().direction == 'n' ? Options::keyBattleNextUnit : Options::keyBattlePrevUnit); stage = WAIT_DISPATCH; }
        else if (!paired() && adversary() == LOAD_TARGET) {
            epoch = savedEpoch; capability = savedCapability; before = snapshot(); command(); stage = WAIT_REJECTED;
        } else if (!paired() && adversary() == MISSING_CONTEXT) {
            capability = randomToken(); before = snapshot(); command(); stage = WAIT_REJECTED;
        } else stage = REQUEST_CONTEXT;
    }
    if (stage == REQUEST_LEDGER_CONTEXT && inputs.empty()) {
        challenge = randomToken(); sendPacket("O" + challenge); stage = WAIT_LEDGER_CONTEXT;
    }
    if (stage == REQUEST_CONTEXT || (stage == REQUEST_POST_CONTEXT && inputs.empty())) {
        const bool after = stage == REQUEST_POST_CONTEXT;
        challenge = randomToken(); sendPacket("O" + challenge); stage = after ? WAIT_POST_CONTEXT : WAIT_CONTEXT;
    }
}
void afterFrame(Game *game)
{
    if (!running || pendingSetup) return;
    if (!game->isState(state) && (stage == WAIT_PENDING || stage == CONFIRM || stage == WAIT_BLOCKED_INPUT)) ++modalFrames;
    if (stage == WAIT_CONTEXT && available()) { context(receive()); require(!available()); stage = PREPARE_COMMAND; }
    else if (stage == WAIT_MANUAL_PANEL) { manualPanel = panel(); finishWorld(); }
    else if (stage == WAIT_LEDGER_CONTEXT && available()) {
        context(receive()); require(!available());
        commandId = commandPacket.substr(65,32);
        const std::string cross = (stanceCommand() ? "N" : "D") + epoch + capability + commandId +
            (stanceCommand() ? std::string("n0") : std::string("sk"));
        sendPacket(cross); stage = WAIT_REPLAY;
    }
    else if (stage == WAIT_POST_CONTEXT && available()) {
        context(receive()); require(!available() && contextPixels == manualPanel && callbacks == 1);
        actionOracle(); finishWorld();
    } else if (stage == WAIT_FAILED_POST && available()) {
        YAML::Node failure = receive(); fields(failure,{"error"});
        require(failure["error"].as<std::string>() == "unavailable" && callbacks == 1);
        compare(before,snapshot()); finishWorld();
    }
    else if (stage == WAIT_REJECTED && !paired() && adversary() == SELECT_RACE) {
        require(panel() == contextPixels); samePanel = true;
    }
}
void selectionClicked(BattlescapeState *who, bool next)
{
    if (!running || pendingSetup || who != state) return;
    require(next == (fixture().direction == 'n'));
    ++callbacks; ++selectionCallbacks;
    actionOracle(); actual = snapshot(); captured = true;
}
void kneelClicked(BattlescapeState *who)
{
    if (!running || pendingSetup || who != state) return;
    ++callbacks; ++stanceCallbacks;
    actionOracle(); actual = snapshot(); captured = true;
}
int exitStatus()
{
    if (enabled && completed) {
        require(parityCount == PairCount && world == PairWorlds + AdversaryCount);
        std::cout << "SELECTION_CONTROL_GATE PASS {\"schema\":\"selection-control-gate/1\",\"master\":\"" << master <<
            "\",\"parity_pairs\":10,\"adversarial_worlds\":19,\"three_eligible_actors\":true,"
            "\"actual_panel_oracle\":true,\"post_action_panels\":10,\"shared_stance_ledger\":true,"
            "\"local_confirmation\":true,\"timing_equal\":false,\"full_fairness\":false}" << std::endl;
    }
    if (peer >= 0) { close(peer); peer = -1; }
    return enabled && !completed ? EXIT_FAILURE : EXIT_SUCCESS;
}
} }
#undef fields
#undef require
