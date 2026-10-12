// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution, 2026-10-11; selection/turn/map-left-click extensions, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include <SDL.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
#include "../Battlescape/Position.h"
#endif
namespace OpenXcom {
class Game; class SavedGame; class State; class BattlescapeState; class BattleUnit; class SavedBattleGame; class InventoryState;
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
struct InventoryClickGateAccess;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
struct InventoryRightClickGateAccess;
#endif
namespace Observer {
class StanceConfirmState;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
class InventoryCloseOverlay;
// Private action binding. No item identity, layout or collection is inspected.
struct InventoryCloseContext {
    InventoryState *state = 0;
    BattlescapeState *parent = 0;
    SavedGame *savedGame = 0;
    SavedBattleGame *save = 0;
    BattleUnit *actor = 0;
    std::uint64_t selectionRevision = 0, actorRevision = 0, uiRevision = 0;
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK)
    std::uint64_t heldRevision = 0;
    int selectionX = 0, selectionY = 0;
#endif
    bool operator==(const InventoryCloseContext &other) const;
};
#endif
// Private binding only. None of these pointers, counters or values is serialized.
struct StanceContext {
    BattlescapeState *state = 0;
    SavedBattleGame *save = 0;
    BattleUnit *actor = 0;
    std::uint64_t selectionRevision = 0, actorRevision = 0, uiRevision = 0;
    bool kneeled = false, kneelReserved = false;
    int tu = 0, energy = 0, health = 0, morale = 0, stun = 0, reserved = 0;
    std::string name;
    bool operator==(const StanceContext &other) const;
};
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
// Private view/pose binding only; never a world or target-eligibility oracle.
struct TurnContext {
    Position position, cameraOffset;
    std::uint64_t poseRevision = 0, cameraRevision = 0;
    int direction = 0, turretDirection = 0;
    bool allLayers = false;
    std::vector<int> geometry;
    bool operator==(const TurnContext &other) const;
};
#endif
class StanceController {
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    friend struct ::OpenXcom::InventoryClickGateAccess;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    friend struct ::OpenXcom::InventoryRightClickGateAccess;
#endif
    struct Command { std::string request, status; };
    Game *_game;
    int _fd;
    std::string _epoch, _capability, _challenge, _pendingId;
    StanceContext _context, _frameContext;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
    TurnContext _turnContext, _frameTurnContext;
#endif
    std::vector<unsigned char> _rgb, _indices;
    SDL_Color _palette[256];
    StanceConfirmState *_dialog, *_frameDialog;
    State *_expectedPush, *_expectedPop, *_challengeState;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    bool _inventoryEnabled, _awaitingInventoryView;
    InventoryState *_inventoryDestination, *_frameInventory;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
    bool _inventoryCloseEnabled, _closeRestoring, _awaitingCloseView;
    InventoryCloseContext _closeContext, _frameCloseContext;
    InventoryState *_closeInventory, *_frameCloseInventory;
    BattlescapeState *_closeParent;
    SavedGame *_closeSavedGame;
    SavedBattleGame *_closeSave;
    std::uint64_t _closeDispatchTime;
    void retireCloseOverlay();
    bool closeParentCurrent() const;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND
    bool _inventoryGroundEnabled;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK
    bool _inventoryClickEnabled;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK
    bool _inventoryRightClickEnabled;
    bool _inventoryRightDispatching, _inventoryRightPopped, _inventoryRightUnexpected;
    InventoryState *_inventoryRightOrigin;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK
    // RIGHT overlay-owned input tails must survive retirement, state pop and disconnect.
    std::uint8_t _inventoryConsumedButtons;
    bool _inventoryOwnedYRelease;
#endif
    char _action; // k/s: stance, n/p: selection, r/l: map button, o/c/g/e/x: inventory open/close/Ground/left/right click.
    bool _selectionEnabled, _turnEnabled, _mapLeftClickEnabled, _awaitingView, _waitingTurnFrame;
    char _contextKind; // O: HUD-bound D/N; V: view-bound R/L/B; I: capless pixels; J/K/H/U: close/Ground/left/right-only view.
    int _targetX, _targetY;
    bool _armed, _desired, _dialogPresented, _confirmEventAllowed, _yDown, _mouseDown, _mouseArmed;
    std::uint64_t _contextTime, _challengeTime, _challengeDeadline;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
    // Private, one-way permission to wait through the normal post-click walk.
    bool _mapLeftClickObservationActive, _confirmEventFromKey, _mapLeftClickConfirmKeyRelease;
    std::uint64_t _mapLeftClickDispatchTime;
#endif
    std::map<std::string, Command> _commands;
    bool sendPacket(const std::string &packet);
    bool connected() const;
    bool bindingCurrent(bool modal = false) const;
    void replyStatus(const std::string &id);
    void unavailable();
public:
    StanceController(Game *game, const char *descriptor);
    ~StanceController();
    bool enabled() const { return _fd >= 0; }
    void close();
    void invalidate(bool newEpoch = false, bool preserveMapWait = false, bool preserveInventoryRead = false);
    void beforePush(State *state);
    void beforePop(State *state);
    bool beforeEvent(const SDL_Event &event, State *top);
    void poll();
    void beginFrame();
    void arm(BattlescapeState *state);
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    void armInventory(InventoryState *state);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
    void confirmInventoryClose(InventoryState *state, bool accept);
    void beforeSaveChange();
#endif
    void presented(SDL_Surface *surface, int baseWidth, int baseHeight, bool profileAllowed, bool panelClear = true);
    void confirm(StanceConfirmState *dialog, bool accept);
    bool ownsDialog(State *state) const;
    const std::vector<unsigned char> &indices() const { return _indices; }
    SDL_Color *palette() { return _palette; }
    bool desiredKneel() const { return _desired; }
    bool selectionRequest() const { return _action == 'n' || _action == 'p'; }
    bool nextSelection() const { return _action == 'n'; }
    bool turnRequest() const { return _action == 'r'; }
    bool mapLeftClickRequest() const { return _action == 'l'; }
    bool inventoryOpenRequest() const { return _action == 'o'; }
    bool inventoryGroundRequest() const { return _action == 'g'; }
    bool inventoryClickRequest() const { return _action == 'e'; }
    bool inventoryRightClickRequest() const { return _action == 'x'; }
    int targetX() const { return _targetX; }
    int targetY() const { return _targetY; }
private:
    StanceController(const StanceController &);
    StanceController &operator=(const StanceController &);
};
} }
