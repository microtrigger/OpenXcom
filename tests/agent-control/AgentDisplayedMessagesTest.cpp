// SPDX-License-Identifier: GPL-3.0-or-later
// Pure text/reducer/retention tests, never a simulated enemy action.
#include "../../src/Observer/AgentPublicEvents.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool b,const char *why) { ++checks; if(!b){std::cerr<<"FAIL "<<why<<'\n'; std::exit(1);} }
int main()
{
    const std::string a="MCP Test\nis under alien control", b="MCP Test\nis under Molecular Control";
    check(std::string(AgentDisplayedMessages::classify(a))=="alien_control","UFO exact visible phrase");
    check(std::string(AgentDisplayedMessages::classify(b))=="alien_control","TFTD exact visible phrase");
    check(std::string(AgentDisplayedMessages::classify("Éva\nhas Panicked"))=="panic","Unicode prefix exact panic");
    check(std::string(AgentDisplayedMessages::classify("X\nhas gone Berserk"))=="berserk","exact berserk");
    for(const auto &bad:{"\nis under alien control","X\nis under alien control.","X\nis under Alien Control","X\nY\nis under alien control","X\t\nis under alien control","X\r\nis under alien control","X\302\200\nis under alien control","X has Panicked","X\nhas panicked"})
        check(!AgentDisplayedMessages::classify(bad),"ambiguous wording is unclassified");
    check(!AgentDisplayedMessages::classify(std::string(1025,'x')+"\nhas Panicked"),"oversize classifier bounded");
    AgentDisplayedMessages r;
    const auto first=r.observe(a,false); check(first.find("\"report\":\"alien_control\"")!=std::string::npos,"message event reports public assertion");
    for(unsigned i=0;i<100;++i) check(r.observe(a,false).empty(),"held message dedup without poll");
    check(r.observe("",false).empty() && r.observe(a,false).empty(),"covered/failed/incomplete frame cannot prove disappearance");
    check(r.observe("",true).empty() && r.observe(a,false)==first,"known public absence then repeat creates new report");
    check(!r.observe(b,false).empty(),"public message change creates report");
    check(r.observe("unreviewed report",false).find("\"report\":null")!=std::string::npos,"unknown visible message retained without invented cause");
    AgentDisplayedMessages cause1,cause2;
    check(cause1.observe(a,false)==cause2.observe(a,false),"equal public text from different private causes identical (no cause input)");
    check(cause1.observe(std::string(1025,'x'),false).empty() && cause1.observe(a,false).empty(),"oversize unknown does not reset dedup");
    AgentPublicEvents ring;AgentPublicEvents::Page page;const std::string epoch(32,'a');
    check(ring.start(epoch),"start");
    check(ring.displayedMessage(a,false) && ring.newest()==1,"poll independent capture");
    for(unsigned i=0;i<100;++i)check(ring.displayedMessage(a,false)&&ring.newest()==1,"no hidden frame counter");
    check(ring.read(epoch,0,16,page)&&page.events.size()==1,"read after short popup already expired retains message");
    const auto record=page.events[0].json;
    check(record.find("\"source\":null,\"position\":null")!=std::string::npos,"no unseen source/location");
    check(ring.read(epoch,0,16,page)&&page.events[0].json==record,"replay immutable");
    for(unsigned i=0;i<260;++i){check(ring.displayedMessage("",true),"public absence");check(ring.displayedMessage(a,false),"later report");}
    check(ring.read(epoch,0,16,page)&&page.gap&&page.oldest==6&&page.newest==261,"bounded retention gap explicit");
    check(!ring.read(std::string(32,'b'),0,16,page),"wrong epoch");
    AgentRequest request;
    check(parseAgentRequest("H openxcom-agent-control/48",request),"current generation-bound protocol");
    for(unsigned v=1;v<41;++v)check(!parseAgentRequest("H openxcom-agent-control/"+std::to_string(v),request),"old ABI refused");
    check(parseAgentRequest("E "+epoch+" 1 "+epoch+" "+epoch+" 0 acknowledge_message",request),"bounded no-argument acknowledgement");
    check(!parseAgentRequest("E "+epoch+" 1 "+epoch+" "+epoch+" 0 acknowledge_message true",request),"no fake approval argument");
    std::cout<<"AgentDisplayedMessagesTest PASS "<<checks<<" checks\n";
}
