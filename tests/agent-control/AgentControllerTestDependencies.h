// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Explicit test-only dependencies. No real Game/Screen/State, SDL device, AI,
// renderer or simulation is constructed. Actual AgentController.cpp is tested.
#include "../../src/Observer/AgentUi.h"
#include <functional>
#include <deque>
namespace OpenXcom {
namespace Observer {
extern std::uint64_t agentTestTime;
class AgentAudioEvents {
public:
 struct Event { enum Kind { Window, Gap, Status } kind=Window; enum State { Pending,Active,Unavailable } state=Pending;
 bool nonzero=false,partialWindowDiscarded=false;std::uint64_t droppedWindows=0; };
 std::deque<Event> events;
 bool take(Event &out){if(events.empty())return false;out=events.front();events.pop_front();return true;}
 Event::State status()const{return Event::Active;}
};
}
class State {
public:
 Observer::AgentUiState ui;
 bool dirty=false,bindingValid=true,dispatchResult=true;
 unsigned calls=0;
 std::function<void(const Observer::AgentRequest&)> handler;
 bool agentUiState(Observer::AgentUiState &out)const{if(dirty)return false;out=ui;return true;}
 bool agentBindingCurrent(const Observer::AgentUiState &)const{return bindingValid;}
 bool agentDispatch(const Observer::AgentRequest &request){++calls;if(handler)handler(request);return dispatchResult;}
};
class Screen {
public: bool ready=true; std::uint64_t revision=1;
 bool agentPresentationReady()const{return ready;}
 std::uint64_t agentDisplayRevision()const{return revision;}
};
class Game {
public: State *top=nullptr;Screen screen;Observer::AgentAudioEvents audio;
 bool cursorEnabled=false,cursorSafe=true;int cursorX=0,cursorY=0;unsigned cursorMoves=0;
 State *agentTopState()const{return top;} Screen *getScreen(){return &screen;}
 bool agentOverlayClear(const Observer::AgentUiState &)const{return true;}
 void agentRecordAdmissionFailure(bool,bool,bool)const{}
 void agentCursorState(Observer::AgentUiState &out)const{
  if(!cursorEnabled)return;
  out.cursor="{\"x\":"+std::to_string(cursorX)+",\"y\":"+std::to_string(cursorY)+",\"width\":320,\"height\":200,\"visible\":true}";
  if(cursorSafe)out.actions.emplace_back("move_cursor","Move cursor","visible_control_semantics","cursor_position");
 }
 bool agentMoveCursor(int x,int y,State*expected){if(!cursorEnabled||!cursorSafe||expected!=top||x<0||x>=320||y<0||y>=200)return false;++cursorMoves;cursorX=x;cursorY=y;return true;}
 Observer::AgentAudioEvents &agentAudioEvents(){return audio;}
};
}
