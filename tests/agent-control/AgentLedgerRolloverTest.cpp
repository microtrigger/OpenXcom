// SPDX-License-Identifier: GPL-3.0-or-later
// Protocol/ledger unit seam; controller transport/paint separately qualified.
#include "../../src/Observer/AgentProtocol.h"
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<'\n';std::exit(1);}}
static std::string token(unsigned n){std::ostringstream s;s<<std::hex<<std::setw(32)<<std::setfill('0')<<n;return s.str();}
int main(){
 AgentRequest r;const std::string id=token(1),epoch=token(2),cap=token(3),challenge=token(4);
 const std::string prefix="E "+id+" 1 "+epoch+" "+cap+" 12 ";
 check(std::string(agentProtocolSchema())=="openxcom-agent-control/41");
 check(parseAgentRequest("H openxcom-agent-control/41",r));check(!parseAgentRequest("H openxcom-agent-control/40",r));
 check(parseAgentRequest(prefix+"open_hand_action_menu right",r)&&r.commandGeneration==1&&r.choice=="right");
 check(parseAgentRequest(prefix+"end_turn",r)&&r.commandGeneration==1);
 check(!parseAgentRequest("E "+id+" "+epoch+" "+cap+" 12 end_turn",r));
 for(const std::string gen:{"0","01","-1","9007199254740992","x"}){
  check(!parseAgentRequest("E "+id+" "+gen+" "+epoch+" "+cap+" 12 end_turn",r));
  check(!parseAgentRequest("S "+gen+" "+id,r));
  check(!parseAgentRequest("L "+challenge+" "+epoch+" "+gen+" 256",r));
 }
 check(parseAgentRequest("S 2 "+id,r)&&r.kind=='S'&&r.commandGeneration==2&&r.id==id);
 check(!parseAgentRequest("S "+id,r));
 check(parseAgentRequest("L "+challenge+" "+epoch+" 1 256",r)&&r.kind=='L'&&r.acknowledgedCount==256);
 check(parseAgentRequest("K "+challenge+" "+epoch+" 1",r)&&r.kind=='K'&&r.acknowledgedCount==0);
 for(const std::string count:{"0","257","01","-1","1 extra"})check(!parseAgentRequest("L "+challenge+" "+epoch+" 1 "+count,r));
 check(!parseAgentRequest("K "+challenge+" "+epoch+" 1 256",r));
 AgentLedger ledger;check(!ledger.retireAcknowledged(0));
 for(unsigned i=1;i<=256;++i)check(ledger.begin(token(i),"request"+token(i))==AgentLedger::Fresh);
 check(ledger.begin(token(257),"new")==AgentLedger::Full);
 check(ledger.begin(token(1),"request"+token(1))==AgentLedger::Duplicate);
 check(ledger.begin(token(1),"changed")==AgentLedger::Conflict);
 check(!ledger.retireAcknowledged(256));
 for(unsigned i=1;i<=256;++i){ledger.find(token(i))->dispatched=true;ledger.find(token(i))->response="unknown_after_dispatch";}
 check(!ledger.retireAcknowledged(256)); // Early dispatch marker/unknown never permits retirement.
 for(unsigned i=1;i<=255;++i)ledger.find(token(i))->terminalDispatched=true;
 check(!ledger.retireAcknowledged(256));check(ledger.size()==256);
 ledger.find(token(256))->terminalDispatched=true;
 check(!ledger.retireAcknowledged(255));check(!ledger.retireAcknowledged(257));
 check(ledger.retireAcknowledged(256));check(ledger.size()==0);check(!ledger.find(token(1)));
 // New generation admission is controller-gated; ledger itself stores only current tuple namespace.
 check(ledger.begin(token(1),"new-generation-request")==AgentLedger::Fresh);
 std::cout<<"PASS ledger rollover "<<checks<<" checks\n";
}
