// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cassert>
#include <iostream>
using namespace OpenXcom::Observer;
int main(){AgentRequest r;const std::string t(32,'1'),p="E "+t+" "+t+" "+t+" 1 ";
assert(parseAgentRequest("H openxcom-agent-control/33",r));assert(!parseAgentRequest("H openxcom-agent-control/32",r));
for(const std::string a:{"open_build_facilities","close_build_facilities","cancel_place_facility"}){assert(parseAgentRequest(p+a,r));assert(!parseAgentRequest(p+a+" extra",r));}
for(const std::string h:{"r0","r31"}){assert(parseAgentRequest(p+"select_build_facility "+h,r));assert(r.choice==h);}
for(const std::string h:{"r32","r00","LAB","0 0","r0 extra",""})assert(!parseAgentRequest(p+"select_build_facility "+h,r));
for(const std::string a:{"place_facility","build_facility","construct_facility"})assert(!parseAgentRequest(p+a+" r0",r));
for(int y=0;y<6;++y)for(int x=0;x<6;++x){assert(parseAgentRequest(p+"place_facility_cell "+std::to_string(x)+" "+std::to_string(y),r));assert(r.x==x&&r.y==y);}
assert(std::string(agentArgumentKind("place_facility_cell"))=="facility_cell");
for(const std::string bad:{"", "0", "-1 0", "0 -1", "6 0", "0 6", "00 0", "0 00", "+1 0", "1.0 2", "r0", "1 1 extra", "999999999999999999999 0"})assert(!parseAgentRequest(p+"place_facility_cell "+bad,r));
AgentLedger ledger;const std::string a=p+"place_facility_cell 1 0";assert(ledger.begin(t,a)==AgentLedger::Fresh);assert(ledger.begin(t,a)==AgentLedger::Duplicate);assert(ledger.begin(t,p+"place_facility_cell 2 0")==AgentLedger::Conflict);
std::cout<<"PASS facility protocol33: preview/cancel and strict 36 logical placement cells, malformed bounds and replay ledger\n";}
