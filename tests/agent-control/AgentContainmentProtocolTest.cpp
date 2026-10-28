// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<'\n';std::exit(1);}}
int main(){AgentRequest r;const std::string t(32,'1'),p="E "+t+" 1 "+t+" "+t+" 1 ";
check(parseAgentRequest("H openxcom-agent-control/48",r));check(!parseAgentRequest("H openxcom-agent-control/36",r));
for(const std::string a:{"remove_selected_aliens","cancel_containment"}){check(parseAgentRequest(p+a,r));check(!parseAgentRequest(p+a+" extra",r));}
for(const std::string a:{"increase_containment_removal","decrease_containment_removal"}){
 check(std::string(agentArgumentKind(a))=="project_row");
 for(const std::string h:{"r0","r31"}){check(parseAgentRequest(p+a+" "+h,r));check(r.choice==h);}
 for(const std::string h:{"r32","r00","STR_SECTOID_SOLDIER","r0 1","r0 extra",""})check(!parseAgentRequest(p+a+" "+h,r));
}
for(int y=0;y<6;++y)for(int x=0;x<6;++x){check(parseAgentRequest(p+"right_click_base_facility_cell "+std::to_string(x)+" "+std::to_string(y),r));check(r.x==x&&r.y==y);}
for(const std::string bad:{"","0","-1 0","0 -1","6 0","0 6","00 0","+1 0","1.0 2","r0","1 1 extra"})check(!parseAgentRequest(p+"right_click_base_facility_cell "+bad,r));
for(const std::string a:{"kill_alien","sell_captive","set_containment","containment_increase"})check(!parseAgentRequest(p+a+" r0",r));
AgentLedger ledger;const std::string a=p+"remove_selected_aliens";check(ledger.begin(t,a)==AgentLedger::Fresh);check(ledger.begin(t,a)==AgentLedger::Duplicate);check(ledger.begin(t,p+"cancel_containment")==AgentLedger::Conflict);
std::cout<<"PASS containment protocol37 "<<checks<<" checks\n";}
