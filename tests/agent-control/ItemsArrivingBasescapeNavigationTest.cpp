// SPDX-License-Identifier: GPL-3.0-or-later
// Literal ItemsArriving callback constructs actual current Basescape; two bounded master cases.
// Explicit Game/Mod/Base/Region/BaseView/Mini/resource/render seams; no full navigation or rename qualification.
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
static bool forbidModelRead=false;static unsigned modelReads=0,modelWrites=0;
static void modelRead(){++modelReads;if(forbidModelRead){std::cerr<<"FAIL post-init model getter\n";std::abort();}}
static unsigned deletedTransfers=0,reuseCalls=0;
static std::vector<std::string> events;
enum {TRANSFER_ITEM,BT_NONE};
struct Transfer {~Transfer(){++deletedTransfers;}int getHours(){modelRead();return 0;}int getQuantity(){modelRead();return 3;}int getType(){modelRead();return TRANSFER_ITEM;}std::string getItems(){modelRead();return "SYNTHETIC_ITEM";}std::string getName(Language*){modelRead();return "SYNTHETIC ITEM";}};
struct RuleItem{int getBattleType(){modelRead();return BT_NONE;}};
struct Craft{void reuseItem(const std::string&){++reuseCalls;}};
struct Base {std::string name="MCP Research";std::vector<Transfer*>transfers;std::vector<Craft*>crafts;std::vector<Transfer*>*getTransfers(){modelRead();return &transfers;}std::vector<Craft*>*getCrafts(){modelRead();return &crafts;}Base(){}template<class T>explicit Base(T*){}std::string getName(){modelRead();return name;}void setName(const std::string&v){name=v;++modelWrites;}double getLongitude(){modelRead();return 1;}double getLatitude(){modelRead();return 2;}};
struct RuleRegion{bool insideRegion(double,double){modelRead();return true;}std::string getType(){modelRead();return "STR_EUROPE";}};
struct Region{RuleRegion rules;RuleRegion*getRules(){modelRead();return &rules;}};
struct SavedGame{std::vector<Base*>bases;std::vector<Region*>regions;unsigned selected=99;std::vector<Base*>*getBases(){modelRead();return &bases;}std::vector<Region*>*getRegions(){modelRead();return &regions;}int getFunds(){modelRead();return 123456;}void setSelectedBase(unsigned n){selected=n;++modelWrites;}};
struct Globe{};
struct GeoscapeState{Globe globe;TextButton speed{10,10};TextButton*_btn5Secs=&speed;TextButton*selected=nullptr;unsigned resets=0;void selectGeoscapeSpeed(TextButton*b){selected=b;++resets;events.push_back("timer");}void timerReset();Globe*getGlobe(){return &globe;}};
#include "Timer.generated.inc"
struct BasescapeState;struct ItemsArrivingState;

struct BaseView:InteractiveSurface{using InteractiveSurface::InteractiveSurface;Base*base=nullptr;void setTexture(void*){}void setBase(Base*b){base=b;}};
struct MiniBaseView:InteractiveSurface{using InteractiveSurface::InteractiveSurface;enum{MAX_BASES=8};struct Slot{unsigned slot;bool selected;};std::vector<Base*>*bases=nullptr;unsigned selected=0;
 void setTexture(void*){}void setBases(std::vector<Base*>*b){bases=b;}void setSelectedBase(size_t n){selected=n;}
 std::uint64_t agentRevision()const{return 1;}std::vector<Slot>agentSlots(const AgentProjectileOutput&)const{return {};}
 void hoverAt(int){} };
