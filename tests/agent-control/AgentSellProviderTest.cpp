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
#include "../../src/Interface/ComboBox.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Engine/Timer.h"
#include <algorithm>
#include <climits>
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
struct Base{};
struct SellState : State {
 Base * _base;TextButton *_btnOk,*_btnCancel;Surface *_window;
 Text *_txtTitle,*_txtSales,*_txtFunds,*_txtSpaceUsed,*_txtQuantity,*_txtSell,*_txtValue;
 ComboBox *_cbxCategory;TextList *_lstItems;Timer *_timerInc,*_timerDec;
 std::vector<int> _items,_rows,drafts;size_t _sel=0;int accepts=0,categoryChanges=0;
 SellState(Base*b):_base(b){_timerInc=new Timer(250);_timerDec=new Timer(250);}
 ~SellState(){delete _timerInc;delete _timerDec;}
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void lstItemsLeftArrowClick(Action*);void lstItemsRightArrowClick(Action*);void btnCancelClick(Action*);
 void btnOkClick(Action*){++accepts;_game->popState();} // Domain acceptance seam, not sale execution proof.
 void cbxCategoryChange(Action*){++categoryChanges;}
 void changeByValue(int amount,int direction){drafts[_sel]=std::max(0,drafts[_sel]+amount*direction);_lstItems->setCellText(_sel,2,std::to_string(drafts[_sel]));}
 void release(){for(auto *surface:_surfaces)delete surface;_surfaces.clear();}
};
#include "Sell.generated.inc"
}
static unsigned checks=0;static void check(bool ok,const char*why){++checks;if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
static Text* text(Fixture::SellState&s,Font&big,Font&small,Language&l,int w,int h,int x,int y,const std::string&str){auto*t=new Text(w,h,x,y);s.add(t,"","");t->setColor(80);t->setText(str);return t;}
static void setup(Fixture::SellState&s,Font&big,Font&small,Language&l){
 s._window=new Surface(320,200);s.add(s._window,"","");s._window->clear(40);
 s._btnOk=new TextButton(148,16,8,176);s.add(s._btnOk,"","");s._btnOk->setColor(80);s._btnOk->setText(l.getString("STR_SELL_SACK"));
 s._btnCancel=new TextButton(148,16,164,176);s.add(s._btnCancel,"","");s._btnCancel->setColor(80);s._btnCancel->setText(l.getString("STR_CANCEL"));
 s._txtTitle=text(s,big,small,l,310,17,5,8,l.getString("STR_SELL_ITEMS_SACK_PERSONNEL"));
 s._txtSales=text(s,big,small,l,150,9,10,24,"Sales $0");s._txtFunds=text(s,big,small,l,150,9,160,24,"Funds $6000000");s._txtSpaceUsed=text(s,big,small,l,150,9,160,34,"Stores 10:100");
 s._txtQuantity=text(s,big,small,l,54,9,136,44,l.getString("STR_QUANTITY_UC"));s._txtSell=text(s,big,small,l,96,9,190,44,l.getString("STR_SELL_SACK"));s._txtValue=text(s,big,small,l,40,9,270,44,l.getString("STR_VALUE"));
 s._lstItems=new TextList(287,120,8,54);s.add(s._lstItems,"","");s._lstItems->setColor(80);s._lstItems->setSecondaryColor(90);s._lstItems->setBackground(s._window);s._lstItems->setArrowColumn(182,ARROW_VERTICAL);s._lstItems->setColumns(4,156,54,24,53);s._lstItems->setSelectable(true);s._lstItems->setMargin(2);
 s._lstItems->onLeftArrowClick((ActionHandler)&Fixture::SellState::lstItemsLeftArrowClick);s._lstItems->onRightArrowClick((ActionHandler)&Fixture::SellState::lstItemsRightArrowClick);
 for(int i=0;i<24;++i){std::string name="Visible item "+std::to_string(i);s._lstItems->addRow(4,name.c_str(),"10","0","$100");s._items.push_back(i);s._rows.push_back(i);s.drafts.push_back(0);}
 s._cbxCategory=new ComboBox(&s,120,16,10,36);s.add(s._cbxCategory,"","");s._cbxCategory->setColor(80);s._cbxCategory->setOptions({"All","Items"},false);s._cbxCategory->onChange((ActionHandler)&Fixture::SellState::cbxCategoryChange);
}
int main(int argc,char**argv){
 check(argc==2,"data argument");check(SDL_Init(0)==0,"SDL no-device");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);
 for(const std::string name:{"xcom1","xcom2"}){
 Language l;l.loadFile(std::string(argv[1])+"/standard/"+name+"/Language/en-US.yml");Fixture::Game g;g.screen.surface=&canvas;g.mod.background=&background;g.language=&l;g.big=&big;g.small=&small;g.palette=palette;Fixture::currentGame=&g;Fixture::Base b;Fixture::SellState s(&b);g.top=&s;setup(s,big,small,l);AgentUiState ui;
 check(!s.agentUiState(ui),"unpainted rows denied");s.blit();check(s.agentUiState(ui)&&ui.screen=="sell","painted Sell provider");
 auto v=YAML::Load(ui.visible);check(v["headings"].size()==3&&v["rows"].size()>0&&v["rows"][0]["cells"].size()==4&&v["scrollbar"].IsNull(),"bounded row-only contract");
 check(ui.offers("accept_sell")&&ui.offers("cancel_sell")&&ui.offers("change_sale_quantity"),"ordinary sale controls");
 AgentRequest req;req.action="change_sale_quantity";req.choice="r0";req.operation="adjust";req.value=1;check(s.agentDispatch(req)&&s.drafts[0]==1,"one increase callback");check(!s.agentUiState(ui),"post-change stale list denied");s.blit();req.value=-1;check(s.agentDispatch(req)&&s.drafts[0]==0,"one decrease callback");s.blit();
 for(int value:{0,2,-2,1000}){req.value=value;check(!s.agentDispatch(req)&&s.drafts[0]==0,"non-step denied");}req.value=1;req.operation="max";check(!s.agentDispatch(req),"max not exposed");req.operation="adjust";req.choice="r31";check(!s.agentDispatch(req),"nonvisible row denied");req.choice="r0";
 s._timerInc->start();check(!s.agentDispatch(req),"physical repeat timer prevents racing semantic input");s._timerInc->stop();
 s._lstItems->setCellText(0,0,std::string(250,'W'));s.blit();check(s.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull(),"clipped cell text withheld independently");s._lstItems->setCellText(0,0,"Visible item 0");s.blit();
 s._lstItems->setVisible(false);s.blit();check(!s.agentUiState(ui),"hidden rows denied");s._lstItems->setVisible(true);s.blit();
 s._btnOk->setVisible(false);s._btnCancel->setVisible(false);s.blit();check(s.agentUiState(ui)&&!ui.offers("accept_sell")&&!ui.offers("cancel_sell"),"overfull hidden buttons never offered");s._btnOk->setVisible(true);s._btnCancel->setVisible(true);s.blit();
 req=AgentRequest();req.action="scroll_sell";req.direction="down";req.amount="page";check(!s.agentDispatch(req),"page not exposed");req.amount="step";check(s.agentDispatch(req),"visible step scroll");s.blit();
 req=AgentRequest();req.action="open_sell_category";check(s.agentDispatch(req),"category open");s.blit();check(s.agentUiState(ui)&&ui.screen=="sell_category","visible category dropdown");check(!ui.rows.empty()&&ui.offers("choose_visible_row"),"category exposes a painted target");req.action="choose_visible_row";req.choice="r0";const int changes=s.categoryChanges;const bool chosen=s.agentDispatch(req);if(!chosen||s.categoryChanges!=changes+1)std::cerr<<ui.visible<<" changes="<<s.categoryChanges<<" chosen="<<chosen<<"\n";check(chosen&&s.categoryChanges==changes+1,"ordinary category callback");s.blit();
 req=AgentRequest();req.action="accept_sell";g.clear=false;check(!s.agentDispatch(req)&&s.accepts==0,"overlay denies accept");g.clear=true;g.top=nullptr;check(!s.agentDispatch(req),"wrong state denies accept");g.top=&s;
 req.action="cancel_sell";check(s.agentDispatch(req)&&g.pops==1&&s.accepts==0,"ordinary cancel no accept");check(!s.agentDispatch(req)&&g.pops==1,"cancel repeat after transition denied");g.top=&s;s.blit();req.action="accept_sell";check(s.agentDispatch(req)&&s.accepts==1&&g.pops==2,"accept routed once to handler seam");s.release();
 }
 check(agentPaintHealthy()&&!SDL_WasInit(SDL_INIT_VIDEO),"offscreen only");std::cout<<"AgentSellProviderTest PASS "<<checks<<" checks\n";
}
