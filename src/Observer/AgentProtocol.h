// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit, bounded native agent-control protocol. New contribution 2026-10-18.
#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace OpenXcom { namespace Observer {
const char *agentProtocolSchema();
std::string agentEnvelope();
std::string agentJsonString(const std::string &value);
bool agentToken(const std::string &value);
bool agentActionId(const std::string &value);
const char *agentArgumentKind(const std::string &action);
bool agentInputText(const std::string &value);
struct AgentRequest {
    char kind = 0;
    std::string id, epoch, capability, action, challenge;
    std::uint64_t revision = 0, after = 0, commandGeneration = 0, acknowledgedCount = 0;
    unsigned limit = 0;
    std::string choice, text, control, operation, direction, amount, snapshot;
    int value = 0;
    bool desired = false;
    int x = 0, y = 0;
};
bool parseAgentRequest(const std::string &packet, AgentRequest &request);

// A generation-scoped, non-evicting ledger. A full ledger never makes
// an old command executable again. Receipts say dispatch, not game success.
class AgentLedger {
public:
    enum Admission { Fresh, Duplicate, Conflict, Full };
    struct Entry { std::string request, response; bool dispatched = false; bool terminalDispatched = false; };
    Admission begin(const std::string &id, const std::string &request);
    Entry *find(const std::string &id);
    bool retireAcknowledged(std::size_t count);
    std::size_t size() const { return _entries.size(); }
private:
    std::map<std::string, Entry> _entries;
};
} }
