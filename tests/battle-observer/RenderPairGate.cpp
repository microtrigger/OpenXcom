// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
// Isolated renderer fixture. No simulation advancement, gameplay controller,
// external test-control channel, hidden-state dump, save loading or resource export.
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
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace OpenXcom { namespace RenderPairGate {
namespace {
bool enabled = false, running = false, pendingSetup = false, completed = false;
bool recording = false;
unsigned int visits = 0;
int peer = -1, world = 0, phase = 0;
BattlescapeState *state = 0;
BattleUnit *occupant = 0;
const Position hover(20, 22, 0), playerPosition(20, 20, 0), parked(2, 2, 0);
std::string epoch, admission, challenge;
std::set<std::string> randomTokens;
struct Frame {
    int width = 0, height = 0;
    std::vector<unsigned char> rgb;
    std::string eligibility, crop;
    std::vector<std::string> packets;
};
std::vector<Frame> histories[3];

void require(bool condition, const char *reason)
{
    if (!condition) {
        // Reasons are fixed aggregate assertion labels, never hidden values.
        std::cerr << "BATTLE_PAIR_GATE FAIL " << reason << std::endl;
        std::exit(EXIT_FAILURE);
    }
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
std::string receive()
{
    char bytes[102401];
    iovec part = {bytes, sizeof(bytes)};
    msghdr message = {};
    message.msg_iov = &part; message.msg_iovlen = 1;
    const ssize_t size = recvmsg(peer, &message, MSG_DONTWAIT);
    require(size > 0 && !(message.msg_flags & MSG_TRUNC), "producer-response");
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
std::string eligibility(const Observer::HudFrame &hud, const Observer::PixelCrop &crop)
{
    std::ostringstream out;
    out << crop.screenWidth << ',' << crop.screenHeight << ',' << crop.x << ',' << crop.y
        << ',' << crop.width << ',' << crop.height;
    for (const Observer::HudDigits &d : hud)
        out << ';' << d.x << ',' << d.y << ',' << d.width << ',' << d.height << ',' << int(d.foreground);
    return out.str();
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
std::vector<std::string> transcript(const std::vector<Frame> &history)
{
    Bijection map;
    std::vector<std::string> result;
    for (const Frame &frame : history)
        for (const std::string &packet : frame.packets) result.push_back(map.packet(packet));
    return result;
}
void verify()
{
    require(histories[0].size() == 8 && histories[1].size() == 8 && histories[2].size() == 8,
            "complete-eight-phase-histories");
    // Establish the complete visual and eligibility oracle BEFORE comparing packets.
    for (size_t i = 0; i < 8; ++i) {
        const Frame &a = histories[0][i], &b = histories[1][i], &positive = histories[2][i];
        require(a.width == b.width && a.height == b.height && a.rgb == b.rgb,
                "hidden-empty-final-rgb");
        require(a.eligibility == b.eligibility, "hidden-empty-eligibility");
        if (i % 2) {
            require(a.rgb != positive.rgb && a.crop != positive.crop, "visible-crop-positive-control");
        }
    }
    const std::vector<std::string> a = transcript(histories[0]), b = transcript(histories[1]);
    require(a.size() == 16 && a == b, "hidden-empty-public-transcript");
    // Comparator controls change real recorded output; these are not engine mutants.
    std::vector<Frame> changed = histories[1];
    std::string &mutated = changed[0].packets[1];
    const std::string field = "\"time_units\":";
    const size_t found = mutated.find(field);
    require(found != std::string::npos, "transcript-control-field");
    const size_t number = found + field.size();
    require(number < mutated.size() && mutated[number] >= '0' && mutated[number] <= '9', "transcript-control-number");
    mutated[number] = mutated[number] == '1' ? '2' : '1';
    require(transcript(changed) != a, "transcript-value-sensitivity");
    changed = histories[1]; changed[0].rgb[0] ^= 1;
    require(changed[0].rgb != histories[0][0].rgb, "full-frame-pixel-sensitivity");
    completed = true;
    std::cout << "BATTLE_PAIR_GATE PASS {\"schema\":\"render-pair-gate/1\",\"master\":\""
              << Options::getActiveMaster() << "\",\"scenarios\":3,"
              << "\"frames_per_scenario\":8,\"equal_pair_frames\":8,\"responses\":24,"
              << "\"positive_odd_phases\":4,\"audio\":\"subsystem-disabled\","
              << "\"simulation\":false,\"full_fairness\":false}" << std::endl;
}
} // namespace

void prepare()
{
    const char *option = std::getenv("OPENXCOM_BATTLE_PAIR_GATE");
    enabled = option && std::string(option) == "1";
    if (!enabled) return;
    // An isolated test owns its channel. Never inherit an observer destination.
    require(!std::getenv("OPENXCOM_PRESENTATION_FD") && !std::getenv("OPENXCOM_EXPERIMENTAL_CURSOR_DIR") &&
            !std::getenv("OPENXCOM_EXPERIMENTAL_HUD_DIR"), "isolated-observer-destinations");
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
    // Before resources are loaded, close the real mixer and audio subsystem.
    Mix_HaltChannel(-1); Mix_HaltMusic(); Mix_CloseAudio(); SDL_QuitSubSystem(SDL_INIT_AUDIO);
    Options::mute = true;
    silent();
}
bool loaded(Game *game)
{
    if (!enabled) return false;
    silent();
    const std::string hello = receive();
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
    // Game has now deleted the old BattlescapeState, including its battle game.
    // Only then may setSavedGame destroy its battle, units, tiles and utilities.
    pendingSetup = false; phase = 0;
    RNG::setSeed(123456789); std::srand(12345);
    SavedGame *save = new SavedGame;
    SavedBattleGame *battle = new SavedBattleGame;
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
    occupant->setPosition(world == 1 ? parked : hover); occupant->setDirection(0);
    battle->getUnits()->push_back(player); battle->getUnits()->push_back(occupant);
    battle->resetUnitTiles(); battle->setSelectedUnit(player);
    state = new BattlescapeState;
    battle->setBattleState(state); game->setState(state);
}
void beforeFrame(Game *game)
{
    if (!running) return;
    require(state && game->isState(state) && !pendingSetup, "normal-battlescape-top-state");
    silent();
    SavedBattleGame *battle = game->getSavedGame()->getSavedBattle();
    Map *map = state->getMap();
    if (phase == 0) {
        // Normal init has run. This is a controlled stored-visibility fixture,
        // not a claim that these worlds came from a particular gameplay history.
        require(!occupant->getVisible(), "initial-hidden-visibility");
        occupant->setVisible(world == 2);
        map->cacheUnits();
        Position pixel; map->getCamera()->convertMapToScreen(hover, &pixel);
        pixel += map->getCamera()->getMapOffset();
        // Ordinary Map hit-testing is used, rather than assigning selector fields.
        SDL_Event event = {}; event.type = SDL_MOUSEMOTION;
        event.motion.x = (pixel.x + 16) * 2; event.motion.y = (pixel.y + 24) * 2;
        Action action(&event, game->getScreen()->getXScale(), game->getScreen()->getYScale(), 0, 0);
        game->getCursor()->handle(&action); state->handle(&action);
    } else {
        state->animate();
    }
    Position selected; map->getSelectorPosition(&selected);
    require(selected == hover && map->getCamera()->getViewLevel() == hover.z &&
            map->getCursorType() == CT_NORMAL && !state->getMouseOverIcons() &&
            game->getCursor()->getVisible() && !Options::debug && !battle->getDebugMode(), "actual-hover-cursor-preconditions");
    require(occupant->getTile() == battle->getTile(occupant->getPosition()) &&
            occupant->getTile()->getUnit() == occupant &&
            battle->getTile(hover)->getUnit() == (world == 1 ? 0 : occupant) &&
            occupant->getVisible() == (world == 2), "real-occupancy-visibility-preconditions");
    empty();
    challenge = randomChallenge();
    const std::string request = epoch + admission + challenge;
    require(send(peer, request.data(), request.size(), MSG_DONTWAIT | MSG_NOSIGNAL) == 96, "queue-before-composition");
    visits = 0; recording = true;
    histories[world].push_back(Frame());
    histories[world].back().packets.push_back(request);
}
void visited(const Tile *tile, int animationPhase)
{
    if (!recording) return;
    require(tile && tile->getPosition() == hover && animationPhase == phase, "actual-renderer-tile-and-phase");
    ++visits;
}
void capture(SDL_Surface *surface, int bw, int bh, bool allowed,
             const Observer::HudFrame &hud, const Observer::PixelCrop &crop)
{
    if (!running) return;
    require(phase < 8 && histories[world].size() == size_t(phase + 1), "complete-presentation-order");
    require(Observer::supportedPresentedSurface(surface, bw, bh, allowed) && Observer::validCrop(crop),
            "supported-focused-presentation");
    require(visits == 1, "actual-renderer-hover-visit");
    Frame &frame = histories[world].back();
    require(frame.rgb.empty(), "exactly-one-final-frame");
    frame.width = surface->w; frame.height = surface->h;
    frame.rgb.reserve(surface->w * surface->h * 3);
    require(SDL_LockSurface(surface) == 0, "final-surface-lock");
    for (int y = 0; y < surface->h; ++y) {
        const unsigned char *row = static_cast<const unsigned char *>(surface->pixels) + y * surface->pitch;
        for (int x = 0; x < surface->w; ++x) {
            const SDL_Color &color = surface->format->palette->colors[row[x]];
            frame.rgb.push_back(color.r); frame.rgb.push_back(color.g); frame.rgb.push_back(color.b);
        }
    }
    SDL_UnlockSurface(surface);
    frame.eligibility = eligibility(hud, crop); frame.crop = Observer::serializeCrop(crop);
}
void afterFrame(Game *game)
{
    if (!running) return;
    Frame &frame = histories[world].back();
    require(frame.rgb.size() == size_t(frame.width * frame.height * 3), "full-final-frame-captured");
    const std::string response = receive();
    YAML::Node packet;
    try {
    packet = YAML::Load(response);
    require(packet["schema"].as<std::string>() == "experimental-presentation/2" &&
            packet["epoch"].as<std::string>() == epoch && packet["challenge"].as<std::string>() == challenge,
            "public-response-binding");
    require(packet["hud"].IsMap() && packet["hud"].size() == 4 && packet["crop"].IsMap(),
            "non-null-hud-and-cursor");
    for (const char *field : {"time_units", "energy", "health", "morale"})
        require(packet["hud"][field].IsScalar() && packet["hud"][field].as<unsigned int>() <= 999, "public-hud-shape");
    admission = packet["next_token"].as<std::string>(); fresh(admission);
    } catch (...) { require(false, "public-response-json"); }
    require(response.find(std::string("\"crop\":") + frame.crop) != std::string::npos, "actual-produced-crop");
    frame.packets.push_back(response); empty(); silent(); recording = false;
    if (++phase == 8) {
        if (++world == 3) { verify(); running = false; game->quit(); }
        else { state = 0; occupant = 0; pendingSetup = true; game->setState(new State); }
    }
}
int exitStatus()
{
    if (peer >= 0) { close(peer); peer = -1; }
    return enabled && !completed ? EXIT_FAILURE : EXIT_SUCCESS;
}
} }
