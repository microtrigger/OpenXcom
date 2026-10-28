// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
namespace {unsigned checks=0;void check(bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<'\n';std::exit(1);}}}
int main(){
 AgentRequest r;const std::string t(32,'1'),p="E "+t+" "+t+" "+t+" 1 ";
 check(std::string(agentProtocolSchema())=="openxcom-agent-control/39");
 check(parseAgentRequest("H openxcom-agent-control/39",r));check(!parseAgentRequest("H openxcom-agent-control/25",r));
 check(agentActionId("move_cursor"));check(std::string(agentArgumentKind("move_cursor"))=="cursor_position");
 for(int x:{0,1,319,4095})for(int y:{0,1,199,4095})check(parseAgentRequest(p+"move_cursor "+std::to_string(x)+" "+std::to_string(y),r)&&r.x==x&&r.y==y);
 for(const std::string bad:{"","0","0 0 1","-1 0","0 -1","4096 0","0 4096","01 0","0 00","true 0","1.0 0","+1 0","0x1 0","0 0 click","0\t0","0 0\n","999999999999999999999 0"})check(!parseAgentRequest(p+"move_cursor "+bad,r));
 for(const std::string alias:{"click_cursor","mouse_down","mouse_up","desktop_mouse","move_and_click"})check(!agentActionId(alias));
 std::cout<<"PASS "<<checks<<" cursor protocol checks\n";
}
