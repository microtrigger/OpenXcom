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
enum{END_LOSE=2,OPT_GEOSCAPE=1,SAVE_IRONMAN=2,SAVE_AUTO_GEOSCAPE=3};
struct SavedGame{Time time;bool warned=false,ironman=false;int ending=0;std::vector<Base*>bases;
 Time*getTime(){return &time;}int getDifficultyCoefficient(){return 0;}long getCountryFunding(){return 1000;}long getBaseMaintenance(){return 100;}long getFunds(){return 2000;}bool getWarned(){return warned;}void setWarned(bool b){warned=b;}void setEnding(int e){ending=e;}bool isIronman(){return ironman;}
 std::vector<Base*>*getBases(){return &bases;}Soldier*getSoldier(int id){for(auto*b:bases)for(auto*s:b->soldiers)if(s->id==id)return s;std::abort();}void*getMissionStatistics(){return nullptr;}
};
struct Queued{std::string kind;explicit Queued(std::string s):kind(s){}virtual~Queued(){}};
struct CommendationState:Queued{explicit CommendationState(const std::vector<Soldier*>&):Queued("commendation") {}};
struct PsiTrainingState:Queued{PsiTrainingState():Queued("psi") {}};
struct SaveGameState:Queued{SaveGameState(int,int mode,SDL_Color*):Queued(mode==SAVE_IRONMAN?"ironman":"autosave") {}};
struct CutsceneState:Queued{enum{LOSE_GAME};explicit CutsceneState(int):Queued("lose") {}};
struct Game {
 Screen screen;Mod mod;SavedGame save;Language*language;Font *big,*small;SDL_Color*palette;const void*top=nullptr;bool clear=true;int pops=0;std::vector<std::string> events;
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
struct MonthlyReportState:State {
 TextButton *_btnOk,*_btnBigOk;Window*_window;Text *_txtTitle,*_txtMonth,*_txtRating,*_txtIncome,*_txtMaintenance,*_txtBalance,*_txtDesc,*_txtFailure;
 bool _psi,_gameOver;int _ratingTotal,_fundingDiff,_lastMonthsRating;std::vector<std::string>_happyList,_sadList,_pactList;Globe*_globe;std::vector<Soldier*>_soldiersMedalled;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;
 MonthlyReportState(bool,Globe*);~MonthlyReportState();void init()override;void btnOkClick(Action*);
 void calculateChanges(){_ratingTotal=100;_lastMonthsRating=100;} // Explicit financial/domain calculation seam, not economic qualification.
 std::string countryList(const std::vector<std::string>&,const std::string&,const std::string&){return "";}
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "MonthlyReport.generated.inc"
}
static unsigned checks = 0;
static void check(bool ok,const char *message){
    ++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}
}
int main(int argc,char **argv){
    check(argc==3,"source and master arguments");
    check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");
    FileMap::load("common","./",false);
    Options::baseXResolution=320;Options::baseYResolution=200;
    Font big,small,overlap;
    for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){
        if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);
        if(f["id"].as<std::string>()=="FONT_SMALL"){
            small.load(f);auto altered=YAML::Clone(f);altered["spacing"]=-2;
            for(auto image:altered["images"])image["spacing"]=-2;
            overlap.load(altered);
        }
    }
    Surface canvas(320,200),background(320,200);SDL_Color palette[256];
    for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
    canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
    AgentRequest request;
    const std::string token(32,'1'),prefix="E "+token+" "+token+" "+token+" 1 ";
    for(const std::string action:{"acknowledge_monthly_report","acknowledge_monthly_failure"}){
        check(parseAgentRequest(prefix+action,request),"empty argument protocol accepted");
        for(const std::string extra:{" extra"," r0"," true"," 0","\n"})
            check(!parseAgentRequest(prefix+action+extra,request),"nonempty argument rejected");
    }
    for(const std::string master:{"xcom1","xcom2"}){
 if(std::string(argv[2])!="both"&&std::string(argv[2])!=master)continue;
 Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");
 Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;Fixture::Globe globe;
 Fixture::MonthlyReportState state(false,&globe);game.top=&state;AgentUiState ui;request.action="acknowledge_monthly_report";
 check(!state.agentUiState(ui),"prepaint refused");state.init();check(game.save.ending==0,"ordinary init no ending");state.blit();check(state.agentUiState(ui)&&ui.screen=="monthly_report","literal constructor admitted");
 auto visible=YAML::Load(ui.visible);check(visible.size()==9&&visible["failure"].IsNull()&&visible["title"].as<std::string>()==std::string(language.getString("STR_XCOM_PROJECT_MONTHLY_REPORT")),"exact fieldset and current title");check(ui.actions.size()==1&&ui.offers(request.action),"normal OK only");
 std::vector<std::pair<Text*,const char*>> captions={{state._txtTitle,"title"},{state._txtMonth,"month"},{state._txtRating,"rating"},{state._txtIncome,"income"},{state._txtMaintenance,"maintenance"},{state._txtBalance,"balance"},{state._txtDesc,"description"}};
 for(auto pair:captions){auto*t=pair.first;const std::string old=t->getText();check(state.agentUiState(ui)&&YAML::Load(ui.visible)[pair.second].as<std::string>()==old,"active widget text exact");t->setText(std::string(500,'W'));check(!state.agentUiState(ui),"text mutation stale");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)[pair.second].as<std::string>()==std::string(500,'W'),"logical overflow exact");SDL_FillRect(t->getSurface(),nullptr,0);check(state.agentUiState(ui)&&YAML::Load(ui.visible)[pair.second].as<std::string>()==std::string(500,'W'),"ink not text admission");t->setText("\xF0\x9F\x99\x82");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)[pair.second].as<std::string>()=="\xF0\x9F\x99\x82","missing glyph exact");t->setText("MULTI\nLINE\nCAPTION");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)[pair.second].as<std::string>()=="MULTI\nLINE\nCAPTION","multiline logical caption exact");t->setText(old);state.blit();}
 Surface cover(4,2,17,10);cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].IsNull(),"retained title strip real occlusion");state._surfaces.pop_back();state.blit();Surface spare(4,1,17,24);spare.setPalette(palette);spare.clear(80);state._surfaces.push_back(&spare);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==state._txtTitle->getText(),"fixed spare row not caption obstruction");state._surfaces.pop_back();state.blit();
 game.cursorVisible=true;game.cursor={17,9,4,4};check(state.agentUiState(ui)&&ui.offers(request.action),"caption cursor not admission");game.cursor={136,181,4,4};check(state.agentUiState(ui)&&ui.offers(request.action)&&game.pops==0,"control cursor does not occlude");game.cursorVisible=false;
 for(Surface*w:std::vector<Surface*>{state._txtTitle,state._txtFailure,state._btnOk,state._btnBigOk}){int x=w->getX();w->setX(x+1);w->setX(x);check(!state.agentUiState(ui),"geometry ABA");state.blit();bool v=w->getVisible();w->setVisible(!v);w->setVisible(v);check(!state.agentUiState(ui),"visibility ABA");state.blit();}
 state._modal=state._btnOk;check(!state.agentUiState(ui)&&!state.agentDispatch(request),"modal refused");state._modal=nullptr;game.top=nullptr;check(!state.agentUiState(ui),"wrong top");game.top=&state;game.clear=false;check(!state.agentUiState(ui),"overlay refused");game.clear=true;
 SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clip refused");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restore needs paint");state.blit();
 check(state.agentUiState(ui),"authority baseline");AgentFrameCommand frame;check(frame.stage(request,ui,&state,1,1,100,100),"stage acknowledgement");Fixture::Globe other;state._globe=&other;AgentUiState changed;check(state.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&state,changed),"actor change invalidates");state._globe=&globe;
 for(const std::string invalid:{"acknowledge_craft_error","acknowledge_monthly_report_5_seconds","acknowledge_monthly_failure","unknown"}){request.action=invalid;check(!state.agentDispatch(request)&&game.pops==0,"invalid neighbour no dispatch");}request.action="acknowledge_monthly_report";
 state._txtFailure->setVisible(true);state.blit();check((!state.agentUiState(ui)||!ui.offers(request.action))&&!state.agentDispatch(request),"failure visible suppresses normal OK even both visible");state._txtFailure->setVisible(false);state._btnBigOk->setVisible(true);state.blit();check((!state.agentUiState(ui)||!ui.offers(request.action))&&!state.agentDispatch(request),"large button alone suppresses ordinary action");state._btnBigOk->setVisible(false);state.blit();
 state._gameOver=true;state.init();check(game.save.ending==Fixture::END_LOSE,"literal defeat init seam");check(state.agentUiState(ui)&&ui.offers(request.action),"no hidden gameOver admission gate before visible defeat");check(state.agentDispatch(request)&&game.pops==0&&state._txtFailure->getVisible()&&state._btnBigOk->getVisible()&&!state._btnOk->getVisible()&&game.mod.music==1,"first defeat normal OK switches visible presentation only");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["failure"].as<std::string>()==state._txtFailure->getText()&&!ui.offers(request.action),"failure caption observable; ordinary acknowledgement unavailable");check(!state.agentDispatch(request)&&game.events.empty(),"ordinary action cannot dispatch failure branch");
 request.action="acknowledge_monthly_failure";
 check(state.agentUiState(ui)&&ui.offers(request.action),"large painted failure OK offered separately");
 state._btnBigOk->setVisible(false);state.blit();check((!state.agentUiState(ui)||!ui.offers(request.action))&&!state.agentDispatch(request),"hidden failure OK refused");state._btnBigOk->setVisible(true);state.blit();
 std::string failureLabel=state._btnBigOk->getText();state._btnBigOk->setText("changed");state._btnBigOk->setText(failureLabel);check(!state.agentDispatch(request),"failure button label ABA refused");state.blit();
 game.clear=false;check(!state.agentDispatch(request)&&game.events.empty(),"actual overlay blocks ending transition");game.clear=true;
 check(state.agentDispatch(request)&&game.events==std::vector<std::string>{"lose"},"failure acknowledgment invokes native terminal cutscene queue once");
 check(!state.agentDispatch(request)&&game.events.size()==1,"duplicate failure dispatch refused after queued cutscene owns top");state.release();request.action="acknowledge_monthly_report";
 // Exercise literal normal handler queue ordering with explicit domain counters.
 for(int mode=0;mode<4;++mode){game.events.clear();game.pops=0;game.save.ironman=mode==3;Options::autosave=mode==2||mode==3;Fixture::Soldier soldier;soldier.diary.medal=mode!=0;Fixture::Base base;base.soldiers.push_back(&soldier);game.save.bases={&base};Fixture::MonthlyReportState normal(mode!=0,&globe);game.top=&normal;normal.init();normal.blit();check(normal.agentDispatch(request)&&game.pops==1&&soldier.diary.service==1,"normal single pop and service award");std::vector<std::string>expected={"pop"};if(mode!=0){expected.push_back("commendation");expected.push_back("psi");}if(mode==2)expected.push_back("autosave");if(mode==3)expected.push_back("ironman");check(game.events==expected,"literal queue order commendation then psi then chosen save");check(!normal.agentDispatch(request)&&soldier.diary.service==1,"no replay or duplicate service");normal.release();game.save.bases.clear();}
 Options::autosave=false;
 }
 check(!SDL_GetVideoSurface(),"no video");std::cout<<"PASS "<<checks<<" monthly report constructor/provider/handler checks; financial and queue seams\n";
}
