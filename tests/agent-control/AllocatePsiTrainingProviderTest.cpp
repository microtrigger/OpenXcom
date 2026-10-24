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
#include <map>
#include <vector>
// Test-only access to actual private native cells; production headers unchanged.
#define private public
#include "../../src/Interface/TextList.h"
#undef private
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
struct Base {std::vector<Soldier*> soldiers;int capacity=1;int getAvailablePsiLabs(){return capacity;}
    int getUsedPsiLabs(){int n=0;for(auto*s:soldiers)n+=s->training;return n;}
    std::vector<Soldier*> *getSoldiers(){return &soldiers;}
};
struct SavedGame {bool researched=false;bool isResearched(int){return researched;}};
struct Game {
    SavedGame saved; SavedGame *getSavedGame(){return &saved;}
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
#include "AllocatePsiTraining.generated.inc"
}
static unsigned checks = 0;
static void check(bool ok,const char *message){
    ++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}
}
int main(int argc,char **argv){
    check(argc==2,"source argument");check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");
    FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
    Font big,small;
    for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
    Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
    canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
    const std::string token(32,'1'),prefix="E "+token+" "+token+" "+token+" 1 ";AgentRequest request;
    check(parseAgentRequest(prefix+"close_psi_training_allocation",request),"close empty args parsed");
    check(!parseAgentRequest(prefix+"close_psi_training_allocation extra",request),"close extra args rejected");
    check(parseAgentRequest(prefix+"toggle_psi_training r0",request),"toggle visible row parsed");
    check(!parseAgentRequest(prefix+"toggle_psi_training r0 extra",request),"toggle extra arg rejected");
    for(const std::string master:{"xcom1","xcom2"}){
        Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");
        Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
        Options::psiStrengthEval=false;Options::allowPsiStrengthImprovement=false;
        Fixture::Soldier first,second;Fixture::Base base;base.soldiers={&first,&second};
        {
            first.name="PRIVATE HIDDEN CELL";
            Fixture::AllocatePsiTrainingState hidden(&base);game.top=&hidden;
            Text *cell=hidden._lstSoldiers->_texts[0][0];cell->setVisible(false);
            hidden._lstSoldiers->invalidate();hidden.blit();AgentUiState hiddenUi;
            check(!hidden.agentUiState(hiddenUi),"never-painted dirty hidden cell conservatively refuses entire projection");
            request.action="close_psi_training_allocation";
            check(!hidden.agentDispatch(request)&&game.pops==0&&hiddenUi.visible.find("PRIVATE HIDDEN CELL")==std::string::npos,"rejected hidden initial projection cannot authorize action or expose hidden string");
            cell->setVisible(true);check(!hidden.agentUiState(hiddenUi),"child show without owning list redraw remains refused");hidden._lstSoldiers->invalidate();hidden.blit();check(hidden.agentUiState(hiddenUi),"shown cell repainted");
            cell->setVisible(false);check(!hidden.agentUiState(hiddenUi),"hide after paint rejects stale caption");hidden._lstSoldiers->invalidate();hidden.blit();
            check(hidden.agentUiState(hiddenUi)&&YAML::Load(hiddenUi.visible)["rows"][0]["cells"][0].IsNull(),"fresh hide exports null");
            check(hiddenUi.visible.find("PRIVATE HIDDEN CELL")==std::string::npos&&YAML::Load(hiddenUi.visible)["rows"][0]["cells"][1].as<std::string>()==std::string(language.getString("STR_UNKNOWN")),"painted then hidden cell absent while other cells retained");
            cell->setVisible(true);hidden._lstSoldiers->invalidate();hidden.blit();cell->setVisible(false);cell->setVisible(true);
            check(!hidden.agentUiState(hiddenUi),"individual cell visibility ABA rejects stale paint");hidden._lstSoldiers->invalidate();hidden.blit();check(hidden.agentUiState(hiddenUi),"ABA fresh paint restores");
            const int originalX=cell->getX();cell->setX(originalX+1);check(!hidden.agentUiState(hiddenUi),"individual cell geometry change refuses stale paint");cell->setX(originalX);hidden._lstSoldiers->invalidate();hidden.blit();check(hidden.agentUiState(hiddenUi),"geometry restored with fresh paint");
            SDL_Rect narrow={0,0,1,1};SDL_SetClipRect(hidden._lstSoldiers->getSurface(),&narrow);hidden.blit();
            check(!hidden.agentUiState(hiddenUi),"actual local clip invalidates cached cell presentation");
            SDL_SetClipRect(hidden._lstSoldiers->getSurface(),nullptr);hidden._lstSoldiers->invalidate();hidden.blit();check(hidden.agentUiState(hiddenUi),"local clip restore fresh draw");
            hidden.release();first.name="DUPLICATE";
        }
        Fixture::AllocatePsiTrainingState state(&base);game.top=&state;AgentUiState ui;
        check(!state.agentUiState(ui),"prepaint denied");state.blit();check(state.agentUiState(ui)&&ui.screen=="allocate_psi_training","literal constructor admitted");
        auto visible=YAML::Load(ui.visible);check(visible["rows"].size()==2,"two visible duplicate-name rows");
        check(visible["rows"][0]["cells"][0].as<std::string>()==visible["rows"][1]["cells"][0].as<std::string>(),"duplicate names not identity");
        const std::string unknown=language.getString("STR_UNKNOWN"),yes=language.getString("STR_YES"),no=language.getString("STR_NO");
        check(visible["rows"][0]["cells"][1].as<std::string>()==unknown,"hidden psi strength remains literal Unknown");
        check(visible["rows"][0]["cells"][2].as<std::string>()=="0/+0","untrained skill shown zero");
        check(visible["rows"][0]["cells"][3].as<std::string>()==no,"initial training No");
        first.stats.psiStrength=999;check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][1].as<std::string>()==unknown,"model stat change not projected");
        request.action="toggle_psi_training";request.choice="r0";
        check(state.agentDispatch(request)&&first.training&&first.changes==1&&state._labSpace==0,"native immediate assignment");
        check(!state.agentUiState(ui)&&!state.agentDispatch(request)&&first.changes==1,"dirty postmutation cannot replay before repaint");
        state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][3].as<std::string>()==yes,"fresh UI Yes after assignment");
        request.choice="r1";check(state.agentDispatch(request)&&!second.training&&second.changes==0&&state._labSpace==0,"full capacity native silent no-op");
        state.blit();request.choice="r0";check(state.agentDispatch(request)&&!first.training&&first.changes==2&&state._labSpace==1,"native removal releases one slot");state.blit();
        check(state.agentUiState(ui)&&YAML::Load(ui.visible)["remaining"].as<std::string>()==std::string(language.getString("STR_REMAINING_PSI_LAB_CAPACITY").arg(1)),"native visible capacity restored after removal");
        check(state._lstSoldiers->agentSelectPsiTrainingRow(0),"ordinary row selection helper");state.blit();
        check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["selected"].as<bool>(),"actually painted selector exported");
        state._lstSoldiers->mouseOut(nullptr,&state);check(!state.agentUiState(ui),"selector mouseOut invalidates paint");state.blit();
        check(state.agentUiState(ui)&&!YAML::Load(ui.visible)["rows"][0]["selected"].as<bool>(),"hidden selector not inferred from retained selected index");
        std::swap(base.soldiers[0],base.soldiers[1]);check(!state.agentUiState(ui)&&!state.agentDispatch(request),"roster reorder cannot retarget row");std::swap(base.soldiers[0],base.soldiers[1]);state.blit();
        request.action="unknown";check(!state.agentDispatch(request),"unknown action refused");request.action="toggle_psi_training";request.choice="r99";check(!state.agentDispatch(request),"unoffered row refused");
        for(const std::string caption:{std::string(500,'W'),std::string("\xF0\x9F\x99\x82"),std::string("LINE\nLINE")}){
            state._lstSoldiers->setCellText(0,0,caption);check(!state.agentUiState(ui),"cell dirty before paint rejected");state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>()==caption,"logical row caption no glyph-fit gate");
            auto rows=YAML::Load(ui.visible)["rows"];check(rows.size()==2&&rows[1]["cells"][0].as<std::string>()=="DUPLICATE","following row retained after long/multiline/missing glyph");
            request.action="toggle_psi_training";request.choice="r1";
            check(state.agentDispatch(request)&&second.training&&!first.training,"following displayed row selects actual second soldier");state.blit();
            check(state.agentDispatch(request)&&!second.training&&!first.training,"following row toggles back without corrupting previous row");state.blit();
        }
        state._lstSoldiers->setCellText(0,0,"DUPLICATE");state.blit();SDL_FillRect(state._lstSoldiers->getSurface(),nullptr,0);
        check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>()=="DUPLICATE","erased list ink does not change logical row");
        Surface cover(8,8,8,52);cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();check(!state.agentUiState(ui),"real later list overlay refuses projection");state._surfaces.pop_back();state.blit();
        game.top=nullptr;check(!state.agentUiState(ui),"inactive popup denied");game.top=&state;
        state._modal=state._btnOk;check(!state.agentUiState(ui),"local modal denied");state._modal=nullptr;
        state._txtTitle->setText(std::string(500,'W'));check(!state.agentUiState(ui),"dirty caption denied");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==std::string(500,'W'),"logical long title retained");
        state._txtTitle->setVisible(false);state._txtTitle->setVisible(true);check(!state.agentUiState(ui),"visibility ABA stale");state.blit();
        int x=state._lstSoldiers->getX();state._lstSoldiers->setX(x+1);state._lstSoldiers->setX(x);check(!state.agentUiState(ui),"list geometry ABA stale");state.blit();
        game.cursorVisible=true;game.cursor={11,9,6,6};check(state.agentUiState(ui),"cursor over logical heading not admission gate");game.cursor={81,175,6,6};request.action="close_psi_training_allocation";check(!state.agentDispatch(request),"cursor on OK blocks action");game.cursorVisible=false;
        SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clipped paint denied");SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();
        check(state.agentUiState(ui),"fresh baseline restored");AgentFrameCommand frame;request.action="close_psi_training_allocation";check(frame.stage(request,ui,&state,1,1,100,100),"stage close authority");
        state._txtRemaining->setText("VISIBLE OVERRIDE");state.blit();AgentUiState changed;check(state.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&state,changed),"changed visible remaining invalidates authority");
        check(state.agentDispatch(request)&&game.pops==1&&first.calcCalls==1&&second.calcCalls==1,"OK recalculates each stat string then pops");
        check(!first.calcEval&&!second.calcEval&&!first.training&&!second.training,"OK preserves training with current eval option");
        check(!state.agentDispatch(request)&&game.pops==1&&first.calcCalls==1,"closed popup no replay");state.release();game.pops=0;
        // Actual native scroll preserves grant-local handles only until new paint.
        std::vector<Fixture::Soldier> many(24);Fixture::Base crowded;crowded.capacity=24;
        for(size_t i=0;i<many.size();++i){many[i].name="ROW "+std::to_string(i);crowded.soldiers.push_back(&many[i]);}
        Fixture::AllocatePsiTrainingState scrolling(&crowded);game.top=&scrolling;scrolling.blit();
        check(scrolling.agentUiState(ui)&&ui.offers("scroll_visible_list"),"native long roster offers scrolling");
        const auto firstRows=ui.rows;AgentFrameCommand scrollFrame;request.action="toggle_psi_training";request.choice="r0";
        check(scrollFrame.stage(request,ui,&scrolling,1,1,100,100),"stage before native scroll");
        request.action="scroll_visible_list";request.choice="down";
        check(scrolling.agentDispatch(request),"ordinary scroll down dispatched");check(!scrolling.agentUiState(ui),"scroll mutation requires repaint");scrolling.blit();
        check(scrolling.agentUiState(ui)&&ui.rows!=firstRows&&!scrollFrame.matchesFrame(101,1,1,&scrolling,ui),"scroll refresh invalidates old row authority");
        request.action="toggle_psi_training";request.choice="r0";const size_t shown=ui.rows.front();
        check(scrolling.agentDispatch(request)&&many[shown].training&&!many[0].training,"fresh handle toggles current displayed row not old first soldier");scrolling.blit();
        request.action="close_psi_training_allocation";Options::psiStrengthEval=true;game.saved.researched=true;
        check(scrolling.agentDispatch(request)&&game.pops==1,"native close on scrolled screen");
        for(auto &soldier:many)check(soldier.calcCalls==1&&soldier.calcEval,"OK recalculates each roster stat string using current eval policy");scrolling.release();game.pops=0;
    }
    check(!SDL_GetVideoSurface(),"no game video opened");std::cout<<"PASS "<<checks<<" allocate-psi constructor/provider/native handler checks; explicit domain seams\n";
}
