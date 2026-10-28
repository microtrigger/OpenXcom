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
}
static unsigned checks = 0;
static void check(bool ok,const char *message){
    ++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}
}
int main(int argc,char **argv){
    check(argc==2,"source argument");
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
    const std::string token(32,'1'),prefix="E "+token+" 1 "+token+" "+token+" 1 ";
    for(const std::string action:{"acknowledge_ufo_lost"}){
        check(parseAgentRequest(prefix+action,request),"empty argument protocol accepted");
        for(const std::string extra:{" extra"," r0"," true"," 0","\n"})
            check(!parseAgentRequest(prefix+action+extra,request),"nonempty argument rejected");
    }
    for(const std::string gameName:{"xcom1","xcom2"}){
        Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+gameName+"/Language/en-US.yml");
        Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;
        game.language=&language;game.big=&big;game.small=&small;game.palette=palette;
        Fixture::currentGame=&game;
        {
            const std::string id=gameName=="xcom1"?"UFO-1":"ALIEN SUB-1";const std::string message=id+"\n"+std::string(language.getString("STR_TRACKING_LOST"));
            Fixture::UfoLostState state(id);game.top=&state;
            AgentUiState ui;
            check(!state.agentUiState(ui),"constructor prepaint denied");
            state.blit();check(!state.agentUiState(ui),"popup not expanded denied");
            state.finishPopup();state.blit();
            check(state.agentUiState(ui)&&ui.screen=="ufo_lost","actual constructor popup admitted");
            auto visible=YAML::Load(ui.visible);
            check(visible.size()==2&&visible["message"].as<std::string>()==message,"only current UI captions projected");
            check(ui.actions.size()==1&&ui.offers("acknowledge_ufo_lost"),"one explicit acknowledgement");
            check(visible["buttons"][0]["id"].as<std::string>()=="ok"&&visible["buttons"].size()==1,"ordinary button order");
            state._id="UNDISPLAYED CHANGED IDENTIFIER";check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==message,"projection reads current UI text not constructor identifier model");
            request.action="acknowledge_craft_error";check(!state.agentDispatch(request)&&game.pops==0,"neighbour action rejected");
            request.action="unknown";check(!state.agentDispatch(request)&&game.pops==0,"unknown action no mutation");
            game.cursorVisible=true;game.cursor={49,73,8,8};
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==message&&ui.actions.size()==1,"pointer over caption does not gate logical text or choices");
            game.cursor={99,113,8,8};request.action="acknowledge_ufo_lost";
            check((!state.agentUiState(ui)||!ui.offers(request.action))&&!state.agentDispatch(request)&&game.pops==0,"pointer on sole control refuses dispatch");game.cursorVisible=false;
            Surface otherCanvas(320,200);otherCanvas.setPalette(palette);game.screen.surface=&otherCanvas;
            check(!state.agentUiState(ui),"different destination without repaint rejected");game.screen.surface=&canvas;
            SDL_FillRect(state._txtTitle->getSurface(),nullptr,0);
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==message,"erased glyph pixels do not gate active logical message");
            state._txtTitle->setText("\xF0\x9F\x99\x82");state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()=="\xF0\x9F\x99\x82","missing glyph not admission");state._txtTitle->setText(message);state.blit();
            for(Surface *widget:std::vector<Surface *>{state._txtTitle,state._btnOk}){
                int x=widget->getX();widget->setX(x+1);widget->setX(x);
                check(!state.agentUiState(ui),"geometry ABA without repaint denied");
                request.action="acknowledge_ufo_lost";check(!state.agentDispatch(request),"stale dispatch no mutation");
                state.blit();check(state.agentUiState(ui),"geometry repaint restores");
                widget->setVisible(false);widget->setVisible(true);
                check(!state.agentUiState(ui),"visibility ABA stale");state.blit();
            }
            state._txtTitle->setText("Changed caption");check(!state.agentUiState(ui),"text mutation stale");state.blit();check(state.agentUiState(ui),"new caption repaint");
            state._txtTitle->setVisible(false);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].IsNull(),"hidden message is null");
            state._txtTitle->setVisible(true);state._txtTitle->setText(message);state.blit();
            state._btnOk->setVisible(false);state.blit();request.action="acknowledge_ufo_lost";
            check(!state.agentDispatch(request),"hidden acknowledgement denied");state._btnOk->setVisible(true);state.blit();
            state._modal=state._btnOk;check(!state.agentUiState(ui),"local modal denied");state._modal=nullptr;
            game.top=nullptr;check(!state.agentUiState(ui),"wrong top denied");game.top=&state;
            game.clear=false;check(!state.agentUiState(ui),"external overlay denied");game.clear=true;
            SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clipped paint denied");
            SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restore needs repaint");state.blit();
            state.paintHook=[&](){SDL_SetClipRect(canvas.getSurface(),&clip);};state.blit();check(!state.agentUiState(ui),"clip during paint denied");
            state.paintHook=nullptr;SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();
            Surface cover(10,10,state._txtTitle->getX(),state._txtTitle->getY());cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].IsNull(),"occluded message withheld");state._surfaces.pop_back();state.blit();
            check(state.agentUiState(ui),"authority baseline");AgentFrameCommand frame;request.action="acknowledge_ufo_lost";
            check(frame.stage(request,ui,&state,1,1,100,100),"stage public choice");AgentUiState changed;
            check(ui.actor==&state,"actor is current popup not hidden model");
            Fixture::UfoLostState other(id);game.top=&other;other.finishPopup();other.blit();
            check(other.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&other,changed),"different popup instance rejects prior authority");other.release();game.top=&state;state.blit();
            state._txtTitle->initText(&big,&overlap,&language);state._txtTitle->setSmall();
            const std::string overlapping="OVERLAP\nOVERLAP";
            state._txtTitle->setText(overlapping);state.blit();
            check(overlap.getHeight()+overlap.getSpacing()<overlap.getHeight()&&state._txtTitle->getTextWidth()>0,"synthetic overlap retains valid horizontal layout");
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==overlapping,"overlapping glyph lines do not gate logical UI text");
            state._txtTitle->initText(&big,&small,&language);state._txtTitle->setSmall();state._txtTitle->setText(message);state.blit();
            state._txtTitle->setText(std::string(500,'W'));state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==std::string(500,'W'),"long logical message remains exported without layout gate");
            state._txtTitle->setText(message);state.blit();
            const std::string originalCaption=state._btnOk->getText();
            state._btnOk->setText(std::string(500,'W'));state.blit();request.action="acknowledge_ufo_lost";
            check(state.agentUiState(ui)&&ui.offers(request.action),"long logical button label not a readability gate");
            state._btnOk->setText(originalCaption);state.blit();
            request.action="acknowledge_ufo_lost";
            check(state.agentDispatch(request)&&game.pops==1,"OK only pops once");
            check(!state.agentDispatch(request)&&game.pops==1,"no replay after popup closed");
            game.pops=0;state.release();
        }
    }
    check(!SDL_GetVideoSurface(),"no game video opened");
    std::cout<<"PASS "<<checks<<" ufo-lost constructor/provider/handler checks; explicit domain seams\n";
}
