// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cassert>
#include <iostream>
using namespace OpenXcom::Observer;
int main() {
 AgentRequest r; const std::string t(32,'1'), p="E "+t+" 1 "+t+" "+t+" 1 acknowledge_crash_recovery";
 assert(parseAgentRequest("H openxcom-agent-control/46",r));
 assert(!parseAgentRequest("H openxcom-agent-control/29",r));
 assert(parseAgentRequest(p,r)); assert(r.action=="acknowledge_crash_recovery");
 for (const std::string suffix : {" 0", " hidden", " yes"}) assert(!parseAgentRequest(p+suffix,r));
 assert(!agentActionId("force_crash_recovery"));
 std::cout << "PASS closed crash recovery protocol\n";
}
