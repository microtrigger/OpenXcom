// SPDX-License-Identifier: GPL-3.0-or-later
// Actual widgets/fonts, exact extracted providers/controller and ordinary
// staffing/commit/remove bodies. Model, owning Game/stack and UI construction
// are explicit test seams. No real Game, simulation, SDL window or RNG runs.
#include "../../src/Engine/Action.h"
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
extern "C" SDLMod SDL_GetModState(){return KMOD_NONE;}
extern "C" Uint8 SDL_GetMouseState(int*,int*){return 0;}
extern "C" void SDL_PumpEvents(){}
extern "C" int SDL_PeepEvents(SDL_Event*,int,SDL_eventaction,Uint32){return 0;}
namespace { unsigned checks=0; void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}} }
namespace OpenXcom {
Game *State::_game=nullptr;
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
struct Production { const RuleManufacture *_rules; int assigned=0,amount=1; bool sold=false; explicit Production(const RuleManufacture *r,int n=1):_rules(r),amount(n){} int getAssignedEngineers()const{return assigned;} void setAssignedEngineers(int n){assigned=n;} bool getInfiniteAmount()const{return false;} int getAmountTotal()const{return amount;} void setSellItems(bool v){sold=v;} void startItem(Base*,SavedGame*,const Mod*)const; };
struct Base { int _engineers=10,freeSpace=20,hangars=1,usedHangars=0; std::vector<Production*> _productions; std::vector<Craft*> crafts; ItemContainer storage; int getAvailableEngineers()const{return _engineers;} int getEngineers()const{return _engineers;} void setEngineers(int n){_engineers=n;} int getFreeWorkshops()const{int n=freeSpace;for(auto p:_productions)n-=p->assigned;return n;} int getAvailableHangars()const{return hangars;} int getUsedHangars()const{return usedHangars;} std::vector<Production*>getProductions()const{return _productions;} ItemContainer*getStorageItems(){return &storage;} std::vector<Craft*>*getCrafts(){return &crafts;} void removeCraft(Craft*p,bool){crafts.erase(std::remove(crafts.begin(),crafts.end(),p),crafts.end());} void removeProduction(Production*); };
struct Game { const void *top=nullptr; Surface *_cursor=nullptr,*_fpsCounter=nullptr; unsigned pops=0,pushes=0; SavedGame save; Mod mod; std::string pushed; bool isState(const void*p)const{return top==p;} void popState(){++pops;top=nullptr;} template<class T>void pushState(T*p){++pushes;pushed=p->kind;top=p;} SavedGame *getSavedGame(){return &save;} Mod*getMod(){return &mod;} bool agentOverlayClear(const AgentUiState&)const; };
#include "AgentBaseTableOverlay.generated.inc"
#include "ProductionCommit.generated.inc"
#include "BaseRemoveProduction.generated.inc"
struct UI { Game *_game=nullptr; Surface *_modal=nullptr; std::vector<Surface*>_surfaces; Language *language=nullptr; SDL_Color *_palette=nullptr; std::string kind; LocalizedText tr(const std::string&id)const{return language->getString(id);} };
struct ManufactureInfoState: UI { std::uint64_t _agentProjectRevision=0; Base *_base; RuleManufacture *_item; Production *_production; TextButton *_btnOk=nullptr,*_btnStop=nullptr; ToggleTextButton *_btnSell=nullptr; ArrowButton *_btnEngineerUp=nullptr,*_btnEngineerDown=nullptr; Text *_txtTitle=nullptr,*_txtAvailableEngineer=nullptr,*_txtAvailableSpace=nullptr,*_txtMonthlyProfit=nullptr,*_txtAllocated=nullptr,*_txtTodo=nullptr,*_txtEngineerUp=nullptr,*_txtEngineerDown=nullptr; ManufactureInfoState(Base*b,RuleManufacture*r):_base(b),_item(r),_production(nullptr){kind="info-new";} ManufactureInfoState(Base*b,Production*p):_base(b),_item(nullptr),_production(p){kind="info-existing";} bool agentUiState(AgentUiState&)const; bool agentDispatch(const AgentRequest&); void moreEngineer(int);void lessEngineer(int);void setAssignedEngineer();void btnOkClick(Action*);void btnStopClick(Action*);void exitState();int getMonthlyNetFunds()const{return 0;} };
struct ManufactureStartState: UI { Base *_base; RuleManufacture *_item; TextButton *_btnStart=nullptr,*_btnCancel=nullptr; Text *_txtTitle=nullptr,*_txtManHour=nullptr,*_txtCost=nullptr,*_txtWorkSpace=nullptr,*_txtRequiredItemsTitle=nullptr,*_txtItemNameColumn=nullptr,*_txtUnitRequiredColumn=nullptr,*_txtUnitAvailableColumn=nullptr; TextList *_lstRequiredItems=nullptr; ManufactureStartState(Base*b,RuleManufacture*r):_base(b),_item(r){kind="start";} bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnStartClick(Action*);void btnCancelClick(Action*); };
struct ErrorMessageState: UI { ErrorMessageState(LocalizedText,SDL_Color*,int,const char*,int){kind="error";} };
struct NewManufactureListState: UI { Base *_base; std::uint64_t _agentMappingRevision=1; std::vector<RuleManufacture*> _possibleProductions;std::vector<std::string> _displayedStrings;TextButton *_btnOk=nullptr;Text *_txtTitle=nullptr,*_txtItem=nullptr,*_txtCategory=nullptr;TextList *_lstManufacture=nullptr;explicit NewManufactureListState(Base*b):_base(b){kind="new-list";}bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnOkClick(Action*);void selectProduction(size_t); };
struct ManufactureState: UI { Base *_base;std::uint64_t _agentMappingRevision=1;std::vector<Production*> _agentProjects;TextButton *_btnNew=nullptr,*_btnOk=nullptr;Text *_txtTitle=nullptr,*_txtAvailable=nullptr,*_txtAllocated=nullptr,*_txtSpace=nullptr,*_txtFunds=nullptr,*_txtItem=nullptr,*_txtEngineers=nullptr,*_txtProduced=nullptr,*_txtCost=nullptr,*_txtTimeLeft=nullptr;TextList *_lstManufacture=nullptr;explicit ManufactureState(Base*b):_base(b){}bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnOkClick(Action*);void btnNewProductionClick(Action*);void selectProduction(size_t); };
#include "ManufactureMethods.generated.inc"
#include "NewManufactureMethods.generated.inc"
#include "ManufactureStartMethods.generated.inc"
#include "ManufactureInfoMethods.generated.inc"
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
 p.text(90,9,192,118,p.tr("STR_INCREASE_UC"));p.text(90,9,192,138,p.tr("STR_DECREASE_UC"));p.arrow(ARROW_BIG_UP,284,114);p.arrow(ARROW_BIG_DOWN,284,136);
 s._btnOk=p.button(136,168,155,"STR_OK");s._btnStop=p.button(136,16,155,"STR_STOP_PRODUCTION");s._btnSell=p.own(new ToggleTextButton(60,16,244,61));s._btnSell->initText(&big,&small,&lang);s._btnSell->setText(p.tr("STR_SELL_PRODUCTION"));
 s._surfaces=p.surfaces;s.setAssignedEngineer();p.draw();AgentUiState ui;check(s.agentUiState(ui)&&ui.offers("adjust_engineers")&&ui.offers("accept_production")&&ui.offers("stop_production"),"actual Info layout offers staffing and actual OK/Stop");
 check(YAML::Load(ui.visible)["allocated"].as<std::string>()==std::string(">\x01" "0"),"allocation is exact rendered formatted string");overlayTests(s,p,g,s._txtTitle,"adjust_engineers");
 TableChannel c(master+(fresh?"-production-new":"-production-existing"));connect(c,s,g);c.read();c.events();
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
void parserTests(){AgentRequest r;const std::string p="E "+token(1)+" "+token(2)+" "+token(3)+" 4 ";for(auto a:{"select_research_project","select_production_project"}){for(int i=0;i<32;++i)check(parseAgentRequest(p+a+" r"+std::to_string(i),r),"bounded project handle parser");for(auto bad:{"r32","r01","r-1","R0","r","r999","r0 extra"})check(!parseAgentRequest(p+a+" "+bad,r),"project parser rejects malformed handle");}for(auto a:{"adjust_scientists","adjust_engineers"}){for(auto d:{"increase","decrease"})check(parseAgentRequest(p+a+" "+d,r),"closed staff direction parser");for(auto d:{"up","down","1","max","increase extra"})check(!parseAgentRequest(p+a+" "+d,r),"staff parser rejects unsupported operation");}check(!parseAgentRequest("H openxcom-agent-control/18",r)&&parseAgentRequest("H openxcom-agent-control/19",r),"wire19 exact handshake");}
}
int main(int argc,char**argv){check(argc==2,"source path");check(!SDL_Init(0)&&SDL_GetVideoSurface()==nullptr,"offscreen SDL only");Options::baseXResolution=320;Options::baseYResolution=200;Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}parserTests();for(const std::string game:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+game+"/Language/en-US.yml");listTests(big,small,lang,game,false);listTests(big,small,lang,game,true);startTests(big,small,lang,game);infoTests(big,small,lang,game,false);infoTests(big,small,lang,game,true);}check(agentPaintHealthy(),"all actual paints healthy");check(!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"no live game/window");std::cout<<"AgentManufactureProjectTest PASS "<<checks<<" checks\n";}
