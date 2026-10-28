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
#include "../../src/Observer/AgentTargetLifetime.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <sstream>
#include "../../src/Engine/Unicode.h"
#include <iostream>
#include <cstdlib>
#include <stdexcept>
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
struct SavedBattleGame { std::string mission; void setMissionType(const std::string &s){mission=s;} };
struct SavedGame { SavedBattleGame *battle=nullptr; void setBattleGame(SavedBattleGame *b){battle=b;} };

struct Screen { Surface *surface; Surface *getSurface(){return surface;} };
struct Mod { Surface *background; Surface *getSurface(const std::string&){return background;} };
struct Game {
    Screen screen; Mod mod; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0, pushes=0; SavedGame saved;
    SavedGame *getSavedGame(){return &saved;}
    template<class T> void pushState(T *state){++pushes; top=state;}
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
struct Target{AgentTargetLifetime lifetime;std::weak_ptr<const void>agentLifetime()const{return lifetime.token();}virtual~Target(){}std::string name;std::string getName(Language*)const{return name;}};struct Base:Target{};struct Texture{};
struct Craft{std::string name;Target*destination=nullptr;Base base;int returns=0;std::string getName(Language*)const{return name;}Target*getDestination(){return destination;}Base*getBase(){return &base;}void returnToBase(){++returns;destination=&base;}};
struct Ufo:Target { enum{CRASHED=1}; int getStatus(){return CRASHED;} std::string getAlienRace(){return "PRIVATE";} };
struct Deployment { std::string getType(){return "PRIVATE";} };
struct MissionSite:Target { Deployment d; Deployment*getDeployment(){return &d;} std::string getAlienRace(){return "PRIVATE";} };
struct AlienBase:Target { Deployment d; Deployment*getDeployment(){return &d;} std::string getAlienRace(){return "PRIVATE";} };
using Exception=std::runtime_error;
static int generated=0; static bool generationThrows=false;
struct BattlescapeGenerator {
    BattlescapeGenerator(Game*){} void setWorldTexture(Texture*){} void setWorldShade(int){} void setCraft(Craft*){}
    void setUfo(Ufo*){} void setAlienRace(const std::string&){} void setMissionSite(MissionSite*){} void setAlienBase(AlienBase*){}
    void run(){++generated;if(generationThrows)throw Exception("synthetic_generation_failure");}
};
struct BriefingState { Craft *craft; explicit BriefingState(Craft *c):craft(c){} };
struct ConfirmLandingState : State {
    Craft*_craft;Texture*_texture;int _shade;
    TextButton *_btnNo,*_btnYes;
    Window *_window;
    Text *_txtMessage,*_txtBegin;
    Target *_agentLandingDestination = nullptr;
    std::weak_ptr<const void> _agentLandingLifetime;
    bool _agentDestinationPaintFull = false;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    ConfirmLandingState(Craft*,Texture*,int);
    ~ConfirmLandingState();
    void btnNoClick(Action*);void btnYesClick(Action*);void init()override;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> agentPaintRevisions() const;
    void blit() override;
    bool agentUiState(AgentUiState&) const override;
    bool agentDispatch(const AgentRequest&) override;
    void finishPopup(){_window->think();for(int i=0;i<30;++i)_window->popup();}
    void release(){for(auto *surface:_surfaces)delete surface;_surfaces.clear();}
};
#include "ConfirmLanding.generated.inc"
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
    for(const std::string action:{"accept_landing_begin_mission","decline_landing_return_to_base"}){
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
            Fixture::Craft craft;craft.name=gameName=="xcom1"?"SKYRANGER-1":"TRITON-1";Fixture::Target destination;destination.name="SITE-1";craft.destination=&destination;Fixture::Texture texture;
            Fixture::ConfirmLandingState state(&craft,&texture,0);game.top=&state;
            const std::string message=std::string(language.getString("STR_CRAFT_READY_TO_LAND_NEAR_DESTINATION").arg(craft.name).arg(destination.name));
            state.init();check(game.pops==0,"ordinary destination init retains popup");
            AgentUiState ui;
            check(!state.agentUiState(ui),"constructor prepaint denied");
            state.blit();check(!state.agentUiState(ui),"popup not expanded denied");
            state.finishPopup();state.blit();
            check(state.agentUiState(ui)&&ui.screen=="landing_confirmation","actual constructor popup admitted");
            auto visible=YAML::Load(ui.visible);
            check(visible.size()==3&&visible["message"].as<std::string>()==message,"only current UI captions projected");
            check(ui.actions.size()==2&&ui.offers("accept_landing_begin_mission")&&ui.offers("decline_landing_return_to_base"),"two explicit choices");
            check(visible["buttons"][0]["id"].as<std::string>()=="yes"&&visible["buttons"][1]["id"].as<std::string>()=="no"&&visible["buttons"].size()==2,"ordinary button order");
            craft.name="NOT DISPLAYED";destination.name="NOT DISPLAYED TARGET";check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==message,"projection current widget not changed craft or target");
            check(YAML::Load(ui.visible)["prompt"].as<std::string>()==state._txtBegin->getText(),"prompt only active UI text");
            for(const std::string forbidden:{"confirm_landing","begin_mission","accept_landing"}){request.action=forbidden;check(!state.agentDispatch(request)&&game.pops==0&&craft.returns==0,"Yes and mission generation never dispatched");}
            state._btnYes->setVisible(false);state.blit();check(state.agentUiState(ui)&&ui.offers("decline_landing_return_to_base"),"hidden Yes does not grant or remove No");state._btnYes->setVisible(true);state.blit();
            request.action="acknowledge_craft_error";check(!state.agentDispatch(request)&&game.pops==0,"neighbour action rejected");
            request.action="unknown";check(!state.agentDispatch(request)&&game.pops==0,"unknown action no mutation");
            const std::string prompt=state._txtBegin->getText();
            for(const std::string text:{std::string(500,'W'),std::string("MULTI\nLINE"),std::string("\xF0\x9F\x99\x82")}){state._txtBegin->setText(text);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["prompt"].as<std::string>()==text,"logical long multiline missingglyph prompt exact");}state._txtBegin->setText(prompt);state.blit();
            game.cursorVisible=true;game.cursor={26,41,8,8};
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==message&&ui.actions.size()==2,"pointer over caption does not gate logical text or choices");
            game.cursor={137,151,8,8};request.action="decline_landing_return_to_base";
            check((!state.agentUiState(ui)||!ui.offers(request.action))&&!state.agentDispatch(request)&&game.pops==0,"pointer on sole control refuses dispatch");game.cursorVisible=false;
            Surface otherCanvas(320,200);otherCanvas.setPalette(palette);game.screen.surface=&otherCanvas;
            check(!state.agentUiState(ui),"different destination without repaint rejected");game.screen.surface=&canvas;
            SDL_FillRect(state._txtMessage->getSurface(),nullptr,0);
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==message,"erased glyph pixels do not gate active logical message");
            state._txtMessage->setText("\xF0\x9F\x99\x82");state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()=="\xF0\x9F\x99\x82","missing glyph not admission");state._txtMessage->setText(message);state.blit();
            for(Surface *widget:std::vector<Surface *>{state._txtMessage,state._txtBegin,state._btnNo}){
                int x=widget->getX();widget->setX(x+1);widget->setX(x);
                check(!state.agentUiState(ui),"geometry ABA without repaint denied");
                request.action="decline_landing_return_to_base";check(!state.agentDispatch(request),"stale dispatch no mutation");
                state.blit();check(state.agentUiState(ui),"geometry repaint restores");
                widget->setVisible(false);widget->setVisible(true);
                check(!state.agentUiState(ui),"visibility ABA stale");state.blit();
            }
            state._txtMessage->setText("Changed caption");check(!state.agentUiState(ui),"text mutation stale");state.blit();check(state.agentUiState(ui),"new caption repaint");
            state._txtMessage->setVisible(false);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].IsNull(),"hidden message is null");
            state._txtMessage->setVisible(true);state._txtMessage->setText(message);state.blit();
            state._btnNo->setVisible(false);state.blit();request.action="decline_landing_return_to_base";
            check(!state.agentDispatch(request),"hidden acknowledgement denied");state._btnNo->setVisible(true);state.blit();
            state._modal=state._btnNo;check(!state.agentUiState(ui),"local modal denied");state._modal=nullptr;
            game.top=nullptr;check(!state.agentUiState(ui),"wrong top denied");game.top=&state;
            game.clear=false;check(!state.agentUiState(ui),"external overlay denied");game.clear=true;
            SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clipped paint denied");
            SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restore needs repaint");state.blit();
            state.paintHook=[&](){SDL_SetClipRect(canvas.getSurface(),&clip);};state.blit();check(!state.agentUiState(ui),"clip during paint denied");
            state.paintHook=nullptr;SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();
            Surface cover(10,10,state._txtMessage->getX(),state._txtMessage->getY());cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].IsNull(),"occluded message withheld");state._surfaces.pop_back();state.blit();
            check(state.agentUiState(ui),"authority baseline");AgentFrameCommand frame;request.action="decline_landing_return_to_base";
            check(frame.stage(request,ui,&state,1,1,100,100),"stage public choice");AgentUiState changed;
            check(ui.actor==&craft,"actor bound to craft private identity");Fixture::Craft replacement;auto*old=state._craft;state._craft=&replacement;
            check(state.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&state,changed),"changed craft invalidates authority");state._craft=old;
            state._txtMessage->initText(&big,&overlap,&language);state._txtMessage->setSmall();
            const std::string overlapping="OVERLAP\nOVERLAP";
            state._txtMessage->setText(overlapping);state.blit();
            check(overlap.getHeight()+overlap.getSpacing()<overlap.getHeight()&&state._txtMessage->getTextWidth()>0,"synthetic overlap retains valid horizontal layout");
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==overlapping,"overlapping glyph lines do not gate logical UI text");
            state._txtMessage->initText(&big,&small,&language);state._txtMessage->setSmall();state._txtMessage->setText(message);state.blit();
            state._txtMessage->setText(std::string(500,'W'));state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==std::string(500,'W'),"long logical message remains exported without layout gate");
            state._txtMessage->setText(message);state.blit();
            const std::string originalCaption=state._btnNo->getText();
            state._btnNo->setText(std::string(500,'W'));state.blit();request.action="decline_landing_return_to_base";
            check(state.agentUiState(ui)&&ui.offers(request.action),"long logical button label not a readability gate");
            state._btnNo->setText(originalCaption);state.blit();
            request.action="decline_landing_return_to_base";
            check(state.agentDispatch(request)&&game.pops==1&&craft.returns==1&&craft.destination==&craft.base,"No native returnToBase exactly once then pop");
            check(!state.agentDispatch(request)&&game.pops==1&&craft.returns==1,"no replay after popup closed");
            game.pops=0;state.release();
            Fixture::ConfirmLandingState ownBase(&craft,&texture,0);game.top=&ownBase;ownBase.init();check(game.pops==1&&craft.returns==1,"own-base init removes popup without second return request");check(!ownBase.agentDispatch(request)&&craft.returns==1,"removed own-base popup cannot dispatch");ownBase.release();game.pops=0;

        }
        for (int mode=0;mode<3;++mode) {
            Fixture::Craft craft; craft.name="CRAFT"; Fixture::Ufo ufo; Fixture::Target invalid;
            craft.destination=mode==2?static_cast<Fixture::Target*>(&invalid):static_cast<Fixture::Target*>(&ufo);
            Fixture::Texture texture; Fixture::ConfirmLandingState state(&craft,&texture,0); game.top=&state;
            game.pops=0;game.pushes=0;Fixture::generated=0;Fixture::generationThrows=mode==1;
            state.finishPopup();state.blit(); request.action="accept_landing_begin_mission";
            AgentUiState ui;check(state.agentUiState(ui)&&ui.offers(request.action),"Yes admitted without hidden eligibility lookup");
            state._btnYes->setVisible(false);state.blit();check(!state.agentDispatch(request)&&game.pops==0,"hidden Yes cannot dispatch");
            state._btnYes->setVisible(true);state.blit();
            auto *selected=craft.destination; Fixture::Target replacement;
            for(auto *changed:std::vector<Fixture::Target*>{&replacement,nullptr}){
                craft.destination=changed;
                check(state.agentUiState(ui)&&!ui.offers(request.action)&&ui.offers("decline_landing_return_to_base"),"retarget invalidates Yes only, no hidden eligibility");
                check(!state.agentDispatch(request)&&game.pops==0&&!game.saved.battle,"retargeted Yes no mutation");
            }
            craft.destination=selected;
            game.cursorVisible=true;game.cursor={41,151,8,8};check(!state.agentDispatch(request)&&game.pops==0,"covered Yes no mutation");game.cursorVisible=false;
            state._btnYes->setX(41);state._btnYes->setX(40);check(!state.agentDispatch(request)&&game.pops==0,"stale Yes geometry denied");state.blit();
            bool threw=false;try{check(state.agentDispatch(request),"Yes native handler dispatched");}catch(const std::runtime_error&){threw=true;}
            check(game.pops==1&&game.saved.battle&&craft.returns==0,"Yes mutated before possible exception, never returnToBase");
            check(threw==(mode!=0),"native generator or invalid target failure remains exception");
            check(Fixture::generated==(mode==2?0:1),"generator invocation exact");
            check(game.pushes==(mode==0?1:0),"Briefing only after successful generator seam");
            check(!state.agentDispatch(request)&&game.pops==1,"closed prompt not replayed after success or mutation failure");
            if(mode==0){delete static_cast<const Fixture::BriefingState*>(game.top);}
            game.top=nullptr;delete game.saved.battle;game.saved.battle=nullptr;state.release();
        }
        // Current displayed destination authority must survive neither ABA nor assignment.
        {
            alignas(Fixture::Ufo) unsigned char storage[sizeof(Fixture::Ufo)];
            auto *oldTarget=new(storage)Fixture::Ufo;oldTarget->name="SITE";
            Fixture::Craft craft;craft.name="CRAFT";craft.destination=oldTarget;Fixture::Texture texture;
            Fixture::ConfirmLandingState state(&craft,&texture,0);game.top=&state;game.pops=0;game.pushes=0;Fixture::generated=0;
            state.finishPopup();state.blit();request.action="accept_landing_begin_mission";
            AgentUiState prior,current;check(state.agentUiState(prior)&&prior.offers(request.action)&&prior.targetLifetimes.size()==1,"live landing Yes binds weak owner");
            {Fixture::Target unrelated;unrelated.name="HIDDEN";}
            check(state.agentUiState(current)&&current.payload()==prior.payload()&&current.sameBinding(prior),"unrelated target destruction leaves UI and grant unchanged");
            oldTarget->~Ufo();auto *replacement=new(storage)Fixture::Ufo;replacement->name="SITE";
            check(state.agentUiState(current)&&!current.offers(request.action)&&current.offers("decline_landing_return_to_base")&&current.targetLifetimes.empty(),"same-address ABA withholds Yes only");
            check(!state.agentDispatch(request)&&game.pops==0&&!game.saved.battle&&Fixture::generated==0&&craft.returns==0,"ABA rejected before native mutation");
            Fixture::ConfirmLandingState fresh(&craft,&texture,0);game.top=&fresh;fresh.finishPopup();fresh.blit();
            check(fresh.agentUiState(current)&&current.offers(request.action)&&current.payload()==prior.payload()&&!current.sameBinding(prior),"new popup on replacement gets distinct authority despite identical visible caption/address");
            *replacement=Fixture::Ufo();
            check(fresh.agentUiState(current)&&!current.offers(request.action)&&current.offers("decline_landing_return_to_base")&&!fresh.agentDispatch(request)&&game.pops==0,"assignment invalidates Yes without changing No");
            state.release();fresh.release();game.top=nullptr;replacement->~Ufo();craft.destination=nullptr;
        }
    }
    check(!SDL_GetVideoSurface(),"no game video opened");
    std::cout<<"PASS "<<checks<<" confirm-landing constructor/provider/handler checks; explicit domain seams\n";
}
