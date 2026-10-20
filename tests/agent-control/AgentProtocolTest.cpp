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
    check(parseAgentRequest("H openxcom-agent-control/4", r) && r.kind == 'H', "exact handshake");
    check(!parseAgentRequest("H openxcom-agent-control/2", r), "v2 mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/3", r), "v3 mismatch rejected");
    check(!parseAgentRequest("H experimental-stance/1", r), "supervised mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/1", r), "archived version mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/4\n", r), "newline rejected");
    check(parseAgentRequest("R " + token(1), r) && r.challenge == token(1), "read challenge");
    check(parseAgentRequest("S " + token(2), r) && r.id == token(2), "receipt only");
    const std::string prefix = "E " + token(3) + " " + token(4) + " " + token(5) + " ";
    const char *actions[] = {"next_soldier", "previous_soldier", "kneel", "stand", "continue_next_turn",
        "new_battle", "start_battle", "continue_briefing", "close_inventory",
        "open_new_game", "open_options", "open_load_game", "open_pause_menu", "open_save_game",
        "resume_game", "cancel_menu", "dismiss_error", "start_new_game", "confirm_base_name",
        "close_dropdown", "open_funding", "close_funding"};
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
    AgentLedger typed;
    const std::string first=prefix+"0 replace_base_name 41";
    check(typed.begin(token(3),first)==AgentLedger::Fresh, "typed identity fresh");
    check(typed.begin(token(3),first)==AgentLedger::Duplicate, "typed identity duplicate");
    check(typed.begin(token(3),prefix+"0 replace_base_name 42")==AgentLedger::Conflict, "changed argument conflicts");
    const char *bad[] = {"toggle_kneel", "end_turn", "get_unit", "click", "approve", "inventory_item",
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
    for (unsigned i = 2; i <= 64; ++i)
        check(ledger.begin(token(i), "different") == AgentLedger::Fresh, "ledger capacity admitted");
    check(ledger.size() == 64, "exact ledger capacity");
    check(ledger.begin(token(65), "overflow") == AgentLedger::Full, "65th command rejected");
    check(ledger.size() == 64 && !ledger.find(token(65)), "full ledger never evicts");
    check(ledger.begin(token(1), "request") == AgentLedger::Duplicate &&
        ledger.find(token(1))->response == "receipt", "old command deduplicates after saturation");
    check(!ledger.find(token(999)), "unknown receipt has no entry");
    std::cout << "AgentProtocolTest PASS " << checks << " checks\n";
}
