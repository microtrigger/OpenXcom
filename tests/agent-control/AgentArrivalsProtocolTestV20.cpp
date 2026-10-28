// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
namespace { unsigned checks=0;void check(bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<'\n';std::exit(1);}} }
int main(){
 AgentRequest r;check(std::string(agentProtocolSchema())=="openxcom-agent-control/44");
 check(parseAgentRequest("H openxcom-agent-control/44",r)&&r.kind=='H');
 for(int n=1;n<=40;++n)check(!parseAgentRequest("H openxcom-agent-control/"+std::to_string(n),r));
 const std::string t(32,'1'),p="E "+t+" 1 "+t+" "+t+" 1 ";
 for(const std::string a:{"dismiss_items_arriving","open_arrival_base","dismiss_production_complete","open_production_destination","dismiss_ufo_contact","close_base_transfers","close_base_stores"}){
  check(parseAgentRequest(p+a,r)&&r.action==a);check(!parseAgentRequest(p+a+" extra",r));
 }
 check(parseAgentRequest(p+"scroll_visible_list up",r));check(parseAgentRequest(p+"scroll_visible_list down",r));
 for(const std::string a:{"scroll_visible_list top","select_arrival_row r0","open_arrival_base r0","H openxcom-agent-control/44\n"})check(!parseAgentRequest(p+a,r));
 std::cout<<"PASS "<<checks<<"\n";
}
