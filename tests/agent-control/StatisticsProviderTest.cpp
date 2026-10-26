// SPDX-License-Identifier: GPL-3.0-or-later
// Actual constructor/provider/callback bodies; explicit Game/Mod/Craft seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Window.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <sstream>
#include "../../src/Engine/Unicode.h"
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
struct Screen { Surface *surface; Surface *getSurface(){return surface;} };
struct Element{int color2=80;};struct Interface{Element e;Element*getElement(const std::string&){return &e;}};
struct Mod {Surface*background;Interface iface;int music=0;Surface*getSurface(const std::string&){return background;}int getDefeatScore(){return -1000;}long getDefeatFunds(){return -1000000;}Interface*getInterface(const std::string&){return &iface;}void playMusic(const std::string&){++music;}};
struct Diary{int service=0;bool medal=false;void addMonthlyService(){++service;}bool manageCommendations(Mod*,void*){return medal;}};
struct Soldier{int id=1;Diary diary;int getId(){return id;}Diary*getDiary(){return &diary;}};
struct Base{std::vector<Soldier*> soldiers;std::vector<Soldier*>*getSoldiers(){return &soldiers;}};
struct Time{int getMonth(){return 2;}int getYear(){return 1999;}};
enum{END_NONE=0,END_LOSE=2,OPT_GEOSCAPE=1,SAVE_IRONMAN=2,SAVE_AUTO_GEOSCAPE=3};
struct SavedGame{Time time;bool warned=false,ironman=false;int ending=0;std::vector<Base*>bases;
 int getEnding(){return ending;}Time*getTime(){return &time;}int getDifficultyCoefficient(){return 0;}long getCountryFunding(){return 1000;}long getBaseMaintenance(){return 100;}long getFunds(){return 2000;}bool getWarned(){return warned;}void setWarned(bool b){warned=b;}void setEnding(int e){ending=e;}bool isIronman(){return ironman;}
 std::vector<Base*>*getBases(){return &bases;}Soldier*getSoldier(int id){for(auto*b:bases)for(auto*s:b->soldiers)if(s->id==id)return s;std::abort();}void*getMissionStatistics(){return nullptr;}
};
struct Queued{std::string kind;explicit Queued(std::string s):kind(s){}virtual~Queued(){}};
struct CommendationState:Queued{explicit CommendationState(const std::vector<Soldier*>&):Queued("commendation") {}};
struct PsiTrainingState:Queued{PsiTrainingState():Queued("psi") {}};
struct SaveGameState:Queued{SaveGameState(int,int mode,SDL_Color*):Queued(mode==SAVE_IRONMAN?"ironman":"autosave") {}};
struct CutsceneState:Queued{enum{LOSE_GAME};explicit CutsceneState(int):Queued("lose") {}};
struct GoToMainMenuState:Queued{GoToMainMenuState():Queued("main_menu") {}};
struct Game {
 Screen screen;Mod mod;SavedGame save;Language*language;Font *big,*small;SDL_Color*palette;const void*top=nullptr;bool clear=true;int pops=0;std::vector<std::string> events;
 int cleared=0;void setSavedGame(int value){if(value==0){++cleared;events.push_back("clear_save");}}void setState(Queued*q){top=nullptr;events.push_back(q->kind);delete q;}
 Screen*getScreen(){return &screen;}Mod*getMod(){return &mod;}SavedGame*getSavedGame(){return &save;}Language*getLanguage(){return language;}bool isState(const void*s)const{return top==s;}
 bool cursorVisible=false;SDL_Rect cursor={0,0,0,0};
 bool agentOverlayClear(const AgentUiState&)const{return clear;} // Ordinary cursor is not an overlay; clear models actual covering UI.
 bool agentNonCursorOverlayClear(const AgentUiState&)const{return clear;}
 void popState(){++pops;top=nullptr;events.push_back("pop");}void pushState(Queued*q){events.push_back(q->kind);top=nullptr;delete q;}
};
static Game *currentGame;
struct State : OpenXcom::State {
    Game *_game = currentGame;
    std::function<void()> paintHook;
    using OpenXcom::State::_surfaces;
    using OpenXcom::State::_modal;
    void setInterface(const std::string&){}
    void centerAllSurfaces(){}
    LocalizedText tr(const std::string &id) const{return _game->language->getString(id);}
    void add(Surface *surface,const std::string&,const std::string&){
        surface->setPalette(_game->palette);
        surface->initText(_game->big,_game->small,_game->language);
        _surfaces.push_back(surface);
    }
    void blit() override{
        for(auto *surface:_surfaces)surface->blit(_game->screen.surface);
        if(paintHook)paintHook();
    }
};
struct Globe{};
struct StatisticsState:State {
 TextButton *_btnOk;Window *_window;Text *_txtTitle;TextList *_lstStats;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;
 StatisticsState(){
  _window=new Window(this,320,200,0,0);_txtTitle=new Text(300,16,10,10);_btnOk=new TextButton(50,12,135,180);_lstStats=new TextList(280,140,20,30);
  add(_window,"","");add(_txtTitle,"","");add(_lstStats,"","");add(_btnOk,"","");
  _window->setBackground(_game->mod.background);_window->setColor(80);_txtTitle->setText("Defeat 1st Mar 1999");_btnOk->setText("OK");
  _lstStats->setColumns(2,180,100);_lstStats->addRow(2,"Shown label","Shown value");
#include "StatisticsTracking.generated.inc"
 }
 void btnOkClick(Action*);bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "Statistics.generated.inc"
}
static unsigned checks=0;static void check(bool ok,const char *message){++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==3,"source and master arguments");check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 AgentRequest request;const std::string token(32,'1'),prefix="E "+token+" "+token+" "+token+" 1 ";
 check(parseAgentRequest(prefix+"close_statistics",request),"empty args protocol accepted");for(const std::string extra:{" extra"," r0"," true"," 0","\n"})check(!parseAgentRequest(prefix+"close_statistics"+extra,request),"extra protocol args rejected");
 for(const std::string master:{"xcom1","xcom2"}){
 if(std::string(argv[2])!="both"&&std::string(argv[2])!=master)continue;
 Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
 for(int ending:{Fixture::END_NONE,Fixture::END_LOSE}){
 Fixture::StatisticsState state;game.top=&state;game.save.ending=ending;game.events.clear();game.pops=0;game.cleared=0;AgentUiState ui;request.action="close_statistics";
 check(!state.agentUiState(ui),"prepaint refused");state.blit();check(state.agentUiState(ui)&&ui.screen=="statistics"&&ui.offers(request.action),"painted title and OK admitted");auto visible=YAML::Load(ui.visible);check(visible.size()==2&&visible["title"].as<std::string>()==state._txtTitle->getText()&&visible["buttons"].size()==1,"title only; no statistics table or ending field");
 game.cursorVisible=true;game.cursor={136,181,4,4};check(state.agentUiState(ui)&&ui.offers(request.action),"ordinary cursor does not occlude OK");game.cursorVisible=false;
 state._txtTitle->setText("changed");check(!state.agentUiState(ui),"title change stale");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()=="changed","current title painted");
 auto label=state._btnOk->getText();state._btnOk->setText("other");state._btnOk->setText(label);check(!state.agentDispatch(request),"button label ABA stale");state.blit();
 state._btnOk->setVisible(false);state.blit();check(!state.agentDispatch(request),"hidden button refused");state._btnOk->setVisible(true);state.blit();
 state._modal=state._btnOk;check(!state.agentDispatch(request),"modal blocks");state._modal=nullptr;game.top=nullptr;check(!state.agentDispatch(request),"wrong top blocks");game.top=&state;game.clear=false;check(!state.agentDispatch(request),"actual overlay blocks");game.clear=true;
 Surface cover(10,10,135,180);cover.setPalette(palette);cover.clear(30);state._surfaces.push_back(&cover);state.blit();check(!state.agentDispatch(request),"later painted cover blocks OK");state._surfaces.pop_back();state.blit();
 SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentDispatch(request),"partial destination paint denied");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentDispatch(request),"clip restore needs repaint");state.blit();
 request.action="acknowledge_monthly_failure";check(!state.agentDispatch(request)&&game.events.empty(),"unrelated action denied");request.action="close_statistics";
 check(state.agentDispatch(request),"native statistics close dispatched");
 if(ending==Fixture::END_NONE)check(game.events==std::vector<std::string>{"pop"}&&game.cleared==0&&game.pops==1,"nonending callback pops without clearing save");
 else check(game.events==std::vector<std::string>{"clear_save","main_menu"}&&game.cleared==1&&game.pops==0,"terminal callback clears active save then main menu");
 auto events=game.events;check(!state.agentDispatch(request)&&game.events==events,"top change prevents repeated close");state.release();
 }
 }
 check(!SDL_GetVideoSurface(),"no game video");std::cout<<"PASS "<<checks<<" statistics provider/dispatch/native callback checks; synthetic widgets/domain seams\n";
}
