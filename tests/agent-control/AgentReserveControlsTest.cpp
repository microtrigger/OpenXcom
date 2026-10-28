// SPDX-License-Identifier: GPL-3.0-or-later
#include <string>
#include <vector>
#include <cassert>
#include <iostream>
enum {FACTION_PLAYER, FACTION_HOSTILE}; enum {BA_NONE,BA_SNAPSHOT,BA_AIMEDSHOT,BA_AUTOSHOT};
enum {SDL_MOUSEBUTTONDOWN=1,SDL_BUTTON_LEFT=1};
struct SDL_Event {int type;struct {int button;} button;};
class BattlescapeButton;
struct Action {SDL_Event *event;BattlescapeButton *sender=nullptr;Action(SDL_Event *e,double,double,int,int):event(e){};void setSender(BattlescapeButton*b){sender=b;}BattlescapeButton*getSender(){return sender;}SDL_Event*getDetails(){return event;}};
struct BattlescapeButton {bool visible=true,painted=true,selected=false;BattlescapeButton **group=nullptr;bool agentTogglePainted(void*)const{return painted;}bool agentShownToggled()const{return selected;}bool agentStandaloneTogglePainted(void*)const{return !group&&painted;}void toggle(bool value){selected=value;}void mousePress(Action*a,void*){assert(a->event->button.button==SDL_BUTTON_LEFT);if(group){(*group)->selected=false;*group=this;selected=true;}}};
namespace Observer {
struct AgentRequest {std::string action;};
struct AgentUiAction {std::string id;AgentUiAction(const char*i,const char*,const char*):id(i){}};
struct AgentUiState {std::string visible="{\"existing\":0}";std::vector<AgentUiAction>actions;bool offers(const std::string&a){for(auto&x:actions)if(x.id==a)return true;return false;}};
std::string agentJsonString(const std::string&s){return "\""+s+"\"";}
bool agentWidgetUncovered(BattlescapeButton*b,const std::vector<int>&){return b->visible;}
void agentIncludeWidget(AgentUiState&,BattlescapeButton*){}
}
struct Save {int side=FACTION_PLAYER;bool debug=false;int getSide(){return side;}bool getDebugMode(){return debug;}};
struct Map {int projectile=0;int getProjectile(){return projectile;}};
struct Path {bool preview=true;int refresh=0;bool isPathPreviewed(){return preview;}void removePreview(){};void previewPath(){++refresh;}};
struct Battle {bool busy=false,panic=true;int reserved=-1,writes=0,kneelWrites=0;bool kneelReserved=false;bool getKneelReserved(){return kneelReserved;}void setKneelReserved(bool v){kneelReserved=v;++kneelWrites;}Path path;bool isBusy(){return busy;}bool getPanicHandled(){return panic;}Path*getPathfinding(){return &path;}void setTUReserved(int v){reserved=v;++writes;}};
struct Screen {void*getSurface(){return nullptr;}}; struct Game {Screen s;Screen*getScreen(){return &s;}};
struct BattlescapeState {
 Game g;Game*_game=&g;
 BattlescapeButton none,snap,aimed,aut,kneel;BattlescapeButton *_reserve=&none,*_btnReserveNone=&none,*_btnReserveSnap=&snap,*_btnReserveAimed=&aimed,*_btnReserveAuto=&aut,*_btnReserveKneel=&kneel;
 Save save;Map map;Battle battle;Save*_save=&save;Map*_map=&map;Battle*_battleGame=&battle;bool _firstInit=false;std::vector<int>_surfaces;
 BattlescapeState(){none.selected=true;for(auto*b:{&none,&snap,&aimed,&aut})b->group=&_reserve;}
 void project(Observer::AgentUiState&)const;bool dispatch(const Observer::AgentRequest&);bool allowButtons(bool allowSaving=false)const;void btnReserveClick(Action*);void btnReserveKneelClick(Action*);
};
#include "Methods.inc"
int main(){
 BattlescapeState b;Observer::AgentUiState o;b.project(o);assert(o.visible=="{\"existing\":0,\"reserve_indicator\":\"none\",\"kneel_reserve_indicator\":\"off\"}"&&o.actions.size()==5);
 assert(b.dispatch({"reserve_snap"}));assert(b.battle.reserved==BA_SNAPSHOT&&b._reserve==&b.snap&&b.battle.path.refresh==1);
 o={};b.project(o);assert(o.visible.find("snap")!=std::string::npos);
 assert(b.dispatch({"reserve_none"}));assert(b.battle.reserved==BA_NONE&&b._reserve==&b.none&&b.battle.path.refresh==2);
 for(int mode=0;mode<7;++mode){BattlescapeState x;if(mode==0)x.battle.busy=true;if(mode==1)x.map.projectile=1;if(mode==2)x.save.side=FACTION_HOSTILE;if(mode==3){x.save.side=FACTION_HOSTILE;x.save.debug=true;}if(mode==4)x._firstInit=true;if(mode==5)x.battle.panic=false;if(mode==6){x.snap.painted=false;x.kneel.painted=false;}for(const char *action:{"reserve_none","reserve_snap","reserve_aimed","reserve_auto","toggle_kneel_reserve"})assert(!x.dispatch({action}));assert(x.battle.writes==0&&x.battle.kneelWrites==0);}
 for(int mode=0;mode<3;++mode){BattlescapeState x;if(mode==0)x.snap.visible=false;if(mode==1)x.none.selected=false;if(mode==2)x.snap.selected=true;o={};x.project(o);assert(o.visible=="{\"existing\":0,\"reserve_indicator\":null,\"kneel_reserve_indicator\":\"off\"}"&&o.actions.size()==1&&o.offers("toggle_kneel_reserve"));}
 const char *actions[]={"reserve_none","reserve_snap","reserve_aimed","reserve_auto"};
 BattlescapeButton *buttons[]={&b.none,&b.snap,&b.aimed,&b.aut};
 for(unsigned from=0;from<4;++from)for(unsigned to=0;to<4;++to){
  assert(b.dispatch({actions[from]}));const int writes=b.battle.writes,refresh=b.battle.path.refresh;
  assert(b.dispatch({actions[to]}));assert(b.battle.reserved==int(to)&&b._reserve==buttons[to]);
  assert(b.battle.writes==writes+1&&b.battle.path.refresh==refresh+1);o={};b.project(o);assert(o.actions.size()==5);
 }

 const int before=b.battle.path.refresh;
 assert(b.dispatch({"toggle_kneel_reserve"})&&b.battle.kneelReserved&&b.kneel.selected);
 o={};b.project(o);assert(o.visible.find("\"kneel_reserve_indicator\":\"on\"")!=std::string::npos);
 assert(b.dispatch({"toggle_kneel_reserve"})&&!b.battle.kneelReserved&&!b.kneel.selected);
 assert(b.battle.kneelWrites==2&&b.battle.path.refresh==before+2);
 for(int mode=0;mode<2;++mode){BattlescapeState x;if(mode==0)x.kneel.visible=false;else x.kneel.painted=false;o={};x.project(o);assert(!o.offers("toggle_kneel_reserve")&&o.actions.size()==4);assert(o.visible.find("\"kneel_reserve_indicator\":null")!=std::string::npos);assert(!x.dispatch({"toggle_kneel_reserve"})&&x.battle.kneelWrites==0);}
 assert(!b.dispatch({"reserve_unknown"}));
 std::cout<<"PASS native reserve projection/dispatch/handler guards (synthetic dependencies)\n";
}
