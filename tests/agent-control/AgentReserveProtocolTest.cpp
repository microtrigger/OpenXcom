// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cassert>
#include <iostream>
using namespace OpenXcom::Observer;
int main() {
 AgentRequest r; const std::string t(32, '1'), p = "E " + t + " 1 " + t + " " + t + " 1 ";
 assert(parseAgentRequest("H openxcom-agent-control/42", r));
 assert(!parseAgentRequest("H openxcom-agent-control/29", r));
 for (const std::string action : {"reserve_none", "reserve_snap", "reserve_aimed", "reserve_auto"}) {
  assert(agentActionId(action));
  assert(parseAgentRequest(p + action, r));
  assert(r.action == action);
  for (const std::string suffix : {" none", " snap", " 0", " hidden"})
   assert(!parseAgentRequest(p + action + suffix, r));
 }
 for (const std::string action : {"reserve_kneel", "set_reserved_tu"})
  assert(!agentActionId(action));
 std::cout << "PASS reserve protocol, closed no-argument commands and schema29 refusal\n";
}
