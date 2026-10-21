// SPDX-License-Identifier: GPL-3.0-or-later
// Actual socket Controller regression with synthetic projections, NOT provider/live proof.
// Full-split policy is constructed explicitly; provider rendering is tested separately.
#include "DogfightTelemetryDependencies.generated.h"
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

static int actorA, actorB, saveA, saveB, buttonWidget, textWidget;
static std::string telemetry(const char *field="status") {
 std::string status=std::string(field)=="status"?"BREAKING OFF":"ATTACKING";
 std::string distance=std::string(field)=="distance"?"480":"560";
 std::string ammo=std::string(field)=="ammo"?"9":"10";
 return "{\"status\":\""+status+"\",\"distance\":\""+distance+"\",\"ammo1\":\""+ammo+"\",\"ammo2\":null,\"interception\":\"1\",\"damage\":null,\"buttons\":[{\"id\":\"standoff\",\"selected\":true},{\"id\":\"aggressive\",\"selected\":false}]}";
}
struct Harness {
 int peer=-1;State state;Game game;std::unique_ptr<AgentController> c;std::string epoch,grant,packet;
 Harness(int projection=0){
  agentTestTime+=10000;int f[2];check(socketpair(AF_UNIX,SOCK_SEQPACKET,0,f)==0,"socketpair");peer=f[1];setenv("OPENXCOM_AGENT_CONTROL","1",1);setenv("OPENXCOM_AGENT_CONTROL_FD",std::to_string(f[0]).c_str(),1);unsetenv("OPENXCOM_STANCE_FD");
  game.top=&state;state.ui.screen="dogfight";state.ui.actor=&actorA;state.ui.save=&saveA;
  state.ui.visible=telemetry("none");state.ui.rows={0,3};state.ui.regions={{10,10,20,10}};state.ui.widgets={{&buttonWidget,1}};
  state.ui.actions.emplace_back("choose_dogfight_mode","Choose dogfight mode","visible_control_semantics","dogfight_mode");
  if(projection)state.ui.controlVisible="{\"buttons\":[{\"id\":\"standoff\",\"selected\":true},{\"id\":\"aggressive\",\"selected\":false}]}";
  if(projection!=2){state.ui.widgets.push_back({&textWidget,1});state.ui.regions.push_back({40,40,20,8});}
  c.reset(new AgentController(&game));epoch=field(receive(),"epoch");send("H "+std::string(agentProtocolSchema()));c->poll();check(has(receive(),"\"ready\""),"handshake");
  send("R "+token(99));c->poll();frame();grant=receive();check(has(grant,"\"capability\":\""),"fresh grant");
 }
 ~Harness(){c.reset();if(peer>=0)::close(peer);}
 void send(const std::string&s){check(::send(peer,s.data(),s.size(),0)==ssize_t(s.size()),"whole packet");}
 std::string receive(){char b[49152];auto n=recv(peer,b,sizeof(b),MSG_DONTWAIT);if(n<0){check(errno==EAGAIN||errno==EWOULDBLOCK,"nonblocking receive");return "";}return n?std::string(b,n):"EOF";}
 void frame(){c->beginFrame();c->presented();}
 void queue(){packet="E "+token(1)+" "+epoch+" "+field(grant,"capability")+" "+field(grant,"revision")+" choose_dogfight_mode aggressive";send(packet);c->poll();check(receive().empty(),"queued command silent");check(state.calls==0,"queue does not dispatch");}
 void finish(bool expected,const std::string&scenario){
  frame();if(expected){check(state.calls==1,"hypothetical unchanged controls dispatch");check(receive().empty(),"later observation required");frame();auto result=receive();check(has(result,"\"status\":\"dispatched\""),"dispatched receipt");check(result.find(state.ui.visible)!=std::string::npos,"post-action receipt retains current telemetry");}
  else{auto result=receive();check(has(result,"\"status\":\"unavailable\""),"changed binding refused");check(state.calls==0,"refusal has zero dispatch");frame();check(state.calls==0,"refusal cannot revive");}
  std::cout<<scenario<<": dispatch_count="<<state.calls<<" expected="<<expected<<"\n";
 }
};
int main(){
 for(const char *field:{"status","distance","ammo"}){
  {Harness h;h.queue();h.state.ui.visible=telemetry(field);h.finish(false,std::string("DEFAULT full projection telemetry-only ")+field);}
  {Harness h(1);h.queue();h.state.ui.visible=telemetry(field);h.state.ui.widgets[1].second++;h.finish(false,std::string("PARTIAL controlVisible retains text revision ")+field);}
  {Harness h(1);h.queue();h.state.ui.visible=telemetry(field);h.state.ui.regions[1].width++;h.finish(false,std::string("PARTIAL controlVisible retains text geometry ")+field);}
  {Harness h(2);h.queue();h.state.ui.visible=telemetry(field);h.finish(true,std::string("HYPOTHETICAL split projection telemetry ")+field);}
 }
 for(int projection=0;projection<3;++projection){Harness h(projection);const auto before=h.state.ui.visible;h.queue();
  unsigned externalTextRevision=1; ++externalTextRevision;
  if(projection!=2)h.state.ui.widgets[1].second=externalTextRevision;
  check(h.state.ui.visible==before,"redundant text mutation leaves identical public payload");
  h.finish(projection==2,std::string(projection==0?"DEFAULT full projection":projection==1?"PARTIAL controlVisible":"HYPOTHETICAL split")+" identical text, presentation revision increment");
 }
 for(int scenario=0;scenario<9;++scenario){Harness h(2);h.queue();State away;
  const char *name="";
  switch(scenario){
   case 0:h.state.ui.controlVisible="{\"buttons\":[]}";name="mode selection/catalog";break;
   case 1:h.state.ui.widgets[0].second++;name="mode widget revision";break;
   case 2:h.state.ui.actor=&actorB;name="actor identity";break;
   case 3:h.state.ui.save=&saveB;name="target identity";break;
   case 4:h.state.ui.regions[0].x++;name="control geometry";break;
   case 5:h.game.top=&away;name="top state";break;
   case 6:h.state.dirty=true;name="modal/unavailable projection seam";break;
   case 7:h.state.bindingValid=false;name="provider binding invalid seam";break;
   case 8:h.state.ui.rows={0};name="available mode indices";break;
  }h.finish(false,std::string("HYPOTHETICAL rejects ")+name);
 }
 std::cout<<"PASS "<<checks<<" actual-controller deterministic checks; synthetic projections only\n";
}
