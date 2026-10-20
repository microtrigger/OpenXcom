// SPDX-License-Identifier: GPL-3.0-or-later
// Actual Text/InteractiveSurface paint and exact extracted providers/shared
// timer handlers. Owning Game/action/actor and menu activation are explicit shims.
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Action.h"
#include "../../src/Engine/InteractiveSurface.h"
#include "../../src/Interface/Text.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Battlescape/BattlescapeGame.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <iostream>
#include <memory>
using namespace OpenXcom;
static unsigned checks=0;
static void check(bool b,const char*m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
namespace GrenadeHarness {
using namespace OpenXcom;
struct Actor { std::uint64_t revision=1,pose=1;int tu=50;unsigned spends=0;
 std::uint64_t stanceControlRevision()const{return revision;}std::uint64_t turnControlPoseRevision()const{return pose;}
 bool spendTimeUnits(int cost){++spends;if(tu<cost)return false;tu-=cost;return true;} };
struct Item {int fuse=-1;unsigned sets=0;void setFuseTimer(int v){fuse=v;++sets;}};
struct BattleAction { Actor*actor;Item*weapon;BattleActionType type=BA_PRIME;int value=-1,TU=10;bool targeting=false;std::string result;Position cameraPosition; };
struct Saved {void*getSavedBattle(){return this;}void reviveUnconsciousUnits(){} };
struct Game {const void*top=nullptr;Saved saved;unsigned pops=0;bool isState(const void*s)const{return top==s;}Saved*getSavedGame(){return &saved;}void popState(){++pops;top=nullptr;}};
struct State {static void handle(Action*){} };
class PrimeGrenadeState {
public: Game*_game;Surface*_modal=nullptr;BattleAction*_action;bool _inInventoryView=false;Item*_grenadeInInventory;
 Text*_number[24];Text*_title;InteractiveSurface*_button[24];std::vector<Surface*>_surfaces;
 void handle(Action*);void btnClick(Action*);void selectTimer(int);void cancelTimer();
 bool agentUiState(Observer::AgentUiState&)const;bool agentDispatch(const Observer::AgentRequest&);
};
// Menu row rendering itself is separately exercised by existing actual composite
// tests. Here the provider consumes public row text from a declared row shim.
struct MenuRow:Surface {BattleActionType type=BA_NONE;int tu=10;std::string label;bool row=true;
 MenuRow():Surface(272,32,24,40){}BattleActionType getAction()const{return type;}int getTUs()const{return tu;}
 bool agentRow(std::string&l,std::string&a,std::string&t)const{l=label;a="null";t="\"TUs>10\"";return row&&agentContextVisible();}};
class ActionMenuState {public:Game*_game;Surface*_modal=nullptr;BattleAction*_action;MenuRow*_actionMenu[6];std::vector<Surface*>_surfaces;int activated=-1;unsigned closes=0;
 void closeMenu(){++closes;_game->popState();}void activateItem(int i){activated=i;}
 bool agentUiState(Observer::AgentUiState&)const;bool agentDispatch(const Observer::AgentRequest&);};
struct Parent {std::vector<std::string>warnings;unsigned updates=0;void warning(const std::string&s){warnings.push_back(s);}void updateSoldierInfo(){++updates;}};
struct MeleeAttackBState {template<class...T>MeleeAttackBState(T&&...){} };
class BattlescapeGame {public:BattleAction _currentAction;Parent*_parentState;Saved*_save;unsigned cursors=0;
 void statePushBack(MeleeAttackBState*p){delete p;}void setupCursor(){++cursors;}void handleNonTargetAction();};
#include "AgentGrenadeMethods.generated.inc"
}
int main(int argc,char**argv){
 check(argc==2,"stock resource root argument");Logger::logFile()="/tmp/openxcom-v13-grenade-widget.log";
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");Options::baseXResolution=320;Options::baseYResolution=200;
 for(const char*gameName:{"xcom1","xcom2"}) {
  const std::string fontPath="Language/Font.dat"; // Both stock masters use the common font definitions.
  std::cout << "stock " << gameName << " shared timer geometry/font checks\n";
  Font big,small;for(const auto&f:YAML::LoadFile(fontPath)["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
  Language lang;Surface canvas(320,200),background(192,93,65,45),cover(22,22,66,64);Text title(192,24,65,44);
  title.initText(&big,&small,&lang);title.setBig();title.setAlign(ALIGN_CENTER);title.setHighContrast(true);title.setText("Set Timer");
  background.drawRect(0,0,192,93,16);cover.drawRect(0,0,22,22,32);cover.setVisible(false);
  GrenadeHarness::Game game;GrenadeHarness::Actor actor;GrenadeHarness::Item item;GrenadeHarness::BattleAction action;
  action.actor=&actor;action.weapon=&item;GrenadeHarness::PrimeGrenadeState state;state._game=&game;state._action=&action;state._grenadeInInventory=&item;state._title=&title;game.top=&state;
  state._surfaces={&background,&title};std::vector<std::unique_ptr<InteractiveSurface>>buttons;std::vector<std::unique_ptr<Text>>numbers;
  for(int i=0;i<24;++i){int x=67+(i%8)*24,y=68+(i/8)*25;
   buttons.emplace_back(new InteractiveSurface(22,22,x-1,y-4));numbers.emplace_back(new Text(20,20,x,y-1));
   auto*b=buttons.back().get();auto*n=numbers.back().get();b->drawRect(0,0,22,22,32);b->drawRect(1,1,20,20,16);
   n->initText(&big,&small,&lang);n->setBig();n->setHighContrast(true);n->setAlign(ALIGN_CENTER);n->setVerticalAlign(ALIGN_MIDDLE);n->setText(std::to_string(i));
   state._button[i]=b;state._number[i]=n;state._surfaces.push_back(b);state._surfaces.push_back(n);
  }
  state._surfaces.push_back(&cover);auto draw=[&](){for(auto*s:state._surfaces)s->blit(&canvas);};Observer::AgentUiState ui;
  check(!state.agentUiState(ui),"unpainted popup refused");draw();
  check(state.agentUiState(ui)&&ui.screen=="prime_grenade","painted tactical timer provider");
  check(ui.rows.size()==24&&ui.actions.size()==2&&ui.offers("choose_grenade_timer")&&ui.offers("cancel_grenade_timer"),"all 24 stock timers and cancel admitted");
  check(ui.visible.find("\"handle\":\"t23\",\"label\":\"23\"")!=std::string::npos,"timer label remains rendered text");
  check(Observer::agentRegionsDisjoint(ui),"actual digit ink contained in disjoint control footprints");
  check(ui.visible.find("fuse")==std::string::npos&&ui.visible.find("actor")==std::string::npos,"no pending fuse or actor fields");
  const auto before=ui;draw();check(state.agentUiState(ui)&&ui.sameBinding(before),"idle repaint keeps timer grant binding");
  actor.revision+=2;check(state.agentUiState(ui)&&!ui.sameBinding(before),"private actor ABA invalidates");actor.revision-=2;
  action.value=2;check(state.agentUiState(ui)&&!ui.sameBinding(before),"pending action change invalidates privately");action.value=-1;
  state._modal=&cover;check(!state.agentUiState(ui),"same-state modal refused");state._modal=nullptr;game.top=&cover;check(!state.agentUiState(ui),"covered top state refused");game.top=&state;
  state._inInventoryView=true;check(!state.agentUiState(ui),"prebattle inventory popup has no tactical authority");state._inInventoryView=false;
  cover.setVisible(true);draw();check(state.agentUiState(ui)&&ui.rows.size()==23&&ui.rows[0]==1,"covered first timer omitted and local handles regenerated");cover.setVisible(false);draw();
  numbers[0]->setText("1000000000");draw();check(state.agentUiState(ui)&&ui.rows.size()==23,"truly clipped number omitted");numbers[0]->setText("0");draw();
  numbers[0]->setY(60);draw();check(state.agentUiState(ui)&&ui.rows.size()==23,"number ink outside its own control omitted");numbers[0]->setY(67);draw();
  buttons[0]->setVisible(false);draw();check(state.agentUiState(ui)&&ui.rows.size()==23,"hidden button cannot authorize visible number");buttons[0]->setVisible(true);draw();
  numbers[0]->setText("0");numbers[0]->invalidate();check(state.agentUiState(ui)&&ui.rows.size()==23,"dirty number withheld until repaint");draw();
  Surface allCover(320,200);allCover.drawRect(0,0,320,200,16);state._surfaces.push_back(&allCover);draw();check(!state.agentUiState(ui),"whole covered popup unavailable");state._surfaces.pop_back();draw();
  Observer::AgentRequest request;request.action="choose_grenade_timer";request.choice="t24";check(!state.agentDispatch(request)&&game.pops==0,"unlisted timer no dispatch");
  request.choice="t23";check(state.agentDispatch(request)&&action.value==23&&game.pops==2&&item.fuse==-1&&actor.spends==0,"ordinary timer selection pops two without spending or priming");
  check(!state.agentDispatch(request)&&game.pops==2,"departed popup cannot dispatch twice");
  game.top=&state;game.pops=0;request.action="cancel_grenade_timer";check(state.agentDispatch(request)&&action.value==-1&&game.pops==1&&item.fuse==-1,"semantic cancel uses one-layer ordinary path");
  game.top=&state;game.pops=0;action.value=12;SDL_Event event={};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_RIGHT;Action physical(&event,1,1,0,0);state.handle(&physical);check(action.value==-1&&game.pops==1,"ordinary right-down cancel preserves one-pop behavior");
  game.top=&state;game.pops=0;event.type=SDL_MOUSEBUTTONUP;event.button.button=SDL_BUTTON_LEFT;physical.setSender(buttons[5].get());state.btnClick(&physical);check(action.value==5&&game.pops==2,"ordinary number-click shares selection path");
  game.top=&state;game.pops=0;state._inInventoryView=true;state.selectTimer(7);check(item.fuse==7&&game.pops==1,"legacy prebattle selection still sets fuse and pops one");state.cancelTimer();check(item.fuse==7&&game.pops==2,"legacy prebattle cancel leaves fuse unchanged");state._inInventoryView=false;
  const unsigned oldPops=game.pops;state.selectTimer(-1);state.selectTimer(24);check(game.pops==oldPops,"shared timer bounds no mutation");
  GrenadeHarness::ActionMenuState menu;menu._game=&game;menu._action=&action;std::vector<std::unique_ptr<GrenadeHarness::MenuRow>>rows;
  for(int i=0;i<6;++i){rows.emplace_back(new GrenadeHarness::MenuRow);menu._actionMenu[i]=rows.back().get();rows.back()->setVisible(i==0);menu._surfaces.push_back(rows.back().get());}game.top=&menu;
  for(BattleActionType type:{BA_SNAPSHOT,BA_AIMEDSHOT,BA_AUTOSHOT,BA_PRIME,BA_THROW}){rows[0]->type=type;rows[0]->label="Public action";rows[0]->blit(&canvas);check(menu.agentUiState(ui)&&ui.rows.size()==1&&ui.offers("choose_weapon_action"),"five admitted ordinary action types");request.action="choose_weapon_action";request.choice="a0";menu.activated=-1;check(menu.agentDispatch(request)&&menu.activated==0,"neutral action routes ordinary row index");}
  for(BattleActionType type:{BA_NONE,BA_LAUNCH,BA_HIT,BA_USE,BA_MINDCONTROL,BA_PANIC}){rows[0]->type=type;check(menu.agentUiState(ui)&&ui.rows.empty()&&!ui.offers("choose_weapon_action")&&ui.offers("cancel_weapon_action"),"unsupported menu type grants close only");}
  rows[0]->type=BA_PRIME;menu.agentUiState(ui);const auto primeBinding=ui;rows[0]->type=BA_THROW;menu.agentUiState(ui);check(!ui.sameBinding(primeBinding),"same-label private row type remap invalidates");
  // Exact ordinary post-menu priming method with explicit actor/weapon seams.
  GrenadeHarness::Parent parent;GrenadeHarness::BattlescapeGame battle;battle._parentState=&parent;battle._save=&game.saved;battle._currentAction=action;
  battle._currentAction.type=BA_PRIME;battle._currentAction.value=3;actor.tu=50;item.fuse=-1;battle.handleNonTargetAction();
  check(actor.tu==40&&item.fuse==3&&parent.warnings==std::vector<std::string>{"STR_GRENADE_IS_ACTIVATED"},"actual ordinary prime succeeds only after TU spend");
  parent.warnings.clear();battle._currentAction.type=BA_PRIME;battle._currentAction.value=4;actor.tu=0;item.fuse=-1;battle.handleNonTargetAction();check(item.fuse==-1&&parent.warnings==std::vector<std::string>{"STR_NOT_ENOUGH_TIME_UNITS"},"actual ordinary insufficient TU refusal never primes");
  parent.warnings.clear();battle._currentAction.type=BA_PRIME;battle._currentAction.value=-1;battle.handleNonTargetAction();check(parent.warnings.empty()&&item.fuse==-1,"cancelled timer has no priming warning");
  battle._currentAction.type=BA_THROW;battle._currentAction.targeting=true;const auto spends=actor.spends;battle.handleNonTargetAction();check(actor.spends==spends&&parent.warnings.empty(),"targeted Throw remains ordinary map flow");
 }
 check(Observer::agentPaintHealthy()&&!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"no game/window or paint failure");
 std::cout<<"AgentGrenadeWidgetTest PASS "<<checks<<" checks\n";
}
