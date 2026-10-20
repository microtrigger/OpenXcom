// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentProtocol.h"
#include <sstream>
#include <vector>

namespace OpenXcom { namespace Observer {
const char *agentProtocolSchema() { return "openxcom-agent-control/14"; }
std::string agentEnvelope()
{
    return "{\"schema\":\"openxcom-agent-control/14\",\"mode\":\"agent-control\"";
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
        value == "start_battle" || value == "continue_briefing" || value == "close_inventory" ||
        value == "open_new_game" || value == "open_options" || value == "open_load_game" ||
        value == "open_pause_menu" || value == "open_save_game" || value == "resume_game" ||
        value == "cancel_menu" || value == "dismiss_error" || value == "select_difficulty" ||
        value == "set_ironman" || value == "start_new_game" || value == "place_base_site" ||
        value == "replace_base_name" || value == "confirm_base_name" ||
        value == "open_new_battle_choice" || value == "choose_visible_row" || value == "scroll_visible_list" ||
        value == "close_dropdown" || value == "open_funding" || value == "close_funding" || value == "end_turn" || value == "acknowledge_message" || value == "map_primary" || value == "map_secondary" ||
        value == "center_enemy" || value == "center_selected_unit" || value == "open_hand_action_menu" ||
        value == "choose_weapon_action" || value == "choose_grenade_timer" || value == "cancel_grenade_timer" || value == "cancel_weapon_action" || value == "close_debriefing" ||
        value == "open_inventory" || value == "inventory_hover" || value == "inventory_activate" ||
        value == "cancel_inventory_hold" || value == "unload_inventory" || value == "next_inventory_ground_page";
}
const char *agentArgumentKind(const std::string &action)
{
    if (action == "map_primary" || action == "map_secondary") return "grid_cell";
    if (action == "center_enemy") return "enemy_button";
    if (action == "open_hand_action_menu") return "hand";
    if (action == "choose_weapon_action") return "weapon_action";
    if (action == "choose_grenade_timer") return "grenade_timer";
    if (action == "inventory_hover" || action == "inventory_activate") return "inventory_target";
    if (action == "open_new_battle_choice") return "choice_control";
    if (action == "choose_visible_row") return "visible_row";
    if (action == "scroll_visible_list") return "scroll_direction";
    if (action == "select_difficulty") return "difficulty";
    if (action == "set_ironman") return "boolean";
    if (action == "replace_base_name") return "text";
    if (action == "place_base_site") return "point";
    return agentActionId(action) ? "none" : "unsupported";
}
bool agentInputText(const std::string &value)
{
    if (value.size() > 128) return false;
    for (std::size_t i = 0; i < value.size();)
    {
        const unsigned char first = static_cast<unsigned char>(value[i++]);
        unsigned code = 0, remaining = 0, minimum = 0;
        if (first < 0x80) code = first;
        else if (first >= 0xc2 && first <= 0xdf) { code = first & 31; remaining = 1; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { code = first & 15; remaining = 2; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { code = first & 7; remaining = 3; minimum = 0x10000; }
        else return false;
        if (i + remaining > value.size()) return false;
        for (unsigned j = 0; j < remaining; ++j)
        {
            const unsigned char next = static_cast<unsigned char>(value[i++]);
            if ((next & 0xc0) != 0x80) return false;
            code = (code << 6) | (next & 63);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ||
            code < 32 || (code >= 127 && code <= 159)) return false;
    }
    return true;
}
namespace {
bool decimal(const std::string &text, std::uint64_t maximum, std::uint64_t &out)
{
    if (text.empty() || text.size() > 16 || (text.size() > 1 && text[0] == '0') ||
        text.find_first_not_of("0123456789") != std::string::npos) return false;
    out = 0;
    for (char c : text) out = out * 10 + (c - '0');
    return out <= maximum;
}
}
bool parseAgentRequest(const std::string &packet, AgentRequest &request)
{
    request = AgentRequest();
    if (packet.empty() || packet.size() > 512 || packet.find_first_of("\r\n\t") != std::string::npos ||
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
    std::vector<std::string> parts; std::string part, canonical;
    while (input >> part)
    {
        if (!canonical.empty()) canonical += " ";
        canonical += part; parts.push_back(part);
    }
    if (canonical == packet && parts.size() == 5 && parts[0] == "V")
    {
        std::uint64_t limit = 0;
        if (!agentToken(parts[1]) || !agentToken(parts[2]) ||
            !decimal(parts[3], 9007199254740991ULL, request.after) ||
            !decimal(parts[4], 16, limit) || !limit) return false;
        request.kind = 'V'; request.challenge = parts[1]; request.epoch = parts[2];
        request.limit = static_cast<unsigned>(limit); return true;
    }
    if (canonical != packet || parts.size() < 6 || parts.size() > 8 || parts[0] != "E") return false;
    request.id = parts[1]; request.epoch = parts[2]; request.capability = parts[3]; request.action = parts[5];
    if (!agentToken(request.id) || !agentToken(request.epoch) || !agentToken(request.capability) ||
        !agentActionId(request.action) || !decimal(parts[4], 9007199254740991ULL, request.revision)) return false;
    const std::string kind = agentArgumentKind(request.action);
    if (kind == "none") { if (parts.size() != 6) return false; }
    else if (kind == "difficulty")
    {
        if (parts.size() != 7) return false;
        request.choice = parts[6];
        if (request.choice != "beginner" && request.choice != "experienced" && request.choice != "veteran" &&
            request.choice != "genius" && request.choice != "superhuman") return false;
    }
    else if (kind == "choice_control")
    {
        if (parts.size() != 7) return false;
        request.choice = parts[6];
        if (request.choice != "mission" && request.choice != "craft" && request.choice != "terrain" &&
            request.choice != "difficulty" && request.choice != "alien_race") return false;
    }
    else if (kind == "grid_cell" || kind == "enemy_button" || kind == "weapon_action" || kind == "grenade_timer")
    {
        const char prefix = kind == "grid_cell" ? 'g' : (kind == "enemy_button" ? 'e' : (kind == "grenade_timer" ? 't' : 'a'));
        const unsigned maximum = kind == "grid_cell" ? 127 : (kind == "enemy_button" ? 9 : (kind == "grenade_timer" ? 23 : 5));
        std::uint64_t index;
        if (parts.size() != 7 || parts[6].empty() || parts[6][0] != prefix ||
            !decimal(parts[6].substr(1), maximum, index)) return false;
        request.choice = parts[6];
    }
    else if (kind == "hand")
    {
        if (parts.size() != 7 || (parts[6] != "left" && parts[6] != "right")) return false;
        request.choice = parts[6];
    }
    else if (kind == "inventory_target")
    {
        std::uint64_t target;
        if (parts.size() != 7 || parts[6].empty() || parts[6][0] != 'v' ||
            !decimal(parts[6].substr(1), 95, target)) return false;
        request.choice = parts[6];
    }
    else if (kind == "visible_row")
    {
        std::uint64_t row;
        if (parts.size() != 7 || parts[6].empty() || parts[6][0] != 'r' ||
            !decimal(parts[6].substr(1), 31, row)) return false;
        request.choice = parts[6];
    }
    else if (kind == "scroll_direction")
    {
        if (parts.size() != 7 || (parts[6] != "up" && parts[6] != "down")) return false;
        request.choice = parts[6];
    }
    else if (kind == "boolean")
    {
        if (parts.size() != 7 || (parts[6] != "0" && parts[6] != "1")) return false;
        request.desired = parts[6] == "1";
    }
    else if (kind == "text")
    {
        if (parts.size() != 7) return false;
        const std::string &hex = parts[6];
        if (hex != "-")
        {
            if (hex.empty() || hex.size() > 256 || hex.size() % 2 ||
                hex.find_first_not_of("0123456789abcdef") != std::string::npos) return false;
            const std::string digits = "0123456789abcdef";
            for (std::size_t i = 0; i < hex.size(); i += 2)
                request.text += static_cast<char>(digits.find(hex[i]) * 16 + digits.find(hex[i+1]));
        }
        if (!agentInputText(request.text)) return false;
    }
    else if (kind == "point")
    {
        std::uint64_t x, y;
        if (parts.size() != 8 || !decimal(parts[6], 4095, x) || !decimal(parts[7], 4095, y)) return false;
        request.x = static_cast<int>(x); request.y = static_cast<int>(y);
    }
    else return false;
    request.kind = 'E';
    return true;
}
AgentLedger::Admission AgentLedger::begin(const std::string &id, const std::string &request)
{
    auto found = _entries.find(id);
    if (found != _entries.end()) return found->second.request == request ? Duplicate : Conflict;
    if (_entries.size() >= 256) return Full;
    _entries[id].request = request;
    return Fresh;
}
AgentLedger::Entry *AgentLedger::find(const std::string &id)
{
    auto found = _entries.find(id);
    return found == _entries.end() ? 0 : &found->second;
}
} }
