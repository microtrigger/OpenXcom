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
 Harness(){focus=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;mods=KMOD_NONE;buttons=0;pendingInput=false;agentTestTime+=10000;int f[2];check(socketpair(AF_UNIX,SOCK_SEQPACKET,0,f)==0,"private socketpair");peer=f[1];setenv("OPENXCOM_AGENT_CONTROL","1",1);setenv("OPENXCOM_AGENT_CONTROL_FD",std::to_string(f[0]).c_str(),1);unsetenv("OPENXCOM_STANCE_FD");game.top=&state;state.ui.screen="inventory";state.ui.visible="{\"value\":0}";state.ui.points={{16,20}};state.ui.context={1};state.ui.actions.emplace_back("inventory_activate","Activate inventory target","visible_control_semantics","inventory_target");c.reset(new AgentController(&game));epoch=field(receive(),"epoch");send("H openxcom-agent-control/13");c->poll();check(has(receive(),"\"ready\""),"ready handshake");read();}
 ~Harness(){c.reset();if(peer>=0)::close(peer);}
 void send(const std::string&s){check(::send(peer,s.data(),s.size(),0)==ssize_t(s.size()),"whole packet sent");}
 std::string receive(){char b[49152];auto n=recv(peer,b,sizeof(b),MSG_DONTWAIT);if(n<0){check(errno==EAGAIN||errno==EWOULDBLOCK,"nonblocking receive");return "";}return n?std::string(b,n):"EOF";}
 void frame(){c->beginFrame();c->presented();}
 void read(){send("R "+token(99));c->poll();frame();grant=receive();check(has(grant,"\"capability\":\""),"fresh grant");}
 void queue(unsigned n=1){id=token(n);packet="E "+id+" 1 "+epoch+" "+field(grant,"capability")+" "+field(grant,"revision")+" inventory_activate v0";send(packet);c->poll();check(receive().empty(),"initial E stays silent while queued");check(!state.calls,"receipt does not dispatch");}
 void queueAction(const std::string &action,const std::string &arg,unsigned n=1){id=token(n);packet="E "+id+" 1 "+epoch+" "+field(grant,"capability")+" "+field(grant,"revision")+" "+action+(arg.empty()?"":" "+arg);send(packet);c->poll();check(receive().empty(),"typed E silent while queued");check(!state.calls,"typed receipt does not dispatch");}
 void refused(){check(has(receive(),"\"status\":\"unavailable\""),"terminal predispatch refusal");check(!state.calls,"no dispatch on refusal");frame();check(!state.calls,"refusal cannot revive");}
};
int main(){
 {Harness h;h.state.dirty=true;h.queue();h.state.dirty=false;h.frame();check(h.state.calls==1,"dirty between-frame redraw repaired at fresh frame");check(h.receive().empty(),"preaction frame is not outcome");h.frame();std::string terminal=h.receive();check(has(terminal,"\"status\":\"dispatched\""),"later frame supplies outcome");h.send(h.packet);h.c->poll();check(h.receive()==terminal&&h.state.calls==1,"completed duplicate never repeats");}
 {Harness h;h.queue();h.send(h.packet);h.c->poll();check(has(h.receive(),"\"pending\""),"queued duplicate pending receipt");h.send("S 1 "+h.id);h.c->poll();check(has(h.receive(),"\"pending\""),"S pending receipt");h.send("V "+token(90)+" "+h.epoch+" 0 16");h.c->poll();check(has(h.receive(),"\"events\""),"V works while queued");h.frame();check(h.state.calls==1,"duplicates and V preserve one dispatch");h.frame();check(has(h.receive(),"\"dispatched\""),"single terminal outcome");}
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
 {Harness h;h.queue();h.state.handler=[&](const AgentRequest&r){check(r.action=="inventory_activate"&&r.choice=="v0","immutable original args");h.state.ui.visible="{\"value\":1}";h.c->beginFrame();h.c->presented();check(h.receive().empty(),"recursive handler frame cannot provide outcome");};h.frame();check(h.state.calls==1&&h.receive().empty(),"handler returns without preaction outcome");h.frame();auto out=h.receive();check(has(out,"\"value\":1")&&has(out,"\"dispatched\""),"strict later frame observes action");}
 {Harness h;h.queue();h.frame();check(h.state.calls==1,"dispatch committed");::close(h.peer);h.peer=-1;h.c->poll();h.frame();check(h.state.calls==1&&!h.c->enabled(),"postdispatch EOF never replays");}
 {Harness h;h.queue();h.game.audio.events.push_back(AgentAudioEvents::Event());h.c->poll();h.frame();check(h.state.calls==1,"independent audio does not invalidate queued control");}
 {Harness h;agentTestTime+=119999;h.queue();agentTestTime+=2;h.c->poll();h.refused();}
 {Harness h;h.queue();agentTestTime+=1000;h.frame();check(h.state.calls==1,"queue exact deadline remains bounded and valid");}
 {Harness h;h.queue();agentTestTime+=1001;h.c->poll();std::string terminal=h.receive();check(has(terminal,"unavailable"),"expired queue terminal receipt");h.send("S 1 "+h.id);h.c->poll();check(h.receive()==terminal,"expired receipt cached");h.send(h.packet);h.c->poll();check(h.receive()==terminal&&!h.state.calls,"expired duplicate cannot reschedule");}
 {Harness h;h.queue();h.send("V "+token(80)+" "+h.epoch+" 0 16");::close(h.peer);h.peer=-1;h.frame();check(!h.state.calls&&!h.c->enabled(),"EOF with unread packet cannot hide peer closure");}
 {Harness h;h.queue();h.send("R "+token(70));h.c->poll();check(has(h.receive(),"error"),"new read cannot grant over queued action");h.frame();h.refused();}
 {Harness h;h.queue();std::string other=h.packet;other.replace(2,32,token(2));h.send(other);h.c->poll();check(has(h.receive(),"unavailable"),"second command cannot occupy queue");h.frame();h.refused();}
 {Harness h;
   const auto basePayload=h.state.ui.payload(), baseAuthority=h.state.ui.authorityPayload();
   const auto binding=h.state.ui;
   h.state.ui.projectileFact=AgentProjectileFact::Presented;
   check(h.state.ui.payload()==basePayload&&h.state.ui.authorityPayload()==baseAuthority&&h.state.ui.sameBinding(binding),"projectile event fact has no control authority");
   h.frame();h.send("V "+token(91)+" "+h.epoch+" 0 16");h.c->poll();auto first=h.receive();
   check(has(first,"projectile_presented"),"actual controller commits event at admitted frame");
   h.queue();h.frame();check(h.state.calls==1,"R to event frame to V to E preserves grant");h.frame();check(has(h.receive(),"dispatched"),"event does not alter queued outcome");
   h.send("V "+token(92)+" "+h.epoch+" 0 16");h.c->poll();auto replay=h.receive();
   check(field(first,"newest")==field(replay,"newest"),"repeated frames do not advance public event sequence");
 }
 {Harness h;h.state.ui.projectileFact=AgentProjectileFact::Presented;h.game.screen.ready=false;h.frame();h.game.screen.ready=true;
   h.send("V "+token(93)+" "+h.epoch+" 0 16");h.c->poll();check(!has(h.receive(),"projectile_presented"),"failed flip cannot publish supplied fact");
 }
 {Harness h;h.queue();h.state.handler=[&](const AgentRequest&){h.state.ui.projectileFact=AgentProjectileFact::Presented;h.c->beginFrame();h.c->presented();};
   h.frame();h.send("V "+token(94)+" "+h.epoch+" 0 16");h.c->poll();check(!has(h.receive(),"projectile_presented"),"reentrant handler frame cannot emit event");
   h.frame();check(has(h.receive(),"dispatched"),"later frame completes handler");h.send("V "+token(95)+" "+h.epoch+" 0 16");h.c->poll();check(has(h.receive(),"projectile_presented"),"later nonreentrant frame emits event");
 }

 for (int kind=0;kind<2;++kind) {
  Harness h; auto &fact=kind?h.state.ui.collapseFact:h.state.ui.impactFact;
  const char *name=kind?"collapse_animation_presented":"impact_effect_presented";
  const auto binding=h.state.ui;fact=AgentProjectileFact::Presented;
  check(h.state.ui.sameBinding(binding)&&h.state.ui.payload()==binding.payload(),"new renderer fact has no control authority");
  h.frame();h.send("V "+token(96)+" "+h.epoch+" 0 16");h.c->poll();auto first=h.receive();check(has(first,name),"new kind retained through real controller");
  h.queue();h.frame();h.frame();check(has(h.receive(),"dispatched"),"new kinds preserve R-V-E queued grant");
  h.send("V "+token(97)+" "+h.epoch+" 0 16");h.c->poll();check(field(first,"newest")==field(h.receive(),"newest"),"new kinds dedup unchanged frames");
 }
 for (int kind=0;kind<2;++kind) {
  Harness h;auto &fact=kind?h.state.ui.collapseFact:h.state.ui.impactFact;
  const char *name=kind?"collapse_animation_presented":"impact_effect_presented";
  fact=AgentProjectileFact::Presented;h.game.screen.ready=false;h.frame();h.game.screen.ready=true;
  h.send("V "+token(96)+" "+h.epoch+" 0 16");h.c->poll();check(!has(h.receive(),name),"failed final frame suppresses new kind");
 }
 for (int kind=0;kind<2;++kind) {
  Harness h;auto &fact=kind?h.state.ui.collapseFact:h.state.ui.impactFact;
  const char *name=kind?"collapse_animation_presented":"impact_effect_presented";
  h.queue();h.state.handler=[&](const AgentRequest&){fact=AgentProjectileFact::Presented;h.c->beginFrame();h.c->presented();};
  h.frame();h.send("V "+token(96)+" "+h.epoch+" 0 16");h.c->poll();check(!has(h.receive(),name),"reentrant handler cannot publish new kind");
  h.frame();check(has(h.receive(),"dispatched"),"new kind result waits for later frame");h.send("V "+token(97)+" "+h.epoch+" 0 16");h.c->poll();check(has(h.receive(),name),"later final frame commits new kind");
 }
 {Harness h;h.state.ui.widgets.push_back(std::make_pair(&h.state,1));h.read();h.queue();h.state.ui.widgets[0].second+=2;h.frame();h.refused();}
 {Harness h;int heldA=0,heldB=0;h.state.ui.widgets.push_back(std::make_pair(&heldA,1));h.read();h.queue();h.state.ui.widgets[0].first=&heldB;h.frame();h.refused();}
 {Harness h;h.queue();h.state.ui.visible="{\"held\":null,\"targets\":[]}";h.frame();h.refused();}
 {Harness h;h.state.ui.actions.clear();h.state.ui.actions.emplace_back("cancel_inventory_hold","Return held item","visible_control_semantics","none");h.read();
  h.id=token(1);h.packet="E "+h.id+" 1 "+h.epoch+" "+field(h.grant,"capability")+" "+field(h.grant,"revision")+" cancel_inventory_hold";h.send(h.packet);h.c->poll();check(h.receive().empty(),"held-return queued without dispatch");h.frame();h.frame();const auto result=h.receive();check(has(result,"dispatched")&&h.state.calls==1,"held-return handler once");h.send(h.packet);h.c->poll();check(h.receive()==result&&h.state.calls==1,"duplicate held return never primes/closes/dispatches again");}

 for(const char *action:{"choose_weapon_action","choose_grenade_timer","cancel_grenade_timer"}) {
  Harness h;const bool timer=std::string(action)=="choose_grenade_timer",cancel=std::string(action)=="cancel_grenade_timer";
  h.state.ui.screen=timer||cancel?"prime_grenade":"weapon_action_menu";
  h.state.ui.visible=timer||cancel?"{\"title\":\"Set Timer\",\"timers\":[{\"handle\":\"t0\",\"label\":\"0\"}]}":"{\"rows\":[{\"handle\":\"a0\",\"label\":\"Prime Grenade\",\"accuracy\":null,\"time_units\":null}]}";
  h.state.ui.actions.clear();h.state.ui.actions.emplace_back(action,"Shown control",cancel?"visible_control_semantics":"visible_text",cancel?"none":timer?"grenade_timer":"weapon_action");h.state.ui.rows={0};h.state.ui.context={1};h.read();
  h.send("V "+token(90)+" "+h.epoch+" 0 16");h.c->poll();check(has(h.receive(),"events"),"journal read preserves timer/menu grant");
  h.queueAction(action,cancel?"":timer?"t0":"a0");h.frame();check(h.state.calls==1&&h.receive().empty(),"new typed action dispatches only at frame");
  h.state.ui.screen="battlescape";h.state.ui.visible="{\"message\":null}";h.state.ui.actions.clear();h.frame();const auto terminal=h.receive();
  check(has(terminal,"dispatched")&&!has(terminal,"GRENADE_IS_ACTIVATED"),"timer receipt has no fabricated priming success");
  h.send(h.packet);h.c->poll();check(h.receive()==terminal&&h.state.calls==1,"typed terminal duplicate is receipt-only");
  h.send("S 1 "+h.id);h.c->poll();check(h.receive()==terminal&&h.state.calls==1,"typed result read never replays");
 }
 for(int variant=0;variant<5;++variant){Harness h;h.state.ui.screen="prime_grenade";h.state.ui.actions.clear();h.state.ui.actions.emplace_back("choose_grenade_timer","0","visible_text","grenade_timer");h.state.ui.rows={0};h.state.ui.widgets={{&h.state,1}};h.read();h.queueAction("choose_grenade_timer","t0");
  if(variant==0)h.state.ui.rows[0]=1;
  if(variant==1)h.state.ui.widgets[0].second+=2;
  if(variant==2){h.c->invalidate();h.c->invalidate();}
  if(variant==3)h.state.ui.actions.clear();
  if(variant==4)h.game.screen.ready=false;
  h.frame();h.refused();
 }
 {Harness h;h.state.ui.screen="prime_grenade";h.state.ui.actions.clear();h.state.ui.actions.emplace_back("choose_grenade_timer","0","visible_text","grenade_timer");h.read();h.queueAction("choose_grenade_timer","t0");::close(h.peer);h.peer=-1;h.frame();check(!h.state.calls&&!h.c->enabled(),"queued timer EOF never arms later");}
 {Harness h;h.queue();agentObservePaint(-1);h.frame();check(!h.state.calls&&!h.c->enabled(),"paint failure prevents deferred dispatch");}
 std::cout<<"AgentGrenadeControllerTest PASS "<<checks<<" checks\n";return 0;
}
