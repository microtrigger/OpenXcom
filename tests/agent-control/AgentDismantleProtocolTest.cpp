// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cassert>
#include <iostream>
using namespace OpenXcom::Observer;
int main() {
 AgentRequest r;const std::string t(32,'1'),p="E "+t+" 1 "+t+" "+t+" 1 ";
 assert(parseAgentRequest("H openxcom-agent-control/46",r));
 assert(!parseAgentRequest("H openxcom-agent-control/33",r));
 for(const std::string a:{"confirm_dismantle_facility","cancel_dismantle_facility"}) {
  assert(parseAgentRequest(p+a,r)); assert(std::string(agentArgumentKind(a))=="none");
  assert(!parseAgentRequest(p+a+" 0 0",r));
 }
 assert(std::string(agentArgumentKind("click_base_facility_cell"))=="facility_cell");
 for(int y=0;y<6;++y)for(int x=0;x<6;++x) {
  assert(parseAgentRequest(p+"click_base_facility_cell "+std::to_string(x)+" "+std::to_string(y),r));assert(r.x==x&&r.y==y);
 }
 for(const std::string bad:{"","0","-1 0","0 -1","6 0","0 6","00 0","0 00","+1 0","1.0 2","r0","1 1 extra","999999999999999999999 0"})
  assert(!parseAgentRequest(p+"click_base_facility_cell "+bad,r));
 for(const std::string bad:{"dismantle_facility","abandon_base","confirm_abandon_base","dismantle_base_cell"})assert(!parseAgentRequest(p+bad,r));
 AgentLedger ledger;const std::string a=p+"confirm_dismantle_facility";
 assert(ledger.begin(t,a)==AgentLedger::Fresh);assert(ledger.begin(t,a)==AgentLedger::Duplicate);
 assert(ledger.begin(t,p+"cancel_dismantle_facility")==AgentLedger::Conflict);
 std::cout<<"PASS dismantle protocol34: 36 strict cells, explicit confirmation/cancel, malformed/alias rejection, replay ledger\n";
}
