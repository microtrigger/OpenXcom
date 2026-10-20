// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentControllerTestDependencies.h"
#include "../../src/Observer/AgentController.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <iomanip>
#include <sstream>
using namespace OpenXcom;using namespace OpenXcom::Observer;
namespace OpenXcom { namespace Observer { std::uint64_t agentTestTime=10000;bool diagnosticProfileQualified(){return true;} } }
static unsigned checks=0;static bool pendingInput=false;static Uint8 focus=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;static SDLMod mods=KMOD_NONE;static Uint8 buttons=0;
extern "C" Uint8 SDL_GetAppState(){return focus;}
extern "C" SDLMod SDL_GetModState(){return mods;}
extern "C" Uint8 SDL_GetMouseState(int*,int*){return buttons;}
extern "C" void SDL_PumpEvents(){}
extern "C" int SDL_PeepEvents(SDL_Event*,int,SDL_eventaction action,Uint32){if(action!=SDL_PEEKEVENT)std::abort();return pendingInput?1:0;}
static void check(bool x,const char*m){++checks;if(!x){std::cerr<<m<<"\n";std::exit(1);}}
static std::string field(const std::string&s,const std::string&k){auto p=s.find("\""+k+"\":");check(p!=std::string::npos,"field exists");p+=k.size()+3;if(s[p]=='"'){auto e=s.find('"',p+1);return s.substr(p+1,e-p-1);}auto e=s.find_first_of(",}",p);return s.substr(p,e-p);}
static bool has(const std::string&s,const char*x){return s.find(x)!=std::string::npos;}
static std::string token(unsigned n){std::ostringstream o;o<<std::hex<<std::setw(32)<<std::setfill('0')<<n;return o.str();}
struct Harness {
 int peer=-1;State state;Game game;std::unique_ptr<AgentController> c;std::string epoch,grant,packet,id;
 Harness(){focus=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;mods=KMOD_NONE;buttons=0;pendingInput=false;agentTestTime+=10000;int f[2];check(socketpair(AF_UNIX,SOCK_SEQPACKET,0,f)==0,"private socketpair");peer=f[1];setenv("OPENXCOM_AGENT_CONTROL","1",1);setenv("OPENXCOM_AGENT_CONTROL_FD",std::to_string(f[0]).c_str(),1);unsetenv("OPENXCOM_STANCE_FD");game.top=&state;state.ui.screen="debriefing";state.ui.visible="{\"value\":0}";state.ui.points={{16,20}};state.ui.context={1};state.ui.actions.emplace_back("close_debriefing","OK","visible_text","none");c.reset(new AgentController(&game));epoch=field(receive(),"epoch");send("H openxcom-agent-control/8");c->poll();check(has(receive(),"\"ready\""),"ready handshake");read();}
 ~Harness(){c.reset();if(peer>=0)::close(peer);}
 void send(const std::string&s){check(::send(peer,s.data(),s.size(),0)==ssize_t(s.size()),"whole packet sent");}
 std::string receive(){char b[49152];auto n=recv(peer,b,sizeof(b),MSG_DONTWAIT);if(n<0){check(errno==EAGAIN||errno==EWOULDBLOCK,"nonblocking receive");return "";}return n?std::string(b,n):"EOF";}
 void frame(){c->beginFrame();c->presented();}
 void read(){send("R "+token(99));c->poll();frame();grant=receive();check(has(grant,"\"capability\":\""),"fresh grant");}
 void queue(unsigned n=1){id=token(n);packet="E "+id+" "+epoch+" "+field(grant,"capability")+" "+field(grant,"revision")+" close_debriefing";send(packet);c->poll();check(receive().empty(),"initial E stays silent while queued");check(!state.calls,"receipt does not dispatch");}
 void refused(){check(has(receive(),"\"status\":\"unavailable\""),"terminal predispatch refusal");check(!state.calls,"no dispatch on refusal");frame();check(!state.calls,"refusal cannot revive");}
};
int main(){
 {Harness h;h.state.dirty=true;h.queue();h.state.dirty=false;h.frame();check(h.state.calls==1,"dirty between-frame redraw repaired at fresh frame");check(h.receive().empty(),"preaction frame is not outcome");h.frame();std::string terminal=h.receive();check(has(terminal,"\"status\":\"dispatched\""),"later frame supplies outcome");h.send(h.packet);h.c->poll();check(h.receive()==terminal&&h.state.calls==1,"completed duplicate never repeats");}
 {Harness h;h.queue();h.send(h.packet);h.c->poll();check(has(h.receive(),"\"pending\""),"queued duplicate pending receipt");h.send("S "+h.id);h.c->poll();check(has(h.receive(),"\"pending\""),"S pending receipt");h.send("V "+token(90)+" "+h.epoch+" 0 16");h.c->poll();check(has(h.receive(),"\"events\""),"V works while queued");h.frame();check(h.state.calls==1,"duplicates and V preserve one dispatch");h.frame();check(has(h.receive(),"\"dispatched\""),"single terminal outcome");}
 {Harness h;h.queue();agentTestTime+=1001;h.c->poll();h.refused();}
 {Harness h;h.queue();agentTestTime+=500;h.send(h.packet);h.c->poll();h.receive();agentTestTime+=501;h.c->poll();h.refused();}
 {Harness h;h.queue();SDL_Event e={};e.type=SDL_MOUSEMOTION;h.c->beforeEvent(e);h.frame();h.refused();}
 {Harness h;h.queue();State away;h.game.top=&away;h.c->invalidate();h.game.top=&h.state;h.c->invalidate();h.frame();h.refused();}
 {Harness h;h.queue();h.game.screen.revision+=2;h.frame();h.refused();}
 {Harness h;h.queue();h.state.ui.actorRevision+=2;h.frame();h.refused();}
 {Harness h;h.queue();h.state.ui.context[0]+=2;h.frame();h.refused();}
 {Harness h;h.queue();h.state.ui.points[0].first++;h.frame();h.refused();}
 {Harness h;h.queue();h.state.ui.actions.clear();h.frame();h.refused();}
 {Harness h;h.queue();h.state.bindingValid=false;h.frame();h.refused();}
 {Harness h;h.queue();focus=0;h.c->poll();focus=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;h.refused();}
 {Harness h;h.queue();mods=KMOD_LALT;h.frame();h.refused();}
 {Harness h;h.queue();buttons=1;h.frame();h.refused();}
 {Harness h;h.queue();pendingInput=true;h.frame();pendingInput=false;h.refused();}
 {Harness h;h.queue();h.game.screen.ready=false;h.frame();h.refused();}
 {Harness h;h.queue();::close(h.peer);h.peer=-1;h.frame();check(!h.state.calls&&!h.c->enabled(),"EOF before frame cancels queue");h.frame();check(!h.state.calls,"EOF cannot revive");}
 {Harness h;h.queue();shutdown(h.peer,SHUT_WR);h.frame();check(!h.state.calls&&!h.c->enabled(),"half-close EOF cancels queue");}
 {Harness h;h.queue();h.c->close();h.frame();check(!h.state.calls,"explicit close/uncertainty cancellation cannot dispatch");}
 {Harness h;h.queue();h.state.handler=[&](const AgentRequest&r){check(r.action=="close_debriefing"&&r.choice.empty(),"immutable original args");h.state.ui.visible="{\"value\":1}";h.c->beginFrame();h.c->presented();check(h.receive().empty(),"recursive handler frame cannot provide outcome");};h.frame();check(h.state.calls==1&&h.receive().empty(),"handler returns without preaction outcome");h.frame();auto out=h.receive();check(has(out,"\"value\":1")&&has(out,"\"dispatched\""),"strict later frame observes action");}
 {Harness h;h.queue();h.frame();check(h.state.calls==1,"dispatch committed");::close(h.peer);h.peer=-1;h.c->poll();h.frame();check(h.state.calls==1&&!h.c->enabled(),"postdispatch EOF never replays");}
 {Harness h;h.queue();h.game.audio.events.push_back(AgentAudioEvents::Event());h.c->poll();h.frame();check(h.state.calls==1,"independent audio does not invalidate queued control");}
 {Harness h;agentTestTime+=119999;h.queue();agentTestTime+=2;h.c->poll();h.refused();}
 {Harness h;h.queue();agentTestTime+=1000;h.frame();check(h.state.calls==1,"queue exact deadline remains bounded and valid");}
 {Harness h;h.queue();agentTestTime+=1001;h.c->poll();std::string terminal=h.receive();check(has(terminal,"unavailable"),"expired queue terminal receipt");h.send("S "+h.id);h.c->poll();check(h.receive()==terminal,"expired receipt cached");h.send(h.packet);h.c->poll();check(h.receive()==terminal&&!h.state.calls,"expired duplicate cannot reschedule");}
 {Harness h;h.queue();h.send("V "+token(80)+" "+h.epoch+" 0 16");::close(h.peer);h.peer=-1;h.frame();check(!h.state.calls&&!h.c->enabled(),"EOF with unread packet cannot hide peer closure");}
 {Harness h;h.queue();h.send("R "+token(70));h.c->poll();check(has(h.receive(),"error"),"new read cannot grant over queued action");h.frame();h.refused();}
 {Harness h;h.queue();std::string other=h.packet;other.replace(2,32,token(2));h.send(other);h.c->poll();check(has(h.receive(),"unavailable"),"second command cannot occupy queue");h.frame();h.refused();}
 {Harness h;h.queue();agentObservePaint(-1);h.frame();check(!h.state.calls&&!h.c->enabled(),"paint failure prevents deferred dispatch");}
 std::cout<<"AgentDebriefControllerTest PASS "<<checks<<" checks\n";return 0;
}
