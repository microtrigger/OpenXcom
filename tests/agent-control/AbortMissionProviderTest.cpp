// SPDX-License-Identifier: GPL-3.0-or-later
// Actual constructor/provider/callback bodies; explicit Game/Mod/battle seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Logger.h"
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
enum { ESCAPE_EXIT=1, ESCAPE_EITHER=2, MSC_ADDCRAFT=1, O_FLOOR=0, END_POINT=2, START_POINT=1, FACTION_PLAYER=0, STATUS_DEAD=1, STATUS_IGNORE_ME=2 };
struct MapScript { int getType() const{return MSC_ADDCRAFT;} };
struct AlienDeployment { bool exit=false; std::string getNextStage() const{return exit?"NEXT":"";} int getEscapeType() const{return 0;} std::string getScript() const{return "MAP";} };
struct Mod { Surface *background; AlienDeployment deployment; std::vector<MapScript*> scripts; Surface *getSurface(const std::string&){return background;} AlienDeployment *getDeployment(const std::string&){return &deployment;} const std::vector<MapScript*> *getMapScript(const std::string&){return &scripts;} };
struct Game {
    Screen screen; Mod mod; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0;
    Screen *getScreen(){return &screen;} Mod *getMod(){return &mod;}
    Language *getLanguage(){return language;}
    bool isState(const void *state) const{return top == state;}
    bool cursorVisible=false; SDL_Rect cursor={0,0,0,0};
    bool agentOverlayClear(const AgentUiState &ui) const{
        if(!clear)return false;
        for(const auto &r:ui.regions)if(cursorVisible && r.x<cursor.x+cursor.w && r.x+r.width>cursor.x && r.y<cursor.y+cursor.h && r.y+r.height>cursor.y)return false;
        return true;
    }
    void popState(){++pops;top=nullptr;}
    bool agentNonCursorOverlayClear(const AgentUiState&) const{return clear;}
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
    LocalizedText tr(const std::string &id,unsigned n) const{return _game->language->getString(id,n);}
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

struct MapData {int special=0; int getSpecialType()const{return special;}};
struct Tile {MapData floor; MapData *getMapData(int){return &floor;}};
struct BattleUnit {int faction=0,status=0,position=0; int getOriginalFaction()const{return faction;} int getStatus()const{return status;} int getPosition()const{return position;}};
struct SavedBattleGame {
    std::string mission="MISSION"; bool aborted=false; int abortCalls=0;
    std::vector<BattleUnit*> units; std::vector<Tile*> tiles;
    void setPaletteByDepth(State*){} std::string getMissionType()const{return mission;}
    int getMapSizeXYZ()const{return tiles.size();} Tile **getTiles(){return tiles.data();}
    std::vector<BattleUnit*> *getUnits(){return &units;}
    Tile *getTile(int i){return i>=0&&i<int(tiles.size())?tiles[i]:nullptr;}
    void setAborted(bool v){aborted=v;++abortCalls;}
};
struct BattlescapeState {int finishes=0,exit=-1;bool abort=false; void finishBattle(bool a,int e){++finishes;abort=a;exit=e;}};
struct AbortMissionState : State {
    Window *_window; Text *_txtInEntrance,*_txtInExit,*_txtOutside,*_txtAbort;
    TextButton *_btnOk,*_btnCancel; SavedBattleGame *_battleGame; BattlescapeState *_state;
    int _inEntrance,_inExit,_outside;
    bool _agentDestinationPaintFull=false;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    AbortMissionState(SavedBattleGame*,BattlescapeState*); ~AbortMissionState();
    void btnOkClick(Action*);void btnCancelClick(Action*);
    bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;
    void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
    void finishPopup(){_window->think();for(int i=0;i<30;++i)_window->popup();}
    void release(){for(auto *s:_surfaces)delete s;_surfaces.clear();}
};
#include "AbortMission.generated.inc"
}
static unsigned checks=0;
static void check(bool ok,const char *message){++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}}
int main(int argc,char **argv){
    check(argc==3,"data/log arguments");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");FileMap::load("common","./",false);
    Options::baseXResolution=320;Options::baseYResolution=200;
    Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
    Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
    for(const std::string gameName:{"xcom1","xcom2"})for(int scenario=0;scenario<4;++scenario){
        Language language;language.loadFile(std::string(argv[1])+"/standard/"+gameName+"/Language/en-US.yml");
        Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
        Fixture::MapScript craft; if(scenario==0)game.mod.scripts.push_back(&craft);game.mod.deployment.exit=scenario==2;
        Fixture::Tile entrance,exit,outside;entrance.floor.special=Fixture::START_POINT;exit.floor.special=Fixture::END_POINT;
        Fixture::BattleUnit a,b,c;b.position=1;c.position=2;
        Fixture::SavedBattleGame battle;battle.units={&a,&b,&c};battle.tiles={&entrance,&exit,&outside};if(scenario==3)battle.mission="STR_BASE_DEFENSE";
        Fixture::BattlescapeState owner;Fixture::AbortMissionState state(&battle,&owner);game.top=&state;AgentUiState ui;AgentRequest request;
        check(!state.agentUiState(ui),"prepaint denied");state.blit();check(state.agentUiState(ui)&&ui.screen=="abort_mission","painted dialog admitted");
        auto v=YAML::Load(ui.visible);check(v.size()==5&&ui.actions.size()==2,"exact caption shape and two buttons");check(v["prompt"].as<std::string>()==state._txtAbort->getText(),"prompt actual caption");
        check(v["exit"].IsNull()==(scenario!=2),"exit only when shown");check(v["entrance"].IsNull()==(scenario==3)&&v["outside"].IsNull()==(scenario==3),"base defense counts hidden");
        if(scenario!=3)check(v["entrance"].as<std::string>()==state._txtInEntrance->getText(),"craft or entrance actual caption");
        const auto prior=ui.visible;state._outside=987;state._inEntrance=654;check(state.agentUiState(ui)&&ui.visible==prior,"private totals never reprojected");
        request.action="unknown";check(!state.agentDispatch(request)&&game.pops==0,"unknown action inert");
        state._txtAbort->setText("CHANGED");check(!state.agentUiState(ui),"unpainted caption stale");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["prompt"].as<std::string>()=="CHANGED","changed active caption");
        for(Surface *w:std::vector<Surface*>{state._txtAbort,state._txtInEntrance,state._txtInExit,state._txtOutside,state._btnOk,state._btnCancel}){int x=w->getX();w->setX(x+1);w->setX(x);check(!state.agentUiState(ui),"geometry ABA stale");state.blit();w->setVisible(false);w->setVisible(true);check(!state.agentUiState(ui),"visibility ABA stale");state.blit();}
        state._btnOk->setVisible(false);state.blit();request.action="confirm_abort_mission";check(state.agentUiState(ui)&&!ui.offers(request.action)&&!state.agentDispatch(request),"hidden OK cannot abort");state._btnOk->setVisible(true);state.blit();
        game.cursorVisible=true;game.cursor={20,112,8,8};check(state.agentUiState(ui)&&!ui.offers(request.action)&&!state.agentDispatch(request),"cursor blocks control");game.cursor={4,82,8,8};check(state.agentUiState(ui)&&ui.offers(request.action),"cursor over caption does not gate");game.cursorVisible=false;
        game.top=nullptr;check(!state.agentUiState(ui),"covered state rejected");game.top=&state;state._modal=state._btnOk;check(!state.agentUiState(ui),"local modal rejected");state._modal=nullptr;game.clear=false;check(!state.agentUiState(ui),"overlay rejected");game.clear=true;
        SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clipped destination denied");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"restored clip needs repaint");state.blit();
        Surface cover(10,10,state._txtAbort->getX(),state._txtAbort->getY());cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["prompt"].IsNull(),"occluded caption omitted");state._surfaces.pop_back();state.blit();
        AgentFrameCommand frame;check(state.agentUiState(ui)&&frame.stage(request,ui,&state,1,1,100,100),"stage shown choice");Fixture::BattlescapeState replacement;state._state=&replacement;AgentUiState changed;check(state.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&state,changed),"callback replacement stale");state._state=&owner;
        request.action="cancel_abort_mission";check(state.agentDispatch(request)&&game.pops==1&&!battle.aborted&&battle.abortCalls==0&&owner.finishes==0,"Cancel pops only");check(!state.agentDispatch(request)&&game.pops==1,"repeat after Cancel denied");
        game.top=&state;state.blit();request.action="confirm_abort_mission";check(state.agentDispatch(request)&&game.pops==2&&battle.aborted&&battle.abortCalls==1&&owner.finishes==1&&owner.abort&&owner.exit==1,"OK ordinary aborted and finish callback with exit count");check(!state.agentDispatch(request)&&owner.finishes==1,"repeat after OK denied");state.release();
    }
    check(!SDL_GetVideoSurface(),"no game video opened");std::cout<<"PASS "<<checks<<" abort constructor/provider/handler checks; explicit domain seams\n";
}
