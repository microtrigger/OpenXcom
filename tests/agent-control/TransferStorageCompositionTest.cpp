// SPDX-License-Identifier: GPL-3.0-or-later
// Literal storage aggregation/rule callbacks and current native confirmation widgets.
// Explicit rule/domain/Game/render seams; no inventory delivery, economic revalidation or live claim.
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
#include "../../src/Interface/Bar.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Observer/AgentProjectileOutput.h"
#include <cmath>
#include <climits>
#include "../../src/fmath.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <map>
#include <sstream>
#include <vector>
#define private public
#include "../../src/Interface/TextEdit.h"
#undef private
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
struct Base;struct Craft;struct Target{};
struct RuleItem{std::string id;double size;int clip=10;bool alien=false;RuleItem(std::string i,double s):id(i),size(s){}double getSize()const{return size;}int getClipSize()const{return clip;}bool isAlien()const{return alien;}const std::string&getType()const{return id;}};
struct InterfaceElement{int color=40;};struct RuleInterface{InterfaceElement element;InterfaceElement*getElement(const char*){return &element;}};
struct Mod{std::map<std::string,RuleItem*>rules;Surface*background=nullptr;RuleInterface iface;RuleItem*getItem(const std::string&id,bool)const{return rules.at(id);}Surface*getSurface(const std::string&){return background;}RuleInterface*getInterface(const char*){return &iface;}};
struct ItemContainer{std::map<std::string,int>_qty;int removals=0;double getTotalSize(const Mod*)const;int getItem(const std::string&id)const{auto i=_qty.find(id);return i==_qty.end()?0:i->second;}void removeItem(const std::string&id,int n){_qty[id]-=n;++removals;}};
struct RuleFacility{int storage=50;int getStorage(){return storage;}};
struct BaseFacility{RuleFacility rule;int build=0;int getBuildTime(){return build;}RuleFacility*getRules(){return &rule;}};
struct RuleCraftWeapon{std::string clip="CLIP";int max=100;std::string getClipItem(){return clip;}int getAmmoMax(){return max;}};
struct CraftWeapon{RuleCraftWeapon rule;int ammo=50;bool rearming=true;bool isRearming(){return rearming;}RuleCraftWeapon*getRules(){return &rule;}int getAmmo(){return ammo;}};
struct Vehicle{RuleItem*rule;RuleItem*getRules(){return rule;}};
struct Soldier{void setPsiTraining(bool){}Craft*getCraft(){return nullptr;}};
struct Craft{ItemContainer items;std::vector<Vehicle*>vehicles;std::vector<CraftWeapon*>weapons;std::string status="STR_READY";ItemContainer*getItems(){return &items;}std::vector<Vehicle*>*getVehicles(){return &vehicles;}std::vector<CraftWeapon*>*getWeapons(){return &weapons;}std::string getStatus(){return status;}int getNumSoldiers(){return 0;}Target*getDestination(){return nullptr;}Base*getBase(){return nullptr;}void setBase(Base*,bool){}int getFuel(){return 100;}int getFuelLimit(Base*){return 0;}void setLowFuel(bool){}void returnToBase(){std::abort();}};
enum TransferType{TRANSFER_ITEM,TRANSFER_CRAFT,TRANSFER_SOLDIER,TRANSFER_SCIENTIST,TRANSFER_ENGINEER};
struct Transfer{int hours,amount=0;TransferType type=TRANSFER_ITEM;std::string item;Craft*craft=nullptr;explicit Transfer(int h):hours(h){}TransferType getType(){return type;}int getQuantity(){return amount;}std::string getItems(){return item;}Craft*getCraft(){return craft;}void setItems(const std::string&id,int n){item=id;amount=n;type=TRANSFER_ITEM;}void setCraft(Craft*c){craft=c;type=TRANSFER_CRAFT;}void setSoldier(Soldier*){type=TRANSFER_SOLDIER;}void setScientists(int n){amount=n;type=TRANSFER_SCIENTIST;}void setEngineers(int n){amount=n;type=TRANSFER_ENGINEER;}};
struct Base:Target{Mod*_mod;ItemContainer storage;ItemContainer*_items=&storage;std::vector<Craft*>_crafts;std::vector<Transfer*>_transfers;std::vector<BaseFacility*>_facilities;std::vector<Soldier*>soldiers;BaseFacility first,second,unfinished;int scientists=0,engineers=0;explicit Base(Mod*m):_mod(m){unfinished.build=1;unfinished.rule.storage=999;_facilities={&first,&second,&unfinished};}~Base(){for(auto*t:_transfers)delete t;}
 double getUsedStores();int getAvailableStores()const;double getIgnoredStores();bool storesOverfull(double offset=0);std::string getName(){return "DEST";}
 std::vector<Craft*>*getCrafts(){return &_crafts;}std::vector<Transfer*>*getTransfers(){return &_transfers;}ItemContainer*getStorageItems(){return _items;}std::vector<Soldier*>*getSoldiers(){return &soldiers;}
 int getAvailableQuarters(){return 100;}int getUsedQuarters(){return 0;}int getAvailableHangars(){return 10;}int getUsedHangars(){return 0;}int getAvailableContainment(){return 10;}int getUsedContainment(){return 0;}
 void removeCraft(Craft*,bool){std::abort();}int getScientists(){return scientists;}void setScientists(int n){scientists=n;}int getEngineers(){return engineers;}void setEngineers(int n){engineers=n;}
};
#include "Storage.generated.inc"
struct SavedGame{long long funds=100000;int debits=0;long long getFunds(){return funds;}void setFunds(long long n){funds=n;++debits;}};
struct ErrorMessageState{std::string message;ErrorMessageState(std::string m,int,int,const char*,int):message(m){}};
struct TransferConfirmState;
struct Screen{Surface*surface;Surface*getSurface(){return surface;}};
struct Game{SavedGame saved;Mod mod;Screen screen;Language*language;Font*big,*small;SDL_Color*palette;const void*top=nullptr;int pops=0;std::vector<std::string>errors;bool clear=true;SavedGame*getSavedGame(){return &saved;}Mod*getMod(){return &mod;}Language*getLanguage(){return language;}Screen*getScreen(){return &screen;}bool isState(const void*s)const{return top==s;}void popState(){++pops;top=nullptr;}void pushState(ErrorMessageState*e){errors.push_back(e->message);delete e;}void pushState(TransferConfirmState*){std::abort();}bool agentOverlayClear(const AgentUiState&)const{return clear;}};
static Game*currentGame;
struct Timer{int stops=0;void stop(){++stops;}};
struct TransferRow{TransferType type;void*rule;std::string name;int cost,qtySrc,qtyDst,amount;};
struct TransferItemsState{Base*_baseFrom,*_baseTo;Game*_game;Timer timer;Timer*_timerInc=&timer;int _palette=0,_total=0,_pQty=0,_cQty=0,_aQty=0;double _iQty=0,_distance=100;std::vector<TransferRow>_items;size_t sel=0;int refreshes=0;
 TransferItemsState(Base&f,Base&t,Game&g):_baseFrom(&f),_baseTo(&t),_game(&g){}TransferRow&getRow(){return _items.at(sel);}std::string tr(const char*s){return s;}void updateItemStrings(){++refreshes;}void increaseByValue(int);void decreaseByValue(int);void completeTransfer();void btnCancelClick(Action*);int getTotal(){return _total;}
 void add(RuleItem&r,int cost=3){_items.push_back({TRANSFER_ITEM,&r,r.id,cost,_baseFrom->storage.getItem(r.id),_baseTo->storage.getItem(r.id),0});}
};
#include "TransferRules.generated.inc"
struct State:OpenXcom::State{Game*_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;void setInterface(const std::string&){}void centerAllSurfaces(){}LocalizedText tr(const std::string&id)const{return _game->language->getString(id);}void add(Surface*s,const std::string&,const std::string&){s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);_surfaces.push_back(s);if(auto*t=dynamic_cast<Text*>(s))t->setColor(40);if(auto*b=dynamic_cast<TextButton*>(s))b->setColor(32);}void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}};
struct TransferConfirmState:State{Base*_base;TransferItemsState*_state;Window*_window;TextButton*_btnOk,*_btnCancel;Text*_txtTitle,*_txtCost,*_txtTotal;bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;
 TransferConfirmState(Base*,TransferItemsState*);~TransferConfirmState();void btnOkClick(Action*);void btnCancelClick(Action*);bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;void paint(){_window->think();for(int i=0;i<30;++i)_window->popup();blit();}void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}};
