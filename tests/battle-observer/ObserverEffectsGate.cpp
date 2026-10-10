// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
// Bounded same-binary request-effects fixture. All gameplay snapshots stay in
// memory. This file is never compiled into the default build.
#include "ObserverEffectsGate.h"
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
#include "../../src/Observer/PresentedCursor.h"
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

namespace OpenXcom { namespace ObserverEffectsGate {
namespace {
using Clock = std::chrono::steady_clock;
bool enabled = false, running = false, pendingSetup = false, completed = false;
bool initialInput = false, setupCompared = false, phaseArmed = false, capturing = false;
bool thinkClear = false;
unsigned int thinks = 0, animations = 0, handles = 0, responses = 0;
unsigned int acceptedInputs = 0;
int peer = -1, world = 0, phase = 0, initialTU = 0;
BattlescapeState *state = 0;
BattleUnit *player = 0, *hostile = 0;
const Position playerPosition(20, 20, 0), hostilePosition(2, 2, 0);
Clock::time_point worldStart, phaseStart;
std::deque<SDL_Event> inputs;
std::string epoch, admission, challenge;
std::set<std::string> tokens;
struct Snapshot {
    YAML::Node save, runtime;
    uint64_t rng;
};
Snapshot setup;
std::vector<Snapshot> checkpoints;

void check(bool ok, int assertion)
{
    if (!ok) {
        // No state, values, hashes, packets, tokens or private paths in diagnostics.
        std::cerr << "OBSERVER_EFFECTS_GATE FAIL assertion-" << assertion << std::endl;
        std::exit(EXIT_FAILURE);
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
        require(tile->getInventory()->empty() && tile->getParticleCloud()->empty());
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
void silent()
{
    int frequency, channels; Uint16 format;
    require(Options::mute && Mix_QuerySpec(&frequency, &format, &channels) == 0 &&
            !(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO));
}
bool settled(Game *game)
{
    SavedBattleGame *battle = game->getSavedGame()->getSavedBattle();
    BattlescapeGame *logic = state->getBattleGame();
    Map *map = state->getMap();
    const BattleAction &action = *logic->getCurrentAction();
    const Uint8 focus = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
    return game->isState(state) && (SDL_GetAppState() & focus) == focus &&
        battle->getSide() == FACTION_PLAYER && battle->getSelectedUnit() == player &&
        battle->getTurn() == 1 && battle->getDepth() == 0 && !battle->getDebugMode() &&
        logic->getPanicHandled() && !logic->isBusy() && !battle->getUnitsFalling() &&
        battle->getFallingUnits()->empty() && action.type == BA_NONE && !action.targeting &&
        action.waypoints.empty() && action.result.empty() && !action.weapon &&
        battle->getPathfinding()->getPath().empty() && !battle->getPathfinding()->isPathPreviewed() &&
        map->getWaypoints()->empty() && !map->getProjectile() && map->getExplosions()->empty() &&
        !map->getBlastFlash() && map->getCursorType() == CT_NORMAL && !state->getMouseOverIcons() &&
        player->getPosition() == playerPosition && hostile->getPosition() == hostilePosition &&
        player->getTile() == battle->getTile(playerPosition) && player->getTile()->getUnit() == player &&
        hostile->getTile() == battle->getTile(hostilePosition) && hostile->getTile()->getUnit() == hostile &&
        player->getStatus() == STATUS_STANDING && hostile->getStatus() == STATUS_STANDING &&
        player->getHealth() == player->getBaseStats()->health && hostile->getHealth() == hostile->getBaseStats()->health &&
        player->getHealth() > 0 && hostile->getHealth() > 0 && !player->isOut() && !hostile->isOut() &&
        player->getStunlevel() == 0 && hostile->getStunlevel() == 0 && player->getFire() == 0 && hostile->getFire() == 0 &&
        player->getFatalWounds() == 0 && hostile->getFatalWounds() == 0 &&
        player->getMorale() == 100 && hostile->getMorale() == 100 &&
        !hostile->getVisible() && player->getVisibleUnits()->empty() && hostile->getVisibleUnits()->empty() &&
        !player->isFloating() && !hostile->isFloating() &&
        player->getInventory()->empty() && hostile->getInventory()->empty();
}
void queue(SDL_Event event)
{
    inputs.push_back(event);
    require(SDL_PushEvent(&event) == 0);
}
void resetPhase()
{
    phaseStart = Clock::now(); thinks = animations = handles = 0;
    thinkClear = false; phaseArmed = true;
}
} // namespace

void prepare()
{
    const char *option = std::getenv("OPENXCOM_OBSERVER_EFFECTS_GATE");
    enabled = option && std::string(option) == "1";
    if (!enabled) return;
    require(!std::getenv("OPENXCOM_BATTLE_PAIR_GATE") && !std::getenv("OPENXCOM_PRESENTATION_FD") &&
            !std::getenv("OPENXCOM_EXPERIMENTAL_CURSOR_DIR") && !std::getenv("OPENXCOM_EXPERIMENTAL_HUD_DIR"));
    int sockets[2]; require(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets) == 0);
    peer = sockets[0];
    const std::string descriptor = std::to_string(sockets[1]);
    require(setenv("OPENXCOM_PRESENTATION_FD", descriptor.c_str(), 1) == 0 &&
            setenv("OPENXCOM_PRESENTATION_VERSION", "2", 1) == 0);
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
    silent();
    const std::string hello = receive();
    require(hello.size() == 88 && hello.substr(0, 24) == "OPENXCOM-PRESENTATION/2 ");
    epoch = hello.substr(24, 32); admission = hello.substr(56, 32);
    fresh(epoch); fresh(admission); empty();
    // Comparator controls use only invented public constants, never runtime state.
    require(equal(YAML::Load("{a: 1, b: [2, 3]}"), YAML::Load("{b: [2, 3], a: 1}")));
    require(!equal(YAML::Load("{a: [2, 3]}"), YAML::Load("{a: [3, 2]}")));
    require(!equal(YAML::Load("{a: 1}"), YAML::Load("{a: 2}")));
    running = true; pendingSetup = true;
    game->setState(new State);
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    game->getScreen()->resetDisplay(false);
    game->getCursor()->setVisible(true); game->getFpsCounter()->setVisible(false);
    return true;
}
void afterCleanup(Game *game)
{
    if (!running || !pendingSetup) return;
    // The previous BattlescapeState must already have been deleted before replacing
    // its SavedGame. SavedBattleGame destruction unloads map datasets, so reload below.
    pendingSetup = false; phase = 0; initialInput = setupCompared = phaseArmed = capturing = false;
    inputs.clear(); acceptedInputs = 0; worldStart = Clock::now();
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
    player->setPosition(playerPosition); player->setDirection(0);
    hostile->setPosition(hostilePosition); hostile->setDirection(0);
    battle->getUnits()->push_back(player); battle->getUnits()->push_back(hostile);
    battle->resetUnitTiles(); battle->setSelectedUnit(player);
    battle->initUtilities(mod);
    battle->getTileEngine()->calculateSunShading();
    battle->getTileEngine()->calculateTerrainLighting();
    battle->getTileEngine()->calculateUnitLighting();
    battle->getTileEngine()->recalculateFOV();
    initialTU = player->getTimeUnits(); require(initialTU >= 12);
    state = new BattlescapeState;
    battle->setBattleState(state); game->setState(state);
}
void beforeEvents(Game *game)
{
    if (!running || pendingSetup) return;
    require(state && game->isState(state));
    require(Clock::now() - worldStart < std::chrono::seconds(15));
    if (!initialInput) {
        SDL_Event event = {}; event.type = SDL_MOUSEMOTION; event.motion.which = 255;
        event.motion.x = 320; event.motion.y = 160;
        queue(event); initialInput = true;
    } else if (setupCompared && !phaseArmed && inputs.empty()) {
        require(phase == 1 || phase == 2);
        SDL_Event event = {}; event.type = SDL_KEYDOWN; event.key.which = 255;
        event.key.state = SDL_PRESSED; event.key.keysym.sym = Options::keyBattleKneel;
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
    if (!scripted) return false; // The fixture owns its fixed input, not gameplay automation.
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
    if (!running || pendingSetup) return;
    require(game->isState(state));
    if (!initialInput || !inputs.empty()) return;
    if (!setupCompared) {
        require(acceptedInputs == 1 && settled(game));
        const Snapshot now = snapshot(game);
        if (world == 0) setup = now; else compare(setup, now);
        setupCompared = true; resetPhase();
    } else if (!phaseArmed) {
        require(acceptedInputs == unsigned(1 + 2 * phase));
        resetPhase();
    }
}
void thought(BattlescapeState *who, bool clear)
{
    if (running && who == state && phaseArmed) { ++thinks; thinkClear = clear; }
}
void animated(BattlescapeState *who)
{
    if (running && who == state && phaseArmed) ++animations;
}
void handledState(BattlescapeState *who)
{
    if (running && who == state && phaseArmed) ++handles;
}
void beforeFrame(Game *game)
{
    if (!running || pendingSetup) return;
    empty(); silent();
    require(state && game->isState(state) && !capturing);
    if (!setupCompared || !phaseArmed || !thinkClear || !thinks || !animations || !handles ||
        Clock::now() - phaseStart < std::chrono::milliseconds(250)) return;
    require(settled(game));
    const int cost[] = {0, 4, 12};
    require(phase < 3 && player->isKneeled() == (phase == 1) && player->getTimeUnits() == initialTU - cost[phase]);
    if (world == 1) {
        challenge = randomChallenge();
        const std::string request = epoch + admission + challenge;
        require(send(peer, request.data(), request.size(), MSG_DONTWAIT | MSG_NOSIGNAL) == 96);
    }
    capturing = true;
}
void afterFrame(Game *game)
{
    if (!running || pendingSetup || !capturing) return;
    // Game calls this only after Screen::flip returns successfully, including the
    // ordinary producer response for world B. No Screen/producer test hook exists.
    if (world == 1) {
        try {
            const YAML::Node packet = YAML::Load(receive());
            require(packet.IsMap() && packet.size() == 8 &&
                packet["schema"].as<std::string>() == "experimental-presentation/2" &&
                packet["epoch"].as<std::string>() == epoch && packet["challenge"].as<std::string>() == challenge &&
                packet["semantics"].as<std::string>() == "presented_after_request" &&
                packet["profile"].as<std::string>() == "software-single-buffer-8bit-integer-scale-no-bands" &&
                packet["hud"].IsMap() && packet["hud"].size() == 4 && packet["crop"].IsMap());
            for (const char *field : {"time_units", "energy", "health", "morale"})
                require(packet["hud"][field].IsScalar() && packet["hud"][field].as<unsigned int>() <= 999);
            const YAML::Node crop = packet["crop"];
            require(crop.size() == 8 && crop["screen_width"].as<int>() == 640 &&
                crop["screen_height"].as<int>() == 400 && crop["format"].as<std::string>() == "rgb24-hex-row-major");
            const int x = crop["x"].as<int>(), y = crop["y"].as<int>();
            const int w = crop["width"].as<int>(), h = crop["height"].as<int>();
            const std::string pixels = crop["pixels"].as<std::string>();
            require(x >= 0 && y >= 0 && w > 0 && h > 0 && w <= Observer::CursorCropSize && h <= Observer::CursorCropSize &&
                x + w <= 640 && y + h <= 400 && pixels.size() == size_t(w * h * 6) &&
                pixels.find_first_not_of("0123456789abcdef") == std::string::npos);
            admission = packet["next_token"].as<std::string>(); fresh(admission); ++responses;
        } catch (...) { require(false); }
    }
    empty(); silent(); require(settled(game));
    const Snapshot now = snapshot(game);
    if (world == 0) checkpoints.push_back(now); else compare(checkpoints.at(phase), now);
    capturing = false; phaseArmed = false;
    if (++phase == 3) {
        require(acceptedInputs == 5);
        if (++world == 2) {
            require(checkpoints.size() == 3 && responses == 3);
            completed = true; running = false;
            std::cout << "OBSERVER_EFFECTS_GATE PASS {\"schema\":\"observer-effects-gate/1\",\"master\":\""
                << Options::getActiveMaster() << "\",\"worlds\":2,"
                "\"checkpoints_per_world\":3,\"requests\":3,\"engine_rng\":true,\"libc_rng\":\"unverified\","
                "\"audio\":\"subsystem-disabled\",\"timing_equal\":false,\"full_fairness\":false}" << std::endl;
            game->quit();
        } else {
            state = 0; player = hostile = 0; pendingSetup = true;
            game->setState(new State);
        }
    }
}
int exitStatus()
{
    if (peer >= 0) { close(peer); peer = -1; }
    return enabled && !completed ? EXIT_FAILURE : EXIT_SUCCESS;
}
} }
#undef require
