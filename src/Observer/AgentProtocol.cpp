// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentProtocol.h"
#include <sstream>

namespace OpenXcom { namespace Observer {
const char *agentProtocolSchema() { return "openxcom-agent-control/1"; }
std::string agentEnvelope()
{
    return "{\"schema\":\"openxcom-agent-control/1\",\"mode\":\"agent-control\"";
}
std::string agentJsonString(const std::string &value)
{
    const char *hex = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char c : value)
    {
        if (c == '\"' || c == '\\') { out += '\\'; out += c; }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += c;
    }
    return out + "\"";
}
bool agentToken(const std::string &value)
{
    return value.size() == 32 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}
bool agentActionId(const std::string &value)
{
    return value == "next_soldier" || value == "previous_soldier" || value == "kneel" ||
        value == "stand" || value == "continue_next_turn" || value == "new_battle" ||
        value == "start_battle" || value == "continue_briefing" || value == "close_inventory";
}
bool parseAgentRequest(const std::string &packet, AgentRequest &request)
{
    request = AgentRequest();
    if (packet.empty() || packet.size() > 256 || packet.find_first_of("\r\n\t") != std::string::npos ||
        packet.find('\0') != std::string::npos) return false;
    if (packet == std::string("H ") + agentProtocolSchema()) { request.kind = 'H'; return true; }
    if (packet.size() == 34 && packet[1] == ' ' && (packet[0] == 'R' || packet[0] == 'S'))
    {
        const std::string value = packet.substr(2);
        if (!agentToken(value)) return false;
        request.kind = packet[0];
        if (request.kind == 'R') request.challenge = value; else request.id = value;
        return true;
    }
    std::istringstream input(packet);
    std::string kind, revision, extra;
    if (!(input >> kind >> request.id >> request.epoch >> request.capability >> revision >> request.action) ||
        input >> extra || kind != "E" || !agentToken(request.id) || !agentToken(request.epoch) ||
        !agentToken(request.capability) || !agentActionId(request.action) || revision.empty() ||
        revision.size() > 16 || revision.find_first_not_of("0123456789") != std::string::npos ||
        (revision.size() > 1 && revision[0] == '0')) return false;
    for (char c : revision) request.revision = request.revision * 10 + (c - '0');
    if (request.revision >= 9007199254740992ULL) return false;
    const std::string canonical = "E " + request.id + " " + request.epoch + " " + request.capability + " " +
        revision + " " + request.action;
    if (canonical != packet) return false;
    request.kind = 'E';
    return true;
}
AgentLedger::Admission AgentLedger::begin(const std::string &id, const std::string &request)
{
    auto found = _entries.find(id);
    if (found != _entries.end()) return found->second.request == request ? Duplicate : Conflict;
    if (_entries.size() >= 64) return Full;
    _entries[id].request = request;
    return Fresh;
}
AgentLedger::Entry *AgentLedger::find(const std::string &id)
{
    auto found = _entries.find(id);
    return found == _entries.end() ? 0 : &found->second;
}
} }
