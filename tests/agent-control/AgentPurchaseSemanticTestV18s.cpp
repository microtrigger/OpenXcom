// SPDX-License-Identifier: GPL-3.0-or-later
// Offscreen stock-font native evidence. Actual Purchase provider/dispatch,
// callbacks, quantity helpers, Timer and widgets; explicit model/owner seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Timer.h"
#include "../../src/Engine/Unicode.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/TextButton.h"
#define private public
#include "../../src/Interface/TextList.h"
#undef private
#include "../../src/Interface/ComboBox.h"
#include "../../src/Interface/ArrowButton.h"
#include "../../src/Interface/ScrollBar.h"
#include "../../src/Savegame/QuantityCommand.h"
#include "../../src/fmath.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include "AgentTableControllerDependencies.generated.h"
#include "AgentTableController.generated.h"
using namespace OpenXcom; using namespace OpenXcom::Observer;
namespace OpenXcom { namespace Observer { std::uint64_t agentTestTime=10000; bool diagnosticProfileQualified(){return true;} } }
static Uint32 ticks=10000; static SDLMod mods=KMOD_NONE; static Uint8 appState=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;
extern "C" Uint32 SDL_GetTicks(){return ticks;}
extern "C" Uint8 SDL_GetAppState(){return appState;}
extern "C" SDLMod SDL_GetModState(){return mods;}
extern "C" Uint8 SDL_GetMouseState(int*,int*){return 0;}
extern "C" void SDL_PumpEvents(){}
extern "C" int SDL_PeepEvents(SDL_Event*,int,SDL_eventaction,Uint32){return 0;}
namespace { unsigned checks=0; void check(bool value,const char *why){++checks;if(!value){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}} }
namespace OpenXcom {
Game *State::_game=nullptr;
State::State():_screen(true),_modal(nullptr),_ruleInterface(nullptr),_ruleInterfaceParent(nullptr){}
State::~State(){}
void State::init(){} void State::handle(Action*){} void State::blit(){} void State::resize(int&,int&){} void State::recenter(int,int){}
#include "PurchaseStateBase.generated.inc"
#include "PurchaseGameIdentity.generated.inc"
}
#include "PurchaseModelTestSeamsV18s.h"
namespace Purchases {
#include "PurchaseBaseStoreRule.generated.inc"
#include "PurchaseCommandsHeader.generated.inc"
struct ErrorMessageState {std::string message;ErrorMessageState(const std::string&s,SDL_Color*,int,const char*,int):message(s){} };
struct Game {const void*top=nullptr;Surface *_cursor=nullptr,*_fpsCounter=nullptr;SavedGame save;Mod mod;unsigned pops=0,pushes=0;std::string error;State *owner=nullptr;
 bool isState(const void*p)const{return top==p;}void popState(){++pops;top=nullptr;}void pushState(ErrorMessageState*p){++pushes;error=p->message;top=nullptr;delete p;}SavedGame*getSavedGame(){return &save;}Mod*getMod(){return &mod;}bool agentOverlayClear(const AgentUiState&)const;};
#include "PurchaseOverlay.generated.inc"
struct PurchaseState:OpenXcom::State {
 using State::_surfaces;using State::_modal; using State::_palette;
 Game *_game;Base *_base;Language *lang;Surface *_window=nullptr;TextButton *_btnOk=nullptr,*_btnCancel=nullptr;Text *_txtTitle=nullptr,*_txtFunds=nullptr,*_txtPurchases=nullptr,*_txtCost=nullptr,*_txtQuantity=nullptr,*_txtSpaceUsed=nullptr;ComboBox *_cbxCategory=nullptr;TextList *_lstItems=nullptr;
 std::vector<TransferRow>_items;std::vector<int>_rows;std::vector<std::string>_cats;size_t _sel=0;int _total=0,_pQty=0,_cQty=0;double _iQty=0;Uint8 _ammoColor=90;Timer *_timerInc,*_timerDec;
 PurchaseState(Game*g,Base*b,Language*l):_game(g),_base(b),lang(l){_timerInc=new Timer(250);_timerDec=new Timer(250);_timerInc->onTimer((StateHandler)&PurchaseState::increase);_timerDec->onTimer((StateHandler)&PurchaseState::decrease);g->top=this;g->owner=this;}
 ~PurchaseState(){delete _timerInc;delete _timerDec;_game->owner=nullptr;}
 LocalizedText tr(const std::string&id)const{return lang->getString(id);}TransferRow &getRow(){return _items[_rows[_sel]];}std::string getCategory(int n)const{return _items[n].type==TRANSFER_CRAFT?"STR_CRAFT_ARMAMENT":"STR_PERSONNEL";}
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void changeQuantity(size_t,QuantityOperation,int value=0);void think()override;void updateList();void cbxCategoryChange(Action*);void btnOkClick(Action*);void btnCancelClick(Action*);void increase();void decrease();void increaseByValue(int);void decreaseByValue(int);void updateItemStrings();
 void lstItemsLeftArrowPress(Action*);void lstItemsLeftArrowRelease(Action*);void lstItemsLeftArrowClick(Action*);void lstItemsRightArrowPress(Action*);void lstItemsRightArrowRelease(Action*);void lstItemsRightArrowClick(Action*);void lstItemsMousePress(Action*);
};
#include "PurchaseCommands.generated.inc"
#include "PurchaseMethods.generated.inc"
}
namespace {
#include "AgentManufactureChannel.inc"
struct Paint {
 Font &big,&small;Language &lang;Surface canvas,background;SDL_Color palette[256];std::vector<std::unique_ptr<Surface>>owned;std::vector<Surface*>surfaces;
 Paint(Font&b,Font&s,Language&l):big(b),small(s),lang(l),canvas(320,200),background(320,200){for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);surfaces.push_back(&background);}
 template<class T>T*own(T*p){p->setPalette(palette);owned.emplace_back(p);surfaces.push_back(p);return p;}
 Text *text(int w,int h,int x,int y,const std::string&value,bool large=false){auto t=own(new Text(w,h,x,y));t->initText(&big,&small,&lang);t->setColor(80);if(large)t->setBig();t->setText(value);return t;}
 TextButton *button(int w,int x,int y,const std::string&key){auto b=own(new TextButton(w,16,x,y));b->initText(&big,&small,&lang);b->setText(lang.getString(key));return b;}
 void draw(){for(auto s:surfaces)s->blit(&canvas);}
};
void setup(Purchases::PurchaseState&s,Paint&p,int count=20){
 using Purchases::PurchaseState;s._window=&p.background;
 s._btnOk=p.button(148,8,176,"STR_OK");s._btnCancel=p.button(148,164,176,"STR_CANCEL");s._btnOk->onMouseClick((ActionHandler)&PurchaseState::btnOkClick);s._btnOk->onKeyboardPress((ActionHandler)&PurchaseState::btnOkClick,Options::keyOk);s._btnCancel->onMouseClick((ActionHandler)&PurchaseState::btnCancelClick);s._btnCancel->onKeyboardPress((ActionHandler)&PurchaseState::btnCancelClick,Options::keyCancel);
 s._txtTitle=p.text(310,17,5,8,p.lang.getString("STR_PURCHASE_HIRE_PERSONNEL"),true);s._txtTitle->setAlign(ALIGN_CENTER);
 s._txtFunds=p.text(150,9,10,24,p.lang.getString("STR_CURRENT_FUNDS").arg("$6,000,000"));s._txtPurchases=p.text(150,9,160,24,p.lang.getString("STR_COST_OF_PURCHASES").arg("$0"));s._txtSpaceUsed=p.text(150,9,160,34,p.lang.getString("STR_SPACE_USED").arg("10:100"));
 s._txtCost=p.text(102,9,152,44,p.lang.getString("STR_COST_PER_UNIT_UC"));s._txtQuantity=p.text(60,9,256,44,p.lang.getString("STR_QUANTITY_UC"));
 s._lstItems=p.own(new TextList(287,120,8,54));s._lstItems->initText(&p.big,&p.small,&p.lang);s._lstItems->setColor(80);s._lstItems->setSecondaryColor(90);s._lstItems->setBackground(&p.background);s._lstItems->setArrowColumn(227,ARROW_VERTICAL);s._lstItems->setColumns(4,150,55,50,32);s._lstItems->setSelectable(true);s._lstItems->setMargin(2);
 s._lstItems->onLeftArrowPress((ActionHandler)&PurchaseState::lstItemsLeftArrowPress);s._lstItems->onLeftArrowRelease((ActionHandler)&PurchaseState::lstItemsLeftArrowRelease);s._lstItems->onLeftArrowClick((ActionHandler)&PurchaseState::lstItemsLeftArrowClick);s._lstItems->onRightArrowPress((ActionHandler)&PurchaseState::lstItemsRightArrowPress);s._lstItems->onRightArrowRelease((ActionHandler)&PurchaseState::lstItemsRightArrowRelease);s._lstItems->onRightArrowClick((ActionHandler)&PurchaseState::lstItemsRightArrowClick);s._lstItems->onMousePress((ActionHandler)&PurchaseState::lstItemsMousePress);
 s._cbxCategory=p.own(new ComboBox(&s,120,16,10,36));s._cbxCategory->initText(&p.big,&p.small,&p.lang);s._cbxCategory->setColor(80);s._cats={"STR_ALL_ITEMS","STR_PERSONNEL","STR_CRAFT_ARMAMENT"};s._cbxCategory->setOptions(s._cats,true);s._cbxCategory->onChange((ActionHandler)&PurchaseState::cbxCategoryChange);
 for(int i=0;i<count;++i){s._items.push_back({Purchases::TRANSFER_ENGINEER,nullptr,p.lang.getString(i%2?"STR_ENGINEER":"STR_SCIENTIST"),10000+i*1000,10,0,0});}
 s._surfaces=p.surfaces;s.updateList();p.draw();
}
void connect(TableChannel&c,Purchases::PurchaseState&s){c.state.project=[&](AgentUiState&u){return s.agentUiState(u);};c.state.handler=[&](const AgentRequest&r){check(s.agentDispatch(r),"exact Purchase semantic dispatcher consumes grant");if(s._game->top!=&s)c.game.top=&c.unsupportedParent;};}
AgentRequest rowRequest(const char*row,const char*operation,int value=0){AgentRequest r;r.action="change_purchase_quantity";r.choice=row;r.operation=operation;r.value=value;return r;}
void complete(TableChannel&c,Paint&p){c.frame();check(c.receive().empty(),"no same-frame action receipt");p.draw();c.frame();auto r=c.receive();check(!r.empty()&&YAML::Load(r)["status"].as<std::string>()=="dispatched","later actual frame dispatched receipt");c.send(c.request);c.controller->poll();check(c.receive()==r,"exact duplicate cached no replay");}
void advance(Purchases::PurchaseState&s,unsigned ms){ticks+=ms;agentTestTime+=ms;s.think();}
void run(Font&big,Font&small,Language&lang,const std::string&master){
 Paint p(big,small,lang);Purchases::Game g;Purchases::Base b;Purchases::PurchaseState s(&g,&b,&lang);setup(s,p);AgentUiState ui;
 check(s.agentUiState(ui)&&ui.screen=="purchase"&&ui.offers("change_purchase_quantity"),"actual stock-font Purchase projection");auto v=YAML::Load(ui.visible);check(v["rows"].size()>1&&v["rows"][0]["cells"].size()==4,"four independently typed cells");check(v["rows"][0]["controls"].size()==2,"both painted row arrows admitted");check(!v["scrollbar"].IsNull()&&v["scrollbar"]["targets"].size()<=3,"painted scrollbar finite targets");check(v["category"]["label"].as<std::string>()==std::string(lang.getString("STR_ALL_ITEMS")),"shown category label");
 std::vector<AgentPurchaseRow> rows;check(s._lstItems->agentPurchaseRows(rows)&&rows.size()>1,"native row map");
 // Each cell can become unknown without erasing either real arrow or another cell.
 for(unsigned col=0;col<4;++col){const auto old=s._lstItems->getCellText(0,col);s._lstItems->setCellText(0,col,std::string(200,'W'));p.draw();check(s.agentUiState(ui)&&!ui.rows.empty(),"unknown cell preserves row controls");auto x=YAML::Load(ui.visible);check(x["rows"][0]["cells"][col].IsNull(),"only fully presented cell text disclosed");check(x["rows"][0]["controls"].size()==2,"both independent arrows survive unknown text");s._lstItems->setCellText(0,col,old);p.draw();}
 for(unsigned col=0;col<4;++col){auto cell=s._lstItems->_texts[0][col];cell->setVisible(false);s._lstItems->invalidate();p.draw();check(s.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][col].IsNull(),"hidden cell independently null");cell->setVisible(true);s._lstItems->invalidate();p.draw();cell->getCrop()->w=2;cell->getCrop()->h=2;s._lstItems->invalidate();p.draw();check(s.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][col].IsNull(),"cropped cell independently null");cell->getCrop()->w=cell->getCrop()->h=0;s._lstItems->invalidate();p.draw();}
 check(s._lstItems->agentPurchaseRows(rows),"restore row projection");const Surface*inc=rows[0].increase;const Surface*dec=rows[0].decrease;
 {Surface cover(1,1,inc->getX(),inc->getY());cover.clear(1);cover.blit(&p.canvas);g._cursor=&cover;check(s.agentUiState(ui),"partial arrow overlay keeps other UI");auto x=YAML::Load(ui.visible);check(x["rows"][0]["controls"].size()==1&&x["rows"][0]["controls"][0].as<std::string>()=="decrease","increase overlay withholds only increase");check(!s.agentDispatch(rowRequest("r0","adjust",1)),"hidden direction cannot dispatch");g._cursor=nullptr;}
 {Surface cover(24,8,inc->getX(),inc->getY());cover.clear(1);cover.blit(&p.canvas);g._fpsCounter=&cover;check(s.agentUiState(ui)&&ui.rows[0]==1,"two covered arrows omit row and compact mapping");g._fpsCounter=nullptr;}
 {SDL_Rect clip={0,0,static_cast<Uint16>(dec->getX()),200};SDL_SetClipRect(p.canvas.getSurface(),&clip);p.draw();check(s.agentUiState(ui),"destination clip retains admitted subset");auto x=YAML::Load(ui.visible);check(x["rows"][0]["controls"].size()==1,"destination-clipped arrow withheld independently");SDL_SetClipRect(p.canvas.getSurface(),nullptr);p.draw();}
 g.top=&g;check(!s.agentUiState(ui),"non-top withheld");g.top=&s;s._modal=s._btnOk;check(!s.agentUiState(ui),"unrelated modal withheld");s._modal=nullptr;
 {SDL_Event e={};e.type=SDL_MOUSEMOTION;e.motion.x=20;e.motion.y=58;Action a(&e,1,1,0,0);s._lstItems->handle(&a,&s);p.draw();check(s.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["selected"].as<bool>(),"native selector actually changes presented pixels");auto fullArrow=s._lstItems->_arrowLeft[0];const int arrowX=fullArrow->getX();fullArrow->setX(8);fullArrow->setWidth(287);p.draw();check(s.agentUiState(ui)&&!YAML::Load(ui.visible)["rows"][0]["selected"].as<bool>(),"later opaque arrow cannot masquerade as surviving selector");fullArrow->setWidth(11);fullArrow->setX(arrowX);p.draw();s._cbxCategory->setY(54);p.draw();check(s.agentUiState(ui)&&!YAML::Load(ui.visible)["rows"][0]["selected"].as<bool>(),"moved later combo revokes selected whole-band witness");s._cbxCategory->setY(36);p.draw();}
 // No-op decrement has no presentation or draft side effect, just as ordinary legacy helpers.
 {const auto revision=s._lstItems->agentListRevision();const auto total=s._total;const auto funds=g.save.funds;s._sel=0;s.decreaseByValue(1);check(s._lstItems->agentListRevision()==revision&&s._total==total&&g.save.funds==funds&&!s._timerDec->isRunning(),"ordinary zero decrement does not refresh or mutate");check(s.agentDispatch(rowRequest("r0","adjust",-1))&&s._lstItems->agentListRevision()==revision,"semantic zero decrement does not refresh");check(s.agentDispatch(rowRequest("r0","set",0))&&s._lstItems->agentListRevision()==revision,"equal semantic set has no refresh");}
 // Fresh row mapping is explicit, independent of prior mouse selection.
 s._sel=7;check(s.agentDispatch(rowRequest("r1","adjust",5)),"semantic row adjustment");check(s._items[1].amount==5&&s._items[7].amount==0&&s._sel==1,"fresh mapping changes exact row");check(!s._timerInc->isRunning()&&!s._timerDec->isRunning(),"MCP quantity starts no timers");p.draw();
 check(s.agentDispatch(rowRequest("r1","set",9))&&s._items[1].amount==9,"semantic absolute quantity");p.draw();check(s.agentDispatch(rowRequest("r1","max"))&&s._items[1].amount==40,"max applies ordinary quarter limit");p.draw();check(s.agentDispatch(rowRequest("r1","clear"))&&s._items[1].amount==0,"clear zeroes draft");p.draw();
 for(auto bad: {rowRequest("r0","adjust",0),rowRequest("r0","adjust",1001),rowRequest("r0","set",-1),rowRequest("r0","set",100001),rowRequest("r01","adjust",1)})check(!s.agentDispatch(bad),"invalid semantic request rejected locally");
 {Surface cover(1,1,inc->getX(),inc->getY());cover.clear(1);cover.blit(&p.canvas);g._cursor=&cover;check(!s.agentDispatch(rowRequest("r0","set",0)),"set requires both shown arrows even for same value");check(!s.agentDispatch(rowRequest("r0","max")),"max requires increase");check(s.agentDispatch(rowRequest("r0","clear")),"clear requires only decrease");g._cursor=nullptr;p.draw();}
 // Multiline name has no full text witness but first-line arrows remain admitted.
 const std::string oldName=s._items[0].name;s._lstItems->setWordWrap(true);s._items[0].name="A very long multiline row label that wraps beyond the first physical row";s.updateList();p.draw();check(s._lstItems->getRows()>s._lstItems->getTexts(),"true wrapped physical row fixture");check(s.agentUiState(ui)&&ui.rows[0]==0&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull(),"multiline text unknown with independently painted arrows");s._lstItems->scrollTo(1);p.draw();check(s.agentUiState(ui)&&ui.rows[0]!=0,"scrolled-away first-line arrows are not admitted");s._items[0].name=oldName;s._lstItems->setWordWrap(false);s.updateList();p.draw();
 {TableChannel c(master+"-purchase");connect(c,s);c.read();c.events();c.queue("change_purchase_quantity","r0 adjust 3");complete(c,p);check(s._items[0].amount==3,"controller semantic dispatch exactly once");
 c.read();c.queue("change_purchase_quantity","r0 set 7");s._lstItems->setCellText(0,3,"2");s._lstItems->setCellText(0,3,"3");p.draw();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable"&&s._items[0].amount==3,"content ABA invalidates grant");
 c.read();c.queue("change_purchase_quantity","r0 max");s._lstItems->scrollTo(3);s._lstItems->scrollTo(0);p.draw();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable"&&s._items[0].amount==3,"page ABA invalidates grant");
 c.read();c.queue("change_purchase_quantity","r0 set 5");complete(c,p);check(s._items[0].amount==5,"absolute set through controller");c.read();c.queue("change_purchase_quantity","r0 adjust -2");complete(c,p);check(s._items[0].amount==3,"negative adjust through controller");c.read();c.queue("change_purchase_quantity","r0 max");complete(c,p);check(s._items[0].amount==40,"max through controller uses shared limit");c.read();c.queue("change_purchase_quantity","r0 clear");complete(c,p);check(s._items[0].amount==0,"clear through controller");
 c.read();c.queue("scroll_purchase","down page");complete(c,p);check(s._lstItems->getScroll()>0,"controller semantic page scroll");c.read();c.queue("position_purchase_list","t0");complete(c,p);check(s._lstItems->getScroll()==0,"controller target remaps to top");c.read();c.queue("open_purchase_category");complete(c,p);c.read();check(s.agentUiState(ui)&&ui.screen=="purchase_category","controller actual category state packet");c.queue("close_dropdown");complete(c,p);}
 {TableChannel c(master+"-no-frame");connect(c,s);c.read();c.queue("change_purchase_quantity","r0 adjust 2");c.frame();check(c.receive().empty()&&s._items[0].amount==2&&!s._timerInc->isRunning(),"semantic command takes effect once without later receipt");agentTestTime+=8100;c.controller->poll();const auto receipt=c.receive();check(!receipt.empty()&&YAML::Load(receipt)["status"].as<std::string>()=="unknown_after_dispatch","no later frame gives unchanged controller uncertainty");c.send(c.request);c.controller->poll();check(c.receive()==receipt&&s._items[0].amount==2,"uncertain semantic duplicate never replays");p.draw();check(s.agentDispatch(rowRequest("r0","clear")),"clear no-frame fixture");p.draw();}
 // GUI timing remains a separate adapter into exactly the same quantity module.
 {SDL_Event e={};e.type=SDL_MOUSEMOTION;e.motion.x=238;e.motion.y=58;Action hover(&e,1,1,0,0);s._lstItems->handle(&hover,&s);e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=238;e.button.y=58;Action press(&e,1,1,0,0);s._lstItems->handle(&press,&s);check(s._timerInc->isRunning(),"ordinary GUI press starts timer");advance(s,250);check(s._items[0].amount==1,"ordinary Timer invokes shared quantity once");advance(s,50);check(s._items[0].amount==2,"ordinary repeat invokes shared quantity again");e.type=SDL_MOUSEBUTTONUP;Action release(&e,1,1,0,0);s._lstItems->handle(&release,&s);check(!s._timerInc->isRunning(),"ordinary GUI release ends timer");const int after=s._items[0].amount;advance(s,500);check(s._items[0].amount==after,"GUI timer no repeat after release");p.draw();}
 AgentRequest req;req.action="scroll_purchase";req.direction="down";req.amount="step";check(s.agentDispatch(req)&&s._lstItems->getScroll()==1,"semantic step uses ordinary scroll");p.draw();req.amount="page";check(s.agentDispatch(req)&&s._lstItems->getScroll()>1,"semantic page scroll");p.draw();req.direction="up";req.amount="end";check(s.agentDispatch(req)&&s._lstItems->getScroll()==0,"semantic end top");p.draw();
 req=AgentRequest();req.action="position_purchase_list";req.choice="t2";check(s.agentDispatch(req)&&s._lstItems->getScroll()==s._lstItems->getRows()-s._lstItems->getVisibleRows(),"painted target bottom via ordinary scrollTo");p.draw();req.choice="t1";check(s.agentDispatch(req)&&s._lstItems->getScroll()==(s._lstItems->getRows()-s._lstItems->getVisibleRows())/2,"painted middle target");p.draw();req.choice="t0";check(s.agentDispatch(req)&&s._lstItems->getScroll()==0,"painted top target");p.draw();
 const int prior=s._items[0].amount;req=AgentRequest();req.action="open_purchase_category";check(s.agentDispatch(req),"category toggle");p.draw();check(s.agentUiState(ui)&&ui.screen=="purchase_category"&&ui.offers("choose_visible_row"),"actual dropdown provider");req.action="choose_visible_row";req.choice="r1";check(s.agentDispatch(req)&&s._cbxCategory->getSelected()==1,"category selection and shared change");p.draw();check(s.agentUiState(ui)&&ui.screen=="purchase"&&s._items[0].amount==prior,"category remap preserves draft");req.action="open_purchase_category";req.choice.clear();check(s.agentDispatch(req),"category reopen");p.draw();req.action="close_dropdown";check(s.agentDispatch(req),"category dismiss");p.draw();
 const auto normalFunds=g.save.funds;const int quarters=b.quarters;g.save.funds=0;b.quarters=b.usedQuarters;p.draw();check(s.agentDispatch(rowRequest("r0","adjust",1))&&g.error==std::string(lang.getString("STR_NOT_ENOUGH_MONEY")),"shared money refusal precedes capacity");g.top=&s;g.save.funds=normalFunds;p.draw();check(s.agentDispatch(rowRequest("r0","adjust",1))&&g.error==std::string(lang.getString("STR_NOT_ENOUGH_LIVING_SPACE")),"shared quarter refusal literal");g.top=&s;b.quarters=quarters;p.draw();
 const auto funds=g.save.funds;const auto transfers=b.transfers.size();req=AgentRequest();req.action="cancel_purchase";{TableChannel c(master+"-cancel");connect(c,s);c.read();c.queue("cancel_purchase");complete(c,p);}check(g.top==nullptr&&g.save.funds==funds&&b.transfers.size()==transfers&&s._total==0,"semantic Cancel discards draft without spend or transfer");g.top=&s;p.draw();check(s.agentDispatch(rowRequest("r0","set",3)),"new draft after cancel test seam");p.draw();const int cost=s._total;req.action="accept_purchase";{TableChannel c(master+"-accept");connect(c,s);c.read();c.queue("accept_purchase");complete(c,p);}check(g.save.funds==funds-cost&&b.transfers.size()>transfers,"shared accept charges and creates pending transfers");
 std::cout<<master<<" Purchase semantic provider/controller and GUI adapter PASS\n";
}
}
int main(int argc,char**argv){check(argc==2,"source path");check(!SDL_Init(0)&&SDL_GetVideoSurface()==nullptr,"offscreen SDL only");Options::baseXResolution=320;Options::baseYResolution=200;Options::keyOk=SDLK_RETURN;Options::keyCancel=SDLK_ESCAPE;Options::mousewheelSpeed=3;Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}for(const std::string game:{"xcom1","xcom2"}){Language l;l.loadFile(std::string(argv[1])+"/bin/standard/"+game+"/Language/en-US.yml");run(big,small,l,game);}check(agentPaintHealthy(),"healthy bounded native paint");check(!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"no game/GUI started");std::cout<<"AgentPurchaseSemanticTestV18s PASS "<<checks<<" checks\n";}
