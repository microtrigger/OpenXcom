// SPDX-License-Identifier: GPL-3.0-or-later
// Literal constructor/provider/dispatch and native resource callback bodies.
// Actual SDL widgets; explicit Game/Mod/storage/craft/render-loop seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Window.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentOutfitting.h"
#include <cmath>
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <map>
#include <sstream>
#include "../../src/Interface/TextList.h"
#include <iostream>
#include <cstdlib>
using namespace OpenXcom;
using namespace OpenXcom::Observer;
namespace OpenXcom {
#include "GameIdentity.generated.inc"
Game *State::_game = nullptr;
State::State():_screen(true),_modal(nullptr),_ruleInterface(nullptr),_ruleInterfaceParent(nullptr){}
State::~State(){}
void State::init(){} void State::handle(Action*){} void State::blit(){}
void State::resize(int&,int&){} void State::recenter(int,int){}
#include "StateBase.generated.inc"
}

namespace Fixture {
struct RuleCraftWeapon {std::string type,launcher,clip;int rate=2; std::string getType()const{return type;}std::string getLauncherItem()const{return launcher;}std::string getClipItem()const{return clip;}int getRearmRate()const{return rate;}};
struct RuleItem {int size=2;int getClipSize()const{return size;}};
struct Mod {Surface*background;std::vector<std::string> order;std::map<std::string,RuleCraftWeapon*>rules;RuleItem clip;Surface*getSurface(const std::string&){return background;}const std::vector<std::string>&getCraftWeaponsList()const{return order;}RuleCraftWeapon*getCraftWeapon(const std::string&s){return rules.at(s);}RuleItem*getItem(const std::string&s)const{return s.empty()?nullptr:const_cast<RuleItem*>(&clip);}};
struct CraftWeapon {RuleCraftWeapon*rule;int ammo;bool rearming=false;CraftWeapon(RuleCraftWeapon*r,int a):rule(r),ammo(a){}RuleCraftWeapon*getRules()const{return rule;}int getAmmo()const{return ammo;}void setRearming(bool b){rearming=b;}int getClipsLoaded(const Mod*)const;};
#include "CraftClips.generated.inc"
struct Craft {std::vector<CraftWeapon*>weapons;std::string status="STR_READY";std::vector<CraftWeapon*>*getWeapons(){return &weapons;}const std::string&getStatus()const{return status;}void setStatus(const std::string&s){status=s;}};
struct Items {std::map<std::string,int>counts;int getItem(const std::string&s)const{auto i=counts.find(s);return i==counts.end()?0:i->second;}void addItem(const std::string&s,int n=1){counts[s]+=n;}void removeItem(const std::string&s,int n=1){counts[s]-=n;}};
struct Base {std::vector<Craft*>crafts;Items items;std::vector<Craft*>*getCrafts(){return &crafts;}Items*getStorageItems(){return &items;}};
struct SavedGame {std::vector<Base*>bases;const std::vector<Base*>*getBases()const{return &bases;}};
struct Screen { Surface *surface; Surface *getSurface(){return surface;} };
struct Game {mutable Screen screen;Mod mod;SavedGame save;Language*language;Font*big,*small;SDL_Color*palette;const void*top=nullptr;bool clear=true;int pops=0;Screen*getScreen()const{return &screen;}Mod*getMod(){return &mod;}SavedGame*getSavedGame(){return &save;}Language*getLanguage(){return language;}bool isState(const void*s)const{return top==s;}bool agentOverlayClear(const AgentUiState&)const{return clear;}void popState(){++pops;top=nullptr;}};
static Game*currentGame;
struct State:OpenXcom::State {Game*_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;void setInterface(const std::string&){}void centerAllSurfaces(){}LocalizedText tr(const std::string&s)const{return _game->language->getString(s);}void add(Surface*s,const std::string&,const std::string&){s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);_surfaces.push_back(s);}void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}};
#include "CraftWeaponsClass.generated.inc"
#include "CraftWeapons.generated.inc"
}
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==3,"data and log args");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string master:{"xcom1","xcom2"})for(int slot:{0,1})for(int mode:{0,1,2,3,4}){
 Language language;language.loadFile(std::string(argv[1])+"/standard/"+master+"/Language/en-US.yml");Fixture::Game g;g.screen.surface=&canvas;g.mod.background=&background;g.language=&language;g.big=&big;g.small=&small;g.palette=palette;Fixture::currentGame=&g;
 Fixture::RuleCraftWeapon first;first.type="STR_STINGRAY";first.launcher="A";first.clip="AC";Fixture::RuleCraftWeapon second;second.type="STR_CANNON_UC";second.launcher="B";second.clip="BC";
 g.mod.order={first.type,second.type};g.mod.rules={{first.type,&first},{second.type,&second}};Fixture::Craft craft,other;craft.weapons={new Fixture::CraftWeapon(&first,5),new Fixture::CraftWeapon(&first,5)};Fixture::Base base;base.crafts={&craft};base.items.counts={{"A",2},{"AC",10},{"B",3},{"BC",20}};g.save.bases={&base};
 std::vector<Fixture::RuleCraftWeapon> extra(20);
 if(mode==3)for(int i=0;i<20;++i){auto&r=extra[i];r.type="EXTRA"+std::to_string(i);r.launcher="EL"+std::to_string(i);r.clip="EC"+std::to_string(i);g.mod.order.push_back(r.type);g.mod.rules[r.type]=&r;base.items.counts[r.launcher]=1;base.items.counts[r.clip]=4;}
 if(mode==4)base.items.counts["A"]=0;
 Fixture::CraftWeaponsState state(&base,0,slot);g.top=&state;AgentUiState ui;AgentRequest req;
 check(!state.agentUiState(ui),"unpainted denied");state.blit();check(state.agentUiState(ui)&&ui.screen=="craft_weapons","actual ctor provider");auto v=YAML::Load(ui.visible);check(v["rows"].size()==(mode==3?10:mode==4?2:3),"only visible stocked choices");check(v["rows"][0]["cells"].size()==3&&v["rows"][0]["cells"][1].IsNull()&&v["rows"][0]["cells"][2].IsNull(),"NONE missing cells null");check(ui.offers("select_craft_weapon")&&ui.offers("cancel_craft_weapons"),"native choices offered");
 const auto original=base.items.counts;auto*untouched=craft.weapons[1-slot];auto*current=craft.weapons[slot];
 base.items.counts["A"]--;check(!state.agentUiState(ui),"stock drift denied");base.items.counts["A"]++;
 base.crafts[0]=&other;check(!state.agentUiState(ui),"craft identity drift denied");base.crafts[0]=&craft;
 craft.weapons[slot]=nullptr;check(!state.agentUiState(ui),"slot pointer drift denied");craft.weapons[slot]=current;
 current->ammo++;check(!state.agentUiState(ui),"ammo drift denied");current->ammo--;
 g.save.bases.clear();check(!state.agentUiState(ui),"base membership drift denied");g.save.bases={&base};
 g.top=nullptr;check(!state.agentUiState(ui),"non-top denied");g.top=&state;state._modal=state._btnCancel;check(!state.agentUiState(ui),"modal denied");state._modal=nullptr;
 g.clear=false;check(!state.agentUiState(ui),"overlay denied");g.clear=true;
 req.action="select_craft_weapon";req.choice="r99";check(!state.agentDispatch(req)&&base.items.counts==original&&g.pops==0,"invalid row inert");
 check(state.agentUiState(ui),"fresh state after rejected authority probes");
 if(mode==3){check(!ui.rows.empty()&&ui.rows.front()==0,"first viewport logical NONE");req.action="scroll_visible_list";req.choice="down";check(state.agentDispatch(req),"ordinary native scroll");check(!state.agentUiState(ui),"scroll stale before repaint");state.blit();check(state.agentUiState(ui)&&ui.rows.front()>0,"scroll logical mapping changes");auto*expected=state._weapons.at(ui.rows.front());req.action="select_craft_weapon";req.choice="r0";check(state.agentDispatch(req)&&craft.weapons[slot]&&craft.weapons[slot]->rule==expected,"visible r0 maps scrolled logical choice");}
 else if(mode==4){req.action="cancel_craft_weapons";check(state.agentDispatch(req)&&base.items.counts==original,"Cancel no-spare installed weapon stock safe");}
 else if(mode==0){req.action="cancel_craft_weapons";check(state.agentDispatch(req)&&base.items.counts==original&&craft.weapons[slot]==current&&craft.status=="STR_READY","Cancel preserves resources");}
 else {req.choice=mode==1?"r0":"r2";check(state.agentDispatch(req),"native row dispatch");check(base.items.getItem("A")==3&&base.items.getItem("AC")==12,"old launcher and floor clips returned");if(mode==1)check(craft.weapons[slot]==nullptr&&base.items.getItem("B")==3&&craft.status=="STR_READY","NONE native status preserved");else check(craft.weapons[slot]&&craft.weapons[slot]->rule==&second&&craft.weapons[slot]->ammo==0&&craft.weapons[slot]->rearming&&craft.status=="STR_REARMING"&&base.items.getItem("B")==2&&base.items.getItem("BC")==20,"replacement native charge/rearm");}
 check(craft.weapons[1-slot]==untouched&&untouched->ammo==5,"other slot unchanged");check(g.pops==1&&!state.agentDispatch(req)&&g.pops==1,"single pop no replay");state.release();for(auto*w:craft.weapons)delete w;
 }
 check(!SDL_GetVideoSurface(),"no game window");std::cout<<"PASS "<<checks<<" craft armament actual constructor/provider/callback checks; explicit domain seams\n";
}
