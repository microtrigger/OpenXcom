// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
// Six normal-input FOV/pause presentations. Private oracle data stays in memory.
// Never compiled into the default build.
#include "NaturalFovGate.h"
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
#include "../../src/Battlescape/Camera.h"
#include "../../src/Menu/PauseState.h"
#include "../../src/Mod/RuleInterface.h"
#include "../../src/Engine/SurfaceSet.h"
#include <algorithm>
#include <climits>
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

namespace OpenXcom { namespace NaturalFovGate {
namespace {
using Clock = std::chrono::steady_clock;
bool enabled = false, running = false, pendingSetup = false, completed = false;
bool actionQueued = false, hoverQueued = false, phaseArmed = false, capturing = false;
bool thinkClear = false;
unsigned int thinks = 0, animations = 0, handles = 0, responses = 0, initializations = 0;
unsigned int acceptedInputs = 0;
int peer = -1, phase = 0, initialTU = 0;
BattlescapeState *state = 0;
BattleUnit *player = 0, *hostile = 0;
const Position playerPosition(20, 20, 0), hostilePosition(22, 20, 0);
Position hoverPixel, cameraOffset;
Clock::time_point worldStart, phaseStart;
std::deque<SDL_Event> inputs;
std::string epoch, admission, challenge, master;
std::set<std::string> tokens;
std::vector<unsigned char> initialCrop, beforeOverlayCrop;
struct HudRegion { int x, y, width, height, ink; };
HudRegion hudRegions[4];
void check(bool ok, int assertion)
{
    if (!ok) {
        // No state, values, hashes, packets, tokens or private paths in diagnostics.
        std::cerr << "NATURAL_FOV_GATE FAIL assertion-" << assertion << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
// The label is a compile-time source location, never runtime game data.
#define require(ok) check((ok), __LINE__)
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
    int frequency, channels; Uint16 format; char driver[64] = {};
    require(Options::mute && Mix_QuerySpec(&frequency, &format, &channels) > 0 &&
        (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) &&
        SDL_AudioDriverName(driver, sizeof(driver)) && std::string(driver) == "dummy" &&
        Mix_Playing(-1) == 0 && Mix_PlayingMusic() == 0);
}
bool settled(Game *game)
{
    SavedBattleGame *battle = game->getSavedGame()->getSavedBattle();
    BattlescapeGame *logic = state->getBattleGame();
    Map *map = state->getMap();
    const BattleAction &action = *logic->getCurrentAction();
    const Uint8 focus = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
    return (SDL_GetAppState() & focus) == focus &&
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
        hostile->getVisibleUnits()->empty() &&
        !player->isFloating() && !hostile->isFloating() &&
        player->getInventory()->empty() && hostile->getInventory()->empty() &&
        !player->getAIModule() && !hostile->getAIModule() && !player->isKneeled() &&
        hostile->getDirection() == 0;
}
bool expected(Game *game)
{
    const int direction[] = {0, 1, 0, 1, 1, 1};
    const int cost[] = {0, 1, 2, 3, 3, 3};
    const bool inFov[] = {false, true, false, true, true, true};
    const std::vector<BattleUnit*> &visible = *player->getVisibleUnits();
    return phase < 6 && settled(game) && player->getDirection() == direction[phase] &&
        player->getTimeUnits() == initialTU - cost[phase] &&
        visible.size() == (inFov[phase] ? 1u : 0u) &&
        (!inFov[phase] || visible.front() == hostile) && hostile->getVisible() == (phase != 0);

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
Position pixelFor(const Position &tile)
{
    Position pixel;
    state->getMap()->getCamera()->convertMapToScreen(tile, &pixel);
    pixel += state->getMap()->getCamera()->getMapOffset();
    pixel.x += 16; pixel.y += 24;
    require(pixel.x > 20 && pixel.x < 300 && pixel.y > 20 && pixel.y < 140);
    return pixel;
}
void motion(const Position &tile)
{
    const Position pixel = pixelFor(tile);
    SDL_Event event = {}; event.type = SDL_MOUSEMOTION; event.motion.which = 255;
    event.motion.x = pixel.x * 2; event.motion.y = pixel.y * 2; queue(event);
}
void turn(const Position &tile)
{
    motion(tile);
    const Position pixel = pixelFor(tile);
    SDL_Event event = {}; event.type = SDL_MOUSEBUTTONDOWN; event.button.which = 255;
    event.button.button = SDL_BUTTON_RIGHT; event.button.state = SDL_PRESSED;
    event.button.x = pixel.x * 2; event.button.y = pixel.y * 2; queue(event);
    event.type = SDL_MOUSEBUTTONUP; event.button.state = SDL_RELEASED; queue(event);
}
void key(SDLKey code)
{
    SDL_Event event = {}; event.type = SDL_KEYDOWN; event.key.which = 255;
    event.key.state = SDL_PRESSED; event.key.keysym.sym = code;
    event.key.keysym.mod = KMOD_NONE; queue(event);
    event.type = SDL_KEYUP; event.key.state = SDL_RELEASED; queue(event);
}
void regionLayout(Mod *mod)
{
    // Independently follow constructor defaults and State::add rule overrides.
    // No widget value, producer HudFrame, or observer decoder is consulted.
    RuleInterface *rules = mod->getInterface("battlescape"); require(rules);
    Element *icons = rules->getElement("icons"); require(icons);
    const int px = 160 - icons->w / 2, py = 200 - icons->h;
    const char *names[] = {"numTUs", "numEnergy", "numHealth", "numMorale"};
    const int xs[] = {136, 154, 136, 154}, ys[] = {42, 42, 50, 50};
    for (int i = 0; i < 4; ++i) {
        HudRegion d = {px + xs[i], py + ys[i], 15, 5, 1};
        Element *rule = rules->getElement(names[i]);
        if (rule) {
            if (rule->x != INT_MAX && rule->y != INT_MAX) { d.x = px + rule->x; d.y = py + rule->y; }
            if (rule->w != INT_MAX && rule->h != INT_MAX) { d.width = rule->w; d.height = rule->h; }
            if (rule->color != INT_MAX) d.ink = static_cast<unsigned char>(rule->color + 1);
        }
        require(d.x >= 0 && d.y >= 0 && d.width >= 15 && d.width <= 64 && d.height == 5 &&
            d.x + d.width <= 320 && d.y + d.height <= 200 && d.ink > 0 && d.ink < 256);
        hudRegions[i] = d;
    }
}
struct Frame {
    std::vector<unsigned char> rgb;
    SDL_Color palette[256];
};
Frame presented()
{
    // This is the final physical display AFTER SDL_Flip, not Screen's logical surface.
    SDL_Surface *display = SDL_GetVideoSurface();
    require(display && display->w == 640 && display->h == 400 && display->format &&
        display->format->BitsPerPixel == 8 && display->format->BytesPerPixel == 1 &&
        display->format->palette && display->format->palette->ncolors == 256 &&
        !(display->flags & (SDL_OPENGL | SDL_HWSURFACE | SDL_DOUBLEBUF)));
    Frame result; result.rgb.resize(640 * 400 * 3);
    require(SDL_LockSurface(display) == 0);
    std::copy(display->format->palette->colors, display->format->palette->colors + 256, result.palette);
    for (int y = 0; y < 400; ++y) {
        const unsigned char *row = static_cast<const unsigned char*>(display->pixels) + y * display->pitch;
        for (int x = 0; x < 640; ++x) {
            const SDL_Color &c = result.palette[row[x]];
            const int at = (y * 640 + x) * 3;
            result.rgb[at] = c.r; result.rgb[at + 1] = c.g; result.rgb[at + 2] = c.b;
        }
    }
    SDL_UnlockSurface(display); return result;
}
void fields(const YAML::Node &node, std::initializer_list<const char*> names)
{
    require(node.IsMap() && node.size() == names.size());
    std::set<std::string> remaining;
    for (const char *name : names) remaining.insert(name);
    for (auto entry : node) require(entry.first.IsScalar() && remaining.erase(entry.first.Scalar()) == 1);
    require(remaining.empty());
}
std::vector<unsigned char> cropOracle(const Frame &frame, Game *game, const YAML::Node &crop)
{
    fields(crop, {"screen_width", "screen_height", "x", "y", "width", "height", "format", "pixels"});
    const int cx = game->getCursor()->getX() * 2, cy = game->getCursor()->getY() * 2;
    require(cx >= 0 && cx < 640 && cy >= 0 && cy < 400);
    const int left = std::max(0, cx - 64), top = std::max(0, cy - 64);
    const int right = std::min(640, cx + 64), bottom = std::min(400, cy + 64);
    require(crop["screen_width"].as<int>() == 640 && crop["screen_height"].as<int>() == 400 &&
        crop["x"].as<int>() == left && crop["y"].as<int>() == top &&
        crop["width"].as<int>() == right - left && crop["height"].as<int>() == bottom - top &&
        crop["format"].as<std::string>() == "rgb24-hex-row-major");
    const std::string hex = crop["pixels"].as<std::string>();
    require(hex.size() == size_t((right - left) * (bottom - top) * 6) &&
        hex.find_first_not_of("0123456789abcdef") == std::string::npos);
    const std::string alphabet = "0123456789abcdef";
    std::vector<unsigned char> pixels;
    size_t pos = 0;
    for (int y = top; y < bottom; ++y) for (int x = left; x < right; ++x) for (int c = 0; c < 3; ++c) {
        const unsigned char expectedByte = frame.rgb[(y * 640 + x) * 3 + c];
        const unsigned char actualByte = alphabet.find(hex[pos]) * 16 + alphabet.find(hex[pos + 1]);
        require(actualByte == expectedByte); // Independent pixel-mutation assertion.
        pos += 2; pixels.push_back(expectedByte);
    }
    return pixels;
}
unsigned int hudOracle(const Frame &frame, const HudRegion &d)
{
    // Row bitmasks adapted directly from NumberText.cpp (OpenXcom Developers,
    // Copyright 2010-2016; GPL-3.0-or-later). This independent exhaustive-template
    // matcher neither calls nor shares control flow with the producer decoder.
    static const unsigned char rows[10][5] = {
        {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
        {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7}
    };
    std::vector<unsigned char> observed(d.width * 5);
    const SDL_Color &ink = frame.palette[d.ink];
    for (int y = 0; y < 5; ++y) for (int x = 0; x < d.width; ++x) {
        int count = 0;
        for (int sy = 0; sy < 2; ++sy) for (int sx = 0; sx < 2; ++sx) {
            const size_t p = (((d.y + y) * 2 + sy) * 640 + (d.x + x) * 2 + sx) * 3;
            count += frame.rgb[p] == ink.r && frame.rgb[p + 1] == ink.g && frame.rgb[p + 2] == ink.b;
        }
        require(count == 0 || count == 4); observed[y * d.width + x] = count == 4;
    }
    int value = -1;
    for (unsigned int candidate = 0; candidate <= 999; ++candidate) {
        std::vector<unsigned char> pattern(d.width * 5, 0);
        const std::string digits = std::to_string(candidate);
        for (size_t digit = 0; digit < digits.size(); ++digit)
            for (int y = 0; y < 5; ++y) for (int x = 0; x < 3; ++x)
                pattern[y * d.width + digit * 4 + x] = (rows[digits[digit] - '0'][y] >> (2 - x)) & 1;
        if (observed == pattern) { require(value == -1); value = candidate; }
    }
    require(value >= 0); return value;
}
} // namespace

void prepare()
{
    const char *option = std::getenv("OPENXCOM_NATURAL_FOV_GATE");
    enabled = option && std::string(option) == "1";
    if (!enabled) return;
    require(setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    require(!std::getenv("OPENXCOM_BATTLE_PAIR_GATE") && !std::getenv("OPENXCOM_OBSERVER_EFFECTS_GATE") && !std::getenv("OPENXCOM_PRESENTATION_FD") &&
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
    Options::pauseMode = 0; Options::battleDragScrollButton = SDL_BUTTON_MIDDLE; Options::strafe = false;
}
void silence()
{
    if (!enabled) return;
    // Keep normal popup callbacks safe: Window asks the mixer for a channel
    // before Sound::play checks mute. Dummy output remains initialized.
    Mix_HaltChannel(-1); Mix_HaltMusic();
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
    pendingSetup = false; phase = 0; actionQueued = hoverQueued = phaseArmed = capturing = false;
    inputs.clear(); acceptedInputs = 0; worldStart = Clock::now();
    RNG::setSeed(123456789); std::srand(12345); // Setup only, never between checkpoints.
    SavedGame *save = new SavedGame;
    SavedBattleGame *battle = new SavedBattleGame;
    save->setBattleGame(battle); game->setSavedGame(save);
    battle->initMap(40, 40, 1); battle->setDepth(0); battle->setAmbientSound(-1);
    Mod *mod = game->getMod();
    master = Options::getActiveMaster();
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
    require(mod->getSurfaceSet("BLANKS.PCK")->getFrame(0)->getWidth() == 32 &&
        mod->getSurfaceSet("BLANKS.PCK")->getFrame(0)->getHeight() == 40);
    regionLayout(mod);
    state = new BattlescapeState;
    battle->setBattleState(state); game->setState(state);
}
void initialized(BattlescapeState *who)
{
    if (running && who == state) ++initializations;
}
void beforeEvents(Game *game)
{
    if (!running || pendingSetup) return;
    require(state && Clock::now() - worldStart < std::chrono::seconds(20));
    require(SDL_GetModState() == KMOD_NONE);
    if (!actionQueued) {
        if (phase == 0) {
            cameraOffset = state->getMap()->getCamera()->getMapOffset();
            hoverPixel = pixelFor(hostilePosition); motion(hostilePosition); hoverQueued = true;
        } else if (phase <= 3) turn(phase == 2 ? Position(20, 18, 0) : Position(22, 18, 0));
        else if (phase == 4) { key(Options::keyBattleOptions); hoverQueued = true; }
        else { require(phase == 5); key(Options::keyCancel); }
        actionQueued = true;
    } else if (!hoverQueued && inputs.empty() && game->isState(state) && expected(game)) {
        motion(hostilePosition); hoverQueued = true; phaseArmed = false;
    }
}
bool acceptEvent(const SDL_Event &event)
{
    if (!running) return true;
    if (event.type == SDL_QUIT || event.type == SDL_ACTIVEEVENT) return true;
    const bool scripted = (event.type == SDL_MOUSEMOTION && event.motion.which == 255) ||
        ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.which == 255) ||
        ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && event.key.which == 255);
    if (!scripted) return false;
    require(!inputs.empty() && event.type == inputs.front().type);
    const SDL_Event &next = inputs.front();
    if (event.type == SDL_MOUSEMOTION) {
        require(event.motion.x == next.motion.x && event.motion.y == next.motion.y && event.motion.state == 0);
    } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        require(event.button.x == next.button.x && event.button.y == next.button.y &&
            event.button.button == next.button.button && event.button.state == next.button.state);
        Position selector; state->getMap()->getSelectorPosition(&selector);
        require(selector == (phase == 2 ? Position(20, 18, 0) : Position(22, 18, 0)));
    } else require(event.key.state == next.key.state && event.key.keysym.sym == next.key.keysym.sym &&
        event.key.keysym.mod == KMOD_NONE);
    inputs.pop_front(); ++acceptedInputs; return true;
}
void afterEvents(Game *)
{
    if (running && !pendingSetup && actionQueued && hoverQueued && inputs.empty() && !phaseArmed) resetPhase();
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
void beforeFrame(Game *game, State *top)
{
    if (!running || pendingSetup) return;
    empty(); silent(); require(!capturing);
    if (!phaseArmed || Clock::now() - phaseStart < std::chrono::milliseconds(250)) return;
    if (phase != 4 && (!thinkClear || !thinks || !animations || !handles)) return;
    require(expected(game));
    require(phase == 4 ? dynamic_cast<PauseState*>(top) != 0 : top == state);
    require(initializations == (phase == 5 ? 2u : 1u));
    if (phase == 4) require(!thinks && !animations && !handles);
    Position selected; state->getMap()->getSelectorPosition(&selected);
    require(selected == hostilePosition && state->getMap()->getCamera()->getViewLevel() == 0 &&
        state->getMap()->getCamera()->getMapOffset() == cameraOffset &&
        game->getCursor()->getVisible() && game->getCursor()->getX() == hoverPixel.x &&
        game->getCursor()->getY() == hoverPixel.y &&
        game->getScreen()->getXScale() == 2.0 && game->getScreen()->getYScale() == 2.0);
    const unsigned int inputCounts[] = {1, 5, 9, 13, 15, 18};
    require(acceptedInputs == inputCounts[phase]);
    challenge = randomChallenge();
    const std::string request = epoch + admission + challenge;
    require(send(peer, request.data(), request.size(), MSG_DONTWAIT | MSG_NOSIGNAL) == 96);
    capturing = true;
}
void afterFrame(Game *game)
{
    if (!running || pendingSetup || !capturing) return;
    const Frame frame = presented();
    try {
        const YAML::Node packet = YAML::Load(receive());
        fields(packet, {"schema", "epoch", "challenge", "next_token", "semantics", "profile", "hud", "crop"});
        require(packet["schema"].as<std::string>() == "experimental-presentation/2" &&
            packet["epoch"].as<std::string>() == epoch && packet["challenge"].as<std::string>() == challenge &&
            packet["semantics"].as<std::string>() == "presented_after_request" &&
            packet["profile"].as<std::string>() == "software-single-buffer-8bit-integer-scale-no-bands");
        const std::vector<unsigned char> pixels = cropOracle(frame, game, packet["crop"]);
        if (phase == 0) initialCrop = pixels;
        if (phase == 1) require(pixels != initialCrop); // Presentation change, not isolated enemy-sprite attribution.
        if (phase == 3) beforeOverlayCrop = pixels;
        if (phase == 4) {
            require(packet["hud"].IsNull()); // Overlay-availability mutation assertion.
            require(pixels != beforeOverlayCrop);
        } else {
            fields(packet["hud"], {"time_units", "energy", "health", "morale"});
            const char *names[] = {"time_units", "energy", "health", "morale"};
            for (int i = 0; i < 4; ++i) {
                const unsigned int visibleValue = hudOracle(frame, hudRegions[i]);
                require(packet["hud"][names[i]].as<unsigned int>() == visibleValue); // HUD-value mutation assertion.
            }
        }
        admission = packet["next_token"].as<std::string>(); fresh(admission); ++responses;
    } catch (...) { require(false); }
    empty(); silent(); require(expected(game));
    capturing = phaseArmed = actionQueued = hoverQueued = false;
    if (++phase == 6) { require(responses == 6); completed = true; running = false; game->quit(); }
}
int exitStatus()
{
    if (enabled && completed) {
        // Called only after Game destruction closes the producer endpoint.
        char byte; require(recv(peer, &byte, 1, MSG_DONTWAIT) == 0);
        std::cout << "NATURAL_FOV_GATE PASS {\"schema\":\"natural-fov-gate/1\",\"master\":\"" << master <<
            "\",\"samples\":6,\"requests\":6,\"pixel_oracle\":true,\"hud_oracle\":true,"
            "\"pause_recovery\":true,\"audio\":\"initialized-muted-dummy\",\"timing_equal\":false,\"full_fairness\":false}" << std::endl;
    }
    if (peer >= 0) { close(peer); peer = -1; }
    return enabled && !completed ? EXIT_FAILURE : EXIT_SUCCESS;
}
} }
#undef require
