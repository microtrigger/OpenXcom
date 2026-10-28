// SPDX-License-Identifier: GPL-3.0-or-later
// Literal constructor/provider/OK; explicit Game/Mod/interface/render-loop seams.
// Actual visible OK dispatch calls literal native pop-only handler; model seams remain explicit.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Window.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <map>
#include <sstream>
#include "../../src/Interface/TextList.h"
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
struct Mod {Surface *background;Surface *getSurface(const std::string&){return background;}};
struct Game {
    Screen screen; Mod mod; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0;
    Screen *getScreen(){return &screen;} Mod *getMod(){return &mod;}
    Language *getLanguage(){return language;}
    bool isState(const void *state) const{return top == state;}
    bool cursorVisible=false; SDL_Rect cursor={0,0,0,0};bool fpsVisible=false;SDL_Rect fps={0,0,0,0};
    bool agentOverlayClear(const AgentUiState &ui) const{
        if(!clear)return false;
        for(const auto &r:ui.regions)if(cursorVisible && r.x<cursor.x+cursor.w && r.x+r.width>cursor.x && r.y<cursor.y+cursor.h && r.y+r.height>cursor.y)return false;
        return true;
    }
    void popState(){++pops;top=nullptr;}
    bool agentNonCursorOverlayClear(const AgentUiState&ui) const{if(!clear)return false;for(const auto&r:ui.regions)if(fpsVisible&&r.x<fps.x+fps.w&&r.x+r.width>fps.x&&r.y<fps.y+fps.h&&r.y+r.height>fps.y)return false;return true;}
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
struct ReequipStat {std::string item;int qty;std::string craft;};
struct CannotReequipState : State {
 TextButton *_btnOk;Window *_window;Text *_txtTitle,*_txtItem,*_txtQuantity,*_txtCraft;TextList *_lstItems;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 CannotReequipState(std::vector<ReequipStat>);~CannotReequipState();void btnOkClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "CannotReequip.generated.inc"
}
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}

int main(int argc,char **argv){
 check(argc==3,"data and isolated log args");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string gameName:{"xcom1","xcom2"})for(int count:{0,1,40}){
 Language language;language.loadFile(std::string(argv[1])+"/standard/"+gameName+"/Language/en-US.yml");Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
 std::vector<Fixture::ReequipStat> shortages;for(int i=0;i<count;++i)shortages.push_back({"STR_RIFLE",i+1,"CRAFT "+std::to_string(i)});
 Fixture::CannotReequipState state(shortages);game.top=&state;AgentUiState ui;AgentRequest req;
 check(!state.agentUiState(ui),"unpainted denied");state.blit();check(state.agentUiState(ui)&&ui.screen=="cannot_reequip","actual constructor admitted");auto v=YAML::Load(ui.visible);check(v.size()==5,"exact fields");
 check(v["title"].as<std::string>()==state._txtTitle->getText(),"actual logical title");for(int j=0;j<3;++j)check(!v["headings"][j].IsNull(),"all stock headings admitted despite allocation overlaps");
 check(v["rows"].size()==(count<14?count:14),"only current viewport rows");check(v["scroll"]["down"].as<bool>()==(count==40),"scroll only with native shown arrows");check(ui.offers("acknowledge_cannot_reequip"),"OK offered");
 if(count){check(v["rows"][0]["cells"].size()==3&&v["rows"][0]["cells"][1].as<std::string>()=="1"&&v["rows"][0]["cells"][2].as<std::string>()=="CRAFT 0","visible cells include rightmost overhang");shortages[0].craft="UNDISPLAYED MODEL";check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][2].as<std::string>()=="CRAFT 0","input vector not reread");}
 if(count==40){AgentUiState before=ui;req.action="scroll_visible_list";req.choice="down";check(state.agentDispatch(req)&&game.pops==0,"ordinary downward scroll");check(!state.agentUiState(ui),"scroll requires repaint");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][1].as<std::string>()=="2"&&!ui.sameBinding(before),"scroll displays next current row and invalidates old binding");req.choice="up";check(state.agentDispatch(req),"ordinary upward scroll");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][1].as<std::string>()=="1","back to first row");}
 if(count){state._lstItems->setCellText(0,2,"CURRENT CAPTION");check(!state.agentUiState(ui),"unpainted cell change stale");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][2].as<std::string>()=="CURRENT CAPTION","active logical cell rather than original shortage");state._lstItems->setCellText(0,2,"CRAFT 0");state.blit();}
 req.action="unknown";check(!state.agentDispatch(req)&&game.pops==0,"unknown inert");req.action="acknowledge_cannot_reequip";
 const auto title=state._txtTitle->getText();state._txtTitle->setText(std::string(500,'W'));check(!state.agentUiState(ui),"unpainted text stale");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==std::string(500,'W'),"logical long caption no glyph fit gate");state._txtTitle->setText(title);state.blit();
 for(Surface*w:std::vector<Surface*>{state._txtTitle,state._txtItem,state._txtQuantity,state._txtCraft,state._lstItems,state._btnOk}){int x=w->getX();w->setX(x+1);w->setX(x);check(!state.agentUiState(ui),"geometry ABA rejected");state.blit();}
 state._txtCraft->setVisible(false);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["headings"][2].IsNull(),"hidden heading null");state._txtCraft->setVisible(true);state.blit();
 game.cursorVisible=true;game.cursor={102,176,8,8};check(state.agentUiState(ui)&&!ui.offers(req.action)&&!state.agentDispatch(req),"cursor on OK blocks action");game.cursor={52,10,8,8};check(state.agentUiState(ui)&&!YAML::Load(ui.visible)["title"].IsNull(),"cursor does not block logical caption");game.cursorVisible=false;
 if(count){game.fpsVisible=true;game.fps={12,58,8,8};check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull()&&!YAML::Load(ui.visible)["rows"][0]["cells"][1].IsNull(),"noncursor overlay independently masks cell");game.fpsVisible=false;}
 state._btnOk->setVisible(false);state.blit();check(state.agentUiState(ui)&&!ui.offers(req.action)&&!state.agentDispatch(req),"hidden OK denied");state._btnOk->setVisible(true);state.blit();
 game.top=nullptr;check(!state.agentUiState(ui),"covered state denied");game.top=&state;state._modal=state._btnOk;check(!state.agentUiState(ui),"modal denied");state._modal=nullptr;
 SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"destination clip denied");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restore needs repaint");state.blit();
 check(state.agentDispatch(req)&&game.pops==1,"actual OK pops only");check(!state.agentDispatch(req)&&game.pops==1,"repeat after pop denied");state.release();
 TextList unrelated(288,112,8,58);std::vector<AgentOutfittingRow> rows;check(!unrelated.agentReequipRows(rows),"new logical capture default off");
 }
 check(!SDL_GetVideoSurface(),"no game opened");std::cout<<"PASS "<<checks<<" re-equip constructor/provider/native-scroll/OK checks; explicit seams\n";
}
