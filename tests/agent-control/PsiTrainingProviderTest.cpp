// SPDX-License-Identifier: GPL-3.0-or-later
// Actual constructor/provider/callback bodies; explicit Game/Mod/Base and capture-only Allocate push seams.
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
static bool forbidModelReads=false;
static void modelRead(){if(forbidModelReads)std::abort();}
struct Base { int assignments=0; std::string name; int labs; Base(std::string n,int l):name(n),labs(l){} int getAvailablePsiLabs(){modelRead();return labs;} std::string getName(){modelRead();return name;} };
struct SavedGame { std::vector<Base*> bases; std::vector<Base*> *getBases(){modelRead();return &bases;} };
struct AllocatePsiTrainingState { Base *base; explicit AllocatePsiTrainingState(Base *b):base(b){} };
struct Game {
    SavedGame saved; SavedGame *getSavedGame(){modelRead();return &saved;}
    Screen screen; Mod mod; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0, pushes=0; AllocatePsiTrainingState *allocation=nullptr;
    void pushState(AllocatePsiTrainingState *s){++pushes;allocation=s;top=s;}
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
struct PsiTrainingState : State {
    std::vector<Base*> _bases;
    std::vector<TextButton*> _btnBases;
    TextButton *_btnOk;
    Window *_window;
    Text *_txtTitle;
    bool _agentDestinationPaintFull = false;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    PsiTrainingState();
    void btnBaseXClick(Action*);
    std::vector<std::pair<TextButton*,Base*> > _agentConstructorBasePairs;
    bool agentBasePairsCurrent() const;
    ~PsiTrainingState();
    void btnOkClick(Action*);
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> agentPaintRevisions() const;
    void blit() override;
    bool agentUiState(AgentUiState&) const override;
    bool agentDispatch(const AgentRequest&) override;
    void finishPopup(){_window->think();for(int i=0;i<30;++i)_window->popup();}
    void release(){for(auto *surface:_surfaces)delete surface;_surfaces.clear();}
};
#include "PsiTraining.generated.inc"
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
    const std::string token(32,'1'),prefix="E "+token+" "+token+" "+token+" 1 ";
    for(const std::string action:{"close_psi_training"}){
        check(parseAgentRequest(prefix+action,request),"empty argument protocol accepted");
        for(const std::string extra:{" extra"," r0"," true"," 0","\n"})
            check(!parseAgentRequest(prefix+action+extra,request),"nonempty argument rejected");
    }
    check(parseAgentRequest(prefix+"open_psi_training_base r0",request),"base row protocol accepted");
    for(const std::string bad:{""," r0 extra"," r01"," true"})check(!parseAgentRequest(prefix+"open_psi_training_base"+bad,request),"base protocol rejects malformed arguments");
    for(const std::string gameName:{"xcom1","xcom2"}){
        Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+gameName+"/Language/en-US.yml");
        Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;
        game.language=&language;game.big=&big;game.small=&small;game.palette=palette;
        Fixture::currentGame=&game;
        for(int count : {0,1,8,10}){
            std::vector<Fixture::Base> bases;for(int i=0;i<count;++i)bases.emplace_back("BASE "+std::to_string(i),1);
            game.saved.bases.clear();for(auto &b:bases)game.saved.bases.push_back(&b);
            Fixture::PsiTrainingState state;game.top=&state;state.finishPopup();state.blit();AgentUiState ui;
            check(state.agentUiState(ui),"constructor count admitted");
            auto visible=YAML::Load(ui.visible);
            check(visible["bases"].size()==size_t(std::min(count,8)),"constructor at most eight currently displayed eligible labels");
            for(size_t i=0;i<visible["bases"].size();++i)check(visible["bases"][i].as<std::string>()=="BASE "+std::to_string(i),"constructor preserves shown label order");
            check(ui.actions.size()==size_t(count?2:1)&&ui.offers("close_psi_training")&&ui.offers("open_psi_training_base")==bool(count),"only displayed base controls offered");
            check(visible["rows"].size()==size_t(std::min(count,8)),"fresh visible choice handles bounded eight");state.release();
        }
        {
            Fixture::Base a("DUP",1),b("DUP",1),replacement("DUP",1);
            game.saved.bases={&a,&b};Fixture::PsiTrainingState state;game.top=&state;state.blit();
            Fixture::forbidModelReads=true;AgentUiState ui,changed;
            request.action="open_psi_training_base";request.choice="r0";
            check(state.agentUiState(ui)&&ui.rows.size()==2,"two duplicate choices admitted without model reads");
            AgentFrameCommand frame;check(frame.stage(request,ui,&state,1,1,100,100),"capture original row authority");
            state._btnBases[0]->setVisible(false);state.blit();
            check(state.agentUiState(changed)&&changed.rows.size()==1&&changed.rows[0]==1&&!frame.matchesFrame(101,1,1,&state,changed),"hiding earlier duplicate cannot reinterpret stale r0");
            state._btnBases[0]->setVisible(true);state.blit();
            const auto reject=[&](){state.blit();check(!state.agentUiState(changed)&&!state.agentDispatch(request)&&game.pushes==0,"changed constructor pairing refuses even after repaint");};
            std::swap(state._bases[0],state._bases[1]);reject();std::swap(state._bases[0],state._bases[1]);
            std::swap(state._btnBases[0],state._btnBases[1]);reject();std::swap(state._btnBases[0],state._btnBases[1]);
            state._bases[0]=&replacement;reject();state._bases[0]=&a;
            TextButton *original=state._btnBases[0];state._btnBases[0]=state._btnOk;reject();state._btnBases[0]=original;
            state._bases.pop_back();reject();state._bases.push_back(&b);
            state._bases[0]=nullptr;reject();state._bases[0]=&a;state.blit();
            state._btnBases[0]->setText("");state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>().empty(),"empty native caption is valid choice");
            for(const std::string bad:{"r2","r8","r99","r01","","BASE DUP"}){request.choice=bad;check(!state.agentDispatch(request)&&game.pushes==0,"foreign row refuses without push");}
            request.choice="r0";
            state._btnBases[0]->setX(81);state._btnBases[0]->setX(80);
            check(!state.agentDispatch(request)&&game.pushes==0,"base geometry ABA refuses stale action");state.blit();
            a.name="HIDDEN NEW NAME";a.labs=0;b.labs=0;
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>().empty(),"model eligibility/name divergence does not alter current choice");
            check(state.agentDispatch(request)&&game.pushes==1&&game.allocation->base==&a&&game.pops==0,"literal sender callback pushes exact paired base without popping menu");
            check(a.assignments==0&&b.assignments==0,"capture-only opening does not assign soldiers");
            check(!state.agentDispatch(request)&&game.pushes==1,"inactive menu refuses repeated direct dispatch not ledger replay proof");
            delete game.allocation;game.allocation=nullptr;game.pushes=0;Fixture::forbidModelReads=false;state.release();
        }
        {
            Fixture::Base base("ALPHA",1),otherBase("ALPHA",1),hiddenBase("INELIGIBLE",0);
            game.saved.bases={&base,&hiddenBase,&otherBase};
            const std::string message=language.getString("STR_PSIONIC_TRAINING");
            Fixture::PsiTrainingState state;game.top=&state;
            AgentUiState ui;
            check(!state.agentUiState(ui),"constructor prepaint denied");
            state.blit();check(state.agentUiState(ui),"full screen window needs no popup animation");
            state.finishPopup();state.blit();
            check(state.agentUiState(ui)&&ui.screen=="psi_training","actual constructor training menu admitted");
            auto visible=YAML::Load(ui.visible);
            check(visible.size()==4&&visible["title"].as<std::string>()==message,"only title bases rows buttons projected");
            check(ui.actions.size()==2&&ui.offers("close_psi_training")&&ui.offers("open_psi_training_base"),"acknowledgement and visible base choice");
            check(visible["buttons"][0]["id"].as<std::string>()=="ok"&&visible["buttons"].size()==1,"ordinary button order");
            base.name="UNDISPLAYED MODEL NAME";check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==message,"projection reads current UI text not base model");
            check(state._btnBases.size()==2&&YAML::Load(ui.visible)["bases"].size()==2,"constructor filters ineligible base and keeps duplicates");
            check(YAML::Load(ui.visible)["bases"][0].as<std::string>()=="ALPHA"&&YAML::Load(ui.visible)["bases"][1].as<std::string>()=="ALPHA","duplicate visible captions retained with independent handles");
            state._btnBases[0]->setText("CURRENT LABEL");check(!state.agentUiState(ui),"dirty base label invalidates prior paint");state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["bases"][0].as<std::string>()=="CURRENT LABEL","current widget label not base name");
            for(const std::string label:{std::string(500,'W'),std::string("LINE\nLINE"),std::string("\xF0\x9F\x99\x82")}){
                state._btnBases[0]->setText(label);state.blit();
                check(state.agentUiState(ui)&&YAML::Load(ui.visible)["bases"][0].as<std::string>()==label,"logical base caption remains despite overflow/multiline/missingglyph");
            }
            state._btnBases[0]->setText("CURRENT LABEL");state.blit();
            SDL_FillRect(state._btnBases[0]->getSurface(),nullptr,0);
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["bases"][0].as<std::string>()=="CURRENT LABEL","base caption does not require painted glyph ink");
            state._btnBases[0]->setVisible(false);state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["bases"].size()==1,"hidden base label omitted without null slot");
            state._btnBases[0]->setVisible(true);state.blit();
            state._btnBases[0]->setVisible(false);state._btnBases[0]->setVisible(true);
            check(!state.agentUiState(ui),"base visibility ABA denied before paint");state.blit();
            Surface baseCover(8,8,80,40);baseCover.setPalette(palette);baseCover.clear(90);state._surfaces.push_back(&baseCover);state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["bases"].size()==1,"occluded base caption omitted");state._surfaces.pop_back();state.blit();
            game.cursorVisible=true;game.cursor={81,41,8,8};
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["bases"].size()==2&&YAML::Load(ui.visible)["rows"].size()==1&&ui.rows[0]==1,"cursor preserves caption but omits covered actionable row");game.cursorVisible=false;
            request.action="select_psi_training_base";check(!state.agentDispatch(request)&&game.pops==0,"base selection remains unsupported");
            request.action="acknowledge_craft_error";check(!state.agentDispatch(request)&&game.pops==0,"neighbour action rejected");
            request.action="unknown";check(!state.agentDispatch(request)&&game.pops==0,"unknown action no mutation");
            game.cursorVisible=true;game.cursor={11,17,8,8};
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==message&&ui.actions.size()==2,"pointer over caption does not gate logical text or choices");
            game.cursor={81,175,8,8};request.action="close_psi_training";
            check((!state.agentUiState(ui)||!ui.offers(request.action))&&!state.agentDispatch(request)&&game.pops==0,"pointer on sole control refuses dispatch");game.cursorVisible=false;
            Surface otherCanvas(320,200);otherCanvas.setPalette(palette);game.screen.surface=&otherCanvas;
            check(!state.agentUiState(ui),"different destination without repaint rejected");game.screen.surface=&canvas;
            SDL_FillRect(state._txtTitle->getSurface(),nullptr,0);
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==message,"erased glyph pixels do not gate active logical message");
            state._txtTitle->setText("\xF0\x9F\x99\x82");state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()=="\xF0\x9F\x99\x82","missing glyph not admission");state._txtTitle->setText(message);state.blit();
            for(Surface *widget:std::vector<Surface *>{state._txtTitle,state._btnOk}){
                int x=widget->getX();widget->setX(x+1);widget->setX(x);
                check(!state.agentUiState(ui),"geometry ABA without repaint denied");
                request.action="close_psi_training";check(!state.agentDispatch(request),"stale dispatch no mutation");
                state.blit();check(state.agentUiState(ui),"geometry repaint restores");
                widget->setVisible(false);widget->setVisible(true);
                check(!state.agentUiState(ui),"visibility ABA stale");state.blit();
            }
            state._txtTitle->setText("Changed caption");check(!state.agentUiState(ui),"text mutation stale");state.blit();check(state.agentUiState(ui),"new caption repaint");
            state._txtTitle->setVisible(false);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].IsNull(),"hidden message is null");
            state._txtTitle->setVisible(true);state._txtTitle->setText(message);state.blit();
            state._btnOk->setVisible(false);state.blit();request.action="close_psi_training";
            check(!state.agentDispatch(request),"hidden acknowledgement denied");state._btnOk->setVisible(true);state.blit();
            state._modal=state._btnOk;check(!state.agentUiState(ui),"local modal denied");state._modal=nullptr;
            game.top=nullptr;check(!state.agentUiState(ui),"wrong top denied");game.top=&state;
            game.clear=false;check(!state.agentUiState(ui),"external overlay denied");game.clear=true;
            SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clipped paint denied");
            SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restore needs repaint");state.blit();
            state.paintHook=[&](){SDL_SetClipRect(canvas.getSurface(),&clip);};state.blit();check(!state.agentUiState(ui),"clip during paint denied");
            state.paintHook=nullptr;SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();
            Surface cover(10,10,state._txtTitle->getX(),state._txtTitle->getY());cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].IsNull(),"occluded message withheld");state._surfaces.pop_back();state.blit();
            check(state.agentUiState(ui),"authority baseline");AgentFrameCommand frame;request.action="close_psi_training";
            check(frame.stage(request,ui,&state,1,1,100,100),"stage public choice");AgentUiState changed;
            check(ui.actor==&state,"actor is current popup not hidden model");
            Fixture::PsiTrainingState other;game.top=&other;other.finishPopup();other.blit();
            check(other.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&other,changed),"different popup instance rejects prior authority");other.release();game.top=&state;state.blit();
            state._txtTitle->initText(&big,&overlap,&language);state._txtTitle->setSmall();
            const std::string overlapping="OVERLAP\nOVERLAP";
            state._txtTitle->setText(overlapping);state.blit();
            check(overlap.getHeight()+overlap.getSpacing()<overlap.getHeight()&&state._txtTitle->getTextWidth()>0,"synthetic overlap retains valid horizontal layout");
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==overlapping,"overlapping glyph lines do not gate logical UI text");
            state._txtTitle->initText(&big,&small,&language);state._txtTitle->setSmall();state._txtTitle->setText(message);state.blit();
            state._txtTitle->setText(std::string(500,'W'));state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==std::string(500,'W'),"long logical message remains exported without layout gate");
            state._txtTitle->setText(message);state.blit();
            const std::string originalCaption=state._btnOk->getText();
            state._btnOk->setText(std::string(500,'W'));state.blit();request.action="close_psi_training";
            check(state.agentUiState(ui)&&ui.offers(request.action),"long logical button label not a readability gate");
            state._btnOk->setText(originalCaption);state.blit();
            request.action="close_psi_training";
            check(state.agentDispatch(request)&&game.pops==1,"OK only pops once");
            check(!state.agentDispatch(request)&&game.pops==1,"no replay after popup closed");
            game.pops=0;state.release();
        }
    }
    check(!SDL_GetVideoSurface(),"no game video opened");
    std::cout<<"PASS "<<checks<<" psi-training constructor/provider/handler checks; explicit domain seams\n";
}
