// SPDX-License-Identifier: GPL-3.0-or-later
// Exact native viewport helpers, ordinary button handlers and camera movement
// bodies with real offscreen Surface/NumberText projection. Game, unit movement,
// timer and physical input dependencies are explicit test seams. No game/window.
#include "../../src/Observer/AgentUi.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/NumberText.h"
#include "../../src/Battlescape/Position.h"
#include "../../src/Battlescape/CameraProjection.h"
#include "../../src/fmath.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace OpenXcom;
using namespace OpenXcom::Observer;
namespace {
unsigned checks = 0;
void check(bool value, const char *message)
{
    ++checks;
    if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
}
namespace ViewportHarness {
using namespace OpenXcom;
enum { CT_NONE, CT_NORMAL, FACTION_PLAYER = 0, FACTION_HOSTILE = 1 };
struct Action {};
struct Timer { bool running = false; bool isRunning() const { return running; } };
class Camera;
struct Map {
    Camera *camera = nullptr;
    int cursor = CT_NORMAL;
    unsigned draws = 0, refreshes = 0, invalidations = 0;
    int getCursorType() const { return cursor; }
    Camera *getCamera() const { return camera; }
    void refreshSelectorPosition() { ++refreshes; }
    void invalidate() { ++invalidations; }
    void draw() { ++draws; }
};
static Uint8 heldMouseButtons = 0;
static Uint8 testMouseState(int *, int *) { return heldMouseButtons; }
class Camera {
public:
    Map *_map = nullptr;
    bool _scrollTrigger = false, _showAllLayers = false;
    int _scrollMouseX = 0, _scrollMouseY = 0, _scrollKeyX = 0, _scrollKeyY = 0;
    Timer *_scrollMouseTimer = nullptr, *_scrollKeyTimer = nullptr;
    int _spriteWidth = 32, _spriteHeight = 40, _screenWidth = 320, _visibleMapHeight = 144;
    int _mapsize_x = 60, _mapsize_y = 60, _mapsize_z = 4;
    Position _mapOffset{160, -400, 0}, _center;
    std::uint64_t _agentViewRevision = 0;
    bool agentScrollReady() const;
    bool agentScrollStep(const std::string &direction);
    void scrollXY(int x, int y, bool redraw);
    void convertScreenToMap(int, int, int *, int *) const;
    void convertScreenToMapUnclamped(int, int, int *, int *) const;
    void up(); void down(); int toggleShowAllLayers();
};
#define SDL_GetMouseState testMouseState
#include "AgentViewportCamera.generated.inc"
#undef SDL_GetMouseState
struct Unit { Position position{12, 13, 0}; const Position &getPosition() const { return position; } };
struct Pathfinding {
    enum { DIR_UP = 8, DIR_DOWN = 9 };
    unsigned validations = 0;
    bool valid = true;
    bool validateUpDown(Unit *, const Position &, int) { ++validations; return valid; }
};
struct Saved {
    Unit *unit = nullptr;
    Pathfinding path;
    int side = FACTION_PLAYER;
    bool debug = false;
    int getSide() const { return side; }
    bool getDebugMode() const { return debug; }
    Pathfinding *getPathfinding() { return &path; }
    Unit *getSelectedUnit() const { return unit; }
};
struct BattleGame {
    unsigned cancellations = 0, moves = 0;
    int lastDirection = -1;
    void cancelAllActions() { ++cancellations; }
    void moveUpDown(Unit *, int direction) { ++moves; lastDirection = direction; }
};
struct MiniMapState { MiniMapState(Camera *, Saved *) {} };
struct Game { unsigned pushes = 0; void pushState(MiniMapState *state) { ++pushes; delete state; } };
class BattlescapeState {
public:
    Map *_map = nullptr;
    Saved *_save = nullptr;
    BattleGame *_battleGame = nullptr;
    Game *_game = nullptr;
    Surface *_btnMapUp = nullptr, *_btnMapDown = nullptr, *_btnShowMap = nullptr;
    Surface *_btnUnitUp = nullptr, *_btnUnitDown = nullptr, *_btnShowLayers = nullptr;
    NumberText *_numLayers = nullptr;
    std::vector<Surface *> _surfaces;
    bool _isMouseScrolling = false, _isMouseScrolled = false, allow = true;
    unsigned selectedChecks = 0;
    bool playableUnitSelected() { ++selectedChecks; return _save->unit && allow; }
    bool allowButtons() const { return allow; }
    std::string agentViewportControls(AgentUiState &) const;
    bool agentViewportDispatch(const AgentRequest &, const AgentUiState &);
    void btnMapUpClick(Action *); void btnMapDownClick(Action *);
    void btnShowLayersClick(Action *); void btnShowMapClick(Action *);
    void btnUnitUpClick(Action *); void btnUnitDownClick(Action *);
};
#include "AgentViewportProvider.generated.inc"
#include "AgentViewportOrdinaryHandlers.generated.inc"
}
int main()
{
    using namespace ViewportHarness;
    check(SDL_Init(0) == 0 && !SDL_GetVideoSurface(), "offscreen only, no window");
    Options::baseXResolution = 320; Options::baseYResolution = 200; Options::battleScrollSpeed = 8;
    Surface canvas(320, 200), unitUp(32, 16, 48, 144), unitDown(32, 16, 48, 160);
    Surface viewUp(32, 16, 80, 144), viewDown(32, 16, 80, 160), minimap(32, 16, 112, 144);
    Surface layers(32, 16, 208, 144), cover(32, 16, 80, 144);
    NumberText number(3, 5, 232, 150);
    number.setColor(14); number.setValue(1); cover.setVisible(false);
    Map map; Camera camera; Saved save; ViewportHarness::Unit unit; BattleGame battle; Game game;
    map.camera = &camera; camera._map = &map; save.unit = &unit;
    BattlescapeState state;
    state._map = &map; state._save = &save; state._battleGame = &battle; state._game = &game;
    state._btnMapUp = &viewUp; state._btnMapDown = &viewDown; state._btnShowMap = &minimap;
    state._btnUnitUp = &unitUp; state._btnUnitDown = &unitDown; state._btnShowLayers = &layers;
    state._numLayers = &number;
    state._surfaces = {&unitUp, &unitDown, &viewUp, &viewDown, &minimap, &layers, &number, &cover};
    for (Surface *button : {&unitUp, &unitDown, &viewUp, &viewDown, &minimap, &layers, &cover})
    { button->clear(30); button->invalidate(); }
    const auto paint = [&]() { for (Surface *surface : state._surfaces) surface->blit(&canvas); };
    AgentUiState ui;
    const auto project = [&]() -> std::string {
        ui = AgentUiState();
        // These are previously admitted map-grid points, not model tile facts.
        ui.points.push_back({160, 72}); ui.regions.push_back({150, 64, 20, 16});
        return state.agentViewportControls(ui);
    };
    const auto dispatch = [&](const std::string &action, const std::string &choice = "") -> bool {
        AgentRequest request; request.action = action; request.choice = choice;
        return state.agentViewportDispatch(request, ui);
    };
    check(state.agentViewportControls(ui) == "null" && ui.actions.empty(), "dirty controls and digit are withheld before first paint");
    paint();
    check(project() == "1" && ui.actions.size() == 7 && agentRegionsDisjoint(ui), "all seven viewport controls and nonoverlapping composite layer proof");
    for (const char *action : {"view_level_up", "view_level_down", "open_minimap", "toggle_layers", "unit_up", "unit_down", "camera_pan"})
        check(ui.offers(action), "only explicit visible viewport controls are offered");
    check(!ui.offers("close_minimap") && !ui.offers("minimap_center"), "minimap-only controls absent from Battlescape");
    check(save.path.validations == 0 && state.selectedChecks == 0 && battle.moves == 0, "projection never preflights unit eligibility");
    save.path.valid = false;
    check(dispatch("unit_up") && save.path.validations == 1 && battle.moves == 0, "ordinary invalid unit movement is still a dispatch");
    check(project() == "1" && ui.offers("unit_up") && ui.offers("unit_down") && save.path.validations == 1,
        "invalid movement reveals no control-list eligibility oracle");
    save.path.valid = true;
    check(dispatch("unit_up") && battle.moves == 1 && battle.lastDirection == Pathfinding::DIR_UP && battle.cancellations == 1,
        "unit up invokes unchanged ordinary validation and movement once");
    check(dispatch("unit_down") && battle.moves == 2 && battle.lastDirection == Pathfinding::DIR_DOWN && battle.cancellations == 2,
        "unit down remains distinct from view level");
    save.unit = nullptr;
    check(project() == "1" && dispatch("unit_up") && battle.moves == 2 && save.path.validations == 3,
        "no selected unit is an ordinary no-op rather than hidden admission test");
    save.unit = &unit;
    const unsigned moves = battle.moves;
    check(dispatch("view_level_up") && camera._mapOffset.z == 1 && battle.moves == moves, "view up changes only camera through normal handler");
    check(dispatch("view_level_down") && camera._mapOffset.z == 0 && battle.moves == moves, "view down changes only camera through normal handler");
    check(dispatch("view_level_down") && camera._mapOffset.z == 0, "camera floor no-op remains a dispatch");
    camera._mapOffset.z = 3;
    check(dispatch("view_level_up") && camera._mapOffset.z == 3, "camera ceiling no-op remains a dispatch");
    check(project() == "1", "painted layers digit is independent of camera z");
    const auto generation = camera._agentViewRevision;
    check(dispatch("toggle_layers") && number.getValue() == 2 && camera._agentViewRevision > generation, "toggle uses ordinary handler and private ABA generation");
    check(project() == "null", "unpainted new layers value is never disclosed");
    paint(); check(project() == "2", "painted ordinary layer indicator two disclosed");
    check(dispatch("toggle_layers"), "toggle back dispatches"); paint();
    check(project() == "1" && camera._agentViewRevision > generation + 1, "away-and-back layer mode retains freshness generation");
    check(dispatch("open_minimap") && game.pushes == 1, "open minimap calls unchanged ordinary handler once");
    state.allow = false;
    check(dispatch("open_minimap") && game.pushes == 1, "ordinary menu readiness is applied inside its handler");
    state.allow = true;
    for (const auto &direction : std::vector<std::pair<std::string, Position>>{
        {"left", {8, 0, 0}}, {"right", {-8, 0, 0}}, {"up", {0, 8, 0}}, {"down", {0, -8, 0}}})
    {
        camera._mapOffset = {160, -400, 0}; const Position start = camera._mapOffset;
        const auto revision = camera._agentViewRevision; const auto refreshes = map.refreshes;
        check(dispatch("camera_pan", direction.first) && camera._mapOffset == start + direction.second &&
            camera._agentViewRevision == revision + 1 && map.refreshes == refreshes + 1,
            "pan is exactly one normal configured scrollXY step with ordinary selector refresh");
    }
    const auto revision = camera._agentViewRevision;
    check(!dispatch("camera_pan", "north") && camera._agentViewRevision == revision, "unknown pan cannot move the camera");
    camera._mapsize_x = camera._mapsize_y = 1; camera._mapOffset = {160, 48, 0};
    for (unsigned i = 0; i < 32; ++i)
        check(dispatch("camera_pan", "left") && camera._center.x == 0 && camera._center.y == 0,
            "ordinary private horizontal bounds clamp never becomes an admission or outcome oracle");
    camera._mapsize_x = camera._mapsize_y = 60;
    for (int speed : {2, 20})
    {
        Options::battleScrollSpeed = speed; camera._mapOffset = {160, -400, 0};
        check(dispatch("camera_pan", "left") && camera._mapOffset.x == 160 + speed, "configured ordinary scroll-speed bounds respected");
    }
    for (int speed : {-1, 0, 1, 21, 2147483647})
    {
        Options::battleScrollSpeed = speed; project();
        check(!ui.offers("camera_pan") && !camera.agentScrollStep("left"), "malformed unsupported speed cannot expand a bounded step");
    }
    Options::battleScrollSpeed = 8;
    for (bool *busy : {&state._isMouseScrolling, &state._isMouseScrolled, &camera._scrollTrigger})
    {
        *busy = true; project(); check(!ui.offers("camera_pan"), "active drag or trigger blocks pan admission"); *busy = false;
    }
    for (int *motion : {&camera._scrollMouseX, &camera._scrollMouseY, &camera._scrollKeyX, &camera._scrollKeyY})
    {
        *motion = 8; project(); check(!ui.offers("camera_pan") && !camera.agentScrollStep("left"), "queued ordinary input blocks pan"); *motion = 0;
    }
    Timer timer; timer.running = true;
    camera._scrollMouseTimer = &timer; project(); check(!ui.offers("camera_pan"), "mouse timer blocks pan"); camera._scrollMouseTimer = nullptr;
    camera._scrollKeyTimer = &timer; project(); check(!ui.offers("camera_pan"), "key timer blocks pan"); camera._scrollKeyTimer = nullptr;
    heldMouseButtons = SDL_BUTTON_LMASK; project(); check(!ui.offers("camera_pan"), "held human input blocks pan"); heldMouseButtons = 0;
    map.cursor = CT_NONE; project(); check(!ui.offers("camera_pan") && !camera.agentScrollStep("left"), "CT_NONE preserves ordinary camera input guard"); map.cursor = CT_NORMAL;
    ui = AgentUiState(); check(state.agentViewportControls(ui) == "1" && !ui.offers("camera_pan"), "unavailable map witness cannot grant pan");
    project(); state._isMouseScrolling = true;
    check(!dispatch("camera_pan", "left"), "dispatch rechecks drag state"); state._isMouseScrolling = false;
    for (const auto &entry : std::vector<std::pair<Surface *, std::string>>{
        {&viewUp, "view_level_up"}, {&viewDown, "view_level_down"}, {&minimap, "open_minimap"},
        {&unitUp, "unit_up"}, {&unitDown, "unit_down"}, {&layers, "toggle_layers"}})
    {
        entry.first->setVisible(false); project(); check(!ui.offers(entry.second) && !dispatch(entry.second), "hidden control cannot dispatch");
        entry.first->setVisible(true); paint();
        cover.setX(entry.first->getX()); cover.setY(entry.first->getY()); cover.setVisible(true); paint(); project();
        check(!ui.offers(entry.second) && !dispatch(entry.second), "later painted cover blocks exact control");
        cover.setVisible(false); paint();
    }
    number.setVisible(false); check(project() == "null", "hidden layers digit withheld"); number.setVisible(true); paint();
    number.setColor(255); paint(); check(project() == "null", "transparent layers digit withheld"); number.setColor(14);
    number.setValue(3); paint(); check(project() == "null", "out-of-contract layers digit withheld"); number.setValue(1);
    number.setX(319); paint(); check(project() == "null", "viewport-clipped layers digit withheld"); number.setX(232);
    number.setHeight(4); paint(); check(project() == "null", "internally clipped layers digit withheld"); number.setHeight(5); paint();
    check(project() == "1" && agentRegionsDisjoint(ui) && agentPaintHealthy(), "restored real widgets produce healthy disjoint projection");
    check(!SDL_GetVideoSurface() && !SDL_WasInit(SDL_INIT_VIDEO), "no GUI was initialized");
    std::cout << "AgentViewportControlsTest PASS " << checks << " checks\n";
}
