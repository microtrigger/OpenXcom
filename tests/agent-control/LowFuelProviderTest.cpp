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
struct Craft {
    std::string name; int returns = 0;
    std::string getName(Language*) const{return name;}
    void returnToBase(){++returns;}
};
struct GeoscapeState {
    TextButton *_btn5Secs = reinterpret_cast<TextButton*>(1);
    int speed=60,resets=0;
    void selectGeoscapeSpeed(TextButton *b){speed=b==_btn5Secs?5:-1;++resets;}
    void timerReset();
};
#include "GeoscapeTimer.generated.inc"
struct LowFuelState : State {
    GeoscapeState *_state;
    Craft *_craft;
    TextButton *_btnOk,*_btnOk5Secs;
    Window *_window;
    Text *_txtTitle,*_txtMessage;
    bool _agentDestinationPaintFull = false;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    LowFuelState(Craft*,GeoscapeState*);
    ~LowFuelState();
    void btnOkClick(Action*); void btnOk5SecsClick(Action*);
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> agentPaintRevisions() const;
    void blit() override;
    bool agentUiState(AgentUiState&) const override;
    bool agentDispatch(const AgentRequest&) override;
    void finishPopup(){_window->think();for(int i=0;i<30;++i)_window->popup();}
    void release(){for(auto *surface:_surfaces)delete surface;_surfaces.clear();}
};
#include "LowFuel.generated.inc"
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
    const std::string token(32,'1'),prefix="E "+token+" 1 "+token+" "+token+" 1 ";
    for(const std::string action:{"acknowledge_low_fuel","acknowledge_low_fuel_5_seconds"}){
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
            const std::string message=language.getString("STR_IS_LOW_ON_FUEL_RETURNING_TO_BASE");
            Fixture::Craft craft;craft.name=gameName=="xcom1"?"INTERCEPTOR-1":"BARRACUDA-1";
            Fixture::GeoscapeState geo;
            Fixture::LowFuelState state(&craft,&geo);game.top=&state;
            AgentUiState ui;
            check(!state.agentUiState(ui),"constructor prepaint denied");
            state.blit();check(!state.agentUiState(ui),"popup not expanded denied");
            state.finishPopup();state.blit();
            check(state.agentUiState(ui)&&ui.screen=="low_fuel","actual constructor popup admitted");
            auto visible=YAML::Load(ui.visible);
            check(visible.size()==3&&visible["title"].as<std::string>()==craft.name&&visible["message"].as<std::string>()==message,"only current UI captions projected");
            check(ui.actions.size()==2&&ui.offers("acknowledge_low_fuel")&&ui.offers("acknowledge_low_fuel_5_seconds"),"two explicit choices");
            check(visible["buttons"][0]["id"].as<std::string>()=="ok"&&visible["buttons"][1]["id"].as<std::string>()=="ok_5_seconds","ordinary button order");
            request.action="unknown";check(!state.agentDispatch(request)&&game.pops==0&&craft.returns==0,"unknown action no mutation");
            const std::string activeName=craft.name;craft.name="MODEL VALUE NOT DISPLAYED";
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==activeName,"provider reads active caption not craft model");craft.name=activeName;
            game.cursorVisible=true;game.cursor={21,60,8,8};
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==activeName&&ui.actions.size()==2,"pointer over caption does not gate logical text or choices");
            game.cursor={30,120,8,8};request.action="acknowledge_low_fuel";
            check(state.agentUiState(ui)&&!ui.offers(request.action)&&!state.agentDispatch(request)&&game.pops==0,"pointer on control retains action ownership guard");game.cursorVisible=false;
            Surface otherCanvas(320,200);otherCanvas.setPalette(palette);game.screen.surface=&otherCanvas;
            check(!state.agentUiState(ui),"different destination without repaint rejected");game.screen.surface=&canvas;
            state._txtTitle->setVisible(false);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].IsNull(),"hidden title not exported");
            state._txtTitle->setVisible(true);state.blit();
            SDL_FillRect(state._txtTitle->getSurface(),nullptr,0);
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==activeName,"erased glyph pixels do not gate active logical title");
            state._txtTitle->setText("\xF0\x9F\x99\x82");state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()=="\xF0\x9F\x99\x82","missing font glyph not a text gate");state._txtTitle->setText(activeName);state.blit();

            for(Surface *widget:std::vector<Surface *>{state._txtTitle,state._txtMessage,state._btnOk,state._btnOk5Secs}){
                int x=widget->getX();widget->setX(x+1);widget->setX(x);
                check(!state.agentUiState(ui),"geometry ABA without repaint denied");
                request.action="acknowledge_low_fuel_5_seconds";check(!state.agentDispatch(request)&&craft.returns==0,"stale dispatch no mutation");
                state.blit();check(state.agentUiState(ui),"geometry repaint restores");
                widget->setVisible(false);widget->setVisible(true);
                check(!state.agentUiState(ui),"visibility ABA stale");state.blit();
            }
            state._txtMessage->setText("Changed caption");check(!state.agentUiState(ui),"text mutation stale");state.blit();check(state.agentUiState(ui),"new caption repaint");
            state._txtMessage->setVisible(false);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].IsNull(),"hidden message is null");
            state._txtMessage->setVisible(true);state._txtMessage->setText(message);state.blit();
            state._btnOk5Secs->setVisible(false);state.blit();request.action="acknowledge_low_fuel_5_seconds";
            check(!state.agentDispatch(request)&&craft.returns==0,"hidden return action denied");state._btnOk5Secs->setVisible(true);state.blit();
            state._modal=state._btnOk5Secs;check(!state.agentUiState(ui),"local modal denied");state._modal=nullptr;
            game.top=nullptr;check(!state.agentUiState(ui),"wrong top denied");game.top=&state;
            game.clear=false;check(!state.agentUiState(ui),"external overlay denied");game.clear=true;
            SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clipped paint denied");
            SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restore needs repaint");state.blit();
            state.paintHook=[&](){SDL_SetClipRect(canvas.getSurface(),&clip);};state.blit();check(!state.agentUiState(ui),"clip during paint denied");
            state.paintHook=nullptr;SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();
            Surface cover(10,10,state._txtMessage->getX(),state._txtMessage->getY());cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].IsNull(),"occluded message withheld");state._surfaces.pop_back();state.blit();
            check(state.agentUiState(ui),"authority baseline");AgentFrameCommand frame;request.action="acknowledge_low_fuel_5_seconds";
            check(frame.stage(request,ui,&state,1,1,100,100),"stage public choice");Fixture::Craft replacement;auto *old=state._craft;state._craft=&replacement;
            AgentUiState changed;check(state.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&state,changed),"changed craft identity rejects old authority");state._craft=old;
            Fixture::GeoscapeState otherGeo;state._state=&otherGeo;
            check(state.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&state,changed),"callback target replacement invalidates authority");state._state=&geo;
            state._txtMessage->initText(&big,&overlap,&language);state._txtMessage->setSmall();
            const std::string overlapping="OVERLAP\nOVERLAP";
            state._txtMessage->setText(overlapping);state.blit();
            check(overlap.getHeight()+overlap.getSpacing()<overlap.getHeight()&&state._txtMessage->getTextWidth()>0,"synthetic overlap retains valid horizontal layout");
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==overlapping,"overlapping glyph lines do not gate logical UI text");
            state._txtMessage->initText(&big,&small,&language);state._txtMessage->setSmall();state._txtMessage->setText(message);state.blit();
            state._txtMessage->setText(std::string(500,'W'));state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==std::string(500,'W'),"long logical message remains exported without layout gate");
            state._txtMessage->setText(message);state.blit();
            const std::string originalCaption=state._btnOk5Secs->getText();
            state._btnOk5Secs->setText(std::string(500,'W'));state.blit();request.action="acknowledge_low_fuel_5_seconds";
            check(state.agentUiState(ui)&&ui.offers(request.action),"long logical button label not a readability gate");
            state._btnOk5Secs->setText(originalCaption);state.blit();
            request.action="acknowledge_low_fuel";
            check(state.agentDispatch(request)&&game.pops==1&&craft.returns==0&&geo.speed==60&&geo.resets==0,"OK pops only and preserves prior speed; no returnToBase");
            check(!state.agentDispatch(request)&&game.pops==1&&geo.resets==0,"repeat after pop cannot dispatch");
            game.pops=0;game.top=&state;request.action="acknowledge_low_fuel_5_seconds";
            check(state.agentDispatch(request)&&game.pops==1&&craft.returns==0&&geo.speed==5&&geo.resets==1,"OK5Secs actual timerReset selects5 then single pop; no returnToBase");
            check(!state.agentDispatch(request)&&game.pops==1&&geo.resets==1,"repeat after5Secs pop cannot dispatch");
            game.pops=0;state.release();
        }
    }
    check(!SDL_GetVideoSurface(),"no game video opened");
    std::cout<<"PASS "<<checks<<" low-fuel constructor/provider/handler checks; explicit domain seams\n";
}
