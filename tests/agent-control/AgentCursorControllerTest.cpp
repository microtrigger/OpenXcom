// SPDX-License-Identifier: GPL-3.0-or-later
// Actual socket Controller regression with synthetic projections, NOT provider/live proof.
// Game movement is an explicit seam; real native hover is tested separately.
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
 int peer=-1;State state;Game game;std::unique_ptr<AgentController> c;std::string epoch,grant,packet;
 Harness(bool dirty=false){
  agentTestTime+=10000;pendingInput=false;buttons=0;focus=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;
  int f[2];check(socketpair(AF_UNIX,SOCK_SEQPACKET,0,f)==0,"socketpair");peer=f[1];setenv("OPENXCOM_AGENT_CONTROL","1",1);setenv("OPENXCOM_AGENT_CONTROL_FD",std::to_string(f[0]).c_str(),1);unsetenv("OPENXCOM_STANCE_FD");
  game.top=&state;game.cursorEnabled=true;state.dirty=dirty;state.ui.screen="basescape";state.ui.visible="{\"name\":\"TEST\"}";
  state.ui.actions.emplace_back("return_geoscape","Return","visible_control_semantics","none");
  c.reset(new AgentController(&game));epoch=field(receive(),"epoch");send("H "+std::string(agentProtocolSchema()));c->poll();check(has(receive(),"\"ready\""),"handshake");
  send("R "+token(99));c->poll();frame();grant=receive();check(has(grant,"\"capability\":\""),"fresh grant");
 }
 ~Harness(){c.reset();if(peer>=0)::close(peer);}
 void send(const std::string&s){check(::send(peer,s.data(),s.size(),0)==ssize_t(s.size()),"whole packet");}
 std::string receive(){char b[49152];auto n=recv(peer,b,sizeof(b),MSG_DONTWAIT);if(n<0){check(errno==EAGAIN||errno==EWOULDBLOCK,"nonblocking receive");return "";}return n?std::string(b,n):"EOF";}
 void frame(){c->beginFrame();c->presented();}
 void queue(int x=319,int y=199){packet="E "+token(1)+" "+epoch+" "+field(grant,"capability")+" "+field(grant,"revision")+" move_cursor "+std::to_string(x)+" "+std::to_string(y);send(packet);c->poll();check(receive().empty(),"queued command silent");check(game.cursorMoves==0,"queue does not move");}
 void finish(bool expected){frame();check(state.calls==0,"global movement never calls semantic state dispatch");if(expected){check(game.cursorMoves==1,"one move");check(receive().empty(),"post presentation receipt only");frame();auto r=receive();check(has(r,"\"status\":\"dispatched\""),"receipt dispatched");check(r.find("\"cursor\":{\"x\":"+std::to_string(game.cursorX)+",\"y\":"+std::to_string(game.cursorY))!=std::string::npos,"receipt actual current cursor");send(packet);c->poll();receive();frame();check(game.cursorMoves==1,"replay no move");}else{auto r=receive();check(has(r,"\"status\":\"unavailable\""),"refused stale/unsafe move");check(game.cursorMoves==0,"no refused mutation");}}
};
int main(){
 {Harness h;h.queue(0,0);h.finish(true);}
 {Harness h;h.queue();h.finish(true);}
 {Harness h(true);check(has(h.grant,"\"state\":\"unsupported\""),"unsupported cursor-only");check(has(h.grant,"\"visible\":{}"),"unsupported no leaked semantic value");check(!has(h.grant,"return_geoscape"),"unsupported no semantic action");h.queue();h.finish(true);}
 {Harness h;h.state.bindingValid=false;h.queue();h.finish(true);}
 for(int i=0;i<9;++i){Harness h;State away;h.queue();switch(i){case 0:h.game.top=&away;break;case 1:h.game.screen.revision++;break;case 2:h.game.cursorX++;break;case 3:h.game.cursorSafe=false;break;case 4:buttons=1;break;case 5:pendingInput=true;break;case 6:focus=0;break;case 7:agentTestTime+=100000;break;case 8:h.game.screen.ready=false;break;}h.finish(false);}
 {Harness h;h.queue(320,199);h.finish(false);}
 {Harness h;h.queue(319,200);h.finish(false);}
 std::cout<<"PASS "<<checks<<" actual-controller cursor checks; Game motion is explicit seam\n";
}
