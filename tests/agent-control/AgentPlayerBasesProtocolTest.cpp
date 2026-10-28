// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cassert>
#include <iostream>
using namespace OpenXcom::Observer;
int main(){AgentRequest r;const std::string t(32,'a');
assert(parseAgentRequest("B "+t+" - 0 16",r));assert(r.kind=='B'&&r.challenge==t&&r.snapshot=="-"&&r.after==0&&r.limit==16);
assert(parseAgentRequest("B "+t+" "+t+" 100000 1",r));assert(r.snapshot==t&&r.after==100000);
for(const std::string tail:{"- 1 16","- 0 0","- 0 17","- 00 1","- -1 1","- 100001 1","bad 0 1","- 0 1 extra"})assert(!parseAgentRequest("B "+t+" "+tail,r));
assert(!parseAgentRequest("B bad - 0 1",r));assert(!parseAgentRequest("B  "+t+" - 0 1",r));
std::cout<<"PASS readonly player bases pagination wire; no action admission\n";}
