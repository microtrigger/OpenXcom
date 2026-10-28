// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
namespace {unsigned checks=0;void check(bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<'\n';std::exit(1);}}}
int main()
{
 AgentRequest r; const std::string t(32,'1'),p="E "+t+" 1 "+t+" "+t+" 1 ";
 for(const std::string a:{"open_transfer","cancel_transfer_destination","review_transfer","cancel_transfer_items","confirm_transfer","cancel_transfer_confirmation"}){
  check(agentActionId(a));check(std::string(agentArgumentKind(a))=="none");
  check(parseAgentRequest(p+a,r)&&r.action==a);check(!parseAgentRequest(p+a+" extra",r));
 }
 check(std::string(agentArgumentKind("change_transfer_quantity"))=="transfer_quantity");
 for(unsigned i=0;i<32;++i){
  const std::string row="r"+std::to_string(i),prefix=p+"change_transfer_quantity "+row+" ";
  for(const std::string op:{"max","clear"})check(parseAgentRequest(prefix+op,r)&&r.choice==row&&r.operation==op);
  for(const std::string val:{"-1000","-1","1","1000"})check(parseAgentRequest(prefix+"adjust "+val,r)&&r.choice==row);
  for(const std::string val:{"0","1","100000"})check(parseAgentRequest(prefix+"set "+val,r)&&r.choice==row);
 }
 for(const std::string suffix:{"r32 max","r00 clear","r-1 max","r0 adjust 0","r0 adjust -0","r0 adjust 1001","r0 adjust -1001","r0 set -1","r0 set 100001","r0 set 01","r0 set +1","r0 set 1.0","r0 max 1","r0 clear extra","r0 set","r0 all","r0 adjust 1 extra"})check(!parseAgentRequest(p+"change_transfer_quantity "+suffix,r));
 for(const std::string a:{"transfer_all","set_transfer","open_transfer_items","transfer_to_base"})check(!agentActionId(a));
 std::cout<<"PASS "<<checks<<" transfer protocol checks\n";
}
