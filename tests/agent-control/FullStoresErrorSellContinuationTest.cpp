// SPDX-License-Identifier: GPL-3.0-or-later
// Literal forced storage-error FIFO and preconstructed nonempty Sell continuation.
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
#include <set>
#include "../../src/Engine/Palette.h"
#include "../../src/Engine/Timer.h"
#include "../../src/Interface/ComboBox.h"
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
struct RuleItem{std::string id;double size;int clip=10;bool alien=false;RuleItem(std::string i,double s):id(i),size(s){}double getSize()const{return size;}int getClipSize()const{return clip;}int getBattleType()const{return 0;}int getSellCost()const{return 7;}bool isAlien()const{return alien;}const std::string&getType()const{return id;}};
struct InterfaceElement{int color=40;};struct RuleInterface{InterfaceElement element,palette;RuleInterface(){palette.color=0;}InterfaceElement*getElement(const char*id){return std::string(id)=="errorPalette"?&palette:&element;}};
struct RuleCraftWeapon;struct Armor;struct FakePalette{SDL_Color colors[256];SDL_Color*getColors(int offset=0){return colors+offset;}};
struct Mod{std::map<std::string,RuleItem*>rules;Surface*background=nullptr;RuleInterface iface;FakePalette palette;std::vector<std::string>items={"SYNTHETIC_ITEM"},empty;const std::vector<std::string>&getItemsList(){return items;}const std::vector<std::string>&getCraftWeaponsList(){return empty;}const std::vector<std::string>&getArmorsList(){return empty;}RuleCraftWeapon*getCraftWeapon(const std::string&){std::abort();}Armor*getArmor(const std::string&){std::abort();}FakePalette*getPalette(const std::string&){return &palette;}RuleItem*getItem(const std::string&id,bool)const{return rules.at(id);}Surface*getSurface(const std::string&){return background;}RuleInterface*getInterface(const char*){return &iface;}};
struct ItemContainer{std::map<std::string,int>_qty;int removals=0;double getTotalSize(const Mod*)const;int getItem(const std::string&id)const{auto i=_qty.find(id);return i==_qty.end()?0:i->second;}void removeItem(const std::string&id,int n){_qty[id]-=n;++removals;}};
struct RuleFacility{int storage=50;int getStorage(){return storage;}};
struct BaseFacility{RuleFacility rule;int build=0;int getBuildTime(){return build;}RuleFacility*getRules(){return &rule;}};
struct RuleCraftWeapon{std::string clip="CLIP";int max=100;std::string getLauncherItem(){return "LAUNCHER";}std::string getClipItem(){return clip;}int getAmmoMax(){return max;}};
struct CraftWeapon{RuleCraftWeapon rule;int ammo=50;bool rearming=true;bool isRearming(){return rearming;}RuleCraftWeapon*getRules(){return &rule;}int getAmmo(){return ammo;}};
struct Vehicle{RuleItem*rule;RuleItem*getRules(){return rule;}};
struct Armor{std::string getStoreItem(){return "ARMOR";}};struct RuleCraft{int getSellCost(){return 0;}};
struct Soldier{std::string getName(bool){return "SOLDIER";}void setPsiTraining(bool){}Craft*getCraft(){return nullptr;}};
struct Craft{RuleCraft rule;RuleCraft*getRules(){return &rule;}std::string getName(Language*){return "CRAFT";}ItemContainer items;std::vector<Vehicle*>vehicles;std::vector<CraftWeapon*>weapons;std::string status="STR_READY";ItemContainer*getItems(){return &items;}std::vector<Vehicle*>*getVehicles(){return &vehicles;}std::vector<CraftWeapon*>*getWeapons(){return &weapons;}std::string getStatus(){return status;}int getNumSoldiers(){return 0;}Target*getDestination(){return nullptr;}Base*getBase(){return nullptr;}void setBase(Base*,bool){}int getFuel(){return 100;}int getFuelLimit(Base*){return 0;}void setLowFuel(bool){}void returnToBase(){std::abort();}};
enum TransferType{TRANSFER_ITEM,TRANSFER_CRAFT,TRANSFER_SOLDIER,TRANSFER_SCIENTIST,TRANSFER_ENGINEER};
struct Transfer{int hours,amount=0;TransferType type=TRANSFER_ITEM;std::string item;Craft*craft=nullptr;explicit Transfer(int h):hours(h){}TransferType getType(){return type;}int getQuantity(){return amount;}std::string getItems(){return item;}Craft*getCraft(){return craft;}void setItems(const std::string&id,int n){item=id;amount=n;type=TRANSFER_ITEM;}void setCraft(Craft*c){craft=c;type=TRANSFER_CRAFT;}void setSoldier(Soldier*){type=TRANSFER_SOLDIER;}void setScientists(int n){amount=n;type=TRANSFER_SCIENTIST;}void setEngineers(int n){amount=n;type=TRANSFER_ENGINEER;}};
struct Base:Target{Mod*_mod;ItemContainer storage;ItemContainer*_items=&storage;std::vector<Craft*>_crafts;std::vector<Transfer*>_transfers;std::vector<BaseFacility*>_facilities;std::vector<Soldier*>soldiers;BaseFacility first,second,unfinished;int scientists=0,engineers=0;explicit Base(Mod*m):_mod(m){unfinished.build=1;unfinished.rule.storage=999;_facilities={&first,&second,&unfinished};}~Base(){for(auto*t:_transfers)delete t;}
 double getUsedStores();int getAvailableStores()const;double getIgnoredStores();bool storesOverfull(double offset=0);std::string getName(){return "MCP";}int getAvailableScientists(){return 0;}int getAvailableEngineers(){return 0;}
 std::vector<Craft*>*getCrafts(){return &_crafts;}std::vector<Transfer*>*getTransfers(){return &_transfers;}ItemContainer*getStorageItems(){return _items;}std::vector<Soldier*>*getSoldiers(){return &soldiers;}
 int getAvailableQuarters(){return 100;}int getUsedQuarters(){return 0;}int getAvailableHangars(){return 10;}int getUsedHangars(){return 0;}int getAvailableContainment(){return 10;}int getUsedContainment(){return 0;}
 void removeCraft(Craft*,bool){std::abort();}int getScientists(){return scientists;}void setScientists(int n){scientists=n;}int getEngineers(){return engineers;}void setEngineers(int n){engineers=n;}
};
#include "Storage.generated.inc"

