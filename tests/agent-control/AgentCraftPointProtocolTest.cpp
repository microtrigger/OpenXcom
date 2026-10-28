// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
 AgentRequest r;const std::string token(32,'1'),prefix="E "+token+" 1 "+token+" "+token+" 1 ";
 check(parseAgentRequest("H openxcom-agent-control/49",r),"schema49");
 check(!parseAgentRequest("H openxcom-agent-control/48",r),"old schema refused");
 check(std::string(agentArgumentKind("choose_visible_craft_point"))=="point","point kind");
 check(parseAgentRequest(prefix+"choose_visible_craft_point 120 120",r)&&r.x==120&&r.y==120,"canonical point request");
 for(const std::string bad:{"", "0", "-1 0", "0 -1", "00 0", "0 00", "+1 0", "1.0 2", "r0", "1 1 extra", "999999999999999999999 0"})
  check(!parseAgentRequest(prefix+"choose_visible_craft_point "+bad,r),"bad point refused");
 check(parseAgentRequest(prefix+"choose_visible_craft_destination r0",r),"marker action preserved");
 check(!parseAgentRequest(prefix+"choose_visible_craft_destination 120 120",r),"marker cannot accept point");
 check(!parseAgentRequest(prefix+"create_waypoint 120 120",r),"world mutation alias absent");
 std::cout<<"PASS "<<checks<<" craft point protocol checks\n";
}
