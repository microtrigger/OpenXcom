// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentUi.h"
namespace OpenXcom {
class State;
namespace Observer {
// One immutable, consumed command awaiting its next successful presentation.
// No polling, rendering, simulation, clock access or game-object dereference.
class AgentFrameCommand
{
    bool _active = false;
    AgentRequest _request;
    AgentUiState _binding;
    State *_target = nullptr;
    std::uint64_t _generation = 0, _display = 0, _grantTime = 0, _queuedTime = 0;
public:
    bool active() const { return _active; }
    const AgentRequest &request() const { return _request; }
    const AgentUiState &binding() const { return _binding; }
    State *target() const { return _target; }
    bool stage(const AgentRequest &request, const AgentUiState &binding, State *target,
        std::uint64_t generation, std::uint64_t display, std::uint64_t grantTime, std::uint64_t queuedTime)
    {
        if (_active || !target || queuedTime < grantTime || queuedTime - grantTime > 120000) return false;
        _request = request; _binding = binding; _target = target;
        _generation = generation; _display = display; _grantTime = grantTime; _queuedTime = queuedTime;
        _active = true; return true;
    }
    bool expired(std::uint64_t time) const
    { return _active && (time < _queuedTime || time - _queuedTime > 1000 || time < _grantTime || time - _grantTime > 120000); }
    bool generationMatches(std::uint64_t generation) const { return _active && generation == _generation; }
    bool matchesFrame(std::uint64_t time, std::uint64_t generation, std::uint64_t display,
        State *target, const AgentUiState &shown) const
    {
        return _active && !expired(time) && generationMatches(generation) && display == _display &&
            target && target == _target && _binding.sameBinding(shown) && shown.offers(_request.action);
    }
    void clear() { _active = false; _target = nullptr; _request = AgentRequest(); _binding = AgentUiState(); }
};
} }
