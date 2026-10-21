// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
namespace {unsigned checks=0;void check(bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<'\n';std::exit(1);}}}
int main(){
 AgentRequest r;const std::string t(32,'1'),p="E "+t+" "+t+" "+t+" 1 ";
 for(const std::string a:{"open_contact_interception","cancel_interception","open_intercept_base","confirm_interception","cancel_interception_confirmation"}){
  check(agentActionId(a));check(std::string(agentArgumentKind(a))=="none");check(parseAgentRequest(p+a,r)&&r.action==a);
  for(const std::string extra:{"r0","0","true","1 2","target","extra"})check(!parseAgentRequest(p+a+" "+extra,r));
 }
 for(int n=0;n<32;++n)check(parseAgentRequest(p+"choose_visible_row r"+std::to_string(n),r));
 for(const std::string row:{"r32","r00","r-1","UFO-1","1","c0","r0 extra"})check(!parseAgentRequest(p+"choose_visible_row "+row,r));
 for(const std::string a:{"intercept","intercept_ufo","center_ufo","dogfight_attack","destroy_target","select_world_target"})check(!agentActionId(a));
 std::cout<<"PASS "<<checks<<" interception protocol checks\n";
}
