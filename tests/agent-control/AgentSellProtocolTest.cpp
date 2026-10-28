// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cassert>
#include <iostream>
using namespace OpenXcom::Observer;
int main() {
 AgentRequest r; const std::string t(32,'1'), p="E "+t+" 1 "+t+" "+t+" 1 ";
 assert(parseAgentRequest("H openxcom-agent-control/47",r));
 assert(!parseAgentRequest("H openxcom-agent-control/30",r));
 for(const std::string a:{"open_sell","accept_sell","cancel_sell","open_sell_category"}) {
  assert(parseAgentRequest(p+a,r)); assert(!parseAgentRequest(p+a+" extra",r));
 }
 for(const std::string v:{"1","-1"}) {
  assert(parseAgentRequest(p+"change_sale_quantity r0 adjust "+v,r));
  assert(r.choice=="r0" && r.operation=="adjust" && r.value==(v=="1"?1:-1));
 }
 for(const std::string tail:{"r0 adjust 0","r0 adjust 2","r0 adjust -2","r0 adjust +1","r0 adjust 01","r0 set 1","r0 max","r0 clear","r32 adjust 1","r00 adjust 1","r0 adjust 1 extra","r0 adjust"})
  assert(!parseAgentRequest(p+"change_sale_quantity "+tail,r));
 for(const std::string d:{"up","down"}) {assert(parseAgentRequest(p+"scroll_sell "+d+" step",r));assert(r.direction==d && r.amount=="step");}
 for(const std::string tail:{"down page","down end","down","left step","down step 2"}) assert(!parseAgentRequest(p+"scroll_sell "+tail,r));
 std::cout<<"PASS closed Sell/Sack protocol schema31, normal-step quantities and scroll\n";
}
