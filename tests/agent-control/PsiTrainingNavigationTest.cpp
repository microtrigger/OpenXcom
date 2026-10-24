// SPDX-License-Identifier: GPL-3.0-or-later
// Literal menu-to-allocation constructors and callbacks; explicit Game/Mod/domain/stack/render seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include <map>
#include <vector>
#include "../../src/Interface/TextList.h"
#include <sstream>
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
struct Mod { int getPsiRequirements(){return 0;} int getStatStrings(){return 0;} Surface *background; Surface *getSurface(const std::string&){return background;} };
struct Stats {int psiStrength=77,psiSkill=0;};
struct Soldier {
    std::string name="DUPLICATE";Stats stats;bool training=false;int changes=0,calcCalls=0;bool calcEval=false;
    Stats *getCurrentStats(){return &stats;}std::string getName(bool){return name;}
    int getPsiStrImprovement(){return 2;}int getImprovement(){return 3;}bool isInPsiTraining(){return training;}
    void setPsiTraining(bool value){training=value;++changes;}
    void calcStatString(int,bool eval){++calcCalls;calcEval=eval;}
};
static int menuReads=0;
struct Base {std::string name="DUPLICATE";std::string getName(){++menuReads;return name;}std::vector<Soldier*> soldiers;int capacity=1;int getAvailablePsiLabs(){++menuReads;return capacity;}
    int getUsedPsiLabs(){int n=0;for(auto*s:soldiers)n+=s->training;return n;}
    std::vector<Soldier*> *getSoldiers(){return &soldiers;}
};
struct SavedGame {std::vector<Base*> bases;std::vector<Base*> *getBases(){++menuReads;return &bases;}bool researched=false;bool isResearched(int){return researched;}};
struct AllocatePsiTrainingState;
struct Game {
    SavedGame saved; SavedGame *getSavedGame(){return &saved;}
    Screen screen; Mod mod; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0,pushes=0;const void *parent=nullptr;AllocatePsiTrainingState *allocation=nullptr;
    void pushState(AllocatePsiTrainingState *s){++pushes;parent=top;allocation=s;top=s;}
    Screen *getScreen(){return &screen;} Mod *getMod(){return &mod;}
    Language *getLanguage(){return language;}
    bool isState(const void *state) const{return top == state;}
    bool cursorVisible=false; SDL_Rect cursor={0,0,0,0};
    bool agentOverlayClear(const AgentUiState &ui) const{
        if(!clear)return false;
        for(const auto &r:ui.regions)if(cursorVisible && r.x<cursor.x+cursor.w && r.x+r.width>cursor.x && r.y<cursor.y+cursor.h && r.y+r.height>cursor.y)return false;
        return true;
    }
    void popState(){++pops;top=parent;parent=nullptr;}
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
struct AllocatePsiTrainingState : State {
    TextButton *_btnOk;Window *_window;
    Text *_txtTitle,*_txtTraining,*_txtName,*_txtRemaining,*_txtPsiStrength,*_txtPsiSkill;
    TextList *_lstSoldiers;std::vector<Soldier*> _soldiers;size_t _sel;int _labSpace;Base *_base;
    bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
    AllocatePsiTrainingState(Base*);~AllocatePsiTrainingState();
    void btnOkClick(Action*);void lstSoldiersClick(Action*);
    bool agentDestinationFull() const;std::vector<std::uint64_t> agentPaintRevisions() const;
    void blit() override;bool agentUiState(AgentUiState&) const override;bool agentDispatch(const AgentRequest&) override;
    void release(){for(auto*surface:_surfaces)delete surface;_surfaces.clear();}
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

#include "AllocatePsiTraining.generated.inc"
#include "PsiTraining.generated.inc"
}
static unsigned checks=0;
static void check(bool ok,const char *message){++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}}
int main(int argc,char **argv){
 check(argc==2,"source argument");check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen native widgets only");
 FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Options::psiStrengthEval=false;Options::allowPsiStrengthImprovement=false;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
 canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string master:{"xcom1","xcom2"}){
  Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");
  Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
  Fixture::Soldier first,second;first.name="FIRST ROSTER";second.name="SECOND ROSTER";
  Fixture::Base a,b;a.soldiers={&first};b.soldiers={&second};game.saved.bases={&a,&b};
  Fixture::PsiTrainingState menu;game.top=&menu;menu.blit();AgentUiState ui;
  int afterMenuCtor=Fixture::menuReads;
  check(menu.agentUiState(ui)&&ui.screen=="psi_training","actual menu current presentation admitted");
  auto visible=YAML::Load(ui.visible);
  check(visible["rows"].size()==2&&visible["rows"][0]["handle"].as<std::string>()=="r0"&&visible["rows"][1]["handle"].as<std::string>()=="r1","fresh two native handles");
  check(visible["rows"][0]["cells"][0].as<std::string>()=="DUPLICATE"&&visible["rows"][1]["cells"][0].as<std::string>()=="DUPLICATE","duplicate names are distinct controls");
  check(Fixture::menuReads==afterMenuCtor,"menu projection does not query model");
  AgentRequest request;request.action="open_psi_training_base";request.choice="r1";
  check(ui.offers(request.action)&&menu.agentDispatch(request),"literal menu dispatch and sender callback open r1");
  check(game.pushes==1&&game.pops==0&&game.parent==&menu&&game.top==game.allocation,"one actual allocation construction and push preserving parent");
  auto *allocation=game.allocation;
  check(allocation->_base==&b&&allocation->_soldiers.size()==1&&allocation->_soldiers[0]==&second,"actual constructor owns exact second base roster");
  check(Fixture::menuReads>afterMenuCtor,"ordinary allocation constructor capacity reads permitted");
  allocation->blit();check(allocation->agentUiState(ui)&&ui.screen=="allocate_psi_training","actual allocation current widgets admitted");
  visible=YAML::Load(ui.visible);check(visible["rows"].size()==1,"selected roster one current row");
  const auto cells=visible["rows"][0]["cells"];
  check(cells.size()==4&&cells[0].as<std::string>()=="SECOND ROSTER","second roster appears not first duplicate base");
  check(cells[1].as<std::string>()==std::string(language.getString("STR_UNKNOWN")),"actual constructor renders localized Unknown");
  check(ui.visible.find("77")==std::string::npos&&second.stats.psiStrength==77,"hidden strength77 is not projected");
  check(first.changes==0&&second.changes==0&&!first.training&&!second.training,"opening and observing do not assign training");
  check(first.calcCalls==0&&second.calcCalls==0,"opening does not recalculate stat strings");
  request.action="close_psi_training_allocation";request.choice.clear();
  check(ui.offers(request.action)&&allocation->agentDispatch(request),"existing allocation OK literal callback");
  check(second.calcCalls==1&&!second.calcEval&&first.calcCalls==0,"OK recalculates selected roster once with ordinary evaluationfalse");
  check(game.pops==1&&game.pushes==1&&game.top==&menu,"fixture stack restores same parent after one pop");
  check(first.changes==0&&second.changes==0&&!first.training&&!second.training,"OK does not assign training; stat recalculation not rollback");
  allocation->release();delete allocation;game.allocation=nullptr;
  menu.blit();int beforeFinal=Fixture::menuReads;check(menu.agentUiState(ui)&&ui.screen=="psi_training"&&YAML::Load(ui.visible)["rows"].size()==2,"restored menu fresh current projection");
  check(Fixture::menuReads==beforeFinal&&game.pushes==1,"restored observation no model reconstruction or automatic navigation");
  menu.release();
 }
 check(!SDL_GetVideoSurface(),"no full game video opened");
 std::cout<<"PASS "<<checks<<" literal menu/allocation constructor navigation checks; explicit domain/stack/render seams\n";
}
