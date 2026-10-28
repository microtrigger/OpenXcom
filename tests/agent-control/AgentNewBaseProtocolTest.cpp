// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cassert>
#include <iostream>
using namespace OpenXcom::Observer;
int main(){AgentRequest r;const std::string t(32,'1'),p="E "+t+" 1 "+t+" "+t+" 1 ";
assert(parseAgentRequest("H openxcom-agent-control/43",r));assert(!parseAgentRequest("H openxcom-agent-control/34",r));
for(const std::string a:{"open_new_base","cancel_new_base","confirm_new_base","cancel_new_base_confirmation"}){assert(parseAgentRequest(p+a,r));assert(!parseAgentRequest(p+a+" extra",r));}
assert(std::string(agentArgumentKind("place_access_lift_cell"))=="facility_cell");
for(int y=0;y<6;++y)for(int x=0;x<6;++x){assert(parseAgentRequest(p+"place_access_lift_cell "+std::to_string(x)+" "+std::to_string(y),r));assert(r.x==x&&r.y==y);}
for(const std::string bad:{"", "0", "-1 0", "0 -1", "6 0", "0 6", "00 0", "0 00", "+1 0", "1.0 2", "r0", "1 1 extra", "999999999999999999999 0"})assert(!parseAgentRequest(p+"place_access_lift_cell "+bad,r));
for(const std::string bad:{"create_base","refund_new_base","cancel_place_lift","place_lift"})assert(!parseAgentRequest(p+bad,r));
assert(parseAgentRequest(p+"place_base_site 160 100",r));assert(parseAgentRequest(p+"replace_base_name 4d43502054776f",r));
AgentLedger ledger;const std::string a=p+"confirm_new_base";assert(ledger.begin(t,a)==AgentLedger::Fresh);assert(ledger.begin(t,a)==AgentLedger::Duplicate);assert(ledger.begin(t,p+"cancel_new_base_confirmation")==AgentLedger::Conflict);
std::cout<<"PASS newbase protocol35: strict liftcells, explicit site/cost/cancel/name, alias rejection and replay ledger\n";}
