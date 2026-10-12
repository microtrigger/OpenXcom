// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
// A finite, default-OFF native right-click fixture. Ordinary timers complete all
// turns; expected direction/TU and actual physical display bytes are independent.
// Oracle values, resources and captures remain private in memory.
#include "TurnControlGate.h"
#include "../../src/Battlescape/BattlescapeGame.h"
#include "../../src/Battlescape/BattlescapeState.h"
#include "../../src/Battlescape/BattleState.h"
#include "../../src/Battlescape/UnitTurnBState.h"
#include "../../src/Battlescape/WarningMessage.h"
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

namespace OpenXcom { namespace TurnControlGate {
namespace {
using Clock = std::chrono::steady_clock;
// Expected direction and TU are literal, independent of dispatch and native
// turning helpers. The last case opens an actual regular resource door.
struct Fixture { int start, target, expected, tu, spent; bool firing, kneel, contact, door; };
const Fixture fixtures[] = {
    {0,1,1,40,1,false,false,false,false},
    {0,3,3,40,3,false,false,false,false},
    {7,0,0,40,1,false,false,false,false},
    {0,7,7,40,1,false,false,false,false},
    {2,2,2,40,0,false,false,false,false},
    {0,3,0,0,0,false,false,false,false},
    {0,3,3,3,3,false,false,false,false},
    {0,3,2,2,2,false,false,false,false},
    {0,3,0,0,0,true,false,false,false},
    {0,3,2,2,2,true,false,false,false},
    {0,3,3,3,3,true,false,false,false},
    {0,3,1,5,1,false,true,false,false},
    {3,0,0,40,3,false,false,false,false},
    {0,3,1,40,1,false,false,true,false},
    {0,0,0,40,0,false,false,false,true}
};
enum { PairCount = sizeof(fixtures) / sizeof(fixtures[0]), PairWorlds = PairCount * 2 };
enum Adversary { SELECT_RACE, MANUAL_RACE, OVERLAY_RACE, FOCUS_RACE,
    LOAD_SOURCE, LOAD_TARGET, TU_RACE, RESERVE_RACE, MISSING_CONTEXT,
    CANCEL_CONFIRMATION, STALE_CONFIRMATION, PENDING_SELECT_RACE,
    PREPRESENTED_Y, HELD_Y_REPEAT, HELD_MOUSE_RELEASE, CAMERA_ROUNDTRIP,
    POSE_ROUNDTRIP, PENDING_CAMERA_ROUNDTRIP, PENDING_POSE_ROUNDTRIP,
    ANIMATION_SURVIVES, LEDGER_TURN, LEDGER_STANCE, LEDGER_SELECTION,
    O_FOR_TURN, V_FOR_STANCE, V_FOR_SELECTION, PENDING_VIEW_REPLACEMENT, POST_DISPATCH_UNAVAILABLE,
    DISCONNECT_PENDING, AdversaryCount };
enum Stage { WARM, REQUEST_CONTEXT, WAIT_CONTEXT, PREPARE_COMMAND,
    WAIT_MUTATION, WAIT_MANUAL_MUTATION, WAIT_PENDING, CONFIRM, WAIT_DISPATCH,
    WAIT_SETTLED, WAIT_ASYNC_POST_CONTEXT, WAIT_REPLACEMENT_VIEW, REQUEST_POST_CONTEXT, WAIT_POST_CONTEXT, CHECK_REPLAY,
    WAIT_REPLAY, REQUEST_LEDGER_CONTEXT, WAIT_LEDGER_CONTEXT, WAIT_REJECTED,
    WAIT_CANCELLED, WAIT_HELD_INPUT, WAIT_BLOCKED_INPUT, WAIT_BLOCKED_STATUS,
    WAIT_ANIMATION, WAIT_BLOCKED_OBSERVATION, WAIT_POST_OVERLAY, WAIT_FAILED_POST, FINISHED };
bool enabled = false, running = false, pendingSetup = false, completed = false;
bool moved = false, dispatched = false, animationChanged = false;
bool preview = false, previewShown = false, warningSeen = false, replacingPendingView = false;
int peer = -1, world = 0, initialTU = 0, callbacks = 0, turnCallbacks = 0,
    stanceCallbacks = 0, selectionCallbacks = 0, frameCount = 0, modalFrames = 0,
    replayStep = 0, parityCount = 0, postCount = 0, contextCount = 0;
Stage stage = WARM;
Game *owner = 0;
BattlescapeState *state = 0;
BattleUnit *player = 0, *twin = 0, *hostile = 0;
MapData *doorClosed = 0, *doorOpen = 0;
const Position playerPosition(20,20,0), twinPosition(30,30,0);
Clock::time_point started, worldStarted, previewStarted;
std::deque<SDL_Event> inputs;
std::string master, epoch, capability, challenge, commandId, commandPacket;
std::string savedEpoch, savedCapability;
std::set<std::string> tokens;
struct Snapshot { YAML::Node save, runtime; uint64_t rng; };
Snapshot manual, actual, before;
std::vector<unsigned char> contextPixels;
Position targetPixel;
void check(bool ok, int assertion)
{
    if (!ok) {
        std::cerr << "TURN_CONTROL_GATE FAIL assertion-" << assertion << std::endl;
        std::_Exit(EXIT_FAILURE);
    }
}
#define require(ok) check((ok), __LINE__)
// The fixture inherits the actual native state without overriding any action,
// event, simulation, or presentation behavior. This private read-only inspector
// witnesses its actual warning widget; nothing is exposed through production.
class FixtureBattlescapeState : public BattlescapeState {
public:
    bool warningVisible() const {
        for (Surface *surface : _surfaces)
            if (WarningMessage *warning = dynamic_cast<WarningMessage*>(surface)) return warning->getVisible();
        require(false); return false;
    }
};
bool warningVisible() { return static_cast<FixtureBattlescapeState*>(state)->warningVisible(); }
bool paired() { return world < PairWorlds; }
int adversary() { return world - PairWorlds; }
bool controlled() { return !paired() || world % 2; }
const Fixture &fixture() { return fixtures[paired() ? world / 2 : adversary() == PENDING_VIEW_REPLACEMENT ? 1 : 0]; }
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
    require(!unit || unit == player || unit == twin || unit == hostile);
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
    char bytes[140000]; iovec part = {bytes, sizeof(bytes)}; msghdr m = {};
    m.msg_iov = &part; m.msg_iovlen = 1; ssize_t n = recvmsg(peer, &m, MSG_DONTWAIT);
    require(n > 0 && !(m.msg_flags & MSG_TRUNC));
    try { return YAML::Load(std::string(bytes, n)); } catch (...) { require(false); }
    return YAML::Node();
}
void status(const YAML::Node &n, const std::string &value)
{
    fields(n, {"schema", "command_id", "status"});
    require(n["schema"].as<std::string>() == "experimental-stance-status/1" &&
        n["command_id"].as<std::string>() == commandId && n["status"].as<std::string>() == value);
}
std::vector<unsigned char> display()
{
    // Independent physical display oracle, sampled only after natural SDL_Flip.
    SDL_Surface *d = SDL_GetVideoSurface();
    require(d && d->w == 640 && d->h == 400 && d->format && d->format->BitsPerPixel == 8 &&
        d->format->BytesPerPixel == 1 && d->format->palette && d->format->palette->ncolors == 256 &&
        !(d->flags & (SDL_OPENGL | SDL_HWSURFACE | SDL_DOUBLEBUF)));
    std::vector<unsigned char> result; result.reserve(320 * 200 * 3);
    require(SDL_LockSurface(d) == 0);
    for (int y = 0; y < 200; ++y) for (int x = 0; x < 320; ++x) {
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
std::vector<unsigned char> unhex(const std::string &hex, size_t bytes)
{
    const std::string alphabet = "0123456789abcdef";
    require(hex.size() == bytes * 2 && hex.find_first_not_of(alphabet) == std::string::npos);
    std::vector<unsigned char> out(bytes);
    for (size_t i = 0; i < bytes; ++i) out[i] = alphabet.find(hex[i*2]) * 16 + alphabet.find(hex[i*2+1]);
    return out;
}
void context(const YAML::Node &n, bool tactical)
{
    if (!n.IsMap() || !n["schema"].IsDefined()) {
        // Fixed compile-time labels identify the expected fixture/read phase;
        // no dynamic values, game state, packet content, or paths are emitted.
        if (stage == WAIT_CONTEXT) { require(false); }
        if (stage == WAIT_ASYNC_POST_CONTEXT) { require(false); }
        if (stage == WAIT_LEDGER_CONTEXT) { require(false); }
        if (stage == WAIT_POST_CONTEXT && paired()) {
            switch (world/2) {
            case 0: require(false); break; // clockwise-one
            case 1: require(false); break; // clockwise-three
            case 2: require(false); break; // wrap-forward
            case 3: require(false); break; // wrap-backward
            case 4: require(false); break; // already-facing
            case 5: require(false); break; // zero-TU
            case 6: require(false); break; // exact-TU
            case 7: require(false); break; // partial-TU
            case 8: require(false); break; // snapshot-zero-free
            case 9: require(false); break; // snapshot-partial-free
            case 10: require(false); break; // snapshot-exact-free
            case 11: require(false); break; // kneel-reserved
            case 12: require(false); break; // counterclockwise-three
            case 13: require(false); break; // first-contact
            case 14: require(false); break; // ordinary-door
            }
        }
        if (!paired()) {
            switch (adversary()) {
            case SELECT_RACE: require(false); break;
            case MANUAL_RACE: require(false); break;
            case OVERLAY_RACE: require(false); break;
            case FOCUS_RACE: require(false); break;
            case LOAD_SOURCE: require(false); break;
            case LOAD_TARGET: require(false); break;
            case TU_RACE: require(false); break;
            case RESERVE_RACE: require(false); break;
            case MISSING_CONTEXT: require(false); break;
            case CANCEL_CONFIRMATION: require(false); break;
            case STALE_CONFIRMATION: require(false); break;
            case PENDING_SELECT_RACE: require(false); break;
            case PREPRESENTED_Y: require(false); break;
            case HELD_Y_REPEAT: require(false); break;
            case HELD_MOUSE_RELEASE: require(false); break;
            case CAMERA_ROUNDTRIP: require(false); break;
            case POSE_ROUNDTRIP: require(false); break;
            case PENDING_CAMERA_ROUNDTRIP: require(false); break;
            case PENDING_POSE_ROUNDTRIP: require(false); break;
            case ANIMATION_SURVIVES: require(false); break;
            case LEDGER_TURN: require(false); break;
            case LEDGER_STANCE: require(false); break;
            case LEDGER_SELECTION: require(false); break;
            case O_FOR_TURN: require(false); break;
            case V_FOR_STANCE: require(false); break;
            case V_FOR_SELECTION: require(false); break;
            case PENDING_VIEW_REPLACEMENT: require(false); break;
            case POST_DISPATCH_UNAVAILABLE: require(false); break;
            case DISCONNECT_PENDING: require(false); break;
            }
        }
        require(false);
    }
    fields(n, {"schema", "challenge", "control_epoch", "capability", "semantics", tactical ? "view" : "panel"});
    require(n["schema"].as<std::string>() == (tactical ? "experimental-tactical-context/1" : "experimental-stance-context/1") &&
        n["challenge"].as<std::string>() == challenge && n["semantics"].as<std::string>() == "presented_after_request");
    epoch = n["control_epoch"].as<std::string>(); capability = n["capability"].as<std::string>();
    require(token(epoch) && token(capability) && tokens.insert(capability).second);
    contextPixels = display();
    if (tactical) {
        fields(n["view"], {"width", "height", "indices_hex", "palette_hex"});
        require(n["view"]["width"].as<int>() == 320 && n["view"]["height"].as<int>() == 200);
        const auto indices = unhex(n["view"]["indices_hex"].as<std::string>(),320*200);
        const auto palette = unhex(n["view"]["palette_hex"].as<std::string>(),256*3);
        // Independent canonical first-seen RGB palette from the physical video
        // surface. No producer helper, source framebuffer, or state renderer.
        std::map<unsigned int,unsigned int> firstSeen;
        for (size_t i = 0; i < indices.size(); ++i) {
            const unsigned int rgb = (contextPixels[i*3] << 16) | (contextPixels[i*3+1] << 8) | contextPixels[i*3+2];
            if (!firstSeen.count(rgb)) { const unsigned int next = firstSeen.size(); firstSeen[rgb] = next; }
            require(firstSeen.size() <= 256 && indices[i] == firstSeen[rgb]);
            for (int c = 0; c < 3; ++c) require(palette[indices[i]*3+c] == contextPixels[i*3+c]);
        }
        for (size_t i = firstSeen.size()*3; i < palette.size(); ++i) require(palette[i] == 0);
    } else {
        fields(n["panel"], {"width", "height", "rgb_hex"});
        require(n["panel"]["width"].as<int>() == 320 && n["panel"]["height"].as<int>() == 56);
        const auto pixels = unhex(n["panel"]["rgb_hex"].as<std::string>(),320*56*3);
        require(std::equal(pixels.begin(), pixels.end(), contextPixels.begin()+320*144*3));
    }
    ++contextCount;
}
void queue(SDL_Event e) { inputs.push_back(e); require(SDL_PushEvent(&e) == 0); }
void motion(int x = 160, int y = 80)
{
    SDL_Event e = {}; e.type = SDL_MOUSEMOTION; e.motion.which = 255;
    e.motion.x = x*2; e.motion.y = y*2; queue(e);
}
void keyEdge(SDLKey code, bool down)
{
    SDL_Event e = {}; e.type = down ? SDL_KEYDOWN : SDL_KEYUP; e.key.which = 255;
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED; e.key.keysym.sym = code; e.key.keysym.mod = KMOD_NONE; queue(e);
}
void key(SDLKey code) { keyEdge(code,true); keyEdge(code,false); }
char commandKind();
Position approvalPixel(bool ok)
{
    return commandKind() == 'R' ? Position(280,ok ? 8 : 24,0) : Position(ok ? 95 : 225,118,0);
}
void mouseEdge(bool down)
{
    SDL_Event e = {}; e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; e.button.which = 255;
    const Position p = approvalPixel(true);
    e.button.x = p.x*2; e.button.y = p.y*2; e.button.button = SDL_BUTTON_LEFT;
    e.button.state = down ? SDL_PRESSED : SDL_RELEASED; queue(e);
}
void click(bool ok)
{
    const Position p = approvalPixel(ok); motion(p.x,p.y);
    SDL_Event e = {}; e.type = SDL_MOUSEBUTTONDOWN; e.button.which = 255;
    e.button.x = p.x*2; e.button.y = p.y*2; e.button.button = SDL_BUTTON_LEFT; e.button.state = SDL_PRESSED; queue(e);
    e.type = SDL_MOUSEBUTTONUP; e.button.state = SDL_RELEASED; queue(e);
}
Position targetTile()
{
    static const int dx[] = {0,1,1,1,0,-1,-1,-1};
    static const int dy[] = {-1,-1,0,1,1,1,0,-1};
    return playerPosition + Position(dx[fixture().target],dy[fixture().target],0);
}
Position pixelFor(const Position &tile)
{
    Position p; state->getMap()->getCamera()->convertMapToScreen(tile,&p);
    p += state->getMap()->getCamera()->getMapOffset(); p.x += 16; p.y += 24;
    require(p.x >= 16 && p.x <= 303 && p.y >= 40 && p.y <= 127); return p;
}
void manualTurn()
{
    const Position p = pixelFor(targetTile()); motion(p.x,p.y);
    SDL_Event e = {}; e.type = SDL_MOUSEBUTTONDOWN; e.button.which = 255;
    e.button.x = p.x*2; e.button.y = p.y*2; e.button.button = SDL_BUTTON_RIGHT;
    e.button.state = SDL_PRESSED; queue(e);
    e.type = SDL_MOUSEBUTTONUP; e.button.state = SDL_RELEASED; queue(e);
}
void quiet()
{
    int frequency, channels; Uint16 format; char driver[64] = {};
    require(Options::mute && Mix_QuerySpec(&frequency,&format,&channels) > 0 &&
        SDL_AudioDriverName(driver,sizeof(driver)) && std::string(driver) == "dummy" &&
        Mix_Playing(-1) == 0 && Mix_PlayingMusic() == 0);
}
bool idle()
{
    const Uint8 focus = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
    SavedBattleGame *b = owner->getSavedGame()->getSavedBattle();
    return owner->isState(state) && (SDL_GetAppState() & focus) == focus &&
        b->getSide() == FACTION_PLAYER && !b->getDebugMode() &&
        state->getBattleGame()->getPanicHandled() && !state->getBattleGame()->isBusy() &&
        state->getBattleGame()->getStates().empty() && player->getStatus() == STATUS_STANDING &&
        !state->getMap()->getProjectile() && state->getMap()->getCursorType() == CT_NORMAL &&
        !b->getUnitsFalling() && state->getBattleGame()->getCurrentAction()->type == BA_NONE;
}
void replacement(Game *game) { pendingSetup = true; game->setState(new State); }
void finishWorld()
{
    require(inputs.empty()); ++world;
    if (world == PairWorlds + AdversaryCount) { running = false; completed = true; owner->quit(); }
    else replacement(owner);
}
char commandKind()
{
    if (!paired() && (adversary() == LEDGER_STANCE || adversary() == V_FOR_STANCE)) return 'D';
    if (!paired() && (adversary() == LEDGER_SELECTION || adversary() == V_FOR_SELECTION)) return 'N';
    return 'R';
}
bool tacticalRequest()
{
    if (!paired() && adversary() == O_FOR_TURN) return false;
    if (!paired() && (adversary() == V_FOR_STANCE || adversary() == V_FOR_SELECTION)) return true;
    return commandKind() == 'R';
}
std::string coordinate(int n)
{
    require(n >= 0 && n < 1000); std::string s = std::to_string(n); return std::string(3-s.size(),'0')+s;
}
std::string packet(char kind, const std::string &id, const std::string &grant)
{
    std::string payload = kind == 'R' ? coordinate(targetPixel.x)+coordinate(targetPixel.y) : kind == 'D' ? "sk" : "n0";
    const std::string s = std::string(1,kind)+epoch+grant+id+payload;
    require(s.size() == (kind == 'R' ? 103u : 99u)); return s;
}
void command()
{
    commandId = randomToken(); commandPacket = packet(commandKind(),commandId,capability); sendPacket(commandPacket);
}
void roundtrip(bool camera)
{
    if (camera) {
        Camera *c = state->getMap()->getCamera(); Position p = c->getMapOffset();
        c->setMapOffset(p+Position(8,0,0)); c->setMapOffset(p); require(c->getMapOffset() == p);
    } else {
        const int direction = player->getDirection();
        player->setDirection((direction+1)%8); player->setDirection(direction);
        require(player->getDirection() == direction);
    }
}
void outcome()
{
    require(idle() && callbacks == 1);
    SavedBattleGame *battle = owner->getSavedGame()->getSavedBattle();
    if (commandKind() == 'D') {
        require(stanceCallbacks == 1 && turnCallbacks == 0 && selectionCallbacks == 0 &&
            player->isKneeled() && player->getTimeUnits() == initialTU-4 && battle->getSelectedUnit() == player);
    } else if (commandKind() == 'N') {
        require(selectionCallbacks == 1 && turnCallbacks == 0 && stanceCallbacks == 0 &&
            battle->getSelectedUnit() == twin && player->getTimeUnits() == initialTU && !player->isKneeled());
    } else {
        require(turnCallbacks == 1 && stanceCallbacks == 0 && selectionCallbacks == 0 &&
            battle->getSelectedUnit() == player && !player->isKneeled());
        const Fixture &f = fixture();
        const int spent = f.door ? (master == "xcom1" ? 8 : 4) : f.spent;
        require(player->getDirection() == f.expected && player->getTimeUnits() == initialTU-spent);
        require(state->getBattleGame()->getCurrentAction()->target == targetTile());
        if (f.contact) {
            require(f.expected != f.target && hostile->getVisible() &&
                player->getUnitsSpottedThisTurn().size() == 1 &&
                player->getUnitsSpottedThisTurn().front() == hostile &&
                player->getVisibleUnits()->size() == 1 && player->getVisibleUnits()->front() == hostile);
        } else require(player->getUnitsSpottedThisTurn().empty());
        if (f.door) {
            Tile *t = battle->getTile(playerPosition);
            require(!t->getMapData(O_NORTHWALL) && t->getMapData(O_WESTWALL) == doorOpen && doorOpen != doorClosed);
        }
    }
    require(twin->getTimeUnits() == 40 && !twin->isKneeled() && hostile->getTimeUnits() == 100 &&
        hostile->getHealth() == hostile->getBaseStats()->health && player->getHealth() == player->getBaseStats()->health);
    // Native turns do not add a reaction-fire call. Queue/TU/RNG equality below
    // checks real semantics without inventing a reaction oracle for this action.
}
BattleItem *arm(BattleUnit *unit, SavedBattleGame *battle, Mod *mod)
{
    RuleItem *gun = mod->getItem(master == "xcom1" ? "STR_RIFLE" : "STR_JET_HARPOON",true);
    RuleItem *clip = mod->getItem(master == "xcom1" ? "STR_RIFLE_CLIP" : "STR_HARPOON_POD",true);
    BattleItem *weapon = new BattleItem(gun,battle->getCurrentItemId());
    BattleItem *ammo = new BattleItem(clip,battle->getCurrentItemId());
    battle->getItems()->push_back(weapon); battle->getItems()->push_back(ammo);
    weapon->moveToOwner(unit); weapon->setSlot(mod->getInventory("STR_RIGHT_HAND",true));
    require(weapon->setAmmoItem(ammo) == 0 && ammo->getAmmoQuantity() > 0); return weapon;
}
void request(bool tactical) { challenge = randomToken(); sendPacket((tactical ? "V" : "O")+challenge); }
void unchanged()
{
    require(callbacks == ((!paired() && (adversary() == SELECT_RACE || adversary() == MANUAL_RACE)) ? 1 : 0));
    compare(before,snapshot()); finishWorld();
}
// Fixed source-line diagnostics distinguish stalled phases without exposing state.
void watchdogFailure()
{
    require(stage != WARM); require(stage != WAIT_CONTEXT); require(stage != WAIT_PENDING);
    require(stage != CONFIRM); require(stage != WAIT_DISPATCH); require(stage != WAIT_SETTLED);
    require(stage != WAIT_POST_CONTEXT); require(stage != WAIT_ASYNC_POST_CONTEXT); require(stage != WAIT_REJECTED); require(stage != WAIT_CANCELLED);
    require(stage != WAIT_REPLAY); require(stage != WAIT_LEDGER_CONTEXT); require(stage != WAIT_ANIMATION);
    require(stage != WAIT_MANUAL_MUTATION); require(stage != WAIT_BLOCKED_STATUS); require(false);
}
} // namespace

bool active()
{
    const char *e = std::getenv("OPENXCOM_TURN_CONTROL_GATE"); return e && std::string(e) == "1";
}
void prepare()
{
    enabled = active(); if (!enabled) return;
    const char *view = std::getenv("OPENXCOM_TURN_MODAL_PREVIEW"); preview = view && std::string(view) == "1";
    require(!std::getenv("OPENXCOM_STANCE_CONTROL_GATE") && !std::getenv("OPENXCOM_SELECTION_CONTROL_GATE"));
    require(setenv("OPENXCOM_TURN_ENABLED","1",1) == 0 && setenv("OPENXCOM_SELECTION_ENABLED","1",1) == 0);
    require(Options::language == "en-US" && !Options::playIntro);
    require(!std::getenv("OPENXCOM_STANCE_FD") && !std::getenv("OPENXCOM_PRESENTATION_FD") &&
        !std::getenv("OPENXCOM_AUDIO_FD") && !std::getenv("OPENXCOM_NATURAL_FOV_GATE") &&
        !std::getenv("OPENXCOM_BATTLEFIELD_AUDIO_GATE") && !std::getenv("OPENXCOM_BATTLE_PAIR_GATE"));
    int sockets[2]; require(socketpair(AF_UNIX,SOCK_SEQPACKET,0,sockets) == 0); peer = sockets[0];
    const std::string descriptor = std::to_string(sockets[1]);
    require(setenv("OPENXCOM_STANCE_FD",descriptor.c_str(),1) == 0 && setenv("SDL_AUDIODRIVER","dummy",1) == 0);
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
    if (!enabled) return; Mix_HaltChannel(-1); Mix_HaltMusic(); Options::mute = true; quiet();
}
bool loaded(Game *game)
{
    if (!enabled) return false;
    owner = game; quiet(); require(Options::language == "en-US" && !Options::playIntro);
    YAML::Node hello = receive(); fields(hello,{"schema","control_epoch"});
    require(hello["schema"].as<std::string>() == "experimental-stance/1");
    epoch = hello["control_epoch"].as<std::string>(); require(token(epoch) && !available());
    running = true; started = Clock::now(); replacement(game);
    Options::baseXResolution = 320; Options::baseYResolution = 200; game->getScreen()->resetDisplay(false);
    game->getCursor()->setVisible(true); game->getFpsCounter()->setVisible(false); return true;
}
void afterCleanup(Game *game)
{
    if (!running || !pendingSetup) return;
    if (replacingPendingView) {
        YAML::Node denied = receive(); fields(denied,{"error"});
        require(denied["error"].as<std::string>() == "unavailable" && !available() && callbacks == 1);
        replacingPendingView = false; ++world;
    }
    pendingSetup = false; moved = dispatched = animationChanged = warningSeen = false;
    callbacks = turnCallbacks = stanceCallbacks = selectionCallbacks = frameCount = modalFrames = replayStep = 0;
    stage = WARM; inputs.clear(); worldStarted = Clock::now();
    RNG::setSeed(123456789); std::srand(12345); // Identical fixture creation only.
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
    doorClosed = doorOpen = 0;
    if (fixture().door) {
        MapDataSet *doors = mod->getMapDataSet(master == "xcom1" ? "URBAN" : "PORT01"); require(doors);
        doors->loadData(mod->getMCDPatch(doors->getName())); battle->getMapDataSets()->push_back(doors);
        const int doorId = master == "xcom1" ? 28 : 48;
        doorClosed = doors->getObject(doorId); doorOpen = doors->getObject(doorClosed->getAltMCD());
        require(doorClosed->isDoor() && !doorClosed->isUFODoor() && doorClosed->getObjectType() == O_NORTHWALL &&
            doorOpen->getObjectType() == O_WESTWALL && doorOpen->getTUCost(MT_WALK) == (master == "xcom1" ? 8 : 4));
        battle->getTile(playerPosition)->setMapData(doorClosed,doorId,1,O_NORTHWALL);
    }
    Base *base = new Base(mod); save->getBases()->push_back(base);
    RuleSoldier *rule = mod->getSoldier("STR_SOLDIER",true); Armor *armor = mod->getArmor(rule->getArmor(),true);
    Soldier *first = new Soldier(rule,armor,1); first->setName("TURN FIXTURE");
    first->getCurrentStats()->tu = 60; first->getCurrentStats()->reactions = 1;
    Soldier *second = new Soldier(rule,armor,2);
    YAML::Node clone = first->save(); clone["id"] = 2; second->load(clone,mod,save);
    base->getSoldiers()->push_back(first); base->getSoldiers()->push_back(second);
    player = new BattleUnit(first,0); twin = new BattleUnit(second,0);
    Unit *alien = mod->getUnit(master == "xcom1" ? "STR_SECTOID_SOLDIER" : "STR_AQUATOID_SOLDIER",true);
    hostile = new BattleUnit(alien,FACTION_HOSTILE,BattleUnit::MAX_SOLDIER_ID,mod->getArmor(alien->getArmor(),true),mod->getStatAdjustment(0),0);
    player->setPosition(playerPosition); player->setDirection(fixture().start);
    twin->setPosition(twinPosition); twin->setDirection(4); twin->setTimeUnits(40);
    hostile->setPosition(fixture().contact ? Position(22,20,0) : Position(2,2,0)); hostile->setDirection(0);
    hostile->getBaseStats()->tu = 100; hostile->getBaseStats()->reactions = 100; hostile->setTimeUnits(100);
    battle->getUnits()->push_back(player); battle->getUnits()->push_back(twin); battle->getUnits()->push_back(hostile);
    battle->resetUnitTiles(); battle->setSelectedUnit(player); battle->initUtilities(mod);
    initialTU = fixture().tu;
    if (fixture().firing) initialTU += player->getActionTUs(BA_SNAPSHOT,arm(player,battle,mod));
    player->setTimeUnits(initialTU); battle->setTUReserved(fixture().firing ? BA_SNAPSHOT : BA_NONE);
    battle->setKneelReserved(fixture().kneel);
    battle->getTileEngine()->calculateSunShading(); battle->getTileEngine()->calculateTerrainLighting();
    battle->getTileEngine()->calculateUnitLighting(); battle->getTileEngine()->recalculateFOV();
    require(player->getUnitsSpottedThisTurn().empty() && player->getVisibleUnits()->empty() && !hostile->getVisible());
    state = new FixtureBattlescapeState; battle->setBattleState(state); game->setState(state);
}
void beforeEvents(Game *game)
{
    if (!running || pendingSetup) return;
    if (Clock::now()-started >= std::chrono::seconds(240) || Clock::now()-worldStarted >= std::chrono::seconds(preview ? 30 : 15)) watchdogFailure();
    quiet(); require(SDL_GetModState() == KMOD_NONE);
    if (!moved) { motion(); moved = true; return; }
    if (stage == PREPARE_COMMAND) {
        if (paired()) { command(); stage = WAIT_PENDING; }
        else switch (adversary()) {
        case SELECT_RACE: key(Options::keyBattleNextUnit); stage = WAIT_MUTATION; break;
        case MANUAL_RACE: manualTurn(); stage = WAIT_MANUAL_MUTATION; break;
        case OVERLAY_RACE: key(Options::keyBattleOptions); stage = WAIT_MUTATION; break;
        case FOCUS_RACE: {
            SDL_Event e = {}; e.type = SDL_ACTIVEEVENT; e.active.gain = 0; e.active.state = SDL_APPINPUTFOCUS;
            queue(e); stage = WAIT_MUTATION; break;
        }
        case LOAD_SOURCE: savedEpoch = epoch; savedCapability = capability; finishWorld(); break;
        case TU_RACE: player->setTimeUnits(initialTU-1); state->updateSoldierInfo(); before = snapshot(); command(); stage = WAIT_REJECTED; break;
        case RESERVE_RACE: state->getBattleGame()->setKneelReserved(true); before = snapshot(); command(); stage = WAIT_REJECTED; break;
        case CAMERA_ROUNDTRIP: case POSE_ROUNDTRIP:
            roundtrip(adversary() == CAMERA_ROUNDTRIP); before = snapshot(); command(); stage = WAIT_REJECTED; break;
        case O_FOR_TURN: case V_FOR_STANCE: case V_FOR_SELECTION:
            before = snapshot(); command(); stage = WAIT_REJECTED; break;
        case ANIMATION_SURVIVES: before = snapshot(); stage = WAIT_ANIMATION; break;
        default: before = snapshot(); command(); stage = WAIT_PENDING; break;
        }
    } else if (stage == WAIT_ANIMATION && animationChanged) {
        // No input, camera/pose changes, or another request intervenes. A
        // naturally changed displayed animation must not revoke an otherwise
        // current grant.
        compare(before,snapshot()); command(); stage = WAIT_PENDING;
    } else if (stage == WAIT_MANUAL_MUTATION && inputs.empty() && callbacks == 1 && idle()) {
        outcome(); before = snapshot(); command(); stage = WAIT_REJECTED;
    } else if (stage == WAIT_PENDING && available()) {
        status(receive(),"pending_confirmation"); require(!game->isState(state) && callbacks == 0);
        if (!paired() && adversary() == PREPRESENTED_Y) {
            require(modalFrames == 0); key(SDLK_y); stage = WAIT_BLOCKED_INPUT;
        } else stage = CONFIRM;
    } else if (stage == CONFIRM && modalFrames > 0) {
        if (preview && world == 1) {
            if (Clock::now()-previewStarted < std::chrono::seconds(20)) return;
            running = false; game->quit(); return;
        }
        // Test-only local approval after actual modal presentation. Never a
        // controller packet, public MCP tool, or production self-approval API.
        if (!paired() && (adversary() == HELD_Y_REPEAT || adversary() == HELD_MOUSE_RELEASE)) {
            if (adversary() == HELD_Y_REPEAT) keyEdge(SDLK_y,true); else mouseEdge(false);
            stage = WAIT_BLOCKED_INPUT;
        } else if (!paired() && adversary() == DISCONNECT_PENDING) {
            close(peer); peer = -1; click(true); stage = WAIT_CANCELLED;
        } else if (!paired() && adversary() == CANCEL_CONFIRMATION) { click(false); stage = WAIT_CANCELLED; }
        else if (!paired() && adversary() == PENDING_SELECT_RACE) {
            game->getSavedGame()->getSavedBattle()->setSelectedUnit(twin);
            state->updateSoldierInfo(); before = snapshot(); click(true); stage = WAIT_CANCELLED;
        } else if (!paired() && adversary() == STALE_CONFIRMATION) {
            player->setTimeUnits(initialTU-1); before = snapshot(); click(true); stage = WAIT_CANCELLED;
        } else if (!paired() && (adversary() == PENDING_CAMERA_ROUNDTRIP || adversary() == PENDING_POSE_ROUNDTRIP)) {
            roundtrip(adversary() == PENDING_CAMERA_ROUNDTRIP); before = snapshot(); click(true); stage = WAIT_CANCELLED;
        } else { if (paired() && world == 3) key(SDLK_y); else click(true); stage = WAIT_DISPATCH; }
    } else if (stage == WAIT_DISPATCH && callbacks == 1 && available()) {
        status(receive(),"dispatched");
        if (!paired() && adversary() == PENDING_VIEW_REPLACEMENT) {
            require(!idle()); request(true); stage = WAIT_REPLACEMENT_VIEW;
        } else if (paired() && world == 3) {
            require(!idle()); request(true); stage = WAIT_ASYNC_POST_CONTEXT;
        } else stage = WAIT_SETTLED;
    } else if (stage == CHECK_REPLAY) {
        if (replayStep == 0) sendPacket(commandPacket);
        else if (replayStep == 1) sendPacket("S"+commandId);
        else if (replayStep == 2) {
            std::string conflict = commandPacket;
            if (commandKind() == 'R') conflict.replace(97,3,coordinate(targetPixel.x == 303 ? 302 : targetPixel.x+1));
            else if (commandKind() == 'D') conflict[98] = 's'; else conflict[97] = 'p';
            sendPacket(conflict);
        } else if (replayStep == 3) {
            commandId = randomToken(); std::string replay = commandPacket; replay.replace(65,32,commandId); sendPacket(replay);
        } else if (replayStep == 4 || (commandKind() == 'R' && replayStep == 6)) {
            stage = REQUEST_LEDGER_CONTEXT; return;
        } else if (replayStep == 5 || (commandKind() == 'R' && replayStep == 7)) {
            const char cross = commandKind() == 'R' ? (replayStep == 5 ? 'D' : 'N') : 'R';
            commandId = randomToken(); sendPacket(packet(cross,commandId,commandPacket.substr(33,32)));
        } else { compare(before,snapshot()); finishWorld(); return; }
        stage = WAIT_REPLAY;
    } else if (stage == WAIT_REPLAY && available()) {
        status(receive(),replayStep < 2 ? "dispatched" : "unavailable");
        require(callbacks == 1); compare(before,snapshot()); ++replayStep; stage = CHECK_REPLAY;
    } else if (stage == WAIT_BLOCKED_OBSERVATION && available()) {
        YAML::Node denied = receive(); fields(denied,{"error"});
        require(denied["error"].as<std::string>() == "unavailable" && !available() && callbacks == 1);
        compare(before,snapshot()); stage = REQUEST_POST_CONTEXT;
    } else if (stage == WAIT_REJECTED && available()) {
        status(receive(),"unavailable"); unchanged();
    } else if (stage == WAIT_BLOCKED_INPUT && inputs.empty()) {
        require(callbacks == 0); sendPacket("S"+commandId); stage = WAIT_BLOCKED_STATUS;
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
        if (adversary() == SELECT_RACE) require(game->getSavedGame()->getSavedBattle()->getSelectedUnit() == twin && selectionCallbacks == 1);
        else if (adversary() == OVERLAY_RACE) require(!game->isState(state));
        before = snapshot(); command(); stage = WAIT_REJECTED;
    } else if (stage == WAIT_DISPATCH && callbacks == 1 && !dispatched) {
        dispatched = true;
        if (paired() && !controlled()) stage = WAIT_SETTLED;
        else sendPacket("S"+commandId);
    } else if (stage == WAIT_HELD_INPUT) stage = REQUEST_CONTEXT;
    else if (stage == WAIT_CANCELLED && peer >= 0) sendPacket("S"+commandId);
    else if (stage == WAIT_POST_OVERLAY) {
        require(!game->isState(state) && callbacks == 1);
        request(true); stage = WAIT_FAILED_POST;
    }
}
void beforeFrame(Game *, State *)
{
    if (!running || pendingSetup) return;
    if (stage == WARM && moved && inputs.empty() && idle()) {
        require(owner->getSavedGame()->getSavedBattle()->getSelectedUnit() == player);
        if (++frameCount < 2) return;
        targetPixel = pixelFor(targetTile());
        if (!paired() && adversary() == HELD_Y_REPEAT) { keyEdge(SDLK_y,true); stage = WAIT_HELD_INPUT; }
        else if (!paired() && adversary() == HELD_MOUSE_RELEASE) { const Position p = approvalPixel(true); motion(p.x,p.y); mouseEdge(true); stage = WAIT_HELD_INPUT; }
        else if (paired() && !controlled()) { manualTurn(); stage = WAIT_DISPATCH; }
        else if (!paired() && adversary() == LOAD_TARGET) {
            epoch = savedEpoch; capability = savedCapability; before = snapshot(); command(); stage = WAIT_REJECTED;
        } else if (!paired() && adversary() == MISSING_CONTEXT) {
            capability = randomToken(); before = snapshot(); command(); stage = WAIT_REJECTED;
        } else stage = REQUEST_CONTEXT;
    }
    if (stage == WAIT_SETTLED && inputs.empty() && idle()) {
        // This runs only after native BattlescapeState::think and its ordinary
        // UnitTurnBState timer. Neither the fixture nor controller advances it.
        outcome(); actual = snapshot();
        if (paired()) {
            const bool expectsWarning = !fixture().contact && fixture().expected != fixture().target;
            require(warningVisible() == expectsWarning);
            warningSeen = expectsWarning;
            if (controlled()) { compare(manual,actual); ++parityCount; }
            else manual = actual;
            motion(); stage = REQUEST_POST_CONTEXT;
        } else {
            before = actual;
            if (adversary() == POST_DISPATCH_UNAVAILABLE) { key(Options::keyBattleOptions); stage = WAIT_POST_OVERLAY; }
            else if (adversary() == LEDGER_TURN) { request(false); stage = WAIT_BLOCKED_OBSERVATION; }
            else { if (adversary() == ANIMATION_SURVIVES) require(animationChanged); stage = REQUEST_POST_CONTEXT; }
        }
    }
    if (stage == REQUEST_CONTEXT) { request(tacticalRequest()); stage = WAIT_CONTEXT; }
    else if (stage == REQUEST_POST_CONTEXT && inputs.empty()) {
        // Refusal warnings are real visible outcomes. Witness their appearance
        // above, then wait for the ordinary WarningMessage timer to hide them.
        // Never clear/force-draw a warning, advance a timer, or retry an action.
        if (warningVisible()) return;
        if (paired() && !fixture().contact && fixture().expected != fixture().target) require(warningSeen);
        request(true); stage = WAIT_POST_CONTEXT;
    }
    else if (stage == REQUEST_LEDGER_CONTEXT && inputs.empty()) {
        request(commandKind() != 'R'); stage = WAIT_LEDGER_CONTEXT;
    }
}
void afterFrame(Game *game)
{
    if (!running || pendingSetup) return;
    if (!game->isState(state) && (stage == WAIT_PENDING || stage == CONFIRM || stage == WAIT_BLOCKED_INPUT)) {
        ++modalFrames;
        if (preview && world == 1 && !previewShown) { previewShown = true; previewStarted = Clock::now(); }
    }
    if (stage == WAIT_CONTEXT && available()) { context(receive(),tacticalRequest()); require(!available()); stage = PREPARE_COMMAND; }
    else if (stage == WAIT_REPLACEMENT_VIEW) {
        // A naturally busy frame has already admitted V and completed its poll
        // and presentation without a response. Replace the actual state only
        // now, proving it cannot return a different scene under that challenge.
        require(!available() && !idle() && callbacks == 1);
        replacingPendingView = true; replacement(game);
    }
    else if (stage == WAIT_ANIMATION && display() != contextPixels) animationChanged = true;
    else if (stage == WAIT_POST_CONTEXT && available()) {
        context(receive(),true); require(!available() && callbacks == 1); outcome();
        if (paired()) { ++postCount; finishWorld(); }
        else if (adversary() == ANIMATION_SURVIVES) finishWorld();
        else { before = snapshot(); stage = CHECK_REPLAY; }
    } else if (stage == WAIT_ASYNC_POST_CONTEXT && available()) {
        context(receive(),true); require(!available() && callbacks == 1 && idle());
        outcome(); actual = snapshot(); compare(manual,actual); ++parityCount; ++postCount; finishWorld();
    } else if (stage == WAIT_LEDGER_CONTEXT && available()) {
        context(receive(),commandKind() != 'R'); require(!available());
        commandId = commandPacket.substr(65,32);
        const char cross = commandKind() == 'R' ? (replayStep == 4 ? 'D' : 'N') : 'R';
        sendPacket(packet(cross,commandId,capability)); stage = WAIT_REPLAY;
    } else if (stage == WAIT_FAILED_POST && available()) {
        YAML::Node failure = receive(); fields(failure,{"error"});
        require(failure["error"].as<std::string>() == "unavailable" && callbacks == 1);
        compare(before,snapshot()); finishWorld();
    }
}
void mapClicked(BattlescapeState *who)
{
    if (!running || pendingSetup || who != state) return;
    ++callbacks; ++turnCallbacks; require(callbacks == 1);
    const BattleAction &a = *state->getBattleGame()->getCurrentAction();
    require(a.actor == player && a.target == targetTile() && a.type == BA_NONE && !a.targeting);
    const auto queue = state->getBattleGame()->getStates();
    if (fixture().start != fixture().target) {
        require(queue.size() == 1 && dynamic_cast<UnitTurnBState*>(queue.front()) != 0 &&
            queue.front()->getAction().actor == player && queue.front()->getAction().target == targetTile() &&
            player->getStatus() == STATUS_TURNING && player->getDirection() == fixture().start &&
            player->getTimeUnits() == initialTU);
    } else require(queue.empty());
}
void kneelClicked(BattlescapeState *who)
{
    if (!running || pendingSetup || who != state) return;
    ++callbacks; ++stanceCallbacks; require(callbacks == 1);
}
void selectionClicked(BattlescapeState *who, bool next)
{
    if (!running || pendingSetup || who != state) return;
    ++callbacks; ++selectionCallbacks; require(callbacks == 1 && next);
}
int exitStatus()
{
    if (enabled && preview && previewShown) {
        std::cout << "TURN_CONTROL_GATE PREVIEW {\"schema\":\"turn-control-preview/1\",\"master\":\"" << master <<
            "\",\"modal_presented\":true,\"runtime_gate_pass\":false}" << std::endl;
    }
    if (enabled && completed) {
        require(parityCount == PairCount && postCount == PairWorlds && world == PairWorlds+AdversaryCount);
        std::cout << "TURN_CONTROL_GATE PASS {\"schema\":\"turn-control-gate/1\",\"master\":\"" << master <<
            "\",\"parity_pairs\":15,\"adversarial_worlds\":29,\"ordinary_timer_outcomes\":true,"
            "\"actual_full_view_oracle\":true,\"post_outcome_views\":30,\"first_contact_interrupt\":true,"
            "\"ordinary_door\":true,\"shared_action_ledger\":true,\"animation_grant_survives\":true,"
            "\"local_confirmation\":true,\"timing_equal\":false,\"full_fairness\":false}" << std::endl;
    }
    if (peer >= 0) { close(peer); peer = -1; }
    return enabled && !completed && !(preview && previewShown) ? EXIT_FAILURE : EXIT_SUCCESS;
}
} }
#undef fields
#undef require
