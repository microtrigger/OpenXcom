// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<'\n';std::exit(1);}}
int main(){AgentRequest r;const std::string t(32,'1'),p="E "+t+" 1 "+t+" "+t+" 1 ";
check(parseAgentRequest("H openxcom-agent-control/42",r));check(!parseAgentRequest("H openxcom-agent-control/38",r));
for(const std::string a:{"open_diary_combat","open_diary_awards","previous_diary_mission","next_diary_mission","close_diary_mission"}){check(parseAgentRequest(p+a,r));check(std::string(agentArgumentKind(a))=="none");check(!parseAgentRequest(p+a+" extra",r));}
check(std::string(agentArgumentKind("select_diary_mission"))=="project_row");
for(const std::string h:{"r0","r31"}){check(parseAgentRequest(p+"select_diary_mission "+h,r));check(r.choice==h);}
for(const std::string h:{"r32","r00","mission_1","-1","r0 extra",""})check(!parseAgentRequest(p+"select_diary_mission "+h,r));
for(const std::string a:{"set_diary_score","select_hidden_mission","read_enemy_graph","grant_award"})check(!parseAgentRequest(p+a,r));
AgentLedger ledger;const std::string a=p+"select_diary_mission r0";check(ledger.begin(t,a)==AgentLedger::Fresh);check(ledger.begin(t,a)==AgentLedger::Duplicate);check(ledger.begin(t,p+"select_diary_mission r1")==AgentLedger::Conflict);
std::cout<<"PASS diary branches protocol39 "<<checks<<" checks\n";}
