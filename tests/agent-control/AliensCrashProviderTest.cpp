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
struct Mod { Surface *background; Surface *getSurface(const std::string&){return background;} };
struct Game {
    Screen screen; Mod mod; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0, pushes = 0;
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
    template<class T> void pushState(T *p){++pushes;top=p;}
    bool agentNonCursorOverlayClear(const AgentUiState&) const{return clear;}
};
static Game *currentGame;
struct State : OpenXcom::State {
    Game *_game = currentGame;
    std::function<void()> paintHook;
    using OpenXcom::State::_surfaces;
    using OpenXcom::State::_modal;
    void setInterface(const std::string&){}
    void setPalette(const std::string&){}
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
struct DebriefingState {};
struct AliensCrashState : State {
 TextButton *_btnOk;Window *_window;Text *_txtTitle;
 bool _agentPainted=false;std::vector<std::uint64_t> _agentPaintRevisions;
 AliensCrashState();~AliensCrashState();void btnOkClick(Action*);
 std::vector<std::uint64_t> agentPaintRevisions() const;
 void blit() override;bool agentUiState(AgentUiState&) const override;
 bool agentDispatch(const AgentRequest&) override;
 void release(){for(auto *surface:_surfaces)delete surface;_surfaces.clear();}
};
#include "Crash.generated.inc"
}
static unsigned checks=0;static void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==2,"source");check(SDL_Init(0)==0,"SDL");FileMap::load("common","./",false);
 Options::baseXResolution=320;Options::baseYResolution=200;Font big,small;
 for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string gameName:{"xcom1","xcom2"}){
  Language language;language.loadFile(std::string(argv[1])+"/standard/"+gameName+"/Language/en-US.yml");
  Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
  Fixture::AliensCrashState state;game.top=&state;AgentUiState ui;AgentRequest request;request.action="acknowledge_crash_recovery";
  check(!state.agentUiState(ui),"prepaint deny");state.blit();check(state.agentUiState(ui),"painted current popup");
  check(ui.screen=="crash_recovery"&&ui.actions.size()==1&&ui.offers(request.action),"exact state/action");
  check(YAML::Load(ui.visible).size()==1&&YAML::Load(ui.visible)["message"].as<std::string>()==state._txtTitle->getText(),"only shown caption");
  check(state._txtTitle->getText().find(gameName=="xcom2"?"subcrash":"in crash")!=std::string::npos,"actual UFO/TFTD localized caption");
  auto first=ui;state._txtTitle->setText("VISIBLE UPDATED CAPTION");check(!state.agentUiState(ui),"text mutation stale");state.blit();check(state.agentUiState(ui)&&!first.sameBinding(ui),"fresh repaint binding differs");
  state._btnOk->setVisible(false);state.blit();check(!state.agentUiState(ui),"hidden OK deny");state._btnOk->setVisible(true);check(!state.agentUiState(ui),"unhide stale");state.blit();
  state._modal=state._btnOk;check(!state.agentDispatch(request),"modal deny");state._modal=nullptr;
  game.top=nullptr;check(!state.agentDispatch(request),"wrong top deny");game.top=&state;
  game.clear=false;check(!state.agentDispatch(request),"real overlay deny");game.clear=true;
  SDL_Rect clip={0,0,200,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clipped frame restore deny");state.blit();
  Surface cover(120,18,100,154);state._surfaces.push_back(&cover);check(!state.agentUiState(ui),"later OK occlusion deny");state._surfaces.pop_back();
  request.action="unknown";check(!state.agentDispatch(request)&&!game.pops,"wrong command deny");request.action="acknowledge_crash_recovery";
  check(state.agentDispatch(request)&&game.pops==1&&game.pushes==1,"one ordinary pop and Debriefing push");check(!state.agentDispatch(request)&&game.pops==1,"repeat after transition denied");
  delete static_cast<const Fixture::DebriefingState*>(game.top);state.release();
 }
 check(Observer::agentPaintHealthy()&&!SDL_WasInit(SDL_INIT_VIDEO),"offscreen only");std::cout<<"AliensCrashProviderTest PASS "<<checks<<" checks\n";
}