#include "Confirm.generated.inc"
}
namespace Fixture {
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
static bool near(double a,double b){return std::abs(a-b)<1e-8;}
int run(int argc,char**argv){
 check(argc==3,"source/log arguments");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen native confirmation widgets");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 RuleItem unit("UNIT",1),half("HALF",.5),clip("CLIP",2),vehicleRule("VEHICLE",5),fraction("FRACTION",.009),centi("CENTI",.02);
 auto rules=[&](Mod&m){for(auto*r:{&unit,&half,&clip,&vehicleRule,&fraction,&centi})m.rules[r->id]=r;};
 // Independent hardcoded arithmetic: 20+6+3+5+4+7-6=39, not a reused implementation oracle.
 {Mod m;rules(m);Base b(&m);b.storage._qty={{"UNIT",20},{"CLIP",3}};Craft resident,pending;resident.items._qty["UNIT"]=3;pending.items._qty["UNIT"]=7;Vehicle rv{&vehicleRule},pv{&vehicleRule};resident.vehicles={&rv};pending.vehicles={&pv};CraftWeapon weapon;resident.status="STR_REARMING";resident.weapons={&weapon};b._crafts={&resident};auto*t=new Transfer(12);t->setItems("HALF",8);auto*c=new Transfer(12);c->setCraft(&pending);b._transfers={t,c};
 check(b.getAvailableStores()==100,"two completed50 facilities, unfinished999 excluded");check(near(b.storage.getTotalSize(&m),26)&&near(resident.items.getTotalSize(&m),3)&&near(pending.items.getTotalSize(&m),7),"independent container subtotals26/3/7");check(near(b.getIgnoredStores(),6)&&near(b.getUsedStores(),39),"actual nonzero min(available3,needed5) exclusion6 and composed39");resident.status="STR_READY";check(near(b.getIgnoredStores(),0)&&near(b.getUsedStores(),45),"ready craft no ignored clips");resident.status="STR_REARMING";weapon.rearming=false;check(near(b.getIgnoredStores(),0)&&near(b.getUsedStores(),45),"nonrearming weapon no ignored clips");weapon.rearming=true;weapon.ammo=80;check(near(b.getIgnoredStores(),4)&&near(b.getUsedStores(),41),"needed2 below available3 yields ignored4");pending.vehicles.clear();check(near(b.getUsedStores(),41),"pending craft vehicles excluded by actual current rule");resident.vehicles.clear();check(near(b.getUsedStores(),36),"resident vehicle5 included unlike pending vehicle");}
 for(bool enforced:{false,true})for(int used:{100,110}){Options::storageLimitsEnforced=enforced;Game g;rules(g.mod);Base from(&g.mod),to(&g.mod);from.storage._qty["UNIT"]=150;to.storage._qty["UNIT"]=used;TransferItemsState draft(from,to,g);draft.add(unit);auto src=from.storage._qty,dst=to.storage._qty;draft.increaseByValue(1);check(to.getAvailableStores()==100&&near(to.getUsedStores(),used),"positive100 capacity computed from concrete storage");check(draft.getRow().amount==0&&draft._total==0&&near(draft._iQty,0)&&g.errors==std::vector<std::string>{"STR_NOT_ENOUGH_STORE_SPACE"}&&draft.timer.stops==1,"full/overfull destination actual refusal regardless option");check(from.storage._qty==src&&to.storage._qty==dst&&to._transfers.empty()&&g.saved.funds==100000,"refusal no container/funds/pending mutation");}
 {Game g;rules(g.mod);Base from(&g.mod),to(&g.mod);from.storage._qty["UNIT"]=150;TransferItemsState draft(from,to,g);draft.add(unit);draft.increaseByValue(3);check(from.storesOverfull()&&draft.getRow().amount==3&&near(draft._iQty,3)&&g.errors.empty()&&from.storage.getItem("UNIT")==150,"overfull source does not prohibit outbound draft to free destination");draft.btnCancelClick(nullptr);check(g.pops==2&&g.saved.debits==0&&from.storage.getItem("UNIT")==150&&to._transfers.empty(),"cancel no commit even with populated draft");}
 {Game g;rules(g.mod);Base from(&g.mod),to(&g.mod);from.storage._qty["HALF"]=4;to.storage._qty["HALF"]=199;TransferItemsState draft(from,to,g);draft.add(half);draft.increaseByValue(1);check(near(to.getUsedStores(),99.5)&&draft.getRow().amount==1&&near(draft._iQty,.5),"99.5 destination plus one0.5 draft exactfull");draft.increaseByValue(1);check(draft.getRow().amount==1&&g.errors.size()==1,"next increment refused at exactfull draft");draft.decreaseByValue(INT_MAX);check(!draft.getRow().amount&&near(draft._iQty,0)&&draft._total==0,"clear restores draft accounting");draft.btnCancelClick(nullptr);check(from.storage.getItem("HALF")==4&&to.storage.getItem("HALF")==199&&to._transfers.empty()&&g.saved.debits==0,"clear/cancel preserve concrete containers");}
 {Game g;rules(g.mod);Base from(&g.mod),to(&g.mod);from.storage._qty={{"HALF",3},{"UNIT",9}};to.storage._qty["UNIT"]=98;TransferItemsState draft(from,to,g);draft.add(half);draft.add(unit);draft.increaseByValue(INT_MAX);check(draft.getRow().amount==3&&near(draft._iQty,1.5),"source stock clamp with concrete counts");draft.sel=1;draft.increaseByValue(1);check(draft.getRow().amount==0&&g.errors.size()==1,"shared draft space prevents next unit");draft.sel=0;draft.decreaseByValue(1);draft.sel=1;draft.increaseByValue(1);check(draft.getRow().amount==1&&near(draft._iQty,2),"decrease releases shared space for other row");}
 {Mod m;rules(m);Base b(&m);b.storage._qty={{"UNIT",100},{"FRACTION",1}};check(!b.storesOverfull(),"actual fractional-item centi truncation100.009");b.storage._qty.erase("FRACTION");b.storage._qty["CENTI"]=1;check(b.storesOverfull(),"actual fractional-item100.02 overfull");}
 for(const std::string master:{"xcom1","xcom2"}){Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");Game g;rules(g.mod);g.mod.background=&background;g.screen.surface=&canvas;g.language=&language;g.big=&big;g.small=&small;g.palette=palette;currentGame=&g;Base from(&g.mod),to(&g.mod);from.storage._qty["HALF"]=7;to.storage._qty["UNIT"]=98;TransferItemsState draft(from,to,g);draft.add(half);draft.increaseByValue(3);check(draft._total==9&&near(draft._iQty,1.5),"selected confirmation draft");
 auto*confirm=new TransferConfirmState(&to,&draft);g.top=confirm;confirm->paint();AgentUiState ui;check(confirm->agentUiState(ui)&&ui.offers("confirm_transfer")&&ui.offers("cancel_transfer_confirmation"),"literal confirmation ctor/native widgets admitted");AgentRequest q;q.action="confirm_transfer";auto noCommit=[&](){check(!confirm->agentDispatch(q)&&g.saved.debits==0&&from.storage.removals==0&&to._transfers.empty()&&g.pops==0,"UI guard refuses without economic mutation");};g.top=nullptr;noCommit();g.top=confirm;confirm->_modal=confirm->_btnOk;noCommit();confirm->_modal=nullptr;confirm->_btnOk->setText("CHANGED");noCommit();confirm->paint();confirm->_btnOk->setVisible(false);confirm->paint();noCommit();confirm->_btnOk->setVisible(true);confirm->paint();Surface cover(5,5,20,118);cover.setPalette(palette);cover.clear(90);confirm->_surfaces.push_back(&cover);confirm->paint();noCommit();confirm->_surfaces.pop_back();confirm->paint();
 q.action="cancel_transfer_confirmation";check(confirm->agentDispatch(q)&&g.pops==1&&g.saved.debits==0&&from.storage.getItem("HALF")==7&&to._transfers.empty(),"literal Cancel no commit");confirm->release();delete confirm; // retirement only after dispatch returns
 confirm=new TransferConfirmState(&to,&draft);g.top=confirm;confirm->paint();q.action="confirm_transfer";int pops=g.pops;check(confirm->agentDispatch(q),"valid native confirmation commits");check(g.pops==pops+3&&g.saved.debits==1&&g.saved.funds==99991&&from.storage.removals==1&&from.storage.getItem("HALF")==4,"actual completion debit/source removal once and three pops");check(to._transfers.size()==1&&to._transfers[0]->getType()==TRANSFER_ITEM&&to._transfers[0]->getItems()=="HALF"&&to._transfers[0]->getQuantity()==3&&to._transfers[0]->hours==16,"pending payload and ordinary travel delay");check(to.storage.getItem("UNIT")==98&&to.storage.getItem("HALF")==0&&near(to.getUsedStores(),99.5),"physical destination unchanged but pending reserves1.5 immediately");check(!confirm->agentDispatch(q)&&g.saved.debits==1&&to._transfers.size()==1,"inactive UI refuses extra callback, not commandledger replay proof");confirm->release();delete confirm;
 }
 std::cout<<"PASS "<<checks<<" actual storage composition/rules/native confirmation checks; explicit domain/render seams\n";
 return 0;
}

}
int main(int argc,char**argv){return Fixture::run(argc,argv);}
