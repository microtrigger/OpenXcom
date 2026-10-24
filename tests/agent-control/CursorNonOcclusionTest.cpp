// SPDX-License-Identifier: GPL-3.0-or-later
// Literal Game helpers, AgentController::sample and UfoLost provider; explicit controller/engine seams.
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
struct State;
struct Screen { Surface *surface; bool ready=true; Surface *getSurface(){return surface;} bool agentPresentationReady(){return ready;} };
struct Mod { Surface *background; Surface *getSurface(const std::string&){return background;} };
struct Game {
    Screen screen; Mod mod; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0;
    Screen *getScreen(){return &screen;} Mod *getMod(){return &mod;}
    Language *getLanguage(){return language;}
    bool isState(const void *state) const{return top == state;}
    Surface *_cursor=nullptr,*_fpsCounter=nullptr;
    mutable int rejected=0;
    bool agentOverlayClear(const AgentUiState&) const;
    bool agentNonCursorOverlayClear(const AgentUiState&) const;
    bool legacyOverlayClear(const AgentUiState&) const;
    State*agentTopState(){return (State*)top;}
    void agentRecordAdmissionFailure(bool,bool,bool) const{++rejected;}
    void agentCursorState(AgentUiState&out)const{out.actions.emplace_back("move_cursor","Move cursor","visible_control_semantics","cursor_position");}
    void popState(){++pops;top=nullptr;}

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
struct UfoLostState : State {
    std::string _id;
    TextButton *_btnOk;
    Window *_window;
    Text *_txtTitle;
    bool _agentDestinationPaintFull = false;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    UfoLostState(const std::string&);
    ~UfoLostState();
    void btnOkClick(Action*);
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> agentPaintRevisions() const;
    void blit() override;
    bool agentUiState(AgentUiState&) const override;
    bool agentDispatch(const AgentRequest&) override;
    void finishPopup(){_window->think();for(int i=0;i<30;++i)_window->popup();}
    void release(){for(auto *surface:_surfaces)delete surface;_surfaces.clear();}
};
#include "UfoLost.generated.inc"
#include "GameOverlay.generated.inc"
static bool diagnosticProfileQualified(){return true;}
struct AgentController{Game*_game;bool focus=true;bool focused()const{return focus;}bool sample(AgentUiState&)const;};
#include "ControllerSample.generated.inc"
}
static unsigned checks = 0;
static void check(bool ok,const char *message){
    ++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}
}
int main(int argc,char **argv){
    check(argc==2,"source argument");
    check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen widgets only");
    FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
    Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
    Surface canvas(320,200),background(320,200),cursor(8,8),fps(10,10);SDL_Color palette[256];
    for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
    for(auto*p:std::vector<Surface*>{&canvas,&background,&cursor,&fps})p->setPalette(palette);background.clear(40);
    for(const std::string master:{"xcom1","xcom2"}){
        Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");
        Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;game._cursor=&cursor;game._fpsCounter=&fps;Fixture::currentGame=&game;
        fps.setVisible(false);cursor.setVisible(true);cursor.setX(10);cursor.setY(10);
        AgentUiState menu;menu.screen="main_menu";menu.regions.push_back({8,8,100,20});
        check(!game.legacyOverlayClear(menu)&&game.agentOverlayClear(menu),"same synthetic menu hitbox old cursor rejection/new nonocclusion");
        fps.setX(10);fps.setY(10);fps.setVisible(true);check(!game.agentOverlayClear(menu),"real FPS overlap remains rejection");fps.setVisible(false);
        Fixture::UfoLostState state(master=="xcom1"?"UFO-1":"ALIEN SUB-1");game.top=&state;Fixture::AgentController controller;controller._game=&game;
        AgentUiState ui;check(controller.sample(ui)&&ui.screen=="unsupported"&&!ui.offers("acknowledge_ufo_lost")&&ui.offers("move_cursor"),"actual sample clears unpainted provider; cursor-only seam remains");
        state.finishPopup();state.blit();cursor.setX(state._btnOk->getX()+1);cursor.setY(state._btnOk->getY()+1);
        check(state.agentUiState(ui)&&ui.offers("acknowledge_ufo_lost"),"native popup OK offered under cursor");
        check(controller.sample(ui)&&ui.screen=="ufo_lost"&&ui.offers("acknowledge_ufo_lost"),"actual global sample retains provider under cursor");
        const std::string baseline=ui.visible;
        cursor.setX(0);cursor.setY(0);check(controller.sample(ui)&&ui.visible==baseline,"cursor move does not change public provider projection");
        fps.setX(state._btnOk->getX()+1);fps.setY(state._btnOk->getY()+1);fps.setVisible(true);
        check(controller.sample(ui)&&!ui.offers("acknowledge_ufo_lost"),"FPS still prevents native control through sample");fps.setVisible(false);state.blit();
        state._modal=state._btnOk;check(controller.sample(ui)&&ui.screen=="unsupported"&&!ui.offers("acknowledge_ufo_lost"),"modal provider rejection retained");state._modal=nullptr;
        state._txtTitle->setText("Current changed caption");check(controller.sample(ui)&&ui.screen=="unsupported","dirty presentation rejected");state.blit();check(controller.sample(ui)&&ui.screen=="ufo_lost","fresh paint restores");
        Surface wrong(320,200);wrong.setPalette(palette);game.screen.surface=&wrong;check(controller.sample(ui)&&ui.screen=="unsupported","equal-geometry wrong target rejected");game.screen.surface=&canvas;
        SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(controller.sample(ui)&&ui.screen=="unsupported","clipped paint rejected");SDL_SetClipRect(canvas.getSurface(),nullptr);check(controller.sample(ui)&&ui.screen=="unsupported","restore without repaint rejected");state.blit();
        Surface cover(12,12,state._btnOk->getX(),state._btnOk->getY());cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();AgentRequest request;request.action="acknowledge_ufo_lost";
        check(controller.sample(ui)&&!ui.offers(request.action)&&!state.agentDispatch(request)&&game.pops==0,"real later widget prevents offered action and dispatch");state._surfaces.pop_back();state.blit();
        check(state.agentUiState(ui),"fresh frame binding");AgentFrameCommand frame;check(frame.stage(request,ui,&state,1,1,100,100),"stage existing action");
        state._btnOk->setX(state._btnOk->getX()+1);state.blit();AgentUiState moved;check(state.agentUiState(moved)&&!frame.matchesFrame(101,1,1,&state,moved),"stale geometry authority retained");
        controller.focus=false;check(!controller.sample(ui),"controller focus guard unchanged");controller.focus=true;game.screen.ready=false;check(!controller.sample(ui),"controller display readiness unchanged");game.screen.ready=true;
        cursor.setX(state._btnOk->getX()+1);cursor.setY(state._btnOk->getY()+1);check(controller.sample(ui)&&ui.offers(request.action),"current control under cursor offered after all repairs");
        check(state.agentDispatch(request)&&game.pops==1,"native OK callback pops once under cursor");check(!state.agentDispatch(request)&&game.pops==1,"inactive provider does not repeat callback");state.release();
    }
    check(!SDL_GetVideoSurface(),"no display created");std::cout<<"PASS "<<checks<<" cursor nonocclusion checks; literal sample with explicit nontransport seams\n";
}
