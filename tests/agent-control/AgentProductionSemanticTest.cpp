// SPDX-License-Identifier: GPL-3.0-or-later
// Actual widgets/fonts, exact extracted providers/controller and ordinary
// staffing/commit/remove bodies. Model, owning Game/stack and UI construction
// are explicit test seams. No real Game, simulation, SDL window or RNG runs.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Timer.h"
#include <climits>
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/LocalizedText.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Engine/Unicode.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/ToggleTextButton.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Interface/ArrowButton.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <memory>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include "AgentTableControllerDependencies.generated.h"
#include "AgentTableController.generated.h"
using namespace OpenXcom;
using namespace OpenXcom::Observer;
namespace OpenXcom { namespace Observer { std::uint64_t agentTestTime=10000; bool diagnosticProfileQualified(){return true;} } }
extern "C" Uint8 SDL_GetAppState(){return SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;}
extern "C" Uint32 SDL_GetTicks(){return Uint32(agentTestTime);}
extern "C" SDLMod SDL_GetModState(){return KMOD_NONE;}
extern "C" Uint8 SDL_GetMouseState(int*,int*){return 0;}
extern "C" void SDL_PumpEvents(){}
extern "C" int SDL_PeepEvents(SDL_Event*,int,SDL_eventaction,Uint32){return 0;}
namespace { unsigned checks=0; void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}} }
namespace OpenXcom {
Game *State::_game=nullptr;
State::State():_screen(true),_modal(nullptr),_ruleInterface(nullptr),_ruleInterfaceParent(nullptr),_cursorColor(0){}
State::~State(){}
void State::init(){} void State::blit(){} void State::resize(int&,int&){} void State::recenter(int,int){}
#include "AgentBaseTableWidgetState.generated.inc"
#include "AgentBaseTableGameIdentity.generated.inc"
}
namespace Projects {
struct RuleManufacture { std::string name="STR_MEDI_KIT",category="STR_EQUIPMENT"; int cost=28000,space=4; std::map<std::string,int> required; const std::string &getName()const{return name;} const std::string &getCategory()const{return category;} int getRequiredSpace()const{return space;} int getManufactureCost()const{return cost;} const std::map<std::string,int>&getRequiredItems()const{return required;} };
struct CraftRule { std::string getType()const{return "CRAFT";} };
struct Craft { CraftRule rule; CraftRule *getRules(){return &rule;} };
struct ItemContainer { std::map<std::string,int> items; void removeItem(const std::string &id,int n){items[id]-=n;} };
struct Interface { int color=1; Interface *getElement(const std::string&){return this;} };
struct Mod { Interface interface; int item; const int *getItem(const std::string&)const{return &item;} const int *getCraft(const std::string&)const{return nullptr;} Interface *getInterface(const std::string&){return &interface;} };
struct SavedGame { long long funds=6000000; long long getFunds()const{return funds;} void setFunds(long long value){funds=value;} };
struct Base;
struct Production { const RuleManufacture *_rules; int assigned=0,amount=1,produced=0; bool sold=false,infinite=false; explicit Production(const RuleManufacture *r,int n=1):_rules(r),amount(n){} int getAssignedEngineers()const{return assigned;} void setAssignedEngineers(int n){assigned=n;} bool getInfiniteAmount()const{return infinite;} void setInfiniteAmount(bool v){infinite=v;} const RuleManufacture *getRules()const{return _rules;} int getAmountProduced()const{return produced;} void setAmountTotal(int n){amount=n;} int getAmountTotal()const{return amount;} bool getSellItems()const{return sold;} void setSellItems(bool v){sold=v;} void startItem(Base*,SavedGame*,const Mod*)const; };
struct Base { int _engineers=10,freeSpace=20,hangars=1,usedHangars=0; std::vector<Production*> _productions; std::vector<Craft*> crafts; ItemContainer storage; int getAvailableEngineers()const{return _engineers;} int getEngineers()const{return _engineers;} void setEngineers(int n){_engineers=n;} int getFreeWorkshops()const{int n=freeSpace;for(auto p:_productions)n-=p->assigned;return n;} int getAvailableHangars()const{return hangars;} int getUsedHangars()const{return usedHangars;} std::vector<Production*>getProductions()const{return _productions;} ItemContainer*getStorageItems(){return &storage;} std::vector<Craft*>*getCrafts(){return &crafts;} void removeCraft(Craft*p,bool){crafts.erase(std::remove(crafts.begin(),crafts.end(),p),crafts.end());} void removeProduction(Production*); };
struct Game { const void *top=nullptr; Surface *_cursor=nullptr,*_fpsCounter=nullptr; unsigned pops=0,pushes=0; SavedGame save; Mod mod; std::string pushed; bool isState(const void*p)const{return top==p;} void popState(){++pops;top=nullptr;} template<class T>void pushState(T*p){++pushes;pushed=p->kind;top=p;} SavedGame *getSavedGame(){return &save;} Mod*getMod(){return &mod;} bool agentOverlayClear(const AgentUiState&)const; };
#include "AgentBaseTableOverlay.generated.inc"
#include "ProductionCommit.generated.inc"
#include "BaseRemoveProduction.generated.inc"
#include "ProductionCommands.generated.inc"
struct UI : OpenXcom::State { Game *_game=nullptr; using State::_modal; using State::_surfaces; Language *language=nullptr; SDL_Color *_palette=nullptr; std::string kind; LocalizedText tr(const std::string&id)const{return language->getString(id);} };
struct ManufactureInfoState: UI {
 std::uint64_t _agentProjectRevision=0; Base *_base; RuleManufacture *_item; Production *_production;
 ProductionDraft _productionDraft;
 TextButton *_btnOk=nullptr,*_btnStop=nullptr; ToggleTextButton *_btnSell=nullptr;
 ArrowButton *_btnEngineerUp=nullptr,*_btnEngineerDown=nullptr,*_btnUnitUp=nullptr,*_btnUnitDown=nullptr;
 Timer *_timerMoreEngineer=nullptr,*_timerLessEngineer=nullptr,*_timerMoreUnit=nullptr,*_timerLessUnit=nullptr;
 Text *_txtTitle=nullptr,*_txtAvailableEngineer=nullptr,*_txtAvailableSpace=nullptr,*_txtMonthlyProfit=nullptr,*_txtAllocated=nullptr,*_txtTodo=nullptr,*_txtEngineerUp=nullptr,*_txtEngineerDown=nullptr,*_txtUnitUp=nullptr,*_txtUnitDown=nullptr;
 ManufactureInfoState(Base*b,RuleManufacture*r):_base(b),_item(r),_production(nullptr){kind="info-new";}
 ManufactureInfoState(Base*b,Production*p):_base(b),_item(nullptr),_production(p){kind="info-existing";}
 bool agentUiState(AgentUiState&)const; bool agentDispatch(const AgentRequest&); void moreEngineer(int);void lessEngineer(int);void setAssignedEngineer();void btnOkClick(Action*);void btnStopClick(Action*);void exitState();int getMonthlyNetFunds()const{return 0;}
 void applyQuantityResult(const ProductionCommandResult&);void moreUnit(int);void lessUnit(int);void btnSellPress(Action*);void btnSellClick(Action*);
 void moreUnitClick(Action*);void lessUnitClick(Action*);void handleWheelUnit(Action*);void handleWheelEngineer(Action*);void onMoreUnit();void onLessUnit();void onMoreEngineer();void onLessEngineer();void think();
 void moreUnitPress(Action*);void moreUnitRelease(Action*);
};
struct ManufactureStartState: UI { Base *_base; RuleManufacture *_item; TextButton *_btnStart=nullptr,*_btnCancel=nullptr; Text *_txtTitle=nullptr,*_txtManHour=nullptr,*_txtCost=nullptr,*_txtWorkSpace=nullptr,*_txtRequiredItemsTitle=nullptr,*_txtItemNameColumn=nullptr,*_txtUnitRequiredColumn=nullptr,*_txtUnitAvailableColumn=nullptr; TextList *_lstRequiredItems=nullptr; ManufactureStartState(Base*b,RuleManufacture*r):_base(b),_item(r){kind="start";} bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnStartClick(Action*);void btnCancelClick(Action*); };
struct ErrorMessageState: UI { ErrorMessageState(LocalizedText,SDL_Color*,int,const char*,int){kind="error";} };
struct NewManufactureListState: UI { Base *_base; std::uint64_t _agentMappingRevision=1; std::vector<RuleManufacture*> _possibleProductions;std::vector<std::string> _displayedStrings;TextButton *_btnOk=nullptr;Text *_txtTitle=nullptr,*_txtItem=nullptr,*_txtCategory=nullptr;TextList *_lstManufacture=nullptr;explicit NewManufactureListState(Base*b):_base(b){kind="new-list";}bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnOkClick(Action*);void selectProduction(size_t); };
struct ManufactureState: UI { Base *_base;std::uint64_t _agentMappingRevision=1;std::vector<Production*> _agentProjects;TextButton *_btnNew=nullptr,*_btnOk=nullptr;Text *_txtTitle=nullptr,*_txtAvailable=nullptr,*_txtAllocated=nullptr,*_txtSpace=nullptr,*_txtFunds=nullptr,*_txtItem=nullptr,*_txtEngineers=nullptr,*_txtProduced=nullptr,*_txtCost=nullptr,*_txtTimeLeft=nullptr;TextList *_lstManufacture=nullptr;explicit ManufactureState(Base*b):_base(b){}bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnOkClick(Action*);void btnNewProductionClick(Action*);void selectProduction(size_t); };
#include "ManufactureMethods.generated.inc"
#include "NewManufactureMethods.generated.inc"
#include "ManufactureStartMethods.generated.inc"
#include "ManufactureInfoMethods.generated.inc"
struct LegacyInfo : UI { Base *_base;Production *_production;RuleManufacture *_item=nullptr;Timer *_timerMoreUnit;ToggleTextButton *_btnSell=nullptr;unsigned refreshes=0;
 void setAssignedEngineer(){++refreshes;}void moreEngineer(int);void lessEngineer(int);void moreUnit(int);void lessUnit(int);void moreUnitClick(Action*);void lessUnitClick(Action*);void btnOkClick(Action*);void btnStopClick(Action*);void exitState(); };
#include "LegacyProductionOracle.generated.inc"

}
namespace {
#include "AgentManufactureChannel.inc"
struct Paint {
 Font &big,&small; Language &language; Surface canvas,background; SDL_Color palette[256]; std::vector<std::unique_ptr<Surface>>owned;std::vector<Surface*>surfaces;
 Paint(Font&b,Font&s,Language&l):big(b),small(s),language(l),canvas(320,200),background(320,200){for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);surfaces.push_back(&background);}
 template<class T>T*own(T*p){p->setPalette(palette);owned.emplace_back(p);surfaces.push_back(p);return p;}
 Text *text(int w,int h,int x,int y,const std::string&value,bool large=false){auto t=own(new Text(w,h,x,y));t->initText(&big,&small,&language);t->setColor(80);if(large)t->setBig();t->setText(value);return t;}
 TextButton *button(int w,int x,int y,const std::string&key){auto b=own(new TextButton(w,16,x,y));b->initText(&big,&small,&language);b->setText(language.getString(key));return b;}
 TextList *list(int w,int h,int x,int y){auto l=own(new TextList(w,h,x,y));l->initText(&big,&small,&language);l->setColor(80);l->setBackground(&background);return l;}
 ArrowButton *arrow(ArrowShape shape,int x,int y){auto a=own(new ArrowButton(shape,13,14,x,y));a->setColor(80);return a;}
 void draw(){for(auto s:surfaces)s->blit(&canvas);}
 std::string tr(const std::string&key){return language.getString(key);}
};
template<class S>void connect(TableChannel&c,S&state,Projects::Game&g){c.state.project=[&](AgentUiState&ui){return state.agentUiState(ui);};c.state.handler=[&](const AgentRequest&r){check(state.agentDispatch(r),"real dispatcher consumes offered action");if(g.top!=&state)c.game.top=&c.unsupportedParent;};}
template<class S>void finishCommand(TableChannel&c,S&,Paint&p){c.frame();check(c.receive().empty(),"no pre-action frame result");p.draw();c.frame();auto result=c.receive();check(!result.empty()&&YAML::Load(result)["status"].as<std::string>()=="dispatched","later frame supplies dispatched receipt");c.send(c.request);c.controller->poll();check(c.receive()==result,"duplicate returns exact cached receipt");}
template<class S>void overlayTests(S&state,Paint&p,Projects::Game&g,Text*title,const std::string&guarded){
 AgentUiState ui;check(state.agentUiState(ui),"painted provider positive");auto original=ui;g.top=&g;check(!state.agentUiState(ui),"non-top blocks");g.top=&state;state._modal=title;check(!state.agentUiState(ui),"modal blocks");state._modal=nullptr;
 SDL_Rect ink={};check(title->agentPaintedBounds(ink),"title ink exists");Surface cover(1,1,ink.x,ink.y);cover.clear(1);cover.blit(&p.canvas);g._cursor=&cover;check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].IsNull()&&!ui.offers(guarded),"covered title cannot authorize project mutation");check(g.agentOverlayClear(ui),"regional withholding preserves global guard");g._cursor=nullptr;
 Surface full(320,200);full.clear(1);full.blit(&p.canvas);g._fpsCounter=&full;check(!state.agentUiState(ui),"full overlay no projection");g._fpsCounter=nullptr;p.draw();check(state.agentUiState(ui)&&ui.sameBinding(original),"unchanged actual paint restores equivalent projection");
}
void listTests(Font&big,Font&small,Language&lang,const std::string&master,bool current){
 using namespace Projects;Paint p(big,small,lang);Projects::Game g;Base b;RuleManufacture rule;Production prod(&rule);b._productions.push_back(&prod);
 auto title=p.text(current?310:320,17,current?5:0,current?8:30,p.tr(current?"STR_CURRENT_PRODUCTION":"STR_PRODUCTION_ITEMS"),true);title->setAlign(ALIGN_CENTER);
 auto close=p.button(current?148:304,current?164:8,current?176:154,"STR_OK");
 auto list=p.list(288,current?88:80,8,current?80:70);list->setSelectable(true);list->setMargin(2);if(current)list->setColumns(5,115,15,52,56,48);else list->setColumns(2,156,130);
 Text *head[5];const char*ids[]={"STR_ITEM","STR_ENGINEERS_ALLOCATED","STR_UNITS_PRODUCED","STR_COST_PER_UNIT","STR_DAYS_HOURS_LEFT"};
 for(int i=0;i<(current?5:2);++i){head[i]=p.text(current?(i==0?80:48):(i?130:156),current?27:9,current?(i==0?10:112+(i-1)*50):(i?166:10),current?44:62,p.tr(current?ids[i]:(i?"STR_CATEGORY":"STR_ITEM")));head[i]->setWordWrap(current);}
 const auto name=p.tr("STR_MEDI_KIT");if(current)list->addRow(5,name.c_str(),"0","0/1","$28,000","-");else list->addRow(2,name.c_str(),p.tr("STR_EQUIPMENT").c_str());
 if(current){
  ManufactureState s(&b);s._game=&g;g.top=&s;s._txtTitle=title;s._btnOk=close;s._lstManufacture=list;s._agentProjects={&prod};s._txtItem=head[0];s._txtEngineers=head[1];s._txtProduced=head[2];s._txtCost=head[3];s._txtTimeLeft=head[4];
  s._txtAvailable=p.text(150,9,8,24,lang.getString("STR_ENGINEERS_AVAILABLE").arg(10));s._txtAllocated=p.text(150,9,160,24,lang.getString("STR_ENGINEERS_ALLOCATED").arg(0));s._txtSpace=p.text(150,9,8,34,lang.getString("STR_WORKSHOP_SPACE_AVAILABLE").arg(20));s._txtFunds=p.text(150,9,160,34,lang.getString("STR_CURRENT_FUNDS").arg("$6,000,000"));s._btnNew=p.button(148,8,176,"STR_NEW_PRODUCTION");s._surfaces=p.surfaces;p.draw();AgentUiState ui;check(s.agentUiState(ui),"current Manufacture provider");check(ui.rows.size()==1&&ui.offers("select_production_project"),"actual five-column Medi-Kit row selectable");auto v=YAML::Load(ui.visible);check(v["rows"][0]["cells"].size()==5&&v["rows"][0]["cells"][0].as<std::string>()==name,"five exact shown cells");overlayTests(s,p,g,title,"select_production_project");
  list->setCellText(0,4,std::string(100,'W'));p.draw();check(s.agentUiState(ui)&&ui.rows.empty()&&!ui.offers("select_production_project"),"clipped fifth cell cannot authorize whole row");list->setCellText(0,4,"-");p.draw();
  TableChannel c(master+"-manufacture");connect(c,s,g);c.read();c.events();c.queue("select_production_project","r0");++s._agentMappingRevision;c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable"&&g.pushes==0,"mapping ABA rejects stale row");c.read();c.queue("select_production_project","r0");finishCommand(c,s,p);check(g.pushes==1&&g.pushed=="info-existing","ordinary existing row handler once");
 }else{
  NewManufactureListState s(&b);s._game=&g;g.top=&s;s._txtTitle=title;s._btnOk=close;s._txtItem=head[0];s._txtCategory=head[1];s._lstManufacture=list;s._possibleProductions={&rule};s._displayedStrings={rule.name};s._surfaces=p.surfaces;p.draw();AgentUiState ui;check(s.agentUiState(ui)&&ui.rows.size()==1&&ui.offers("select_production_project"),"actual two-column new-production row selectable");overlayTests(s,p,g,title,"select_production_project");
  AgentUiState legacy;AgentListProjection old;check(agentProjectList(list,list,agentFlattenPaintOrder(s._surfaces),2,true,legacy,old)&&legacy.rows.empty(),"old helper keeps multicolumn selectable disabled by default");
  list->setCellText(0,1,std::string(200,'W'));p.draw();check(s.agentUiState(ui)&&ui.rows.empty()&&!ui.offers("select_production_project"),"clipped second column cannot authorize row");list->setCellText(0,1,p.tr("STR_EQUIPMENT"));p.draw();
  TableChannel c(master+"-new-manufacture");connect(c,s,g);c.read();c.events();c.queue("select_production_project","r0");finishCommand(c,s,p);check(g.pushes==1&&g.pushed=="start","ordinary new row handler once");
 }
 b._productions.clear();std::cout<<master<<" columns="<<(current?5:2)<<" rows=1 PASS\n";
}
void infoTests(Font&big,Font&small,Language&lang,const std::string&master,bool fresh){
 using namespace Projects;Paint p(big,small,lang);Projects::Game g;Base b;RuleManufacture rule;auto prod=new Production(&rule);b._productions.push_back(prod);
 ManufactureInfoState s(&b,prod);if(fresh)s._item=&rule;s._game=&g;s.language=&lang;g.top=&s;
 s._txtTitle=p.text(320,17,0,30,p.tr("STR_MEDI_KIT"),true);s._txtTitle->setAlign(ALIGN_CENTER);
 s._txtAvailableEngineer=p.text(160,9,16,50,"");s._txtAvailableSpace=p.text(160,9,16,60,"");s._txtMonthlyProfit=p.text(160,9,168,50,"");
 auto allocatedLabel=p.text(112,32,16,80,p.tr("STR_ENGINEERS__ALLOCATED"),true);allocatedLabel->setWordWrap(true);allocatedLabel->setVerticalAlign(ALIGN_BOTTOM);
 auto unitsLabel=p.text(112,48,168,64,p.tr("STR_UNITS_TO_PRODUCE"),true);unitsLabel->setWordWrap(true);unitsLabel->setVerticalAlign(ALIGN_BOTTOM);
 s._txtAllocated=p.text(40,16,128,88,"",true);s._txtTodo=p.text(40,16,280,88,"",true);
 s._txtEngineerUp=p.text(90,9,40,118,p.tr("STR_INCREASE_UC"));s._txtEngineerDown=p.text(90,9,40,138,p.tr("STR_DECREASE_UC"));s._btnEngineerUp=p.arrow(ARROW_BIG_UP,132,114);s._btnEngineerDown=p.arrow(ARROW_BIG_DOWN,132,136);
 s._txtUnitUp=p.text(90,9,192,118,p.tr("STR_INCREASE_UC"));s._txtUnitDown=p.text(90,9,192,138,p.tr("STR_DECREASE_UC"));s._btnUnitUp=p.arrow(ARROW_BIG_UP,284,114);s._btnUnitDown=p.arrow(ARROW_BIG_DOWN,284,136);
 s._btnOk=p.button(136,168,155,"STR_OK");s._btnStop=p.button(136,16,155,"STR_STOP_PRODUCTION");s._btnSell=p.own(new ToggleTextButton(60,16,244,61));s._btnSell->initText(&big,&small,&lang);s._btnSell->setText(p.tr("STR_SELL_PRODUCTION"));
 Timer te(250),tl(250),tu(250),td(250);s._timerMoreEngineer=&te;s._timerLessEngineer=&tl;s._timerMoreUnit=&tu;s._timerLessUnit=&td;
 tu.onTimer((StateHandler)&ManufactureInfoState::onMoreUnit);td.onTimer((StateHandler)&ManufactureInfoState::onLessUnit);
 s._btnSell->onMousePress((ActionHandler)&ManufactureInfoState::btnSellPress,0);s._btnSell->onMouseClick((ActionHandler)&ManufactureInfoState::btnSellClick,0);
 s._btnOk->onKeyboardPress((ActionHandler)&ManufactureInfoState::btnOkClick,SDLK_RETURN);
 s._surfaces=p.surfaces;s.setAssignedEngineer();p.draw();AgentUiState ui;check(s.agentUiState(ui)&&ui.offers("adjust_engineers")&&ui.offers("accept_production")&&ui.offers("stop_production"),"actual Info layout offers staffing and actual OK/Stop");
 check(YAML::Load(ui.visible)["allocated"].as<std::string>()==std::string(">\x01" "0"),"allocation is exact rendered formatted string");overlayTests(s,p,g,s._txtTitle,"adjust_engineers");
 TableChannel c(master+(fresh?"-production-new":"-production-existing"));connect(c,s,g);c.read();c.events();
 auto execute=[&](const std::string &args){c.read();c.queue("change_production_quantity",args);finishCommand(c,s,p);};
 execute("units adjust -1");check(prod->amount==1,"semantic units floor no-effect");
 execute("units adjust 1");check(prod->amount==2,"semantic units +1");
 execute("units max");check(prod->infinite,"semantic max requests ordinary infinity");
 execute("units adjust 1");check(prod->infinite&&prod->amount==2,"semantic positive adjust at infinity no-effect");
 execute("units set 2");check(!prod->infinite&&prod->amount==2,"semantic finite set exits infinity");
 execute("units clear");check(prod->amount==1,"semantic clear means ordinary produced+1 minimum");
 execute("engineers max");check(prod->assigned==10&&b._engineers==0,"semantic max respects available engineers");
 execute("engineers adjust 1");check(prod->assigned==10,"semantic request at capacity dispatched no-effect");
 execute("engineers clear");check(prod->assigned==0&&b._engineers==10,"semantic clear returns engineers");
 execute("engineers set 3");check(prod->assigned==3&&b._engineers==7,"semantic bounded set allocates three");
 execute("engineers adjust -1");check(prod->assigned==2&&b._engineers==8,"semantic signed delta removes one");
 execute("engineers clear");
 c.read();c.queue("set_production_sell","true");finishCommand(c,s,p);check(s._productionDraft.sell&&s._btnSell->getPressed()&&!prod->sold,"explicit Sell changes only shared draft until commit");
 c.read();c.queue("set_production_sell","false");finishCommand(c,s,p);check(!s._productionDraft.sell&&!s._btnSell->getPressed(),"explicit Sell false is idempotent draft intent");
 // Ordinary real ToggleTextButton press runs before the GUI callback. Release
 // outside does not click, so its painted toggle remains the authoritative draft.
 for(unsigned button:{unsigned(SDL_BUTTON_LEFT),unsigned(SDL_BUTTON_RIGHT)}){
  SDL_Event down={};down.type=SDL_MOUSEBUTTONDOWN;down.button.button=button;down.button.x=s._btnSell->getX()+3;down.button.y=s._btnSell->getY()+3;const auto revisionBeforePress=s._agentProjectRevision;Action press(&down,1,1,0,0);s._btnSell->handle(&press,&s);
  check(s._productionDraft.sell==s._btnSell->getPressed(),"real Sell mousePress synchronizes explicit draft");
  SDL_Event up={};up.type=SDL_MOUSEBUTTONUP;up.button.button=button;up.button.x=0;up.button.y=0;Action release(&up,1,1,0,0);s._btnSell->handle(&release,&s);
  check(s._productionDraft.sell==s._btnSell->getPressed()&&s._agentProjectRevision==revisionBeforePress,"actual outside release preserves press draft without click refresh");
 }
 // Existing-project keyboard OK after a press released outside preserves
 // the ordinary draft toggle even though no mouse-click refresh occurred.
 if(!std::getenv("OPENXCOM_TEST_SKIP_LEGACY_KEYBOARD")){SDL_Event down={};down.type=SDL_MOUSEBUTTONDOWN;down.button.button=SDL_BUTTON_LEFT;down.button.x=s._btnSell->getX()+3;down.button.y=s._btnSell->getY()+3;Action press(&down,1,1,0,0);s._btnSell->handle(&press,&s);SDL_Event up={};up.type=SDL_MOUSEBUTTONUP;up.button.button=SDL_BUTTON_LEFT;Action release(&up,1,1,0,0);s._btnSell->handle(&release,&s);auto item=s._item;s._item=nullptr;const auto fundsBefore=g.save.funds;SDL_Event key={};key.type=SDL_KEYDOWN;key.key.keysym.sym=SDLK_RETURN;Action keyboard(&key,1,1,0,0);s._btnOk->handle(&keyboard,&s);check(prod->sold&&g.pops==1&&g.save.funds==fundsBefore,"ordinary keyboard OK commits press-only Sell draft with no new-unit charge");s._item=item;g.top=&s;g.pops=0;prod->sold=false;ProductionCommands::setSell(s._productionDraft,false);s._btnSell->setPressed(false);}
 // Timer/wheel translation uses the raw ordinary delta, which intentionally
 // preserves infinity. MCP uses semantic finite/max/clear operations instead.
 prod->infinite=true;prod->amount=2;SDL_Event e={};e.type=SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_LEFT;Action a(&e,1,1,0,0);
 s.moreUnitClick(&a);check(prod->amount==2&&prod->infinite,"GUI left click at infinity stays no-effect");
 Options::changeValueByMouseWheel=1;e.button.button=SDL_BUTTON_WHEELUP;s.handleWheelUnit(&a);check(prod->amount==3&&prod->infinite,"GUI wheel retains ordinary infinity flag");
 const auto start=agentTestTime;tu.start();agentTestTime=start+251;s.think();check(prod->amount==4&&prod->infinite,"ordinary main-loop Timer tick invokes raw shared quantity");tu.stop();
 prod->infinite=false;prod->amount=1;s.setAssignedEngineer();p.draw();c.read();
 c.queue("adjust_engineers","decrease");finishCommand(c,s,p);check(prod->assigned==0&&b._engineers==10,"ordinary zero decrease is dispatched no-effect");
 c.read();c.queue("adjust_engineers","increase");finishCommand(c,s,p);check(prod->assigned==1&&b._engineers==9,"exact one engineer allocated, duplicate neutral");
 c.read();c.queue("adjust_engineers","decrease");finishCommand(c,s,p);check(prod->assigned==0&&b._engineers==10,"exact one engineer returned");
 c.read();c.queue("adjust_engineers","increase");
 Surface cover(1,1,s._btnEngineerUp->getX(),s._btnEngineerUp->getY());cover.clear(1);cover.blit(&p.canvas);g._cursor=&cover;c.frame();auto rejected=c.receive();check(!rejected.empty()&&YAML::Load(rejected)["status"].as<std::string>()=="unavailable"&&prod->assigned==0,"overlay between read/execute rejects without staffing");g._cursor=nullptr;p.draw();
 c.read();c.queue("adjust_engineers","increase");finishCommand(c,s,p);check(prod->assigned==1,"fresh recovered grant allocates exactly one");
 c.read();c.queue("adjust_engineers","increase");s.moreEngineer(1);s.lessEngineer(1);p.draw();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable"&&prod->assigned==1,"staff away-back revision rejects queued stale grant");
 b._engineers=0;s.setAssignedEngineer();p.draw();c.read();c.queue("adjust_engineers","increase");finishCommand(c,s,p);check(prod->assigned==1&&b._engineers==0,"ordinary no-available-engineer boundary is no-effect");b._engineers=9;s.setAssignedEngineer();p.draw();
 b.freeSpace=1;s.setAssignedEngineer();p.draw();c.read();c.queue("adjust_engineers","increase");finishCommand(c,s,p);check(prod->assigned==1&&b._engineers==9,"ordinary full-workshop boundary is no-effect");b.freeSpace=20;s.setAssignedEngineer();p.draw();
 const auto funds=g.save.funds;c.read();c.queue("accept_production");finishCommand(c,s,p);check(g.save.funds==funds-(fresh?28000:0)&&g.pops==(fresh?2u:1u),"new OK charges once/pop2, existing OK no charge/pop1");c.events();
 // New independent opened Info after paid acceptance: Stop is ordinary removal,
 // releasing staff but never refunding the completed first-unit charge.
 g.top=&s;s._item=nullptr;g.pops=0;const auto paid=g.save.funds;p.draw();TableChannel stop(master+(fresh?"-stop-paid-new":"-stop-existing"));connect(stop,s,g);stop.read();stop.queue("stop_production");finishCommand(stop,s,p);check(b._productions.empty()&&b._engineers==10&&g.save.funds==paid&&g.pops==1,"ordinary Stop releases engineers with no spent-cost refund");
}
void startTests(Font&big,Font&small,Language&lang,const std::string&master){
 using namespace Projects;Paint p(big,small,lang);Projects::Game g;Base b;RuleManufacture rule;ManufactureStartState s(&b,&rule);s._game=&g;s.language=&lang;g.top=&s;
 s._txtTitle=p.text(320,17,0,30,p.tr("STR_MEDI_KIT"),true);s._txtTitle->setAlign(ALIGN_CENTER);s._txtManHour=p.text(290,9,16,50,lang.getString("STR_ENGINEER_HOURS_TO_PRODUCE_ONE_UNIT").arg(420));s._txtCost=p.text(290,9,16,60,lang.getString("STR_COST_PER_UNIT_").arg("$28,000"));s._txtWorkSpace=p.text(290,9,16,70,lang.getString("STR_WORK_SPACE_REQUIRED").arg(4));
 s._btnCancel=p.button(136,16,155,"STR_CANCEL_UC");s._txtRequiredItemsTitle=p.text(290,9,16,84,p.tr("STR_SPECIAL_MATERIALS_REQUIRED"));s._txtItemNameColumn=p.text(60,16,30,92,p.tr("STR_ITEM_REQUIRED"));s._txtUnitRequiredColumn=p.text(60,16,155,92,p.tr("STR_UNITS_REQUIRED"));s._txtUnitAvailableColumn=p.text(60,16,230,92,p.tr("STR_UNITS_AVAILABLE"));for(auto t:{s._txtItemNameColumn,s._txtUnitRequiredColumn,s._txtUnitAvailableColumn})t->setWordWrap(true);
 s._lstRequiredItems=p.list(270,40,30,108);s._lstRequiredItems->setColumns(3,140,75,55);s._btnStart=p.button(136,168,155,"STR_START_PRODUCTION");s._surfaces=p.surfaces;
 for(auto w:std::vector<Surface*>{s._txtRequiredItemsTitle,s._txtItemNameColumn,s._txtUnitRequiredColumn,s._txtUnitAvailableColumn,s._lstRequiredItems})w->setVisible(false);
 p.draw();AgentUiState ui;check(s.agentUiState(ui)&&ui.offers("start_production")&&YAML::Load(ui.visible)["rows"].size()==0&&YAML::Load(ui.visible)["materials_heading"].IsNull(),"ordinary no-material Start detail explicitly empty/hidden");overlayTests(s,p,g,s._txtTitle,"start_production");
 s._btnStart->setVisible(false);p.draw();AgentRequest req;req.action="start_production";check(s.agentUiState(ui)&&!ui.offers(req.action)&&!s.agentDispatch(req),"hidden ordinary Start never invents authority or reason");s._btnStart->setVisible(true);
 for(auto w:std::vector<Surface*>{s._txtRequiredItemsTitle,s._txtItemNameColumn,s._txtUnitRequiredColumn,s._txtUnitAvailableColumn,s._lstRequiredItems})w->setVisible(true);
 s._lstRequiredItems->addRow(3,p.tr("STR_MEDI_KIT").c_str(),"1","2");p.draw();check(s.agentUiState(ui)&&ui.rows.size()==1&&!YAML::Load(ui.visible)["rows"][0]["selected"].as<bool>(),"actual three-column displayed materials read-only");
 {TableChannel c(master+"-start-cancel");connect(c,s,g);c.read();c.events();c.queue("cancel_production_start");finishCommand(c,s,p);check(g.pops==1&&g.pushes==0&&g.save.funds==6000000,"ordinary Start Cancel is side-effect-free one pop");}
 for(int mode=0;mode<3;++mode){g.top=&s;g.pushes=0;g.pops=0;rule.category=mode==1?"STR_CRAFT":"STR_EQUIPMENT";b.hangars=0;b.freeSpace=mode==2?1:20;p.draw();TableChannel c(master+"-start-mode"+std::to_string(mode));connect(c,s,g);c.read();c.queue("start_production");finishCommand(c,s,p);check(g.pushed==(mode?"error":"info-new"),"ordinary Start normal success or hangar/workspace error");check(g.save.funds==6000000,"Start itself does not charge first unit");}
}

