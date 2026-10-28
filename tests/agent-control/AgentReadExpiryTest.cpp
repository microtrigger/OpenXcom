// SPDX-License-Identifier: GPL-3.0-or-later
// Actual socket Controller regression with synthetic projections, NOT provider/live proof.
// Fake clock and Game/State/Screen/input/audio seams; real parser/socket/controller expiry.
#include "CursorDependencies.generated.h"
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
 int peer=-1; State state; Game game; std::unique_ptr<AgentController> c;
 std::string epoch, grant;
 Harness(){
  agentTestTime+=10000; pendingInput=false; buttons=0; mods=KMOD_NONE;
  focus=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;
  int f[2];check(socketpair(AF_UNIX,SOCK_SEQPACKET,0,f)==0,"socketpair");peer=f[1];
  setenv("OPENXCOM_AGENT_CONTROL","1",1);setenv("OPENXCOM_AGENT_CONTROL_FD",std::to_string(f[0]).c_str(),1);unsetenv("OPENXCOM_STANCE_FD");
  game.top=&state;state.ui.screen="basescape";state.ui.visible="{\"name\":\"TEST\"}";
  state.ui.actions.emplace_back("return_geoscape","Return","visible_control_semantics","none");
  c.reset(new AgentController(&game));epoch=field(receive(),"epoch");
  send("H "+std::string(agentProtocolSchema()));c->poll();
  check(receive()==agentEnvelope()+",\"status\":\"ready\"}","ordinary handshake unchanged");
  begin(99);frame();grant=receive();check(has(grant,"\"capability\":\""),"initial fresh grant");
 }
 ~Harness(){c.reset();if(peer>=0)::close(peer);}
 void send(const std::string&s){check(::send(peer,s.data(),s.size(),0)==ssize_t(s.size()),"whole packet");}
 std::string receive(){char b[49152];auto n=recv(peer,b,sizeof(b),MSG_DONTWAIT);if(n<0){check(errno==EAGAIN||errno==EWOULDBLOCK,"nonblocking receive");return "";}return n?std::string(b,n):"EOF";}
 void frame(){c->beginFrame();c->presented();}
 void begin(unsigned n){send("R "+token(n));c->poll();check(receive().empty(),"pending R silent");}
 std::string expected(unsigned n){return agentEnvelope()+",\"type\":\"read_error\",\"read_error_schema\":\"openxcom-agent-read-error/1\",\"epoch\":"+agentJsonString(epoch)+",\"challenge\":"+agentJsonString(token(n))+",\"reason\":\"read_expired\"}";}
};
int main(){
 {Harness h;h.begin(1);agentTestTime+=1000;h.c->poll();check(h.receive().empty(),"exact existing deadline not expired");h.frame();auto s=h.receive();check(field(s,"challenge")==token(1)&&field(s,"epoch")==h.epoch,"ordinary state correlation");check(field(s,"capability")!=field(h.grant,"capability"),"normal fresh grant");agentTestTime++;h.c->poll();check(h.receive().empty(),"fulfilled read never expires");}
 {Harness h;h.begin(2);agentTestTime+=1001;h.c->poll();auto e=h.receive();check(e==h.expected(2),"exact bounded expiry envelope with copied challenge");check(e.size()<300,"fixed expiry packet bound");check(h.c->enabled()&&h.state.calls==0,"expiry no game action or close");h.c->poll();h.frame();check(h.receive().empty(),"cleared pending read emits once and no stale state");h.begin(3);h.frame();auto fresh=h.receive();check(field(fresh,"challenge")==token(3)&&field(fresh,"epoch")==h.epoch,"same session accepts distinct new R");check(field(fresh,"capability")!=field(h.grant,"capability"),"expired grant never reused");}
 {Harness h;h.begin(4);agentTestTime+=1001;h.c->poll();check(h.receive()==h.expected(4),"expiry before stale E");std::string id=token(45);std::string e="E "+id+" 1 "+h.epoch+" "+field(h.grant,"capability")+" "+field(h.grant,"revision")+" return_geoscape";h.send(e);h.c->poll();auto refusal=h.receive();check(has(refusal,"\"status\":\"unavailable\""),"old capability rejected");h.frame();check(h.state.calls==0,"stale E cannot dispatch");h.send("S 1 "+id);h.c->poll();check(h.receive()==refusal,"ledger result retained");h.send(e);h.c->poll();check(h.receive()==refusal&&h.state.calls==0,"duplicate E no replay");}
 {Harness h;h.send("malformed");h.c->poll();check(h.receive()==agentEnvelope()+",\"type\":\"error\",\"status\":\"unavailable\"}","non-expiry generic error unchanged");check(h.receive().empty(),"generic emits once");}
 {Harness h;h.begin(6);agentTestTime+=1001;h.send("R "+token(7));h.c->poll();check(h.receive()==h.expected(6),"literal poll ordering expires old before processing one queued R");check(h.receive().empty(),"new R pending no extra packet");h.frame();check(field(h.receive(),"challenge")==token(7),"new challenge survives expiry clear");check(h.state.calls==0,"poll ordering no gameplay");}
 std::cout<<"AgentReadExpiryTest PASS "<<checks<<" checks\n";return 0;
}