enum{BT_NONE=0,BT_CORPSE=1,BT_AMMO=2};enum OptionsOrigin{OPT_GEOSCAPE,OPT_BATTLESCAPE};
struct SavedGame{long long funds=100000;int writes=0;long long getFunds(){return funds;}void setFunds(long long){++writes;std::abort();}};
struct State;struct Screen{Surface*surface;Surface*getSurface(){return surface;}};
struct Game{SavedGame saved;Mod mod;Screen screen;Language*language;Font*big,*small;SDL_Color*palette;std::vector<State*>stack;std::vector<State*>retired;int pushes=0,pops=0;SavedGame*getSavedGame(){return &saved;}Mod*getMod(){return &mod;}Language*getLanguage(){return language;}Screen*getScreen(){return &screen;}bool isState(const void*s)const{return !stack.empty()&&stack.back()==s;}void pushState(State*s){++pushes;stack.push_back(s);}void popState(){if(stack.size()<2)std::abort();++pops;retired.push_back(stack.back());stack.pop_back();}bool agentOverlayClear(const AgentUiState&)const{return true;}};
static Game*currentGame;static unsigned statesConstructed=0,statesDestroyed=0,callbackTraps=0;
static void forbidden(){++callbackTraps;std::abort();}
struct State:OpenXcom::State{Game*_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;State(){++statesConstructed;std::copy(_game->palette,_game->palette+256,_palette);}~State(){++statesDestroyed;}void setInterface(const std::string&){}void centerAllSurfaces(){}LocalizedText tr(const std::string&id)const{return _game->language->getString(id);}void setPalette(SDL_Color*p,int first=0,int n=256,bool=true){std::copy(p,p+n,_palette+first);}void add(Surface*s){s->setPalette(_palette);s->initText(_game->big,_game->small,_game->language);_surfaces.push_back(s);}void add(Surface*s,const std::string&,const std::string&){add(s);if(auto*t=dynamic_cast<Text*>(s))t->setColor(40);if(auto*b=dynamic_cast<TextButton*>(s))b->setColor(32);}void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}void think()override{}void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}};
struct ErrorMessageState:State{TextButton*_btnOk;Window*_window;Text*_txtMessage;bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentPaintTextRevisions;std::vector<std::uint64_t>agentPaintTextRevisions()const;bool agentDestinationFull()const;void create(const std::string&,SDL_Color*,Uint8,const std::string&,int);ErrorMessageState(const std::string&,SDL_Color*,Uint8,const std::string&,int);~ErrorMessageState();void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const std::string&)override;void btnOkClick(Action*);void paint(){_window->think();for(int i=0;i<30;++i)_window->popup();blit();}};
struct TransferRow{TransferType type;void*rule;std::string name;int cost,qtySrc,qtyDst,amount;};
struct SellState:State{Base*_base;TextButton*_btnOk,*_btnCancel;Window*_window;Text*_txtTitle,*_txtSales,*_txtFunds,*_txtQuantity,*_txtSell,*_txtValue,*_txtSpaceUsed;ComboBox*_cbxCategory;TextList*_lstItems;std::vector<TransferRow>_items;std::vector<int>_rows;std::vector<std::string>_cats;std::set<std::string>_craftWeapons,_armors;size_t _sel;int _total;double _spaceChange;Timer*_timerInc,*_timerDec;Uint8 _ammoColor;OptionsOrigin _origin;
 SellState(Base*,OptionsOrigin=OPT_GEOSCAPE);~SellState();std::string getCategory(int)const;void updateList();
 void btnOkClick(Action*){forbidden();}void btnCancelClick(Action*){forbidden();}void lstItemsLeftArrowPress(Action*){forbidden();}void lstItemsLeftArrowRelease(Action*){forbidden();}void lstItemsLeftArrowClick(Action*){forbidden();}void lstItemsRightArrowPress(Action*){forbidden();}void lstItemsRightArrowRelease(Action*){forbidden();}void lstItemsRightArrowClick(Action*){forbidden();}void lstItemsMousePress(Action*){forbidden();}void cbxCategoryChange(Action*){forbidden();}void increase(){forbidden();}void decrease(){forbidden();}
 void paint(){_window->think();blit();}
};
#include "Error.generated.inc"
#include "Sell.generated.inc"
struct IdleTimer{int calls=0;void think(State*,void*){++calls;}bool isRunning(){return false;}};
struct Globe{int stops=0;void rotateStop(){++stops;}};
struct GeoscapeState:State{bool _pause=false,_zoomInEffectDone=true,_zoomOutEffectDone=true;unsigned _minimizedDogfights=0;std::vector<State*>_popups;std::vector<void*>_dogfights;IdleTimer zoomIn,zoomOut,start,gameTimer,dogfight;IdleTimer*_zoomInEffectTimer=&zoomIn,*_zoomOutEffectTimer=&zoomOut,*_dogfightStartTimer=&start,*_gameTimer=&gameTimer,*_dogfightTimer=&dogfight;Globe globe;Globe*_globe=&globe;
 void popup(State*);void think()override;void storageBlock(std::vector<Base*>::iterator i){
#include "StorageAdmission.generated.inc"
 }
};
#include "Queue.generated.inc"
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int run(int argc,char**argv){
 check(argc==3,"source/log arguments");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen widgets only");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;Options::storageLimitsEnforced=true;Options::canSellLiveAliens=false;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string master:{"xcom1","xcom2"}){Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");Game game;game.mod.background=&background;game.screen.surface=&canvas;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;std::copy(palette,palette+256,game.mod.palette.colors);currentGame=&game;RuleItem rule("SYNTHETIC_ITEM",1);rule.clip=0;game.mod.rules[rule.id]=&rule;Base base(&game.mod);base.storage._qty[rule.id]=101;std::vector<Base*>bases{&base};
 unsigned constructed=statesConstructed,destroyed=statesDestroyed;auto*geo=new GeoscapeState;game.stack.push_back(geo);auto stock=base.storage._qty;auto pending=base._transfers.size();auto funds=game.saved.funds;check(base.getAvailableStores()==100&&base.getUsedStores()==101&&base.storesOverfull(),"actual positive facilities100 and item101 overfull");geo->storageBlock(bases.begin());check(geo->_pause&&geo->_popups.size()==2&&game.stack.back()==geo&&statesConstructed==constructed+3,"literal block eagerly constructs Error and Sell without activation");auto*error=dynamic_cast<ErrorMessageState*>(geo->_popups[0]);auto*sell=dynamic_cast<SellState*>(geo->_popups[1]);check(error&&sell&&sell->_base==&base,"exact FIFO Error then real Sell identity");check(sell->_items.size()==1&&sell->_items[0].qtySrc==101&&sell->_items[0].amount==0&&sell->_items[0].cost==7,"nonempty literal Sell constructor draft quantities and price");check(sell->_lstItems->getCellText(0,1)=="101"&&sell->_lstItems->getCellText(0,2)=="0"&&sell->_lstItems->getCellText(0,3)==Unicode::formatFunding(7),"actual updateList cells not empty-stock substitution");check(!sell->_btnOk->getVisible()&&!sell->_btnCancel->getVisible(),"forced Sell controls hidden at eager construction");
 geo->think();check(game.stack.back()==error&&geo->_popups.size()==1&&geo->_popups.front()==sell&&game.pushes==1&&geo->globe.stops==1&&geo->gameTimer.calls==0,"whole native think activates FIFO Error only without game time");error->paint();AgentUiState ui;check(error->agentUiState(ui)&&ui.screen=="error_message"&&ui.actions.size()==1&&ui.offers("dismiss_error"),"current Error provider exact sole dismiss action");auto visible=YAML::Load(ui.visible);std::string expected=language.getString("STR_STORAGE_EXCEEDED").arg("MCP");check(visible["message"].IsScalar()&&visible["message"].as<std::string>()==expected,"exact current localized full storage message, not null fallback");check(visible["heading"].IsScalar()&&visible["heading"].as<std::string>()=="STORAGE SPACE EXCEEDED!","exact current painted heading");check(error->agentDispatch("dismiss_error")&&game.pops==1&&game.stack.back()==geo&&geo->_popups.front()==sell&&game.pushes==1&&statesConstructed==constructed+3,"one Error OK onlypop, no Sell construction/directpush");check(game.retired.size()==1&&game.retired.front()==error,"Error retirement deferred past dispatch");error->release();delete error;game.retired.clear();
 geo->think();check(game.stack.back()==sell&&geo->_popups.empty()&&game.pushes==2&&geo->globe.stops==2&&geo->gameTimer.calls==0&&statesConstructed==constructed+3,"next native think activates same preconstructed Sell");sell->paint();std::string expectedTitle=language.getString("STR_SELL_ITEMS_SACK_PERSONNEL");check(sell->_txtTitle->getText()==expectedTitle&&agentTextJson(sell->_txtTitle)!="null","current Sell title natively painted");std::string space=language.getString("STR_SPACE_USED").arg("101:100");check(sell->_txtSpaceUsed->getText()==space&&agentTextJson(sell->_txtSpaceUsed)!="null","current native space101:100 caption painted");check(!sell->_btnOk->getVisible()&&!sell->_btnCancel->getVisible()&&sell->_lstItems->getCellText(0,1)=="101"&&sell->_lstItems->getCellText(0,2)=="0","forced hiddencontrols and nonempty unchanged draft after paint");AgentUiState unsupported;check(!sell->agentUiState(unsupported),"Sell inherited unsupported, no invented provider/actions");check(base.storage._qty==stock&&base._transfers.size()==pending&&game.saved.funds==funds&&game.saved.writes==0&&callbackTraps==0,"no sale/event/economic mutation");check(game.pops==1&&game.pushes==2,"bounded continuation stops before any Sell action");
 game.stack.clear();sell->release();delete sell;geo->release();delete geo;check(statesDestroyed==destroyed+3,"all owned states retired once after observation");
 }
 std::cout<<"PASS "<<checks<<" forced Error to preconstructed nonempty Sell continuation checks; explicit lifecycle/domain/render seams\n";return 0;
}
}
int main(int argc,char**argv){return Fixture::run(argc,argv);}
