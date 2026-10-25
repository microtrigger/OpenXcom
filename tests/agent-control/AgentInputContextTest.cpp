// SPDX-License-Identifier: GPL-3.0-or-later
// Literal projection/cursor/cancel bodies and post-shot block; synthetic dependencies.
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
namespace OpenXcom {
#include "Enums.generated.inc"
enum { FACTION_PLAYER=0, FACTION_HOSTILE=1, PATH_NONE=0 };
namespace Options { int battleNewPreviewPath=1; bool battleConfirmFireMode=false; }
struct Armor {int getSize(){return 1;}};
struct BattleUnit {int faction=0;Armor armor;int getFaction(){return faction;}Armor*getArmor(){return &armor;}};
struct BattleAction {bool targeting=false;BattleActionType type=BA_NONE;BattleUnit*actor=nullptr;std::vector<int>waypoints;};
struct Pathfinding {bool preview=false;bool removePreview(){bool p=preview;preview=false;return p;}};
struct Save {bool debug=false;int side=0;BattleUnit unit;BattleUnit*selected=&unit;Pathfinding path;bool getDebugMode(){return debug;}int getSide(){return side;}BattleUnit*getSelectedUnit(){return selected;}Pathfinding*getPathfinding(){return &path;}};
struct Map {CursorType cursor=CT_NORMAL;std::vector<int>waypoints;void setCursorType(CursorType c,int=1){cursor=c;}CursorType getCursorType(){return cursor;}std::vector<int>*getWaypoints(){return &waypoints;}};
struct Cursor {void setVisible(bool){}};struct Game {Cursor cursor;Cursor*getCursor(){return &cursor;}};
struct Parent {Game game;Game*getGame(){return &game;}void showLaunchButton(bool){}};
struct State {void cancel(){}};
struct BattlescapeGame {Save*_save;Map*map;Parent parent;Parent*_parentState=&parent;BattleAction _currentAction;std::vector<State*>_states;bool busy=false;BattleAction*getCurrentAction(){return &_currentAction;}bool isBusy(){return busy;}Map*getMap(){return map;}void setupCursor();bool cancelCurrentAction(bool bForce=false);void afterShot(BattleAction action,bool actionFailed);};
struct BattlescapeState {Save*_save;Map*_map;BattlescapeGame*_battleGame;std::string agentInputContext(bool)const;};
#include "Methods.generated.inc"
}
using namespace OpenXcom;
static int checks=0;void check(bool b,const char*m){++checks;if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){Save s;Map m;BattlescapeGame b;b._save=&s;b.map=&m;b._currentAction.actor=&s.unit;BattlescapeState v{&s,&m,&b};
auto emit=[&](const char*name){std::cout<<"PAYLOAD "<<name<<" "<<v.agentInputContext(true)<<'\n';};
check(v.agentInputContext(true)=="{\"mode\":\"normal\",\"selected_action\":\"none\",\"cursor\":\"normal\"}","normal");emit("normal");
b._currentAction.targeting=true;b._currentAction.type=BA_AUTOSHOT;b.setupCursor();const auto before=v.agentInputContext(true);b.afterShot(b._currentAction,false);check(before==v.agentInputContext(true)&&m.cursor==CT_AIM,"native Auto retains targeting");emit("retained_auto");
s.path.preview=true;check(b.cancelCurrentAction()&&b._currentAction.targeting,"first cancel only preview");emit("preview_cancel");check(b.cancelCurrentAction()&&!b._currentAction.targeting&&m.cursor==CT_NORMAL,"second cancel clears target");emit("cancelled");
b._currentAction.targeting=true;b._currentAction.type=BA_THROW;b.setupCursor();emit("throw");b.afterShot(b._currentAction,false);check(!b._currentAction.targeting,"successful throw cancels");
b._currentAction.targeting=true;b._currentAction.type=BA_LAUNCH;b._currentAction.waypoints={1,2};m.waypoints={1,2};b.setupCursor();check(b.cancelCurrentAction()&&b._currentAction.targeting&&b._currentAction.waypoints.size()==1,"waypoint cancel remains targeting");emit("waypoint_cancel");
const std::string unavailable="{\"mode\":\"unavailable\",\"selected_action\":null,\"cursor\":null}";
check(v.agentInputContext(false)==unavailable,"hidden view");s.side=1;check(v.agentInputContext(true)==unavailable,"enemy turn");s.side=0;s.debug=true;check(v.agentInputContext(true)==unavailable,"debug");s.debug=false;s.unit.faction=1;check(v.agentInputContext(true)==unavailable,"nonown");s.unit.faction=0;b._currentAction.actor=nullptr;check(v.agentInputContext(true)==unavailable,"actor mismatch");b._currentAction.actor=&s.unit;s.selected=nullptr;check(v.agentInputContext(true)==unavailable,"no selection");s.selected=&s.unit;b.busy=true;check(v.agentInputContext(true)=="{\"mode\":\"busy\",\"selected_action\":null,\"cursor\":null}","busy redacts");emit("busy");b.busy=false;
BattleActionType types[]={BA_SNAPSHOT,BA_AIMEDSHOT,BA_AUTOSHOT,BA_THROW,BA_LAUNCH,BA_PANIC,BA_MINDCONTROL,BA_USE,BA_NONE};for(auto t:types){b._currentAction.type=t;b.setupCursor();emit("target_enum");check(v.agentInputContext(true).find("\"mode\":\"targeting\"")!=std::string::npos,"target enum projection");}
std::cout<<"PASS "<<checks<<" literal-body checks, synthetic dependencies; not live\n";
}
