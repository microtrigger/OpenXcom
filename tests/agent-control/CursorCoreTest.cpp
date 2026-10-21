// SPDX-License-Identifier: GPL-3.0-or-later
// Native InteractiveSurface/Action; extracted Game cursor methods with explicit window/owner seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/InteractiveSurface.h"
#include "../../src/Observer/AgentUi.h"
#include <cmath>
#include <limits>
#include <iostream>
#include <cstdlib>
using namespace OpenXcom;
static Uint8 held=0, app=SDL_APPACTIVE|SDL_APPINPUTFOCUS;static int warps=0;
extern "C" Uint8 __wrap_SDL_GetMouseState(int*x,int*y){if(x)*x=0;if(y)*y=0;return held;}
extern "C" Uint8 __wrap_SDL_GetAppState(){return app;}
extern "C" void __wrap_SDL_WarpMouse(Uint16,Uint16){++warps;}
namespace OpenXcom {
Game *State::_game=nullptr;
State::State():_screen(true),_modal(nullptr),_ruleInterface(nullptr),_ruleInterfaceParent(nullptr){}
State::~State(){}void State::init(){}void State::handle(Action*){}void State::blit(){}void State::resize(int&,int&){}void State::recenter(int,int){}
#include "CoreState.generated.inc"
}
struct HoverSurface:InteractiveSurface {
 int ins=0,outs=0,overs=0,presses=0,releases=0,clicks=0;
 HoverSurface():InteractiveSurface(20,20,10,10){setListButton();}
 void mouseIn(Action*,State*)override{++ins;}void mouseOut(Action*,State*)override{++outs;}void mouseOver(Action*,State*)override{++overs;}
 void mousePress(Action*,State*)override{++presses;}void mouseRelease(Action*,State*)override{++releases;}void mouseClick(Action*,State*)override{++clicks;}
};
struct NativeState:OpenXcom::State {void attach(Surface*s){_surfaces.push_back(s);}void modal(InteractiveSurface*s){_modal=s;}};
namespace Fixture {
using OpenXcom::Action;using OpenXcom::Surface;namespace Observer=OpenXcom::Observer;
struct State {bool safe=true;int calls=0;bool flagged=false;Uint8 type=0;void handle(Action*a){++calls;flagged=a->isAgentMotion();type=a->getDetails()->type;}bool agentCursorMotionSafe()const{return safe;}};
struct Cursor {int x=0,y=0,calls=0;bool visible=true,flagged=false;int getX(){return x;}int getY(){return y;}bool getVisible(){return visible;}void handle(Action*a){++calls;flagged=a->isAgentMotion();a->setMouseAction(a->getDetails()->motion.x,a->getDetails()->motion.y,0,0);x=static_cast<int>(a->getAbsoluteXMouse());y=static_cast<int>(a->getAbsoluteYMouse());}};
struct Screen {Surface canvas{320,200};bool canvasPresent=true;double sx=1,sy=1;int left=0,top=0,width=320,height=200,calls=0;std::uint64_t rev=11;Surface*getSurface(){return canvasPresent?&canvas:nullptr;}double getXScale(){return sx;}double getYScale(){return sy;}int getCursorLeftBlackBand(){return left;}int getCursorTopBlackBand(){return top;}int getWidth(){return width;}int getHeight(){return height;}std::uint64_t agentDisplayRevision(){return rev;}void handle(Action*){++calls;}};
struct Game {Screen screen;Cursor cursor,fps;State state;Screen*_screen=&screen;Cursor*_cursor=&cursor,*_fpsCounter=&fps;State*top=&state;bool _mouseActive=true,_agentVirtualCursor=false;int _agentCursorX=0,_agentCursorY=0;std::uint64_t _agentCursorRevision=0;State*agentTopState()const{return top;}
 static bool agentCursorWindowPoint(int,int,int,int,double,double,int,int,int&,int&);void agentCursorState(Observer::AgentUiState&)const;bool agentMoveCursor(int,int,State*);
};
#include "CoreGame.generated.inc"
}
static int checks=0;static void check(bool b,const char*s){++checks;if(!b){std::cerr<<"FAIL "<<s<<'\n';std::exit(1);}}
int main(){
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");SDL_Event initial={};Action ordinary(&initial,1,1,0,0);check(!ordinary.isAgentMotion(),"ordinary Action defaults to unflagged motion");
 for(double sx:{1.,2.,3.,1.25,1.5,2.75})for(double sy:{1.,2.,3.,1.25,1.5,2.75})for(int left:{0,13})for(int top:{0,17})for(int x:{0,1,159,318,319})for(int y:{0,1,99,198,199}){int wx=-1,wy=-1;check(Fixture::Game::agentCursorWindowPoint(x,y,320,200,sx,sy,left,top,wx,wy),"valid scale/band conversion");SDL_Event e={};e.type=SDL_MOUSEMOTION;Action a(&e,sx,sy,top,left);a.setMouseAction(wx,wy,0,0);check(static_cast<int>(a.getAbsoluteXMouse())==x&&static_cast<int>(a.getAbsoluteYMouse())==y,"actual Action inverse preserves logical cell");}
 for(int x:{-1,320,100000}){int wx=7,wy=8;check(!Fixture::Game::agentCursorWindowPoint(x,0,320,200,1,1,0,0,wx,wy)&&wx==7&&wy==8,"invalid X no output mutation");}
 for(int y:{-1,200,100000}){int wx=7,wy=8;check(!Fixture::Game::agentCursorWindowPoint(0,y,320,200,1,1,0,0,wx,wy),"invalid Y");}
 for(double s:{0.,0.5,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),1e30}){int wx=0,wy=0;check(!Fixture::Game::agentCursorWindowPoint(319,199,320,200,s,s,0,0,wx,wy),"invalid scale/overflow");}
 for(int extent:{-1,0,4097}){int wx=0,wy=0;check(!Fixture::Game::agentCursorWindowPoint(0,0,extent,200,1,1,0,0,wx,wy),"invalid canvas width");check(!Fixture::Game::agentCursorWindowPoint(0,0,320,extent,1,1,0,0,wx,wy),"invalid canvas height");}
 for(int band:{-1,65536}){int wx=0,wy=0;check(!Fixture::Game::agentCursorWindowPoint(0,0,320,200,1,1,band,0,wx,wy),"invalid horizontal band");check(!Fixture::Game::agentCursorWindowPoint(0,0,320,200,1,1,0,band,wx,wy),"invalid vertical band");}
 HoverSurface surface;held=SDL_BUTTON(SDL_BUTTON_LEFT);auto motion=[&](int x,int y,bool agent){SDL_Event e={};e.type=SDL_MOUSEMOTION;e.motion.x=x;e.motion.y=y;Action a(&e,1,1,0,0);a.setAgentMotion(agent);surface.handle(&a,nullptr);};
 motion(15,15,true);motion(16,16,true);motion(40,40,true);check(surface.ins==1&&surface.overs==2&&surface.outs==1,"flagged motion still dispatches native hover enter/over/leave");check(surface.presses==0&&surface.releases==0&&surface.clicks==0,"held SDL buttons cannot synthesize list press/release/click from agent hover");
 motion(15,15,false);check(surface.presses==1,"ordinary SDL motion preserves held-list press");check(!surface.agentCursorMotionSafe(),"pressed native widget unsafe");NativeState native;native.attach(&surface);check(!native.agentCursorMotionSafe(),"actual State rejects pressed child");int before=surface.releases;motion(40,40,true);check(surface.releases==before&&!surface.agentCursorMotionSafe(),"flagged leave does not release existing press");motion(15,15,false);motion(40,40,false);check(surface.releases>before&&surface.agentCursorMotionSafe(),"ordinary leave preserves original list release");held=0;check(native.agentCursorMotionSafe(),"actual State ready after ordinary release");native.modal(&surface);check(!native.agentCursorMotionSafe(),"actual State modal forbids cursor motion");native.modal(nullptr);
 Fixture::Game game;for(double scale:{1.,2.,3.,1.5}){game.screen.sx=game.screen.sy=scale;game.screen.left=13;game.screen.top=17;game.screen.width=static_cast<int>(320*scale)+26;game.screen.height=static_cast<int>(200*scale)+34;for(auto point:{std::make_pair(0,0),std::make_pair(319,199),std::make_pair(100,50)}){int c=game.state.calls;auto r=game._agentCursorRevision;check(game.agentMoveCursor(point.first,point.second,&game.state),"actual move cursor admitted");check(game.state.calls==c+1&&game.state.flagged&&game.state.type==SDL_MOUSEMOTION,"only flagged native motion reaches current top");check(game.cursor.x==point.first&&game.cursor.y==point.second&&game._agentVirtualCursor&&game._agentCursorX==point.first&&game._agentCursorY==point.second&&game._agentCursorRevision==r+1,"virtual cursor state retained at requested logical coordinate");OpenXcom::Observer::AgentUiState out;game.agentCursorState(out);check(out.offers("move_cursor")&&out.cursor.find("\"visible\":true")!=std::string::npos&&out.context.size()==2,"public cursor admits logical state and private revisions");}}
 const auto rejected=[&](Fixture::State*expected){int c=game.state.calls,screen=game.screen.calls,cursorCalls=game.cursor.calls,x=game.cursor.x,y=game.cursor.y;auto r=game._agentCursorRevision;check(!game.agentMoveCursor(5,5,expected),"unsafe cursor request rejected");check(game.state.calls==c&&game.screen.calls==screen&&game._agentCursorRevision==r&&game.cursor.calls==cursorCalls&&game.cursor.x==x&&game.cursor.y==y,"rejected request has no event or cursor mutation");};
 rejected(nullptr);Fixture::State other;rejected(&other);game.state.safe=false;rejected(&game.state);game.state.safe=true;game._mouseActive=false;rejected(&game.state);game._mouseActive=true;game.cursor.visible=false;rejected(&game.state);game.cursor.visible=true;held=1;rejected(&game.state);held=0;for(Uint8 focus:{Uint8(0),Uint8(SDL_APPACTIVE),Uint8(SDL_APPINPUTFOCUS)}){app=focus;rejected(&game.state);}app=SDL_APPACTIVE|SDL_APPINPUTFOCUS;game.screen.canvasPresent=false;rejected(&game.state);game.screen.canvasPresent=true;game.screen.width=20;rejected(&game.state);game.screen.width=1000;game.cursor.x=-1;rejected(&game.state);game.cursor.x=10;
 int c=game.state.calls;check(!game.agentMoveCursor(320,0,&game.state)&&!game.agentMoveCursor(0,-1,&game.state)&&game.state.calls==c,"out-of-canvas move does not dispatch");check(warps==0&&!SDL_GetVideoSurface(),"no desktop warp or video creation");std::cout<<checks<<" native cursor core checks passed; window/top/cursor render seams, no live GUI\n";
}
