// SPDX-License-Identifier: GPL-3.0-or-later
// Actual constructor/provider/callback bodies; explicit Game/Mod/Craft seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
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
    const void *top = nullptr; bool clear = true; int pops = 0;
    Screen *getScreen(){return &screen;} Mod *getMod(){return &mod;}
    Language *getLanguage(){return language;}
    bool isState(const void *state) const{return top == state;}
    bool agentOverlayClear(const AgentUiState&) const{return clear;}
    void popState(){++pops;}
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
struct Craft {
    std::string name; int returns = 0;
    std::string getName(Language*) const{return name;}
    void returnToBase(){++returns;}
};
struct DogfightErrorState : State {
    Craft *_craft;
    TextButton *_btnIntercept,*_btnBase;
    Window *_window;
    Text *_txtCraft,*_txtMessage;
    bool _agentDestinationPaintFull = false;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    DogfightErrorState(Craft*,const std::string&);
    ~DogfightErrorState();
    void btnInterceptClick(Action*); void btnBaseClick(Action*);
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> agentPaintRevisions() const;
    void blit() override;
    bool agentUiState(AgentUiState&) const override;
    bool agentDispatch(const AgentRequest&) override;
    void finishPopup(){_window->think();for(int i=0;i<30;++i)_window->popup();}
    void release(){for(auto *surface:_surfaces)delete surface;_surfaces.clear();}
};
#include "DogfightError.generated.inc"
}
static unsigned checks = 0;
static void check(bool ok,const char *message){
    ++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}
}
int main(int argc,char **argv){
    check(argc==2,"source argument");
    check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");
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
    for(const std::string action:{"continue_pursuit","return_interceptor_to_base"}){
        check(parseAgentRequest(prefix+action,request),"empty argument protocol accepted");
        for(const std::string extra:{" extra"," r0"," true"," 0","\n"})
            check(!parseAgentRequest(prefix+action+extra,request),"nonempty argument rejected");
    }
    for(const std::string gameName:{"xcom1","xcom2"}){
        Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+gameName+"/Language/en-US.yml");
        Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;
        game.language=&language;game.big=&big;game.small=&small;game.palette=palette;
        Fixture::currentGame=&game;
        for(const std::string message:{"UNABLE TO ENGAGE ENEMY AT THIS DEPTH","UNABLE TO ENGAGE AIRBORNE"}){
            Fixture::Craft craft;craft.name=gameName=="xcom1"?"INTERCEPTOR-1":"BARRACUDA-1";
            Fixture::DogfightErrorState state(&craft,message);game.top=&state;
            AgentUiState ui;
            check(!state.agentUiState(ui),"constructor prepaint denied");
            state.blit();check(!state.agentUiState(ui),"popup not expanded denied");
            state.finishPopup();state.blit();
            check(state.agentUiState(ui)&&ui.screen=="dogfight_error","actual constructor popup admitted");
            auto visible=YAML::Load(ui.visible);
            check(visible.size()==3&&visible["craft_name"].as<std::string>()==craft.name&&visible["message"].as<std::string>()==message,"only current UI captions projected");
            check(ui.actions.size()==2&&ui.offers("continue_pursuit")&&ui.offers("return_interceptor_to_base"),"two explicit choices");
            check(visible["buttons"][0]["id"].as<std::string>()=="continue"&&visible["buttons"][1]["id"].as<std::string>()=="return","ordinary button order");
            request.action="unknown";check(!state.agentDispatch(request)&&game.pops==0&&craft.returns==0,"unknown action no mutation");
            for(Surface *widget:std::vector<Surface *>{state._txtCraft,state._txtMessage,state._btnIntercept,state._btnBase}){
                int x=widget->getX();widget->setX(x+1);widget->setX(x);
                check(!state.agentUiState(ui),"geometry ABA without repaint denied");
                request.action="return_interceptor_to_base";check(!state.agentDispatch(request)&&craft.returns==0,"stale dispatch no mutation");
                state.blit();check(state.agentUiState(ui),"geometry repaint restores");
                widget->setVisible(false);widget->setVisible(true);
                check(!state.agentUiState(ui),"visibility ABA stale");state.blit();
            }
            state._txtMessage->setText("Changed caption");check(!state.agentUiState(ui),"text mutation stale");state.blit();check(state.agentUiState(ui),"new caption repaint");
            state._txtMessage->setVisible(false);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].IsNull(),"hidden message is null");
            state._txtMessage->setVisible(true);state._txtMessage->setText(message);state.blit();
            state._btnBase->setVisible(false);state.blit();request.action="return_interceptor_to_base";
            check(!state.agentDispatch(request)&&craft.returns==0,"hidden return action denied");state._btnBase->setVisible(true);state.blit();
            state._modal=state._btnBase;check(!state.agentUiState(ui),"local modal denied");state._modal=nullptr;
            game.top=nullptr;check(!state.agentUiState(ui),"wrong top denied");game.top=&state;
            game.clear=false;check(!state.agentUiState(ui),"external overlay denied");game.clear=true;
            SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clipped paint denied");
            SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restore needs repaint");state.blit();
            state.paintHook=[&](){SDL_SetClipRect(canvas.getSurface(),&clip);};state.blit();check(!state.agentUiState(ui),"clip during paint denied");
            state.paintHook=nullptr;SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();
            Surface cover(10,10,state._txtMessage->getX(),state._txtMessage->getY());cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].IsNull(),"occluded message withheld");state._surfaces.pop_back();state.blit();
            check(state.agentUiState(ui),"authority baseline");AgentFrameCommand frame;request.action="return_interceptor_to_base";
            check(frame.stage(request,ui,&state,1,1,100,100),"stage public choice");Fixture::Craft replacement;auto *old=state._craft;state._craft=&replacement;
            AgentUiState changed;check(state.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&state,changed),"changed craft identity rejects old authority");state._craft=old;
            state._txtMessage->initText(&big,&overlap,&language);state._txtMessage->setSmall();
            const std::string overlapping="OVERLAP\nOVERLAP";
            state._txtMessage->setText(overlapping);state.blit();
            check(overlap.getHeight()+overlap.getSpacing()<overlap.getHeight()&&state._txtMessage->getTextWidth()>0,"synthetic overlap retains valid horizontal layout");
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==overlapping,"overlapping glyph lines do not gate logical UI text");
            state._txtMessage->initText(&big,&small,&language);state._txtMessage->setSmall();state._txtMessage->setText(message);state.blit();
            state._txtMessage->setText(std::string(500,'W'));state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].IsNull(),"layout overflow message not exported");
            state._txtMessage->setText(message);state.blit();
            const std::string originalCaption=state._btnBase->getText();
            state._btnBase->setText(std::string(500,'W'));state.blit();request.action="return_interceptor_to_base";
            check(state.agentUiState(ui)&&!ui.offers(request.action)&&!state.agentDispatch(request),"layout overflow button omitted without action");
            state._btnBase->setText(originalCaption);state.blit();
            request.action="continue_pursuit";check(state.agentDispatch(request)&&game.pops==1&&craft.returns==0,"continue ordinary handler only pops");
            game.pops=0;request.action="return_interceptor_to_base";check(state.agentDispatch(request)&&game.pops==1&&craft.returns==1,"return ordinary handler changes destination and pops");
            game.pops=0;state.release();
        }
    }
    check(!SDL_GetVideoSurface(),"no game video opened");
    std::cout<<"PASS "<<checks<<" dogfight-error constructor/provider/handler checks; explicit domain seams\n";
}
