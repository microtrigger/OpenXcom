// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentUi.h"
#include "AgentPublicEvents.h"
#include "AgentFrameCommand.h"
#include "AgentPlayerBases.h"
#include <SDL.h>
namespace OpenXcom {
class Game; class State; class SavedGame;
namespace Observer {
class AgentController {
    Game *_game;
    int _fd = -1;
    bool _ready = false;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_REPORT_FIXTURE
    bool _fixtureHelloSent = false;
#endif
    std::string _epoch, _capability, _readChallenge, _pendingId;
    AgentLedger _ledger;
    std::uint64_t _commandGeneration = 1, _lastRetiredGeneration = 0;
    std::string _lastRolloverRequest, _lastRolloverResponse, _lastRolloverChallenge;
    void rollover(const AgentRequest &request, const std::string &packet);
    AgentFrameCommand _queued;
    bool _dispatchInProgress = false;
    std::uint64_t _presentationSerial = 0, _dispatchPresentation = 0;
    AgentPublicEvents _publicEvents;
    AgentPlayerBases _playerBases;
    SavedGame *_basesSave = nullptr;
    std::vector<std::string> _basesRecords;
    std::string _basesEpoch, _basesSnapshot;
    std::uint64_t _basesGeneration = 0;
    void playerBasesPage(const AgentRequest &request);
    State *_boundTop = 0, *_frameTop = 0;
    AgentUiState _binding;
    std::uint64_t _revision = 0, _eventSequence = 0, _contextTime = 0;
    std::uint64_t _readTime = 0, _dispatchTime = 0, _frameGeneration = 0, _generation = 0;
    std::uint64_t _boundDisplayRevision = 0;
    std::string _lastPublic, _lastAuthority;
    bool sendPacket(const std::string &packet);
    bool sample(AgentUiState &out) const;
    bool focused() const;
    void unavailable();
    void readExpired(const std::string &challenge);
    std::string statePacket(const AgentUiState &ui, const std::string &challenge, bool grant);
    std::string resultPacket(const std::string &id, const std::string &status, const std::string &observation = "null") const;
    void receipt(const std::string &id);
    void eventPage(const AgentRequest &request);
    void drainAudioEvents();
    void rejectQueued();
    bool transportAlive() const;
    bool queuedInputClear() const;
    void dispatchQueued(const AgentUiState &shown);
public:
    explicit AgentController(Game *game);
    ~AgentController();
    void close();
    void resetPlayerBases();
    bool enabled() const { return _fd >= 0; }
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
