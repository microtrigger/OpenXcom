// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution, 2026-10-11; selection/turn extensions, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
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
class Game; class State; class BattlescapeState; class BattleUnit; class SavedBattleGame;
namespace Observer {
class StanceConfirmState;
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
    char _action; // k/s: desired stance, n/p: ordinary selection direction.
    bool _selectionEnabled, _turnEnabled, _awaitingView, _waitingTurnFrame;
    char _contextKind; // O: HUD-bound D/N; V: tactical-view-bound R only.
    int _targetX, _targetY;
    bool _armed, _desired, _dialogPresented, _confirmEventAllowed, _yDown, _mouseDown, _mouseArmed;
    std::uint64_t _contextTime, _challengeTime;
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
    void invalidate(bool newEpoch = false);
    void beforePush(State *state);
    void beforePop(State *state);
    void beforeEvent(const SDL_Event &event, State *top);
    void poll();
    void beginFrame();
    void arm(BattlescapeState *state);
    void presented(SDL_Surface *surface, int baseWidth, int baseHeight, bool allowed);
    void confirm(StanceConfirmState *dialog, bool accept);
    bool ownsDialog(State *state) const;
    const std::vector<unsigned char> &indices() const { return _indices; }
    SDL_Color *palette() { return _palette; }
    bool desiredKneel() const { return _desired; }
    bool selectionRequest() const { return _action == 'n' || _action == 'p'; }
    bool nextSelection() const { return _action == 'n'; }
    bool turnRequest() const { return _action == 'r'; }
    int targetX() const { return _targetX; }
    int targetY() const { return _targetY; }
private:
    StanceController(const StanceController &);
    StanceController &operator=(const StanceController &);
};
} }
