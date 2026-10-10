// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
// Bounded battlefield mixed-output fidelity and within-orientation effects test.
// Gameplay snapshots and PCM remain private in memory; never in public output.
#include "BattlefieldAudioGate.h"
#include "../../src/Battlescape/BattlescapeGame.h"
#include "../../src/Battlescape/BattlescapeState.h"
#include "../../src/Battlescape/Map.h"
#include "../../src/Battlescape/Pathfinding.h"
#include "../../src/Battlescape/TileEngine.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/RNG.h"
#include "../../src/Engine/Screen.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/Cursor.h"
#include "../../src/Interface/FpsCounter.h"
#include "../../src/Mod/Armor.h"
#include "../../src/Mod/MapData.h"
#include "../../src/Mod/MapDataSet.h"
#include "../../src/Mod/Mod.h"
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
#include <unistd.h>
#include <vector>

#include "../../src/Battlescape/Camera.h"
#include "../../src/Battlescape/NextTurnState.h"
#include "../../src/Engine/Sound.h"
#include "../../src/Mod/RuleItem.h"
#include <algorithm>
#include <atomic>
#include <cstring>

extern "C" void __real_Mix_SetPostMix(void (SDLCALL *)(void *, Uint8 *, int), void *);

namespace OpenXcom { namespace BattlefieldAudioGate {
namespace {
using Clock = std::chrono::steady_clock;
bool enabled = false, running = false, pendingSetup = false, completed = false;
bool helloReady = false, initialInput = false, setupCompared = false;
bool pendingAdmission = false, admissionBarrier = false, admitted = false;
bool triggered = false, responseReady = false;
int peer = -1, world = 0;
unsigned int responses = 0, acceptedInputs = 0, cueCount = 0;
BattlescapeState *state = 0;
BattleUnit *player = 0, *hostile = 0;
const Sound *expectedSound = 0;
const Position playerPosition(20, 20, 0), hostilePosition(2, 2, 0), grenadePosition(20, 10, 0);
Clock::time_point worldStart, lastAdmission;
std::deque<SDL_Event> inputs;
std::string epoch, admission, challenge;
std::set<std::string> tokens;
struct Snapshot { YAML::Node save, runtime; uint64_t rng; };
Snapshot setup, endpoint;
// Independent reference state: no producer slot, serializer or capture helper.
enum { ReferenceBytes = 44100, MaximumCallbackBytes = 262144 };
enum Capture { IDLE, SKIP, COLLECT, DONE };
std::atomic<unsigned> capture(IDLE), copied(0);
std::atomic<bool> callbackFailure(false);
unsigned char reference[ReferenceBytes], before[MaximumCallbackBytes];
void (SDLCALL *nativeCallback)(void *, Uint8 *, int) = 0;
void *nativeContext = 0;
void check(bool ok, int assertion)
{
    if (!ok) {
        // No state, values, hashes, packets, tokens or private paths in diagnostics.
        std::cerr << "BATTLEFIELD_AUDIO_GATE FAIL assertion-" << assertion << std::endl;
        std::_Exit(EXIT_FAILURE);
    }
}
// The label is a compile-time source location, never runtime game data.
#define require(ok) check((ok), __LINE__)
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
    // Engine save maps use scalar keys. Compare keys independently of insertion
    // order and preserve sequence order. Never compare emitter text or addresses.
    std::map<std::string, std::pair<YAML::Node, YAML::Node> > left, right;
    for (auto i : a) {
        require(i.first.IsScalar());
        require(left.insert(std::make_pair(i.first.Scalar(), std::make_pair(i.first, i.second))).second);
    }
    for (auto i : b) {
        require(i.first.IsScalar());
        require(right.insert(std::make_pair(i.first.Scalar(), std::make_pair(i.first, i.second))).second);
    }
    for (const auto &i : left) {
        auto found = right.find(i.first);
        if (found == right.end() || !equal(i.second.first, found->second.first) ||
            !equal(i.second.second, found->second.second)) return false;
    }
    return true;
}
void compare(const Snapshot &a, const Snapshot &b)
{
    require(a.rng == b.rng);
    require(equal(a.save, b.save));
    require(equal(a.runtime, b.runtime));
}
int unitId(const BattleUnit *unit)
{
    require(!unit || unit == player || unit == hostile);
    return unit ? unit->getId() : -1;
}
int tileId(SavedBattleGame *battle, const Tile *tile)
{
    if (!tile) return -1;
    require(battle->getTile(tile->getPosition()) == tile);
    return battle->getTileIndex(tile->getPosition());
}
Snapshot snapshot(Game *game)
{
    SavedBattleGame *battle = game->getSavedGame()->getSavedBattle();
    Snapshot result;
    result.rng = RNG::getSeed(); // Read-only: no random draw used as a probe.
    result.save = battle->save(); // Whole semantic save, never emitted or written.
    YAML::Node &n = result.runtime;
    n["selected"] = unitId(battle->getSelectedUnit());
    n["side"] = int(battle->getSide()); n["turn"] = battle->getTurn();
    n["falling"] = battle->getUnitsFalling();
    for (BattleUnit *unit : *battle->getFallingUnits()) n["fallingUnits"].push_back(unitId(unit));
    n["panicHandled"] = state->getBattleGame()->getPanicHandled();
    n["busy"] = state->getBattleGame()->isBusy();
    n["camera"] = state->getMap()->getCamera()->getMapOffset();
    n["viewLevel"] = state->getMap()->getCamera()->getViewLevel();
    n["layers"] = state->getMap()->getCamera()->getShowAllLayers();
    n["blastFlash"] = state->getMap()->getBlastFlash();
    n["cursor"] = int(state->getMap()->getCursorType());
    for (BattleUnit *unit : *battle->getUnits()) {
        YAML::Node u;
        u["stats"] = *unit->getBaseStats();
        u["id"] = unitId(unit); u["tile"] = tileId(battle, unit->getTile());
        u["position"] = unit->getPosition(); u["lastPosition"] = unit->getLastPosition();
        u["destination"] = unit->getDestination(); u["direction"] = unit->getDirection();
        u["faceDirection"] = unit->getFaceDirection(); u["turretDirection"] = unit->getTurretDirection();
        u["turretToDirection"] = unit->getTurretToDirection();
        u["verticalDirection"] = unit->getVerticalDirection(); u["status"] = int(unit->getStatus());
        u["walkingPhase"] = unit->getWalkingPhase(); u["fallingPhase"] = unit->getFallingPhase();
        u["kneeled"] = unit->isKneeled(); u["floating"] = unit->isFloating();
        u["timeUnits"] = unit->getTimeUnits(); u["energy"] = unit->getEnergy();
        u["health"] = unit->getHealth(); u["morale"] = unit->getMorale();
        u["stun"] = unit->getStunlevel(); u["visible"] = unit->getVisible();
        u["faction"] = int(unit->getFaction()); u["originalFaction"] = int(unit->getOriginalFaction());
        u["charging"] = unitId(unit->getCharging()); u["motionPoints"] = unit->getMotionPoints();
        u["turnsSinceSpotted"] = unit->getTurnsSinceSpotted(); u["floorAbove"] = unit->getFloorAbove();
        u["hiding"] = unit->isHiding(); u["activeHand"] = unit->getActiveHand();
        u["reselect"] = unit->reselectAllowed();
        for (BattleUnit *visible : *unit->getVisibleUnits()) u["visibleUnits"].push_back(unitId(visible));
        for (Tile *visible : *unit->getVisibleTiles()) u["visibleTiles"].push_back(tileId(battle, visible));
        for (BattleUnit *spotted : unit->getUnitsSpottedThisTurn()) u["spotted"].push_back(unitId(spotted));
        require(unit->getInventory()->empty() && !unit->getAIModule());
        n["units"].push_back(u);
    }
    for (int i = 0; i < battle->getMapSizeXYZ(); ++i) {
        Tile *tile = battle->getTiles()[i];
        YAML::Node t;
        t["occupant"] = unitId(tile->getUnit()); t["visible"] = tile->getVisible();
        t["shade"] = tile->getShade(); t["explosive"] = tile->getExplosive();
        t["explosiveType"] = tile->getExplosiveType(); t["danger"] = tile->getDangerous();
        t["overlaps"] = tile->getOverlaps(); t["preview"] = tile->getPreview();
        t["tuMarker"] = tile->getTUMarker(); t["markerColor"] = tile->getMarkerColor();
        for (int part = 0; part < 4; ++part) t["obstacle"].push_back(tile->getObstacle(part));
        t["fire"] = tile->getFire(); t["smoke"] = tile->getSmoke();
        t["animationOffset"] = tile->getAnimationOffset();
        for (BattleItem *item : *tile->getInventory()) t["inventory"].push_back(item->getId());
        require(tile->getParticleCloud()->empty());
        n["tiles"].push_back(t);
    }
    const BattleAction &action = *state->getBattleGame()->getCurrentAction();
    YAML::Node a;
    a["type"] = int(action.type); a["actor"] = unitId(action.actor);
    require(!action.weapon); a["weapon"] = -1;
    a["target"] = action.target;
    for (const Position &p : action.waypoints) a["waypoints"].push_back(p);
    a["TU"] = action.TU; a["targeting"] = action.targeting; a["value"] = action.value;
    a["result"] = action.result; a["strafe"] = action.strafe; a["run"] = action.run;
    // All members, including diff, are initialized by this revision's constructor.
    a["diff"] = action.diff; a["autoShotCounter"] = action.autoShotCounter;
    a["cameraPosition"] = action.cameraPosition; a["desperate"] = action.desperate;
    a["finalFacing"] = action.finalFacing; a["finalAction"] = action.finalAction;
    a["number"] = action.number; n["action"] = a;
    require(result.rng == RNG::getSeed()); // Snapshotting must itself be non-consuming.
    return result;
}
void empty()
{
    char byte;
    const ssize_t count = recv(peer, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
    require(count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
}
std::string receive()
{
    char bytes[102401];
    iovec part = {bytes, sizeof(bytes)};
    msghdr message = {};
    message.msg_iov = &part; message.msg_iovlen = 1;
    const ssize_t size = recvmsg(peer, &message, MSG_DONTWAIT);
    require(size > 0 && !(message.msg_flags & MSG_TRUNC));
    return std::string(bytes, size);
}
bool token(const std::string &value)
{
    return value.size() == 32 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}
void fresh(const std::string &value) { require(token(value) && tokens.insert(value).second); }
std::string randomChallenge()
{
    unsigned char bytes[16];
    const int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    require(fd >= 0);
    const ssize_t count = read(fd, bytes, sizeof(bytes)); close(fd);
    require(count == sizeof(bytes));
    const char *hex = "0123456789abcdef";
    std::string result;
    for (unsigned char b : bytes) { result += hex[b >> 4]; result += hex[b & 15]; }
    fresh(result); return result;
}

void audioProfile()
{
    int frequency = 0, channels = 0; Uint16 format = 0; char driver[32] = {};
    const SDL_version *version = Mix_Linked_Version();
    require(!Options::mute && Options::soundVolume == 84 && Options::musicVolume == 84 && Options::uiVolume == 42 &&
        SDL_AudioDriverName(driver, sizeof(driver)) && std::string(driver) == "dummy" &&
        Mix_QuerySpec(&frequency, &format, &channels) > 0 && frequency == 22050 && format == AUDIO_S16LSB && channels == 2 &&
        version && version->major == 1 && version->minor == 2 && version->patch == 12);
    require(!callbackFailure.load(std::memory_order_acquire));
}
void visibility(SavedBattleGame *battle)
{
    unsigned livingPlayers = 0;
    for (BattleUnit *unit : *battle->getUnits())
        if (unit->getFaction() == FACTION_PLAYER && !unit->isOut() && unit->getHealth() > 0) ++livingPlayers;
    require(livingPlayers == 1 && player->getPosition() == playerPosition && player->getDirection() == (world < 2 ? 0 : 4));
    Tile *tile = battle->getTile(grenadePosition);
    require(tile && ((tile->getVisible() > 0) == (world < 2)));
    if (world >= 2) require(!tile->isDiscovered(2));
}
bool settled(Game *game, bool final)
{
    SavedBattleGame *battle = game->getSavedGame()->getSavedBattle();
    BattlescapeGame *logic = state->getBattleGame();
    Map *map = state->getMap();
    const BattleAction &action = *logic->getCurrentAction();
    const Uint8 focus = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
    return (SDL_GetAppState() & focus) == focus &&
        battle->getSide() == (final ? FACTION_HOSTILE : FACTION_PLAYER) &&
        battle->getTurn() == 1 && battle->getDepth() == 0 && !battle->getDebugMode() &&
        !logic->isBusy() && !battle->getUnitsFalling() && battle->getFallingUnits()->empty() &&
        action.type == BA_NONE && !action.targeting && action.waypoints.empty() && action.result.empty() && !action.weapon &&
        battle->getPathfinding()->getPath().empty() && !battle->getPathfinding()->isPathPreviewed() &&
        map->getWaypoints()->empty() && !map->getProjectile() && map->getExplosions()->empty() && !map->getBlastFlash() &&
        player->getPosition() == playerPosition && hostile->getPosition() == hostilePosition &&
        player->getTile() == battle->getTile(playerPosition) && player->getTile()->getUnit() == player &&
        hostile->getTile() == battle->getTile(hostilePosition) && hostile->getTile()->getUnit() == hostile &&
        player->getStatus() == STATUS_STANDING && hostile->getStatus() == STATUS_STANDING &&
        player->getHealth() == player->getBaseStats()->health && hostile->getHealth() == hostile->getBaseStats()->health &&
        player->getHealth() > 0 && hostile->getHealth() > 0 && !player->isOut() && !hostile->isOut() &&
        player->getStunlevel() == 0 && hostile->getStunlevel() == 0 && player->getFire() == 0 && hostile->getFire() == 0 &&
        player->getFatalWounds() == 0 && hostile->getFatalWounds() == 0 &&
        !player->getAIModule() && !hostile->getAIModule() &&
        player->getInventory()->empty() && hostile->getInventory()->empty() &&
        battle->getItems()->size() == (final ? 0u : 1u);
}
void queue(SDL_Event event)
{
    inputs.push_back(event); require(SDL_PushEvent(&event) == 0);
}
void SDLCALL forwardPostmix(void *, Uint8 *stream, int length)
{
    const bool valid = stream && length > 0 && length <= MaximumCallbackBytes && length % 4 == 0;
    if (valid) std::memcpy(before, stream, length);
    // This exact callback/context is the one registered by the native observer.
    nativeCallback(nativeContext, stream, length);
    if (!valid || std::memcmp(before, stream, length) != 0) {
        callbackFailure.store(true, std::memory_order_release); return;
    }
    const unsigned stage = capture.load(std::memory_order_acquire);
    if (stage == SKIP) {
        capture.store(COLLECT, std::memory_order_release); return;
    }
    if (stage != COLLECT) return;
    const unsigned offset = copied.load(std::memory_order_relaxed);
    const unsigned take = std::min(unsigned(length), unsigned(ReferenceBytes) - offset);
    std::memcpy(reference + offset, before, take);
    copied.store(offset + take, std::memory_order_release);
    if (offset + take == ReferenceBytes) capture.store(DONE, std::memory_order_release);
}
void finishWorld(Game *game)
{
    audioProfile(); empty();
    require(capture.load(std::memory_order_acquire) == DONE && cueCount == 1 && acceptedInputs == 3 && inputs.empty());
    require(std::any_of(reference, reference + ReferenceBytes, [](unsigned char byte) { return byte != 0; }));
    require(settled(game, true));
    const Snapshot now = snapshot(game);
    if (world % 2 == 0) endpoint = now; else compare(endpoint, now);
    if (++world == 4) {
        require(responses == 2);
        completed = true; running = false;
        std::cout << "BATTLEFIELD_AUDIO_GATE PASS {\"schema\":\"battlefield-audio-gate/1\",\"master\":\""
            << Options::getActiveMaster() << "\",\"worlds\":4,\"orientation_pairs\":2,\"requests\":2,"
            "\"pcm_oracle\":true,\"cue_witness\":true,\"engine_rng\":true,\"libc_rng\":\"unverified\","
            "\"audio\":\"initialized-normal-volume-dummy\",\"music\":\"normal\",\"timing_equal\":false,\"full_fairness\":false}" << std::endl;
        game->quit();
    } else {
        state = 0; player = hostile = 0; expectedSound = 0; pendingSetup = true;
        game->setState(new State);
    }
}
} // namespace

// SDL_mixer's setter itself acquires the audio lock. Reject, rather than nest,
// any unexpected attach/detach during the single admission barrier.
void installPostmix(void (SDLCALL *callback)(void *, Uint8 *, int), void *context)
{
    if (!enabled) { __real_Mix_SetPostMix(callback, context); return; }
    require(!admissionBarrier);
    if (!callback) {
        __real_Mix_SetPostMix(NULL, NULL); nativeCallback = 0; nativeContext = 0; return;
    }
    require(!nativeCallback);
    nativeCallback = callback; nativeContext = context;
    __real_Mix_SetPostMix(forwardPostmix, NULL);
}
void prepare()
{
    const char *option = std::getenv("OPENXCOM_BATTLEFIELD_AUDIO_GATE");
    enabled = option && std::string(option) == "1";
    if (!enabled) return;
    for (const char *name : {"OPENXCOM_MIXED_AUDIO_FD", "OPENXCOM_PRESENTATION_FD", "OPENXCOM_EXPERIMENTAL_CURSOR_DIR",
        "OPENXCOM_EXPERIMENTAL_HUD_DIR", "OPENXCOM_BATTLE_PAIR_GATE", "OPENXCOM_OBSERVER_EFFECTS_GATE", "OPENXCOM_NATURAL_FOV_GATE"})
        require(!std::getenv(name));
    int sockets[2]; require(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets) == 0);
    peer = sockets[0];
    const std::string descriptor = std::to_string(sockets[1]);
    require(setenv("OPENXCOM_MIXED_AUDIO_FD", descriptor.c_str(), 1) == 0 && setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    Options::displayWidth = 640; Options::displayHeight = 400;
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    Options::fullscreen = false; Options::allowResize = false; Options::asyncBlit = false;
    Options::useOpenGL = false; Options::useScaleFilter = false;
    Options::useHQXFilter = false; Options::useXBRZFilter = false;
    Options::fpsCounter = false; Options::debug = false; Options::debugUi = false;
    Options::traceAI = false; Options::playIntro = false; Options::autosave = false;
    Options::pauseMode = 0; Options::skipNextTurnScreen = false; Options::backgroundMute = false;
    Options::mute = false; Options::soundVolume = 84; Options::musicVolume = 84; Options::uiVolume = 42;
    Options::audioSampleRate = 22050; Options::audioBitDepth = 16; Options::audioChunkSize = 1024;
}
bool loaded(Game *game)
{
    if (!enabled) return false;
    audioProfile();
    require(equal(YAML::Load("{a: 1, b: [2, 3]}"), YAML::Load("{b: [2, 3], a: 1}")));
    require(!equal(YAML::Load("{a: [2, 3]}"), YAML::Load("{a: [3, 2]}")));
    require(!equal(YAML::Load("{a: 1}"), YAML::Load("{a: 2}")));
    running = true; pendingSetup = true; worldStart = Clock::now();
    game->setState(new State);
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    game->getScreen()->resetDisplay(false);
    game->getCursor()->setVisible(true); game->getFpsCounter()->setVisible(false);
    return true;
}
void afterCleanup(Game *game)
{
    if (!running || !pendingSetup || !helloReady) return;
    pendingSetup = false;
    initialInput = setupCompared = pendingAdmission = admitted = triggered = responseReady = false;
    acceptedInputs = cueCount = 0; inputs.clear(); worldStart = Clock::now();
    // World replacement is setup only. Preserve normal soundtrack selection and
    // normal effect volumes; no cue is manufactured or replayed by this fixture.
    Mix_HaltChannel(-1);
    RNG::setSeed(123456789); std::srand(12345); // Setup only, never between checkpoints.
    SavedGame *save = new SavedGame;
    SavedBattleGame *battle = new SavedBattleGame;
    save->setBattleGame(battle); game->setSavedGame(save);
    battle->initMap(40, 40, 1); battle->setDepth(0); battle->setAmbientSound(-1);
    Mod *mod = game->getMod();
    const std::string master = Options::getActiveMaster();
    require((master == "xcom1" || master == "xcom2") && Options::getActiveMods().size() == 1);
    MapDataSet *data = mod->getMapDataSet("BLANKS"); require(data);
    data->loadData(mod->getMCDPatch(data->getName()));
    battle->getMapDataSets()->push_back(data);
    require(data->getSize() > 1);
    MapData *floor = data->getObject(1);
    require(floor && floor == MapDataSet::getScorchedEarthTile() && floor->getObjectType() == O_FLOOR &&
        !floor->isNoFloor() && !floor->isDoor() && !floor->isUFODoor() && !floor->isGravLift() &&
        floor->getTerrainLevel() == 0 && floor->getExplosive() == 0 && floor->getTUCost(MT_WALK) > 0 && floor->getTUCost(MT_WALK) < 255);
    for (int i = 1; i < 8; ++i) require(floor->getSprite(i) == floor->getSprite(0));
    for (int i = 0; i < battle->getMapSizeXYZ(); ++i)
        battle->getTiles()[i]->setMapData(floor, 1, 0, O_FLOOR);
    Base *base = new Base(mod); save->getBases()->push_back(base);
    RuleSoldier *soldierRule = mod->getSoldier("STR_SOLDIER", true);
    Armor *soldierArmor = mod->getArmor(soldierRule->getArmor(), true);
    Soldier *soldier = new Soldier(soldierRule, soldierArmor, 1);
    base->getSoldiers()->push_back(soldier);
    player = new BattleUnit(soldier, 0);
    Unit *alien = mod->getUnit(master == "xcom1" ? "STR_SECTOID_SOLDIER" : "STR_AQUATOID_SOLDIER", true);
    Armor *alienArmor = mod->getArmor(alien->getArmor(), true);
    hostile = new BattleUnit(alien, FACTION_HOSTILE, BattleUnit::MAX_SOLDIER_ID, alienArmor, mod->getStatAdjustment(0), 0);
    require(soldierArmor->getSize() == 1 && alienArmor->getSize() == 1 &&
        !soldierArmor->getConstantAnimation() && !alienArmor->getConstantAnimation() &&
        player->getMovementType() == MT_WALK && hostile->getMovementType() == MT_WALK &&
        player->getGeoscapeSoldier() == soldier && save->getSoldier(1) == soldier && player->getType() == "SOLDIER");
    player->setPosition(playerPosition); player->setDirection(world < 2 ? 0 : 4);
    hostile->setPosition(hostilePosition); hostile->setDirection(0);
    battle->getUnits()->push_back(player); battle->getUnits()->push_back(hostile);
    battle->resetUnitTiles(); battle->setSelectedUnit(player);
    battle->initUtilities(mod);
    battle->getTileEngine()->calculateSunShading();
    battle->getTileEngine()->calculateTerrainLighting();
    battle->getTileEngine()->calculateUnitLighting();
    battle->getTileEngine()->recalculateFOV();
    RuleItem *rule = mod->getItem(master == "xcom1" ? "STR_GRENADE" : "STR_MAGNA_BLAST_GRENADE", true);
    require(rule->getBattleType() == BT_GRENADE && rule->getPower() == 50 && rule->getExplosionRadius() == 5);
    BattleItem *grenade = new BattleItem(rule, battle->getCurrentItemId());
    grenade->setFuseTimer(0);
    battle->getItems()->push_back(grenade);
    battle->getTile(grenadePosition)->addItem(grenade, mod->getInventory("STR_GROUND", true));
    expectedSound = mod->getSoundByDepth(0, Mod::SMALL_EXPLOSION);
    require(expectedSound && grenade->getTile() == battle->getTile(grenadePosition));
    visibility(battle);
    state = new BattlescapeState;
    battle->setBattleState(state); game->setState(state);
}

void beforeEvents(Game *game)
{
    if (!running) return;
    require(Clock::now() - worldStart < std::chrono::seconds(20));
    if (pendingSetup) return;
    audioProfile();
    if (!initialInput) {
        SDL_Event event = {}; event.type = SDL_MOUSEMOTION; event.motion.which = 255;
        event.motion.x = 320; event.motion.y = 160; queue(event); initialInput = true;
    } else if (setupCompared && admitted && !triggered && capture.load(std::memory_order_acquire) == COLLECT) {
        require(copied.load(std::memory_order_acquire) < ReferenceBytes / 4);
        require(game->isState(state) && settled(game, false));
        visibility(game->getSavedGame()->getSavedBattle());
        triggered = true;
        SDL_Event event = {}; event.type = SDL_KEYDOWN; event.key.which = 255;
        event.key.state = SDL_PRESSED; event.key.keysym.sym = Options::keyBattleEndTurn;
        event.key.keysym.mod = KMOD_NONE; queue(event);
        event.type = SDL_KEYUP; event.key.state = SDL_RELEASED; queue(event);
    }
}
bool acceptEvent(const SDL_Event &event)
{
    if (!running) return true;
    if (event.type == SDL_QUIT || event.type == SDL_ACTIVEEVENT) return true;
    const bool scripted = (event.type == SDL_MOUSEMOTION && event.motion.which == 255) ||
        ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && event.key.which == 255);
    if (!scripted) return false;
    require(!inputs.empty() && event.type == inputs.front().type);
    const SDL_Event &expected = inputs.front();
    if (event.type == SDL_MOUSEMOTION)
        require(event.motion.x == expected.motion.x && event.motion.y == expected.motion.y);
    else require(event.key.state == expected.key.state && event.key.keysym.sym == expected.key.keysym.sym &&
                 event.key.keysym.mod == expected.key.keysym.mod);
    inputs.pop_front(); ++acceptedInputs; return true;
}
void afterEvents(Game *game)
{
    if (!running || pendingSetup || !initialInput || !inputs.empty() || setupCompared) return;
    if (Clock::now() - worldStart < std::chrono::milliseconds(250)) return;
    require(game->isState(state) && settled(game, false) && acceptedInputs == 1);
    visibility(game->getSavedGame()->getSavedBattle());
    if (world > 0 && Clock::now() - lastAdmission < std::chrono::seconds(1)) return;
    const Snapshot now = snapshot(game);
    if (world % 2 == 0) setup = now; else compare(setup, now);
    setupCompared = true;
    empty();
    // Both twins establish the same reference capture through the next ordinary
    // game-loop poll. Only the request twin sends an observer request.
    require(world == 0 || Clock::now() - lastAdmission >= std::chrono::seconds(1));
    if (world % 2) {
        challenge = randomChallenge();
        const std::string request = epoch + admission + challenge;
        require(send(peer, request.data(), request.size(), MSG_DONTWAIT | MSG_NOSIGNAL) == 96);
    }
    pendingAdmission = true;
}
void beforePoll()
{
    if (!running || !pendingAdmission) return;
    require(helloReady && nativeCallback && !admissionBarrier);
    SDL_LockAudio(); admissionBarrier = true;
    copied.store(0, std::memory_order_relaxed);
    capture.store(SKIP, std::memory_order_release);
}
void afterPoll(Game *)
{
    if (admissionBarrier) {
        admissionBarrier = false; SDL_UnlockAudio();
        pendingAdmission = false; admitted = true; lastAdmission = Clock::now();
    }
    if (!running) return;
    audioProfile();
    char byte;
    const ssize_t available = recv(peer, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
    if (available < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    require(available > 0);
    if (!helloReady) {
        const std::string hello = receive();
        const std::string prefix = "OPENXCOM-MIXED-AUDIO/1 ";
        require(hello.size() == prefix.size() + 64 && hello.substr(0, prefix.size()) == prefix);
        epoch = hello.substr(prefix.size(), 32); admission = hello.substr(prefix.size() + 32, 32);
        fresh(epoch); fresh(admission); helloReady = true; empty(); return;
    }
    require(world % 2 && admitted && !responseReady && capture.load(std::memory_order_acquire) == DONE);
    try {
        const YAML::Node packet = YAML::Load(receive());
        const std::set<std::string> fields = {"schema", "epoch", "challenge", "next_token", "semantics", "profile",
            "duration_ms", "sample_rate", "channels", "sample_format", "frames", "pcm_hex"};
        require(packet.IsMap() && packet.size() == fields.size());
        for (auto entry : packet) require(entry.first.IsScalar() && fields.count(entry.first.as<std::string>()) == 1);
        require(packet["schema"].as<std::string>() == "experimental-mixed-audio/1" && packet["epoch"].as<std::string>() == epoch &&
            packet["challenge"].as<std::string>() == challenge && packet["semantics"].as<std::string>() == "post_admission_mixed_output" &&
            packet["profile"].as<std::string>() == "sdl-mixer-postmix-s16le-stereo-22050" &&
            packet["duration_ms"].as<int>() == 500 && packet["sample_rate"].as<int>() == 22050 &&
            packet["channels"].as<int>() == 2 && packet["sample_format"].as<std::string>() == "s16le" && packet["frames"].as<int>() == 11025);
        const std::string pcm = packet["pcm_hex"].as<std::string>();
        require(pcm.size() == ReferenceBytes * 2 && pcm.find_first_not_of("0123456789abcdef") == std::string::npos);
        const std::string hex = "0123456789abcdef";
        for (unsigned i = 0; i < ReferenceBytes; ++i) {
            const unsigned actualByte = unsigned(hex.find(pcm[2*i]) * 16 + hex.find(pcm[2*i+1]));
            require(actualByte == reference[i]);
        }
        admission = packet["next_token"].as<std::string>(); fresh(admission);
        ++responses; responseReady = true;
    } catch (...) { require(false); }
    empty();
}
void played(const Sound *sound, int channel)
{
    if (!running || pendingSetup || sound != expectedSound) return;
    require(triggered && admitted && capture.load(std::memory_order_acquire) == COLLECT &&
        copied.load(std::memory_order_acquire) < ReferenceBytes / 2);
    require(channel >= 0 && Mix_Playing(channel) > 0 && Mix_Volume(channel, -1) > 0 && ++cueCount == 1);
    visibility(state->getGame()->getSavedGame()->getSavedBattle());
}
void beforeFrame(Game *game, State *top)
{
    if (!running || pendingSetup || !triggered) return;
    if (!dynamic_cast<NextTurnState *>(top)) return;
    require(game->isState(top) && !Options::skipNextTurnScreen);
    if (capture.load(std::memory_order_acquire) != DONE || (world % 2 && !responseReady)) return;
    finishWorld(game);
}
int exitStatus()
{
    if (peer >= 0) { close(peer); peer = -1; }
    return enabled && !completed ? EXIT_FAILURE : EXIT_SUCCESS;
}
} }
extern "C" void __wrap_Mix_SetPostMix(void (SDLCALL *callback)(void *, Uint8 *, int), void *context)
{
    OpenXcom::BattlefieldAudioGate::installPostmix(callback, context);
}
#undef require
