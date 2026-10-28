// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<'\n';std::exit(1);}}
int main(){
 AgentRequest r;const std::string token(32,'1'),prefix="E "+token+" 1 "+token+" "+token+" 1 ";
 check(parseAgentRequest("H openxcom-agent-control/45",r));check(!parseAgentRequest("H openxcom-agent-control/39",r));
 for(const std::string action:{"open_memorial","close_memorial"}){
  check(agentActionId(action));check(std::string(agentArgumentKind(action))=="none");
  check(parseAgentRequest(prefix+action,r));check(!parseAgentRequest(prefix+action+" extra",r));
 }
 check(agentActionId("select_memorial_soldier"));check(std::string(agentArgumentKind("select_memorial_soldier"))=="project_row");
 for(const std::string row:{"r0","r1","r31"})check(parseAgentRequest(prefix+"select_memorial_soldier "+row,r)&&r.choice==row);
 for(const std::string row:{"","r-1","r32","r01","soldier_1","r0 extra"})check(!parseAgentRequest(prefix+"select_memorial_soldier "+row,r));
 for(const std::string action:{"revive_soldier","rename_memorial_soldier","assign_dead_soldier","open_memorial_statistics"})check(!parseAgentRequest(prefix+action,r));
 AgentLedger ledger;const auto packet=prefix+"select_memorial_soldier r0";
 check(ledger.begin(token,packet)==AgentLedger::Fresh);check(ledger.begin(token,packet)==AgentLedger::Duplicate);
 check(ledger.begin(token,prefix+"select_memorial_soldier r1")==AgentLedger::Conflict);
 std::cout<<"PASS memorial "<<agentProtocolSchema()<<" "<<checks<<" checks\n";
}
