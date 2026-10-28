// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cassert>
#include <iostream>
using namespace OpenXcom::Observer;
int main(){AgentRequest r;const std::string t(32,'1'),p="E "+t+" "+t+" "+t+" 1 toggle_kneel_reserve";
assert(parseAgentRequest("H openxcom-agent-control/36",r));assert(!parseAgentRequest("H openxcom-agent-control/29",r));
assert(parseAgentRequest(p,r));assert(r.action=="toggle_kneel_reserve");
for(const std::string s:{" on"," off"," 1"})assert(!parseAgentRequest(p+s,r));
std::cout<<"PASS closed kneel-reserve protocol\n";}
