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
 bool dirty=false,bindingValid=true;
 unsigned calls=0;
 std::function<void(const Observer::AgentRequest&)> handler;
 bool agentUiState(Observer::AgentUiState &out)const{if(dirty)return false;out=ui;return true;}
 bool agentBindingCurrent(const Observer::AgentUiState &)const{return bindingValid;}
 bool agentDispatch(const Observer::AgentRequest &request){++calls;if(handler)handler(request);return true;}
};
class Screen {
public: bool ready=true; std::uint64_t revision=1;
 bool agentPresentationReady()const{return ready;}
 std::uint64_t agentDisplayRevision()const{return revision;}
};
class Game {
public: State *top=nullptr;Screen screen;Observer::AgentAudioEvents audio;
 State *agentTopState()const{return top;} Screen *getScreen(){return &screen;}
 bool agentOverlayClear(const Observer::AgentUiState &)const{return true;}
 void agentRecordAdmissionFailure(bool,bool,bool)const{}
 Observer::AgentAudioEvents &agentAudioEvents(){return audio;}
};
}
