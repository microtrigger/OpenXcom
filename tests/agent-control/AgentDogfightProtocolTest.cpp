// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
namespace {unsigned checks=0;void check(bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<'\n';std::exit(1);}}}
int main(){
 AgentRequest r;const std::string t(32,'1'),p="E "+t+" "+t+" "+t+" 1 ";
 check(agentActionId("choose_dogfight_mode"));check(std::string(agentArgumentKind("choose_dogfight_mode"))=="dogfight_mode");
 for(const std::string mode:{"standoff","cautious","standard","aggressive","disengage"}){
  check(parseAgentRequest(p+"choose_dogfight_mode "+mode,r)&&r.choice==mode&&r.action=="choose_dogfight_mode");
  for(const std::string extra:{" extra"," 0","\n","\t"," "})check(!parseAgentRequest(p+"choose_dogfight_mode "+mode+extra,r));
 }
 for(const std::string mode:{"","normal","attack","Standard","0","r0","standoff cautious","disengage\nconfirm","standard;attack"})check(!parseAgentRequest(p+"choose_dogfight_mode "+mode,r));
 check(!parseAgentRequest(p+"choose_dogfight_mode",r));
 for(const std::string action:{"dogfight_fire","destroy_ufo","set_ufo_health","set_distance","choose_dogfight_target","attack_ufo"})check(!agentActionId(action));
 std::cout<<"PASS "<<checks<<" dogfight protocol checks\n";
}
