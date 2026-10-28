// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<'\n';std::exit(1);}}
int main(){
 AgentRequest r;const std::string t(32,'1'),p="E "+t+" "+t+" "+t+" 1 ";
 check(parseAgentRequest("H openxcom-agent-control/37",r));
 check(!parseAgentRequest("H openxcom-agent-control/35",r));
 for(const std::string a:{"open_monthly_costs","close_monthly_costs"}){
  check(agentActionId(a));check(std::string(agentArgumentKind(a))=="none");
  check(parseAgentRequest(p+a,r));check(!parseAgentRequest(p+a+" extra",r));
 }
 for(const std::string a:{"pay_monthly_costs","set_salary","change_rent","monthly_costs"})check(!parseAgentRequest(p+a,r));
 AgentLedger ledger;const std::string action=p+"open_monthly_costs";
 check(ledger.begin(t,action)==AgentLedger::Fresh);
 check(ledger.begin(t,action)==AgentLedger::Duplicate);
 check(ledger.begin(t,p+"close_monthly_costs")==AgentLedger::Conflict);
 std::cout<<"PASS monthly-costs protocol36 "<<checks<<" checks\n";
}
