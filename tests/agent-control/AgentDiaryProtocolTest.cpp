// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<'\n';std::exit(1);}}
int main(){AgentRequest r;const std::string t(32,'1'),p="E "+t+" 1 "+t+" "+t+" 1 ";
 check(parseAgentRequest("H openxcom-agent-control/43",r));check(!parseAgentRequest("H openxcom-agent-control/37",r));
 for(const std::string a:{"open_soldier_diary","open_diary_performance","close_soldier_diary","close_diary_performance"}){
  check(agentActionId(a));check(std::string(agentArgumentKind(a))=="none");check(parseAgentRequest(p+a,r));check(!parseAgentRequest(p+a+" extra",r));check(!parseAgentRequest(p+a+" soldier_1",r));
 }
 for(const std::string a:{"set_diary_score","select_hidden_mission","open_diary_mission","set_soldier_history"})check(!parseAgentRequest(p+a,r));
 AgentLedger ledger;const std::string a=p+"open_soldier_diary";check(ledger.begin(t,a)==AgentLedger::Fresh);check(ledger.begin(t,a)==AgentLedger::Duplicate);check(ledger.begin(t,p+"close_soldier_diary")==AgentLedger::Conflict);
 std::cout<<"PASS diary base "<<agentProtocolSchema()<<" "<<checks<<" checks\n";
}
