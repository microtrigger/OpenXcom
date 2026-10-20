// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentUi.h"
#include <SDL.h>
namespace OpenXcom {
class Game; class State;
namespace Observer {
class AgentController {
    Game *_game;
    int _fd = -1;
    bool _ready = false;
    std::string _epoch, _capability, _readChallenge, _pendingId;
    AgentLedger _ledger;
    State *_boundTop = 0, *_frameTop = 0;
    AgentUiState _binding;
    std::uint64_t _revision = 0, _eventSequence = 0, _contextTime = 0;
    std::uint64_t _readTime = 0, _dispatchTime = 0, _frameGeneration = 0, _generation = 0;
    std::uint64_t _boundDisplayRevision = 0;
    std::string _lastPublic;
    bool sendPacket(const std::string &packet);
    bool sample(AgentUiState &out) const;
    bool focused() const;
    void unavailable();
    std::string statePacket(const AgentUiState &ui, const std::string &challenge, bool grant);
    std::string resultPacket(const std::string &id, const std::string &status, const std::string &observation = "null") const;
    void receipt(const std::string &id);
public:
    explicit AgentController(Game *game);
    ~AgentController();
    void close();
    void invalidate(bool publicChange = true);
    void beforeEvent(const SDL_Event &event);
    void poll();
    void beginFrame();
    void presented();
private:
    AgentController(const AgentController &) = delete;
    AgentController &operator=(const AgentController &) = delete;
};
} }