struct Screen{Surface*surface;Surface*getSurface(){return surface;}bool agentCaptureOutput(const Surface*,AgentProjectileOutput&)const{return false;}};
struct Mod{RuleItem item;RuleItem*getItem(const std::string&,bool){modelRead();return &item;}Surface*background;Surface*getSurface(const std::string&){return background;}void*getSurfaceSet(const std::string&){return nullptr;}};
struct Game{SavedGame saved;SavedGame*getSavedGame(){modelRead();return &saved;}Screen screen;Mod mod;Language*language;Font*big,*small;SDL_Color*palette;
 const void*top=nullptr;ItemsArrivingState*retired=nullptr;BasescapeState*opened=nullptr;int pushes=0;bool lifecycleOpened=false;int pops=0;bool clear=true,cursorVisible=false,fpsVisible=false;SDL_Rect cursor={},fps={};
 Surface*getCursor(){return nullptr;}Surface*getFpsCounter(){return nullptr;}Screen*getScreen(){return &screen;}Mod*getMod(){return &mod;}Language*getLanguage(){return language;}bool isState(const void*s)const{return top==s;}void popState();void pushState(BasescapeState*);void activate();void retire();
 bool agentNonCursorOverlayClear(const AgentUiState&u)const{if(!clear)return false;for(const auto&r:u.regions)if(fpsVisible&&r.x<fps.x+fps.w&&r.x+r.width>fps.x&&r.y<fps.y+fps.h&&r.y+r.height>fps.y)return false;return true;}
 bool agentOverlayClear(const AgentUiState&u)const{if(!agentNonCursorOverlayClear(u))return false;for(const auto&r:u.regions)if(cursorVisible&&r.x<cursor.x+cursor.w&&r.x+r.width>cursor.x&&r.y<cursor.y+cursor.h&&r.y+r.height>cursor.y)return false;return true;}
};
static Game*currentGame;
struct State:OpenXcom::State{Game*_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;
 void setInterface(const std::string&){}void centerAllSurfaces(){}LocalizedText tr(const std::string&id)const{return _game->language->getString(id);}
 void add(Surface*s){s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);_surfaces.push_back(s);}
 void add(Surface*s,const std::string&,const std::string&){add(s);if(auto*t=dynamic_cast<Text*>(s))t->setColor(40);if(auto*b=dynamic_cast<TextButton*>(s))b->setColor(32);if(auto*e=dynamic_cast<TextEdit*>(s))e->setColor(40);}
 void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}
};
struct BasescapeState:State {
	BaseView *_view;
	MiniBaseView *_mini;
	Text *_txtFacility, *_txtLocation, *_txtFunds;
	TextEdit *_edtBase;
	TextButton *_btnNewBase, *_btnBaseInfo, *_btnSoldiers, *_btnCrafts, *_btnFacilities, *_btnResearch, *_btnManufacture, *_btnTransfer, *_btnPurchase, *_btnSell, *_btnGeoscape;
	Base *_base;
	Globe *_globe;
BasescapeState(Base*,Globe*);~BasescapeState();void init();void setBase(Base*);void edtBaseChange(Action*);void btnGeoscapeClick(Action*);
void btnNewBaseClick(Action*){std::abort();}
void btnBaseInfoClick(Action*){std::abort();}
void btnSoldiersClick(Action*){std::abort();}
void btnCraftsClick(Action*){std::abort();}
void btnFacilitiesClick(Action*){std::abort();}
void btnResearchClick(Action*){std::abort();}
void btnManufactureClick(Action*){std::abort();}
void btnPurchaseClick(Action*){std::abort();}
void btnSellClick(Action*){std::abort();}
void btnTransferClick(Action*){std::abort();}
void viewLeftClick(Action*){std::abort();}
void viewRightClick(Action*){std::abort();}
void viewMouseOver(Action*){std::abort();}
void viewMouseOut(Action*){std::abort();}
void miniClick(Action*){std::abort();}
void handleKeyPress(Action*){std::abort();}
bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
struct ItemsArrivingState:State {
 GeoscapeState *_state;Base *_base;TextButton *_btnOk,*_btnGotoBase;Window *_window;Text *_txtTitle,*_txtItem,*_txtQuantity,*_txtDestination;TextList *_lstTransfers;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 ItemsArrivingState(GeoscapeState*);~ItemsArrivingState();void btnOkClick(Action*);void btnGotoBaseClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void paint(){_window->think();for(int i=0;i<30;++i)_window->popup();blit();}
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "Basescape.generated.inc"
#include "ItemsArriving.generated.inc"
void Game::popState(){if(!forbidModelRead||retired||opened){std::abort();}++pops;events.push_back("pop");retired=(ItemsArrivingState*)top;top=nullptr;lifecycleOpened=true;forbidModelRead=false;}
void Game::pushState(BasescapeState*s){if(!lifecycleOpened||!retired||top||opened)std::abort();++pushes;events.push_back("push");opened=s;top=s;}
void Game::activate(){if(!opened||!lifecycleOpened)std::abort();opened->init();events.push_back("init");forbidModelRead=true;lifecycleOpened=false;}
void Game::retire(){if(!retired||lifecycleOpened)std::abort();retired->release();delete retired;retired=nullptr;events.push_back("retire");}

}
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==3,"source/log arguments");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen nativewidgets");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;Options::keyboardMode=KEYBOARD_ON;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string master:{"xcom1","xcom2"}){
 Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
 Fixture::Base base;Fixture::Region region;Fixture::Craft craft;Fixture::GeoscapeState geo;base.transfers={new Fixture::Transfer};base.crafts={&craft};game.saved.bases={&base};game.saved.regions={&region};unsigned deleted=Fixture::deletedTransfers,reused=Fixture::reuseCalls;
 auto*arrival=new Fixture::ItemsArrivingState(&geo);game.top=arrival;
 check(base.transfers.empty()&&Fixture::deletedTransfers==deleted+1&&Fixture::reuseCalls==reused+1,"literal constructor consumes one completed synthetic batch, not delivery");check(arrival->_base==&base,"private native lastdestination binding");
 Fixture::forbidModelRead=true;unsigned reads=Fixture::modelReads,writes=Fixture::modelWrites;arrival->paint();AgentUiState ui;check(arrival->agentUiState(ui)&&ui.screen=="items_arriving"&&ui.offers("open_arrival_base"),"current arrival offered go-to-base");auto rows=YAML::Load(ui.visible)["rows"];check(rows.size()==1&&rows[0]["cells"][0].as<std::string>()=="SYNTHETIC ITEM"&&rows[0]["cells"][1].as<std::string>()=="3"&&rows[0]["cells"][2].as<std::string>()=="MCP Research","one current three-cell arrival row");check(Fixture::modelReads==reads&&Fixture::modelWrites==writes,"source projection does not access models");
 Fixture::events.clear();AgentRequest q;q.action="open_arrival_base";check(arrival->agentDispatch(q),"literal dispatch constructs actual Basescape");
 check(Fixture::events==std::vector<std::string>({"timer","pop","push"})&&geo.resets==1&&geo.selected==geo._btn5Secs,"literal timerReset and callback order");check(game.retired==arrival&&game.opened&&game.top==game.opened,"source deferred retirement until whole dispatch returns");check(game.opened->_base==&base&&game.opened->_globe==&geo.globe,"actual constructed target preserves private base/globe identity");
 game.activate();check(Fixture::forbidModelRead&&!game.lifecycleOpened&&game.saved.selected==0&&game.opened->_view->base==&base,"actual target init/setBase complete, trap restored");game.retire();check(!game.retired&&Fixture::events==std::vector<std::string>({"timer","pop","push","init","retire"}),"deferred source deletion after completed callback and target activation");
 auto*target=game.opened;reads=Fixture::modelReads;writes=Fixture::modelWrites;target->blit();check(target->agentUiState(ui)&&ui.screen=="basescape"&&YAML::Load(ui.visible)["base_name"].as<std::string>()=="MCP Research","actual current target painted name nonnull");base.name="HIDDEN MODEL FALLBACK";check(target->agentUiState(ui)&&YAML::Load(ui.visible)["base_name"].as<std::string>()=="MCP Research","painted name does not reread changed model");
 auto*edit=target->_edtBase;edit->setVisible(false);target->blit();check(target->agentUiState(ui)&&YAML::Load(ui.visible)["base_name"].IsNull()&&ui.visible.find("HIDDEN MODEL FALLBACK")==std::string::npos,"hidden target editor exact null no fallback");edit->setVisible(true);target->blit();check(target->agentUiState(ui)&&YAML::Load(ui.visible)["base_name"].as<std::string>()=="MCP Research","fresh shown editor restores actual child");SDL_Rect tiny={0,0,1,1};SDL_SetClipRect(edit->getSurface(),&tiny);edit->invalidate();target->blit();check(target->agentUiState(ui)&&YAML::Load(ui.visible)["base_name"].IsNull()&&ui.visible.find("HIDDEN MODEL FALLBACK")==std::string::npos,"fresh clipped editor exact null no fallback");SDL_SetClipRect(edit->getSurface(),nullptr);edit->invalidate();target->blit();check(target->agentUiState(ui)&&YAML::Load(ui.visible)["base_name"].as<std::string>()=="MCP Research","fresh unclipped target name restores");
 check(Fixture::modelReads==reads&&Fixture::modelWrites==writes&&Fixture::deletedTransfers==deleted+1&&Fixture::reuseCalls==reused+1,"target observations no model access, init repeat or transfer replay");check(game.pops==1&&game.pushes==1&&geo.resets==1,"exactly one navigation no automatic extra action");
 target->release();Fixture::forbidModelRead=false;game.top=nullptr;delete target;game.opened=nullptr;
 }
 std::cout<<"PASS "<<checks<<" ItemsArriving to actual Basescape constructor/init checks; explicit stack/domain/render seams\n";
}
