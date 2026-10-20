// SPDX-License-Identifier: GPL-3.0-or-later
// Literal old/new physical handler bodies with explicit event/stack/item seams.
#include <iostream>
#include <cstdlib>
#include <vector>
static unsigned checks=0;
static void check(bool b,const char*m){++checks;if(!b){std::cerr<<m<<'\n';std::exit(1);}}
enum {SDL_MOUSEBUTTONDOWN=1,SDL_MOUSEBUTTONUP=2,SDL_MOUSEMOTION=3,SDL_BUTTON_LEFT=1,SDL_BUTTON_RIGHT=3};
struct Event{int type=SDL_MOUSEBUTTONUP;struct{int button=SDL_BUTTON_LEFT;}button;};
struct Action{Event event;const void*sender=nullptr;Event*getDetails(){return &event;}const void*getSender(){return sender;}};
struct State{static void handle(Action*){}};
struct Item{int fuse=-1;unsigned calls=0;void setFuseTimer(int n){fuse=n;++calls;}};
struct BattleAction{int value=-1;};
struct Game{unsigned pops=0;void popState(){++pops;}};
#define MEMBERS Game*_game;BattleAction*_action;bool _inInventoryView;Item*_grenadeInInventory;const void*_button[24];void handle(Action*);void btnClick(Action*);
namespace Legacy {class PrimeGrenadeState {public:MEMBERS};
#include "LegacyGrenadeMethods.generated.inc"
}
namespace Current {class PrimeGrenadeState {public:MEMBERS void selectTimer(int);void cancelTimer();};
#include "CurrentGrenadeMethods.generated.inc"
}
int main(){
 for(bool inventory:{false,true})for(int timer=-1;timer<24;++timer)for(int button:{SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT})for(int route=0;route<2;++route){
  Game aGame,bGame;Item aItem,bItem;BattleAction aAction,bAction;aAction.value=bAction.value=19;
  Legacy::PrimeGrenadeState a;Current::PrimeGrenadeState b;a._game=&aGame;b._game=&bGame;a._action=&aAction;b._action=&bAction;a._grenadeInInventory=&aItem;b._grenadeInInventory=&bItem;a._inInventoryView=b._inInventoryView=inventory;
  int buttons[24];for(int i=0;i<24;++i)a._button[i]=b._button[i]=buttons+i;
  Action event;event.sender=timer<0?nullptr:buttons+timer;event.event.button.button=button;event.event.type=route?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;
  if(route){a.handle(&event);b.handle(&event);}else{a.btnClick(&event);b.btnClick(&event);}
  check(aGame.pops==bGame.pops,"ordinary stack effects unchanged");check(aAction.value==bAction.value,"ordinary pending action unchanged");check(aItem.fuse==bItem.fuse&&aItem.calls==bItem.calls,"ordinary prebattle item effects unchanged");
 }
 std::cout<<"AgentGrenadeRoutingTest PASS "<<checks<<" checks\n";
}