void commandParity(){
 using namespace Projects;
 for(bool craft:{false,true})for(bool infinite:{false,true})for(int assigned:{0,1,9})for(int available:{0,1,12})for(int free:{0,1,20})for(int amount:{0,1,2,100,INT_MAX})for(int produced:{0,1,10})for(int delta:{-1000,-1,0,1,1000,INT_MAX}){
  RuleManufacture rule;rule.category=craft?"STR_CRAFT":"STR_EQUIPMENT";Production old(&rule),now(&rule);old.assigned=now.assigned=assigned;old.amount=now.amount=amount;old.produced=now.produced=produced;old.infinite=now.infinite=infinite;
  Base bo,bn;bo._engineers=bn._engineers=available;bo.freeSpace=bn.freeSpace=free;bo.hangars=bn.hangars=free;Projects::Game go;Timer timer(250);LegacyInfo ref;ref._base=&bo;ref._production=&old;ref._game=&go;ref._timerMoreUnit=&timer;Language language;ref.language=&language;
  auto out=ProductionCommands::rawUnits(bn,now,delta);if(delta>0)ref.moreUnit(delta);else if(delta<0)ref.lessUnit(-delta);
  check(old.amount==now.amount&&old.infinite==now.infinite&&ref.refreshes==unsigned(out.refresh)&&go.pushes==unsigned(out.warning==ProductionWarning::NoFreeHangars),"raw units equal frozen ordinary helper including floor/infinity/overflow boundary");
  ref.refreshes=0;out=ProductionCommands::rawEngineers(bn,now,delta);if(delta>0)ref.moreEngineer(delta);else if(delta<0)ref.lessEngineer(-delta);
  check(old.assigned==now.assigned&&bo._engineers==bn._engineers&&ref.refreshes==unsigned(out.refresh),"raw staffing equals frozen ordinary helper constraints");
 }
 for(bool craft:{false,true})for(bool infinite:{false,true})for(int amount:{0,1,2,100,INT_MAX})for(int produced:{0,1,10})for(int free:{0,1,5})for(int direction:{-1,1})for(int button:{SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT}){
  RuleManufacture rule;rule.category=craft?"STR_CRAFT":"STR_EQUIPMENT";Production old(&rule),now(&rule);old.amount=now.amount=amount;old.produced=now.produced=produced;old.infinite=now.infinite=infinite;Base bo,bn;bo.hangars=bn.hangars=free;Projects::Game go;Timer timer(250);LegacyInfo ref;ref._base=&bo;ref._production=&old;ref._game=&go;ref._timerMoreUnit=&timer;Language language;ref.language=&language;
  SDL_Event event={};event.type=SDL_MOUSEBUTTONUP;event.button.button=button;Action action(&event,1,1,0,0);if(direction>0)ref.moreUnitClick(&action);else ref.lessUnitClick(&action);
  auto operation=button==SDL_BUTTON_RIGHT?(direction>0?QuantityOperation::Maximum:QuantityOperation::Clear):QuantityOperation::Adjust;
  auto out=ProductionCommands::quantity(bn,now,ProductionControl::Units,operation,direction);
  check(old.amount==now.amount&&old.infinite==now.infinite&&ref.refreshes==unsigned(out.refresh)&&go.pushes==unsigned(out.warning==ProductionWarning::NoFreeHangars),"semantic click/min/max equals independently extracted old click path");
 }
 // New finite set composes ordinary clear-infinity and bounded delta; a
 // capacity refusal still refreshes its changed mode, without invented success.
 {RuleManufacture rule;rule.category="STR_CRAFT";Production p(&rule);p.infinite=true;Base b;b.hangars=0;auto out=ProductionCommands::quantity(b,p,ProductionControl::Units,QuantityOperation::Set,3);check(!p.infinite&&p.amount==1&&out.refresh&&out.warning==ProductionWarning::NoFreeHangars,"set at infinity plus hangar refusal preserves finite mode and warning");}
 // Prior OK and Stop bodies are the oracle, not the new wrapper calling itself.
 for(bool fresh:{false,true})for(bool sell:{false,true}){
  RuleManufacture rule;Base bo,bn;auto old=new Production(&rule);auto now=new Production(&rule);old->assigned=now->assigned=2;bo._engineers=bn._engineers=8;bo._productions={old};bn._productions={now};Projects::Game go,gn;LegacyInfo ref;ref._base=&bo;ref._production=old;ref._item=fresh?&rule:nullptr;ref._game=&go;ToggleTextButton toggle(60,16,0,0);toggle.setPressed(sell);ref._btnSell=&toggle;ProductionDraft draft;ProductionCommands::setSell(draft,sell);
  ref.btnOkClick(nullptr);ProductionCommands::accept(bn,*now,gn.save,&gn.mod,draft,fresh);check(go.save.funds==gn.save.funds&&old->sold==now->sold&&go.pops==unsigned(fresh?2:1),"shared accept matches old charge and Sell mutation");
  ref._item=nullptr;ref.btnStopClick(nullptr);ProductionCommands::stop(bn,*now);check(bo._engineers==bn._engineers&&bo._productions.empty()&&bn._productions.empty()&&go.save.funds==gn.save.funds,"shared Stop matches ordinary removal without refund");
 }
}
void parserTests(){AgentRequest r;const std::string p="E "+token(1)+" 1 "+token(2)+" "+token(3)+" 4 ";for(auto a:{"select_research_project","select_production_project"}){for(int i=0;i<32;++i)check(parseAgentRequest(p+a+" r"+std::to_string(i),r),"bounded project handle parser");for(auto bad:{"r32","r01","r-1","R0","r","r999","r0 extra"})check(!parseAgentRequest(p+a+" "+bad,r),"project parser rejects malformed handle");}for(auto a:{"adjust_scientists","adjust_engineers"}){for(auto d:{"increase","decrease"})check(parseAgentRequest(p+a+" "+d,r),"closed staff direction parser");for(auto d:{"up","down","1","max","increase extra"})check(!parseAgentRequest(p+a+" "+d,r),"staff parser rejects unsupported operation");}check(!parseAgentRequest("H openxcom-agent-control/19",r)&&parseAgentRequest("H openxcom-agent-control/21",r),"wire21 exact handshake");
 for(const std::string arg:{"engineers adjust 1","units adjust -1000","units set 100000","engineers set 0","units max","units clear"})check(parseAgentRequest(p+"change_production_quantity "+arg,r),"closed semantic quantity parser accepts canonical values");
 for(const std::string arg:{"units adjust 0","units adjust -0","units adjust +1","units adjust 01","units adjust 1001","units set -1","units set 100001","units max 1","units hold 1","units adjust true","units adjust 1 extra"})check(!parseAgentRequest(p+"change_production_quantity "+arg,r),"semantic parser rejects noncanonical, timing and out-of-range values");}
}
int main(int argc,char**argv){check(argc==2,"source path");check(!SDL_Init(0)&&SDL_GetVideoSurface()==nullptr,"offscreen SDL only");Options::baseXResolution=320;Options::baseYResolution=200;Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}parserTests();commandParity();for(const std::string game:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+game+"/Language/en-US.yml");listTests(big,small,lang,game,false);listTests(big,small,lang,game,true);startTests(big,small,lang,game);infoTests(big,small,lang,game,false);infoTests(big,small,lang,game,true);}check(agentPaintHealthy(),"all actual paints healthy");check(!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"no live game/window");std::cout<<"AgentProductionSemanticTest PASS "<<checks<<" checks\n";}
