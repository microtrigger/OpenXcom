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
    check(parseAgentRequest("H openxcom-agent-control/1", r) && r.kind == 'H', "exact handshake");
    check(!parseAgentRequest("H experimental-stance/1", r), "supervised mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/2", r), "version mismatch rejected");
    check(!parseAgentRequest("H openxcom-agent-control/1\n", r), "newline rejected");
    check(parseAgentRequest("R " + token(1), r) && r.challenge == token(1), "read challenge");
    check(parseAgentRequest("S " + token(2), r) && r.id == token(2), "receipt only");
    const std::string prefix = "E " + token(3) + " " + token(4) + " " + token(5) + " ";
    const char *actions[] = {"next_soldier", "previous_soldier", "kneel", "stand", "continue_next_turn",
        "new_battle", "start_battle", "continue_briefing", "close_inventory"};
    for (const char *action : actions)
        check(parseAgentRequest(prefix + "12 " + action, r) && r.kind == 'E' && r.revision == 12 &&
            r.id == token(3) && r.epoch == token(4) && r.capability == token(5), "bounded action parse");
    const char *bad[] = {"toggle_kneel", "end_turn", "get_unit", "click", "approve", "inventory_item"};
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
    check(!parseAgentRequest(std::string(257, 'x'), r), "packet bound");
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
