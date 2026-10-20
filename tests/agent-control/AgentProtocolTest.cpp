// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace OpenXcom::Observer;
namespace {
unsigned checks = 0;
void check(bool value, const char *message)
{
    ++checks;
    if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
std::string token(unsigned value)
{
    std::ostringstream out; out << std::hex << std::setw(32) << std::setfill('0') << value; return out.str();
}
}
int main()
{
    AgentRequest r;
    check(std::string(agentProtocolSchema()) == "openxcom-agent-control/14", "inventory schema version");
    check(agentEnvelope() == "{\"schema\":\"openxcom-agent-control/14\",\"mode\":\"agent-control\"",
        "inventory envelope schema matches handshake");
    check(parseAgentRequest("H openxcom-agent-control/14", r) && r.kind == 'H', "exact handshake");
    check(!parseAgentRequest("H openxcom-agent-control/13", r), "prior64-target schema rejected");
    check(!parseAgentRequest("H openxcom-agent-control/12", r), "prior body schema rejected");
    check(!parseAgentRequest("H openxcom-agent-control/10", r), "v10 mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/6", r), "v6 mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/11", r), "prior hand/ground-only schema rejected");
    check(!parseAgentRequest("H openxcom-agent-control/8", r), "v8 mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/7", r), "v7 mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/5", r), "v5 mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/4", r), "v4 mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/2", r), "v2 mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/3", r), "v3 mismatch rejected");
    check(!parseAgentRequest("H experimental-stance/1", r), "supervised mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/1", r), "archived version mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/9", r), "prior renderer schema rejected");
    check(!parseAgentRequest("H openxcom-agent-control/14\n", r), "newline rejected");
    check(parseAgentRequest("R " + token(1), r) && r.challenge == token(1), "read challenge");
    check(parseAgentRequest("S " + token(2), r) && r.id == token(2), "receipt only");
    const std::string eventPrefix = "V " + token(1) + " " + token(2) + " ";
    check(parseAgentRequest(eventPrefix + "0 16", r) && r.kind == 'V' && r.challenge == token(1) &&
        r.epoch == token(2) && r.after == 0 && r.limit == 16, "event cursor request");
    check(parseAgentRequest(eventPrefix + "9007199254740991 1", r), "event safe integer boundary");
    for (const char *suffix : {"00 1", "-1 1", "1 0", "1 17", "1 01", "1 1 extra", "9007199254740992 1"})
        check(!parseAgentRequest(eventPrefix + suffix, r), "event noncanonical cursor or limit rejected");
    const std::string prefix = "E " + token(3) + " " + token(4) + " " + token(5) + " ";
    const char *actions[] = {"next_soldier", "previous_soldier", "kneel", "stand", "continue_next_turn",
        "new_battle", "start_battle", "continue_briefing", "close_inventory",
        "open_new_game", "open_options", "open_load_game", "open_pause_menu", "open_save_game",
        "resume_game", "cancel_menu", "dismiss_error", "start_new_game", "confirm_base_name",
        "close_dropdown", "open_funding", "close_funding", "end_turn", "acknowledge_message", "close_debriefing",
        "open_inventory", "cancel_inventory_hold", "unload_inventory", "next_inventory_ground_page"};
    for (const char *action : actions)
        check(parseAgentRequest(prefix + "12 " + action, r) && r.kind == 'E' && r.revision == 12 &&
            r.id == token(3) && r.epoch == token(4) && r.capability == token(5), "bounded action parse");
    check(parseAgentRequest(prefix+"0 select_difficulty experienced",r) && r.choice=="experienced", "typed enum");
    check(!parseAgentRequest(prefix+"0 select_difficulty custom",r), "closed enum");
    check(!parseAgentRequest(prefix+"0 select_difficulty",r), "enum requires payload");
    check(parseAgentRequest(prefix+"0 set_ironman 1",r) && r.desired, "desired true");
    check(parseAgentRequest(prefix+"0 set_ironman 0",r) && !r.desired, "desired false");
    check(!parseAgentRequest(prefix+"0 set_ironman true",r), "canonical boolean");
    check(parseAgentRequest(prefix+"0 replace_base_name -",r) && r.text.empty(), "explicit empty edit");
    check(parseAgentRequest(prefix+"0 replace_base_name 41d0b1",r) && r.text=="Aб", "utf8 edit");
    check(parseAgentRequest(prefix+"0 replace_base_name "+std::string(256,'6'),r) && r.text.size()==128, "128 byte edit fits expanded packet");
    const char *badText[]={"0", "414A", "00", "0a", "09", "7f", "c280", "c29f", "c0af", "eda080", "f4908080", "80", "e282"};
    for(const char *hex:badText) check(!parseAgentRequest(prefix+"0 replace_base_name "+hex,r), "malformed or control text");
    check(!agentInputText(std::string(129,'x')), "129 byte text refused");
    check(parseAgentRequest(prefix+"0 place_base_site 0 4095",r) && r.x==0 && r.y==4095, "bounded point");
    const char *badPoints[]={"01 0","-1 0","0 4096","0 1.0","0","0 0 0","+1 2"};
    for(const char *point:badPoints) check(!parseAgentRequest(prefix+"0 place_base_site "+point,r), "noncanonical point");
    for (const char *control : {"mission", "craft", "terrain", "difficulty", "alien_race"})
        check(parseAgentRequest(prefix+"0 open_new_battle_choice "+control,r) && r.choice==control, "closed combo control");
    for (const char *control : {"all", "alien_tech", "", "mission 0"})
        check(!parseAgentRequest(prefix+"0 open_new_battle_choice "+control,r), "unlisted combo control rejected");
    for (const char *row : {"r0", "r9", "r31"})
        check(parseAgentRequest(prefix+"0 choose_visible_row "+row,r) && r.choice==row, "canonical local row");
    for (const char *row : {"r32", "r01", "r-1", "0", "r", "r1 2"})
        check(!parseAgentRequest(prefix+"0 choose_visible_row "+row,r), "malformed local row rejected");
    for (const char *direction : {"up", "down"})
        check(parseAgentRequest(prefix+"0 scroll_visible_list "+direction,r) && r.choice==direction, "one step direction");
    for (const char *direction : {"top", "bottom", "up 2", ""})
        check(!parseAgentRequest(prefix+"0 scroll_visible_list "+direction,r), "unbounded scroll rejected");
    for (const char *action : {"map_primary", "map_secondary"}) {
        for (const char *cell : {"g0", "g9", "g127"}) check(parseAgentRequest(prefix+"0 "+action+" "+cell,r), "listed grid syntax");
        for (const char *cell : {"g128", "g01", "g-1", "r0"}) check(!parseAgentRequest(prefix+"0 "+action+" "+cell,r), "invalid grid syntax");
    }
    for (const char *button : {"e0", "e9"}) check(parseAgentRequest(prefix+"0 center_enemy "+button,r), "enemy control syntax");
    for (const char *button : {"e10", "e01", "g0"}) check(!parseAgentRequest(prefix+"0 center_enemy "+button,r), "invalid enemy control");
    for (const char *row : {"a0", "a5"}) check(parseAgentRequest(prefix+"0 choose_weapon_action "+row,r), "fire row syntax");
    for (const char *row : {"a6", "a01", "snapshot"}) check(!parseAgentRequest(prefix+"0 choose_weapon_action "+row,r), "invalid fire row");
    for (const char *hand : {"left", "right"}) check(parseAgentRequest(prefix+"0 open_hand_action_menu "+hand,r), "hand syntax");
    check(!parseAgentRequest(prefix+"0 open_hand_action_menu weapon42",r), "no item identity");
    for (const char *action : {"open_inventory", "cancel_inventory_hold", "unload_inventory",
        "next_inventory_ground_page", "close_inventory"})
    {
        check(agentActionId(action) && std::string(agentArgumentKind(action)) == "none",
            "inventory control has no argument");
        check(!parseAgentRequest(prefix+"0 "+action+" v0",r), "inventory control rejects target argument");
        check(!parseAgentRequest(prefix+"0 "+action+" 1",r), "inventory control rejects extra argument");
    }
    for (const char *action : {"inventory_hover", "inventory_activate"})
    {
        check(agentActionId(action) && std::string(agentArgumentKind(action)) == "inventory_target",
            "inventory target argument kind");
        for (unsigned i = 0; i < 96; ++i)
        {
            const std::string target = "v" + std::to_string(i);
            check(parseAgentRequest(prefix+"9007199254740991 "+action+" "+target,r) && r.kind == 'E' &&
                r.action == action && r.choice == target && r.revision == 9007199254740991ULL &&
                r.id == token(3) && r.epoch == token(4) && r.capability == token(5),
                "all bounded local inventory targets preserve request identity");
        }
        check(!parseAgentRequest(prefix+"0 "+action,r), "inventory target argument required");
        for (const char *target : {"v96", "v127", "v18446744073709551616", "v00", "v01", "v063",
            "v-1", "v+1", "v1.0", "v1e0", "v0x1", "v", "V0", "0", "g0", "e0", "a0", "r0",
            "weapon42", "STR_RIFLE", "v0 v1", "v0 extra", " v0", "v0 ", "v0\t", "v0\n", "v0\r"})
            check(!parseAgentRequest(prefix+"0 "+action+" "+target,r),
                "noncanonical unbounded or nonlocal inventory target rejected");
    }
    check(!parseAgentRequest(prefix+"00 inventory_activate v0",r), "inventory revision remains canonical");
    check(!parseAgentRequest(prefix+"9007199254740992 inventory_activate v0",r),
        "inventory revision remains JSON-safe");
    check(!parseAgentRequest("H openxcom-agent-control/12",r), "old body-only schema rejected");
    check(!agentActionId("choose_fire_mode") && !parseAgentRequest(prefix+"0 choose_fire_mode a0",r), "old fire-only action removed");
    check(std::string(agentArgumentKind("choose_weapon_action"))=="weapon_action", "neutral weapon row kind");
    check(std::string(agentArgumentKind("choose_grenade_timer"))=="grenade_timer", "timer row kind");
    for (unsigned i=0;i<24;++i) {
        const std::string slot="t"+std::to_string(i);
        check(parseAgentRequest(prefix+"0 choose_grenade_timer "+slot,r) && r.choice==slot, "all canonical timer slots");
    }
    for(const char *slot:{"t24","t01","t-1","T0","0","a0","t","t0 extra","t0 "," t0"})
        check(!parseAgentRequest(prefix+"0 choose_grenade_timer "+slot,r), "noncanonical or unbounded timer rejected");
    check(parseAgentRequest(prefix+"0 cancel_grenade_timer",r), "ordinary timer cancel no args");
    check(!parseAgentRequest(prefix+"0 cancel_grenade_timer t0",r), "timer cancel rejects spoofed value");
    AgentLedger timerLedger;
    const std::string timerPacket=prefix+"0 choose_grenade_timer t0";
    check(timerLedger.begin(token(3),timerPacket)==AgentLedger::Fresh,"timer reserves shared identity");
    check(timerLedger.begin(token(3),timerPacket)==AgentLedger::Duplicate,"timer duplicate never repeats");
    check(timerLedger.begin(token(3),prefix+"0 choose_grenade_timer t1")==AgentLedger::Conflict,"changed timer conflicts");
    const std::string inventoryRequest = prefix+"0 inventory_activate v0";
    AgentLedger inventoryLedger;
    check(inventoryLedger.begin(token(3),inventoryRequest) == AgentLedger::Fresh, "inventory identity fresh");
    inventoryLedger.find(token(3))->dispatched = true;
    inventoryLedger.find(token(3))->response = "inventory receipt";
    check(inventoryLedger.begin(token(3),inventoryRequest) == AgentLedger::Duplicate,
        "inventory same ID never redispatched");
    check(inventoryLedger.begin(token(3),prefix+"0 inventory_activate v1") == AgentLedger::Conflict,
        "inventory changed target conflicts");
    check(inventoryLedger.begin(token(3),prefix+"0 inventory_hover v0") == AgentLedger::Conflict,
        "inventory changed action conflicts");
    check(inventoryLedger.find(token(3))->dispatched &&
        inventoryLedger.find(token(3))->response == "inventory receipt", "inventory conflicts preserve receipt");
    check(!parseAgentRequest("H openxcom-agent-control/6",r), "old6 rejected");
    check(!parseAgentRequest(prefix+"0 close_debriefing yes",r), "debrief action has no argument");
    check(!parseAgentRequest(prefix+"0 show_debrief_stats",r), "stats control unsupported");
    AgentLedger typed;
    const std::string first=prefix+"0 replace_base_name 41";
    check(typed.begin(token(3),first)==AgentLedger::Fresh, "typed identity fresh");
    check(typed.begin(token(3),first)==AgentLedger::Duplicate, "typed identity duplicate");
    check(typed.begin(token(3),prefix+"0 replace_base_name 42")==AgentLedger::Conflict, "changed argument conflicts");
    const char *bad[] = {"toggle_kneel", "map_click", "get_unit", "click", "approve", "inventory_item",
        "start_campaign", "load_game", "save_game", "overwrite_save", "delete_save", "abandon_game", "apply_options"};
    for (const char *action : bad) check(!parseAgentRequest(prefix + "12 " + action, r), "unsupported action");
    check(!parseAgentRequest(prefix + "012 kneel", r), "noncanonical revision");
    check(!parseAgentRequest(prefix + "-1 kneel", r), "negative revision");
    check(!parseAgentRequest(prefix + "9007199254740992 kneel", r), "unsafe JSON integer");
    check(!parseAgentRequest(prefix + "18446744073709551616 kneel", r), "integer overflow");
    check(parseAgentRequest(prefix + "9007199254740991 stand", r), "largest safe integer");
    check(!parseAgentRequest(prefix + "12 kneel true", r), "approval spoof extra token");
    check(!parseAgentRequest(" " + prefix + "12 kneel", r), "leading whitespace");
    check(!parseAgentRequest(prefix + "12  kneel", r), "double whitespace");
    check(!parseAgentRequest(prefix + "12 kneel ", r), "trailing whitespace");
    check(!parseAgentRequest(std::string(513, 'x'), r), "packet bound");
    check(!parseAgentRequest("R " + std::string(32, 'A'), r), "noncanonical token");
    check(!parseAgentRequest("R " + std::string(31, '0'), r), "short token");
    std::string nul = "R " + token(1); nul[3] = '\0';
    check(!parseAgentRequest(nul, r), "embedded NUL");
    check(agentJsonString("a\"\\\n\001") == "\"a\\\"\\\\\\u000a\\u0001\"", "JSON escaping");
    check(agentJsonString("Кнель") == "\"Кнель\"", "UTF-8 preserved");
    AgentLedger ledger;
    check(ledger.begin(token(1), "request") == AgentLedger::Fresh, "first command");
    ledger.find(token(1))->dispatched = true;
    ledger.find(token(1))->response = "receipt";
    check(ledger.begin(token(1), "request") == AgentLedger::Duplicate, "same ID never redispatched");
    check(ledger.begin(token(1), "changed") == AgentLedger::Conflict, "same ID payload conflict");
    check(ledger.find(token(1))->response == "receipt", "conflict preserves original receipt");
    for (unsigned i = 2; i <= 256; ++i)
        check(ledger.begin(token(i), "different") == AgentLedger::Fresh, "ledger capacity admitted");
    check(ledger.size() == 256, "exact ledger capacity");
    check(ledger.begin(token(257), "overflow") == AgentLedger::Full, "257th command rejected");
    check(ledger.size() == 256 && !ledger.find(token(257)), "full ledger never evicts");
    check(ledger.begin(token(1), "request") == AgentLedger::Duplicate &&
        ledger.find(token(1))->response == "receipt", "old command deduplicates after saturation");
    ledger.find(token(256))->response = "last receipt";
    check(ledger.begin(token(256), "different") == AgentLedger::Duplicate && ledger.find(token(256))->response == "last receipt", "last command retained at saturation");
    check(ledger.begin(token(256), "changed") == AgentLedger::Conflict && ledger.find(token(256))->response == "last receipt", "full-ledger last conflict preserves receipt");
    check(!ledger.find(token(999)), "unknown receipt has no entry");
    std::cout << "AgentProtocolTest PASS " << checks << " checks\n";
}
