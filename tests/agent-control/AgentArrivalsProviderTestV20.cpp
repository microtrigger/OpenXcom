// SPDX-License-Identifier: GPL-3.0-or-later
// Source-extracted native providers/dispatch and ordinary callbacks, real SDL
// offscreen widgets/stock fonts, explicit construction/owning Game/model seams.
// The native constructors are NOT run: no arrivals, UFO IDs or resources mutate.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/LocalizedText.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Palette.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/TextButton.h"
#define private public
#include "../../src/Interface/TextList.h"
#undef private
#include "../../src/Savegame/Production.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
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
extern "C" Uint8 SDL_GetAppState(){return SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;}
extern "C" SDLMod SDL_GetModState(){return KMOD_NONE;}
extern "C" Uint8 SDL_GetMouseState(int*,int*){return 0;}
extern "C" void SDL_PumpEvents(){}
extern "C" int SDL_PeepEvents(SDL_Event*,int,SDL_eventaction,Uint32){return 0;}
namespace { unsigned checks=0; void check(bool value,const char *why){++checks;if(!value){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}} }
namespace OpenXcom {
Game *State::_game=nullptr;
State::State():_screen(true),_modal(nullptr),_ruleInterface(nullptr),_ruleInterfaceParent(nullptr){}
State::~State(){}
void State::init(){} void State::handle(Action*){} void State::blit(){} void State::resize(int&,int&){} void State::recenter(int,int){}
#include "CompletionStateBase.generated.inc"
#include "CompletionGameIdentity.generated.inc"
}
namespace Completion {
// Explicit simulation seams: counters prove callbacks and only callbacks alter
// navigation/timer state. These types expose no arrival/UFO hidden model API.
struct Base {};
struct Globe {};
struct GeoscapeState { unsigned resets=0,globeReads=0;Globe globe;void timerReset(){++resets;}Globe*getGlobe(){++globeReads;return &globe;} };
struct BasescapeState {Base*base;Globe*globe;BasescapeState(Base*b,Globe*g):base(b),globe(g){} };
struct ManufactureState {Base*base;explicit ManufactureState(Base*b):base(b){} };
struct Screen {Surface*logical=nullptr;Surface*getSurface(){return logical;}};
struct Game {
 Screen screen;Screen*getScreen(){return &screen;}
 const void*top=nullptr;int parent=0;Surface *_cursor=nullptr,*_fpsCounter=nullptr;unsigned pops=0,pushes=0;std::string destination;Base*destinationBase=nullptr;Globe*destinationGlobe=nullptr;
 bool isState(const void*p)const{return top==p;}
 void popState(){++pops;top=&parent;}
 void pushState(BasescapeState*p){++pushes;destination="base";destinationBase=p->base;destinationGlobe=p->globe;top=&parent;delete p;}
 void pushState(ManufactureState*p){++pushes;destination="manufacture";destinationBase=p->base;top=&parent;delete p;}
 bool agentOverlayClear(const AgentUiState&)const;
};
#include "CompletionOverlay.generated.inc"
struct State:OpenXcom::State {using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;Game*_game;GeoscapeState*_state;Base*_base;State(Game*g,GeoscapeState*s,Base*b):_game(g),_state(s),_base(b){g->top=this;}void blit() override;};
#include "CompletionOrdinaryBlit.generated.inc"
using Common=State;
struct ItemsArrivingState:Common {
 TextButton*_btnOk=nullptr,*_btnGotoBase=nullptr;Text*_txtTitle=nullptr,*_txtItem=nullptr,*_txtQuantity=nullptr,*_txtDestination=nullptr;TextList*_lstTransfers=nullptr;
 bool _agentDestinationPaintFull=false;bool agentDestinationFull()const;std::vector<std::uint64_t> _agentStoredPaintRevisions;std::vector<std::uint64_t> agentPaintRevisions()const;void blit() override;
 using Common::Common;bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnOkClick(Action*);void btnGotoBaseClick(Action*);
};
struct TransfersState:Common {
 TextButton *_btnOk=nullptr;Text *_txtTitle=nullptr,*_txtItem=nullptr,*_txtQuantity=nullptr,*_txtArrivalTime=nullptr;TextList *_lstTransfers=nullptr;
 using Common::Common;bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnOkClick(Action*);
};
#include "ItemsArrivingMethods.generated.inc"
#include "TransfersMethods.generated.inc"
}
namespace {
#include "AgentManufactureChannel.inc"
class CountedList:public TextList {
public: unsigned draws=0;CountedList(int w,int h,int x,int y):TextList(w,h,x,y){} void draw()override{++draws;TextList::draw();} bool dirty()const{return _redraw;}
};
struct Paint {
 Font&big;Font&small;Language&lang;Surface canvas,background;SDL_Color palette[256];std::vector<std::unique_ptr<Surface>>owned;std::vector<Surface*>surfaces;YAML::Node rules;std::string kind,master;std::function<void()>render;
 Paint(Font&b,Font&s,Language&l,const std::string&source,const std::string&game,const std::string&type,bool hyper=false):big(b),small(s),lang(l),canvas(320,200),background(320,200),kind(type),master(game){
  for(auto r:YAML::LoadFile(source+"/bin/standard/"+game+"/interfaces.rul")["interfaces"])if(r["type"].as<std::string>()==type)rules=YAML::Node(r);
  check(bool(rules),"stock interface found");Palette base,back;std::string dir=std::string("../")+(game=="xcom1"?"UFO":"TFTD")+"/GEODATA/";
  base.loadDat(dir+"PALETTES.DAT",256,Palette::palOffset(0));back.loadDat(dir+"BACKPALS.DAT",128);
  unsigned backIndex=0;for(auto e:rules["elements"])if(e["id"].as<std::string>()=="palette")backIndex=e[hyper?"color2":"color"].as<unsigned>();
  for(unsigned i=0;i<256;++i){palette[i]=base.getColors()[i];}
  for(unsigned i=0;i<16;++i){palette[Palette::backPos+i]=back.getColors(Palette::blockOffset(backIndex))[i];}
  canvas.setPalette(palette);background.setPalette(palette);background.clear(40);surfaces.push_back(&background);
 }
 int color(const std::string&id)const{for(auto e:rules["elements"])if(e["id"].as<std::string>()==id)return e["color"].as<int>();return 1;}
 template<class T>T*own(T*p){p->setPalette(palette);owned.emplace_back(p);surfaces.push_back(p);return p;}
 Text*text(int w,int h,int x,int y,const std::string&value,bool bigText=false,const std::string&id="text1"){
  auto*t=own(new Text(w,h,x,y));t->initText(&big,&small,&lang);t->setColor(color(id));if(bigText)t->setBig();t->setText(value);return t;
 }
 TextButton*button(int w,int h,int x,int y,const std::string&key){auto*b=own(new TextButton(w,h,x,y));b->initText(&big,&small,&lang);b->setColor(color("button"));b->setText(lang.getString(key));return b;}
 TextList*list(int w,int h,int x,int y,const std::string&id){auto*l=own(new CountedList(w,h,x,y));l->initText(&big,&small,&lang);l->setColor(color(id));l->setBackground(&background);return l;}
 void draw(){if(render)render();else for(auto*s:surfaces)s->blit(&canvas);}
};
void setup(Completion::ItemsArrivingState&s,Paint&p,unsigned count=25,bool paint=true){
 s._btnOk=p.button(142,16,16,166,"STR_OK");s._btnGotoBase=p.button(142,16,162,166,"STR_GO_TO_BASE");
 s._btnOk->onMouseClick((ActionHandler)&Completion::ItemsArrivingState::btnOkClick);s._btnGotoBase->onMouseClick((ActionHandler)&Completion::ItemsArrivingState::btnGotoBaseClick);
 s._txtTitle=p.text(310,17,5,18,p.lang.getString("STR_ITEMS_ARRIVING"),true);s._txtTitle->setAlign(ALIGN_CENTER);
 s._txtItem=p.text(114,9,16,34,p.lang.getString("STR_ITEM"));s._txtQuantity=p.text(54,9,152,34,p.lang.getString("STR_QUANTITY_UC"));s._txtDestination=p.text(112,9,212,34,p.lang.getString("STR_DESTINATION_UC"));
 s._lstTransfers=p.list(271,112,14,50,"text2");s._lstTransfers->agentEnableArrivalCells();s._lstTransfers->setColumns(3,155,41,98);s._lstTransfers->setSelectable(true);s._lstTransfers->setMargin(2);
 for(unsigned i=0;i<count;++i){const std::string name=p.lang.getString(i%2?"STR_ENGINEER":"STR_SCIENTIST"),quantity=std::to_string(i+1);s._lstTransfers->addRow(3,name.c_str(),quantity.c_str(),"BASE");}
 s._surfaces=p.surfaces;s._game->screen.logical=&p.canvas;p.render=[&](){s.blit();};if(paint)p.draw();
}
template<class State>void connect(TableChannel&c,State&s){c.state.project=[&](AgentUiState&out){return s.agentUiState(out);};c.state.handler=[&](const AgentRequest&r){check(s.agentDispatch(r),"exact native dispatcher accepts fresh controller grant");if(s._game->top!=&s)c.game.top=&c.unsupportedParent;};}
std::vector<std::string> records;
YAML::Node record(TableChannel&c,const std::string&game,const std::string&provider,const std::string&name,const std::string&expected=""){
 auto state=YAML::Load(c.read());const auto want=expected.empty()?provider:expected;check(state["state"].as<std::string>()==want,"canonical state uses actual provider or explicit failclosed state");
 records.push_back("{\"name\":"+agentJsonString(name)+",\"game\":"+agentJsonString(game)+",\"provider\":"+agentJsonString(provider)+",\"expected_state\":"+agentJsonString(want)+",\"state_file\":"+agentJsonString(c.prefix+"-wire-"+std::to_string(c.schedule.size()-1)+".json")+"}");return state;
}
void validate(const AgentUiState&ui,const std::string&screen){
 check(ui.screen==screen&&ui.visible.size()<=9000&&agentRegionsDisjoint(ui),"exact provider bounded disjoint UI");auto v=YAML::Load(ui.visible);
 check(v.IsMap()&&v.size()==(screen=="production_complete"?2:5),"exact canonical visible key count");
 for(auto b:v["buttons"])check(b.size()==2&&!b["label"].as<std::string>().empty()&&b["label"].as<std::string>().size()<=1024,"bounded displayed button labels");
 for(auto a:ui.actions)check(a.argumentKind=="none"||a.id=="scroll_visible_list","five new actions take no arguments");
 if(screen=="items_arriving"){
  check(v["headings"].size()==3&&v["rows"].size()<=32&&v["scroll"].size()==2,"bounded arrival table");
  for(unsigned i=0;i<v["rows"].size();++i){auto r=v["rows"][i];check(r.size()==3&&r["handle"].as<std::string>()=="r"+std::to_string(i)&&r["cells"].size()==3&&!r["selected"].as<bool>(),"read-only contiguous handles and three cells");bool nonempty=false;for(auto cell:r["cells"]){check(cell.IsNull()||(cell.IsScalar()&&cell.as<std::string>().size()<=1024),"bounded nullable displayed row cell");if(!cell.IsNull()&&!cell.as<std::string>().empty())nonempty=true;}check(nonempty,"retained row has completely proved nonempty cell");}
 }else if(screen=="ufo_contact"){
  check(v["details_status"].as<std::string>()=="unavailable"&&ui.visible.find("Hidden")==std::string::npos&&ui.visible.find("SECRET")==std::string::npos,"UFO detail data is never exposed");
  for(auto a:ui.actions)check(a.id=="dismiss_ufo_contact","shown UFO Intercept/Center grant no action");
 }
}
void click(TextButton*b,OpenXcom::State*s){SDL_Event e={};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=b->getX()+b->getWidth()/2;e.button.y=b->getY()+b->getHeight()/2;Action down(&e,1,1,0,0);b->handle(&down,s);e.type=SDL_MOUSEBUTTONUP;Action up(&e,1,1,0,0);b->handle(&up,s);}
std::string receipt(TableChannel&c,Paint&p){c.frame();check(c.receive().empty(),"E gets no receipt on dispatch frame");p.draw();c.frame();auto result=c.receive();check(!result.empty()&&YAML::Load(result)["status"].as<std::string>()=="dispatched","E receipt after subsequent painted frame");return result;}
std::string status(TableChannel&c){const auto end=c.request.find(' ',2);c.send("S "+c.request.substr(2,end-2));c.controller->poll();return c.receive();}
template<class State>void lifecycle(State&s,Paint&p,Text*message,TextButton*button,const std::string&action,const std::string&screen,const std::string&game,const std::string&variant,bool destination=false){
 Completion::Game&g=*s._game;auto&geo=*s._state;TableChannel c(game+"-"+screen+"-"+variant+"-lifecycle");connect(c,s);AgentUiState ui;
 check(s.agentUiState(ui),"stock popup projectable");validate(ui,screen);check(ui.offers(action),"tested action has complete stock caption");record(c,game,screen,variant+"_lifecycle_initial");
 const auto initial=ui;const auto beforePops=g.pops,beforePush=g.pushes,beforeResets=geo.resets;
 auto event=YAML::Load(c.events());bool found=false;for(auto e:event["events"])if(e["kind"].as<std::string>()=="presentation"&&e["state"].as<std::string>()==screen)found=true;check(found,"actual V retains provider presentation");check(g.pops==beforePops&&g.pushes==beforePush&&geo.resets==beforeResets,"R and V never invoke ordinary model callbacks");
 c.queue(action);
 const std::string original=message->getText();message->setText("Changed");message->setText(original);p.draw();c.frame();auto rejected=c.receive();check(!rejected.empty()&&YAML::Load(rejected)["status"].as<std::string>()=="unavailable"&&g.pops==beforePops,"displayed text ABA rejects queued action");c.send(c.request);c.controller->poll();check(c.receive()==rejected&&g.pops==beforePops,"stale duplicate is cached without callback");
 c.read();c.queue(action);const auto caption=button->getText();button->setText("Changed");button->setText(caption);p.draw();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable"&&g.pops==beforePops,"button caption ABA rejects queued authority");
 c.read();c.queue(action);message->setText("Dirty");c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable"&&g.pops==beforePops,"unpainted message mutation rejects queued authority");message->setText(original);p.draw();
 c.read();c.queue(action);g.top=&g.parent;c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable"&&g.pops==beforePops,"top-state replacement rejects queued action");g.top=&s;p.draw();
 c.read();c.queue(action);s._modal=button;c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable"&&g.pops==beforePops,"modal popup rejects queued authority");s._modal=nullptr;p.draw();
 c.read();c.queue(action);Surface overlay(button->getWidth(),button->getHeight(),button->getX(),button->getY());overlay.setPalette(p.palette);overlay.clear(1);overlay.blit(&p.canvas);g._cursor=&overlay;c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable"&&g.pops==beforePops,"full global button overlay invalidates queued action");g._cursor=nullptr;p.draw();
 c.read();c.queue(action);agentTestTime+=1001;c.controller->poll();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable"&&g.pops==beforePops,"expired queued E has no callback");
 record(c,game,screen,variant+"_fresh_after_mutations");c.queue(action);const auto completed=receipt(c,p);check(g.pops==beforePops+1&&g.pushes==beforePush+(destination?1u:0u)&&geo.resets==beforeResets+(destination?1u:0u),"native callback exactly one pop and destination-only timer reset/push");check(YAML::Load(completed)["observation"]["state"].as<std::string>()=="unsupported","return/destination observation is honestly unsupported");check(status(c)==completed,"S returns cached terminal result");c.send(c.request);c.controller->poll();check(c.receive()==completed&&g.pops==beforePops+1,"duplicate terminal E never pops again");c.events();
 check(initial.screen==screen,"read-only initial snapshot preserved");
}
template<class State>void negatives(State&s,Paint&p,Text*text,TextButton*button,const std::string&field,const std::string&action,const std::string&screen,const std::string&game,const std::string&variant){
 AgentUiState ui;auto&g=*s._game;TableChannel c(game+"-"+screen+"-"+variant+"-paint");connect(c,s);
 s._agentDestinationPaintFull=false;check(!s.agentUiState(ui),"initial no completed full paint rejects provider");p.draw();auto stock=record(c,game,screen,variant+"_stock_positive");check(!stock["visible"][field].IsNull(),"stock displayed message/title is positively admitted");
 const auto originalText=text->getText();text->setText("Changed");text->setText(originalText);record(c,game,screen,variant+"_text_aba_without_state_repaint","unsupported");check(!s.agentUiState(ui),"text ABA cannot reuse last state paint");text->blit(&p.canvas);check(!s.agentUiState(ui),"isolated child text draw cannot refresh state paint grant");p.draw();record(c,game,screen,variant+"_text_aba_full_repaint");
 const auto originalLabel=button->getText();button->setText("Changed");button->setText(originalLabel);record(c,game,screen,variant+"_label_aba_without_state_repaint","unsupported");check(!s.agentUiState(ui),"caption ABA cannot reuse last state paint");button->blit(&p.canvas);check(!s.agentUiState(ui),"isolated button draw cannot refresh state paint grant");p.draw();record(c,game,screen,variant+"_label_aba_full_repaint");
 SDL_Rect ink={};check(text->agentPaintedBounds(ink),"actual text ink for overlay");
 Surface cover(ink.w,ink.h,ink.x,ink.y);cover.setPalette(p.palette);cover.clear(1);cover.blit(&p.canvas);g._cursor=&cover;
 auto hidden=record(c,game,screen,variant+"_full_text_overlay");check(hidden["visible"][field].IsNull(),"fully overlaid text cannot expose full source");check(s.agentUiState(ui)&&ui.offers(action),"independent button remains enabled through text overlay");g._cursor=nullptr;p.draw();
 g._fpsCounter=&cover;cover.blit(&p.canvas);check(record(c,game,screen,variant+"_fps_text_overlay")["visible"][field].IsNull(),"FPS text overlay withheld");g._fpsCounter=nullptr;p.draw();
 s._surfaces.push_back(&cover);cover.blit(&p.canvas);check(record(c,game,screen,variant+"_sibling_overlay")["visible"][field].IsNull(),"later sibling surface withholds text");s._surfaces.pop_back();p.draw();
 const auto value=text->getText();text->setText(std::string(1025,'W'));p.draw();check(record(c,game,screen,variant+"_overlong_text")["visible"][field].IsNull(),"overlong complete source withheld");text->setText(value);p.draw();
 text->setText("A\xF0\x9F\x8C\x8C");p.draw();check(record(c,game,screen,variant+"_missing_glyph")["visible"][field].IsNull(),"missing font glyph cannot expose source");text->setText(value);p.draw();
 text->getCrop()->w=1;text->getCrop()->h=text->getHeight();p.draw();check(record(c,game,screen,variant+"_true_source_clip")["visible"][field].IsNull(),"actual source clip withholds text");text->getCrop()->w=text->getCrop()->h=0;p.draw();
 SDL_Rect clip={Sint16(ink.x+std::max(1,int(ink.w)/2)),0,Uint16(320-ink.x-std::max(1,int(ink.w)/2)),200};SDL_SetClipRect(p.canvas.getSurface(),&clip);check(!s.agentUiState(ui),"current clipped destination rejects provider before repaint");p.draw();record(c,game,screen,variant+"_true_destination_clip","unsupported");check(!s._agentDestinationPaintFull&&!s.agentUiState(ui),"actual clipped repaint cannot certify popup");SDL_SetClipRect(p.canvas.getSurface(),nullptr);record(c,game,screen,variant+"_clip_restored_without_repaint","unsupported");check(!s.agentUiState(ui),"restoring destination clip cannot bless old partial paint");p.draw();record(c,game,screen,variant+"_full_destination_repaint");check(s._agentDestinationPaintFull&&s.agentUiState(ui),"ordinary full repaint restores valid popup");
 const auto label=button->getText();button->setText(std::string(1025,'W'));p.draw();record(c,game,screen,variant+"_button_overlong");check(s.agentUiState(ui)&&!ui.offers(action),"overlong button cannot authorize callback");button->setText(label);p.draw();
 button->setVisible(false);p.draw();record(c,game,screen,variant+"_button_hidden");check(s.agentUiState(ui)&&!ui.offers(action),"hidden button cannot authorize callback");button->setVisible(true);p.draw();
 Surface all(button->getWidth(),button->getHeight(),button->getX(),button->getY());all.setPalette(p.palette);all.clear(1);all.blit(&p.canvas);g._cursor=&all;record(c,game,screen,variant+"_button_full_overlay");check(s.agentUiState(ui)&&!ui.offers(action),"full button overlay cannot authorize callback");g._cursor=nullptr;p.draw();
 s._modal=button;check(!s.agentUiState(ui),"any active modal rejects popup provider");s._modal=nullptr;g.top=&g.parent;check(!s.agentUiState(ui),"non-top provider rejected");g.top=&s;p.draw();record(c,game,screen,variant+"_paint_restored");
}

// Exact constructor geometry with zero model rows: no clearList/invalidate is
// permitted before the first ordinary paint in either empty-table regression.
void freshEmpty(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game){
 {Paint p(big,small,lang,source,game,"itemsArriving");Completion::Game g;Completion::GeoscapeState geo;Completion::Base base;Completion::ItemsArrivingState s(&g,&geo,&base);setup(s,p,0,false);auto*l=static_cast<CountedList*>(s._lstTransfers);AgentUiState ui;check(l->dirty()&&l->draws==0,"fresh arrivals schedules its first ordinary draw");check(!s.agentUiState(ui),"fresh arrivals unpainted stays unavailable");TableChannel c(game+"-fresh-empty-arrivals");connect(c,s);record(c,game,"items_arriving","fresh_empty_arrivals_before_paint","unsupported");
 for(unsigned f=0;f<3;++f){p.draw();check(l->draws==1&&!l->dirty(),"empty arrivals draws exactly once across ordinary blits");auto state=record(c,game,"items_arriving","fresh_empty_arrivals_paint_"+std::to_string(f));check(state["visible"]["rows"].size()==0&&s.agentUiState(ui)&&ui.offers("dismiss_items_arriving")&&ui.offers("open_arrival_base")&&!ui.offers("scroll_visible_list"),"fresh empty arrivals has independent buttons only");}c.events();c.queue("dismiss_items_arriving");auto result=receipt(c,p);check(g.pops==1&&geo.resets==0,"fresh empty arrivals ordinary close once");check(status(c)==result,"fresh empty arrival S cached");}
 {Paint p(big,small,lang,source,game,"transferInfo");Completion::Game g;Completion::GeoscapeState geo;Completion::Base base;Completion::TransfersState s(&g,&geo,&base);
 s._btnOk=p.button(288,16,16,166,"STR_OK");s._btnOk->onMouseClick((ActionHandler)&Completion::TransfersState::btnOkClick);
 s._txtTitle=p.text(278,17,21,18,lang.getString("STR_TRANSFERS"),true,"text");s._txtTitle->setAlign(ALIGN_CENTER);s._txtItem=p.text(114,9,16,34,lang.getString("STR_ITEM"),false,"text");s._txtQuantity=p.text(54,9,152,34,lang.getString("STR_QUANTITY_UC"),false,"text");s._txtArrivalTime=p.text(112,9,212,34,lang.getString("STR_ARRIVAL_TIME_HOURS"),false,"text");s._lstTransfers=p.list(273,112,14,50,"list");s._lstTransfers->setColumns(3,155,75,46);s._lstTransfers->setSelectable(true);s._lstTransfers->setMargin(2);s._lstTransfers->setX(s._lstTransfers->getX());s._lstTransfers->setY(s._lstTransfers->getY());s._surfaces=p.surfaces;g.screen.logical=&p.canvas;p.render=[&](){s.blit();};
 auto*l=static_cast<CountedList*>(s._lstTransfers);AgentUiState ui;check(l->dirty()&&l->draws==0&&!s.agentUiState(ui),"fresh untouched empty Transfers needs first paint");TableChannel c(game+"-fresh-empty-transfers");connect(c,s);record(c,game,"base_transfers","fresh_empty_transfers_before_paint","unsupported");
 for(unsigned f=0;f<3;++f){p.draw();std::vector<AgentListRow>rows;std::vector<AgentArrivalRow>arrival;check(l->draws==1&&!l->dirty()&&l->agentRows(rows)&&rows.empty(),"fresh empty Transfers proves zero rows with exactly one native draw");check(!l->agentArrivalRows(arrival),"ordinary Transfers never opts into nullable arrival cells");auto state=record(c,game,"base_transfers","fresh_empty_transfers_paint_"+std::to_string(f));check(s.agentUiState(ui)&&state["visible"].size()==4&&state["visible"]["rows"].size()==0&&ui.offers("close_base_transfers")&&!ui.offers("scroll_visible_list"),"unchanged Transfers schema admits only shown close");}
 c.events();c.queue("close_base_transfers");auto result=receipt(c,p);check(g.pops==1&&geo.resets==0,"fresh empty Transfers close uses ordinary callback exactly once");check(status(c)==result,"fresh empty Transfers status cached");c.send(c.request);c.controller->poll();check(c.receive()==result&&g.pops==1,"fresh empty Transfers duplicate never closes twice");}
}
YAML::Node cellRecord(TableChannel&c,const std::string&game,const std::string&name,unsigned row,const std::vector<std::string>&values,const std::vector<bool>&known){
 auto state=record(c,game,"items_arriving",name);std::string expected="[";for(unsigned i=0;i<3;++i){if(i)expected+=",";expected+=known[i]?agentJsonString(values[i]):"null";}expected+="]";records.back().pop_back();records.back()+=",\"row_index\":"+std::to_string(row)+",\"visible_cells_expected\":"+expected+"}";auto rows=state["visible"]["rows"];check(rows.size()>row,"retained nullable cell row exists");for(unsigned i=0;i<3;++i){auto cell=rows[row]["cells"][i];check(known[i]?(!cell.IsNull()&&cell.as<std::string>()==values[i]):cell.IsNull(),("cell case "+game+"/"+name+"/"+std::to_string(i)).c_str());}return state;
}
void arrivals(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game){
 Paint p(big,small,lang,source,game,"itemsArriving");Completion::Game g;Completion::GeoscapeState geo;Completion::Base base;Completion::ItemsArrivingState s(&g,&geo,&base);setup(s,p);AgentUiState ui;auto*l=s._lstTransfers;
 check(s.agentUiState(ui),"stock arrivals provider");validate(ui,"items_arriving");check(ui.rows.size()>=4,"stock arrival rows positively proved");check(YAML::Load(ui.visible)["headings"][2].IsNull(),"native clipped destination heading stays null");
 negatives(s,p,s._txtTitle,s._btnOk,"title","dismiss_items_arriving","items_arriving",game,"arrivals");
 TableChannel c(game+"-arrivals-cells");connect(c,s);record(c,game,"items_arriving","stock_rows");AgentRequest forbidden;forbidden.action="choose_visible_row";forbidden.choice="r0";check(!s.agentDispatch(forbidden),"nullable arrival cells grant no row action");
 c.queue("scroll_visible_list","down");receipt(c,p);check(l->getScroll()==1&&g.pops==0&&geo.resets==0,"arrival scroll changes only ordinary visible page");c.read();c.queue("scroll_visible_list","down");l->scrollTo(3);l->scrollTo(1);p.draw();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable","arrival page ABA rejects stale authority");l->scrollTo(0);p.draw();
 const auto old=l->getCellText(0,0);l->setCellText(0,0,"ROW");l->setCellText(0,0,old);record(c,game,"items_arriving","list_aba_without_state_repaint","unsupported");l->blit(&p.canvas);check(!s.agentUiState(ui),"isolated list draw cannot refresh full popup grant");p.draw();record(c,game,"items_arriving","list_aba_full_repaint");c.queue("dismiss_items_arriving");l->setCellText(0,0,"ROW");l->setCellText(0,0,old);p.draw();c.frame();auto stale=c.receive();check(YAML::Load(stale)["status"].as<std::string>()=="unavailable"&&g.pops==0,"row ABA invalidates dismissal without callback");c.send(c.request);c.controller->poll();check(c.receive()==stale&&g.pops==0,"row ABA duplicate cached");
 const Surface*up=nullptr,*down=nullptr;l->agentScrollControls(up,down);check(up&&down,"stock scroll arrows");{Surface cover(down->getWidth(),down->getHeight(),down->getX(),down->getY());cover.setPalette(p.palette);cover.clear(1);cover.blit(&p.canvas);g._fpsCounter=&cover;auto state=record(c,game,"items_arriving","down_arrow_overlay");check(!state["visible"]["scroll"]["down"].as<bool>(),"covered down arrow omitted");AgentRequest q;q.action="scroll_visible_list";q.choice="down";check(!s.agentDispatch(q),"covered arrow never dispatches");g._fpsCounter=nullptr;p.draw();}

 // Opt-in witnesses must be observational: compare actual widget pixels and
 // indexed palette output to a separately painted ordinary non-opt-in list.
 {Surface ordinaryCanvas(320,200);ordinaryCanvas.setPalette(p.palette);TextList ordinary(271,112,14,50);ordinary.setPalette(p.palette);ordinary.initText(&big,&small,&lang);ordinary.setColor(p.color("text2"));ordinary.setBackground(&p.background);ordinary.setColumns(3,155,41,98);ordinary.setSelectable(true);ordinary.setMargin(2);for(unsigned i=0;i<25;++i){const std::string name=lang.getString(i%2?"STR_ENGINEER":"STR_SCIENTIST"),quantity=std::to_string(i+1);ordinary.addRow(3,name.c_str(),quantity.c_str(),"BASE");}ordinary.blit(&ordinaryCanvas);l->lock();ordinary.lock();bool same=true;for(int y=0;y<l->getHeight();++y)for(int x=0;x<l->getWidth();++x)if(l->getPixel(x,y)!=ordinary.getPixel(x,y))same=false;ordinary.unlock();l->unlock();check(same,"arrival opt-in preserves ordinary list pixels exactly");std::vector<AgentArrivalRow>unavailable;check(!ordinary.agentArrivalRows(unavailable),"ordinary non-opt-in three-column list never exports nullable cells");record(c,game,"items_arriving","opt_in_pixel_equality");}
 const std::string scientist=lang.getString("STR_SCIENTIST"),medikit=lang.getString("STR_MEDI_KIT");
 auto reset=[&](const std::string&name,const std::string&quantity,const std::string&destination,unsigned count=1){l->clearList();l->setHeight(112);l->setPalette(p.palette);for(unsigned i=0;i<count;++i)l->addRow(3,name.c_str(),quantity.c_str(),destination.c_str());p.draw();};
 reset(scientist,"1","MCP Manufacture");cellRecord(c,game,"single_scientist_mcp_manufacture",0,{scientist,"1",""},{true,true,false});
 l->setCellText(0,0,medikit);p.draw();cellRecord(c,game,"single_medikit_mcp_manufacture",0,{medikit,"1",""},{true,true,false});
 l->setCellText(0,2,"MCP19");p.draw();cellRecord(c,game,"single_medikit_mcp19",0,{medikit,"1","MCP19"},{true,true,true});l->setCellText(0,0,scientist);p.draw();cellRecord(c,game,"single_scientist_mcp19",0,{scientist,"1","MCP19"},{true,true,true});
 const std::vector<std::string>values={scientist,"1","MCP19"};
 for(unsigned column=0;column<3;++column){auto*cell=l->_texts[0][column];std::vector<bool>known={true,true,true};known[column]=false;
 cell->setVisible(false);l->invalidate();p.draw();cellRecord(c,game,"hidden_cell_"+std::to_string(column),0,values,known);cell->setVisible(true);l->invalidate();p.draw();
 cell->getCrop()->w=1;cell->getCrop()->h=cell->getHeight();l->invalidate();p.draw();cellRecord(c,game,"cropped_cell_"+std::to_string(column),0,values,known);cell->getCrop()->w=cell->getCrop()->h=0;l->invalidate();p.draw();
 l->setCellText(0,column,std::string(1025,'W'));p.draw();cellRecord(c,game,"overlong_cell_"+std::to_string(column),0,values,known);l->setCellText(0,column,values[column]);p.draw();
 l->setCellText(0,column,"A\xF0\x9F\x8C\x8C");p.draw();cellRecord(c,game,"missing_glyph_cell_"+std::to_string(column),0,values,known);l->setCellText(0,column,values[column]);p.draw();
 std::vector<AgentArrivalRow>rows;check(l->agentArrivalRows(rows)&&rows.size()==1&&!rows[0].cells[column].regions.empty(),"native independent cell region");auto ink=rows[0].cells[column].regions.front();Surface cover(1,1,ink.x,ink.y);cover.setPalette(p.palette);cover.clear(1);cover.blit(&p.canvas);g._cursor=&cover;cellRecord(c,game,"cursor_overlay_cell_"+std::to_string(column),0,values,known);check(s.agentUiState(ui)&&ui.offers("dismiss_items_arriving")&&ui.offers("open_arrival_base"),"row text never removes independent button authority");g._cursor=nullptr;p.draw();
 cover.blit(&p.canvas);g._fpsCounter=&cover;cellRecord(c,game,"fps_overlay_cell_"+std::to_string(column),0,values,known);g._fpsCounter=nullptr;p.draw();s._surfaces.push_back(&cover);cover.blit(&p.canvas);cellRecord(c,game,"sibling_overlay_cell_"+std::to_string(column),0,values,known);s._surfaces.pop_back();p.draw();
 l->setCellText(0,column,"");p.draw();auto emptyValues=values;emptyValues[column]="";cellRecord(c,game,"empty_cell_"+std::to_string(column),0,emptyValues,{true,true,true});l->setCellText(0,column,values[column]);p.draw();
 }
 l->setCellText(0,1,"7");record(c,game,"items_arriving","changed_cell_without_state_paint","unsupported");p.draw();cellRecord(c,game,"changed_cell_after_full_paint",0,{scientist,"7","MCP19"},{true,true,true});reset(scientist,"1","MCP19");
 for(auto*cell:l->_texts[0])cell->setVisible(false);l->invalidate();p.draw();auto omitted=record(c,game,"items_arriving","all_unknown_row_omitted");check(omitted["visible"]["rows"].size()==0&&s.agentUiState(ui)&&ui.offers("dismiss_items_arriving")&&ui.offers("open_arrival_base"),"all unknown row omitted with independent buttons retained");reset("","","");check(record(c,game,"items_arriving","all_empty_row_omitted")["visible"]["rows"].size()==0,"all empty row omitted");
 reset("","","");l->_texts[0][0]->setVisible(false);l->invalidate();p.draw();check(record(c,game,"items_arriving","unknown_and_empty_row_omitted")["visible"]["rows"].size()==0,"unknown plus empty row omitted");
 reset(scientist,"1","MCP19");
 // Later-column writes prove mapped RGB, not palette-index equality.
 {auto*earlier=l->_texts[0][1];auto*later=l->_texts[0][2];l->setCellText(0,2,"1");later->setX(earlier->getX());l->invalidate();p.draw();cellRecord(c,game,"same_rgb_later_column",0,{scientist,"1","1"},{true,true,true});
 const Uint8 color=p.color("text2");SDL_Color alias[256];for(unsigned i=0;i<256;++i)alias[i]=p.palette[i];for(unsigned i=0;i<6;++i)alias[100+i]=alias[color+i];l->setPalette(alias);for(auto*cell:l->_texts[0])cell->setPalette(alias);later->setColor(100);l->invalidate();p.draw();bool differing=false;earlier->lock();l->lock();for(int y=0;y<earlier->getHeight();++y)for(int x=0;x<earlier->getWidth();++x){const auto a=earlier->getPixel(x,y),b=l->getPixel(earlier->getX()+x,earlier->getY()+y);if(a&&b&&a!=b){const auto oldRgb=earlier->getPalette()[a],newRgb=l->getPalette()[b];if(oldRgb.r==newRgb.r&&oldRgb.g==newRgb.g&&oldRgb.b==newRgb.b)differing=true;}}l->unlock();earlier->unlock();check(differing,"real same RGB at different palette indices");cellRecord(c,game,"same_rgb_different_indices",0,{scientist,"1","1"},{true,true,true});
 later->setColor(game=="xcom1"?246:1);l->invalidate();p.draw();cellRecord(c,game,"changed_rgb_later_column",0,{scientist,"","1"},{true,false,true});}
 reset(scientist,"1","Alpha",3);auto dense=record(c,game,"items_arriving","dense_alpha_same_rgb_rows");check(dense["visible"]["rows"].size()==3,"independent cell witnesses retain dense Alpha rows");for(auto row:dense["visible"]["rows"]){check(!row["cells"][0].IsNull()&&!row["cells"][1].IsNull(),"dense row independently proves name and quantity");}
 l->setRowColor(1,game=="xcom1"?246:1);p.draw();auto changed=record(c,game,"items_arriving","changed_rgb_later_row");check(changed["visible"]["rows"][0]["cells"][2].IsNull()&&!changed["visible"]["rows"][0]["cells"][0].IsNull()&&!changed["visible"]["rows"][0]["cells"][1].IsNull(),"later row RGB change withholds only overlapped destination");
 reset(scientist,"1","MCP19",25);SDL_Event event={};event.type=SDL_MOUSEMOTION;Action hover(&event,1,1,0,0);hover.setMouseAction(20,54,l->getX(),l->getY());l->mouseOver(&hover,&s);p.draw();auto hovered=record(c,game,"items_arriving","hover_read_only");for(auto row:hovered["visible"]["rows"])check(!row["selected"].as<bool>(),"ordinary hover never serializes selected true");
 // Preserve genuine expensive draw failures. These are widget pixel scans,
 // never fabricated DTOs or edits to the production coverage counters.
 reset(scientist,"1","MCP19",4);for(unsigned r=0;r<3;++r)for(auto*cell:l->_texts[r]){cell->setWidth(640);cell->setHeight(400);}l->invalidate();p.draw();check(!l->_agentCoverageHealthy&&!s.agentUiState(ui),"actual oversized cell area exhausts work failclosed");record(c,game,"items_arriving","work_budget_exhausted","unsupported");reset(scientist,"1","MCP19");record(c,game,"items_arriving","work_budget_restored");
 {auto*cell=l->_texts[0][0];cell->setWidth(640);cell->setHeight(400);cell->draw();cell->lock();for(int y=0;y<400;++y)for(int x=0;x<640;++x)cell->setPixel(x,y,(x+y)%2?0:p.color("text2")+1);cell->unlock();l->invalidate();p.draw();check(!l->_agentCoverageHealthy&&l->_agentCoverageRuns>8192&&!s.agentUiState(ui),"actual excessive ink runs exhaust run cap failclosed");record(c,game,"items_arriving","run_budget_exhausted","unsupported");}reset(scientist,"1","MCP19");record(c,game,"items_arriving","run_budget_restored");
 lifecycle(s,p,s._txtTitle,s._btnOk,"dismiss_items_arriving","items_arriving",game,"dismiss");g.top=&s;p.draw();lifecycle(s,p,s._txtTitle,s._btnGotoBase,"open_arrival_base","items_arriving",game,"destination",true);check(g.destination=="base"&&g.destinationBase==&base&&g.destinationGlobe==&geo.globe,"Go To Base ordinary callback uses popup base and globe");g.top=&s;p.draw();auto before=g.pops,resetCount=geo.resets;click(s._btnOk,&s);check(g.pops==before+1&&geo.resets==resetCount,"ordinary GUI OK remains one pop");g.top=&s;p.draw();before=g.pops;resetCount=geo.resets;click(s._btnGotoBase,&s);check(g.pops==before+1&&geo.resets==resetCount+1,"ordinary GUI Go To Base remains timer reset and one pop");
 std::cout<<game<<" arrivals nullable-cell and fresh-authority matrix PASS\n";
}
#include "CompletionBudget.generated.inc"
void budget(){
 Completion::Game budgetGame;
 const auto table=[](unsigned count,bool unicode){AgentUiState candidate;candidate.screen="items_arriving";std::string cell;for(unsigned i=0;i<count;++i)cell+=unicode?"\xE2\x82\xAC":"A";std::string rows="[";for(unsigned r=0;r<3;++r){if(r)rows+=",";rows+="{\"handle\":\"r"+std::to_string(r)+"\",\"cells\":["+agentJsonString(cell)+","+agentJsonString(cell)+","+agentJsonString(cell)+"],\"selected\":false}";}rows+="]";candidate.visible="{\"title\":null,\"headings\":[null,null,null],\"rows\":"+rows+",\"scroll\":{\"up\":false,\"down\":false},\"buttons\":[{\"id\":\"ok\",\"label\":\"OK\"}]}";candidate.actions.emplace_back("dismiss_items_arriving","OK","visible_text");return candidate;};
 for(bool unicode:{false,true}){unsigned last=0;for(unsigned n=1;n<=(unicode?341u:1024u);++n){if(completionBudgetGate(table(n,unicode),&budgetGame))last=n;else break;}check(last>0&&completionBudgetGate(table(last,unicode),&budgetGame)&&!completionBudgetGate(table(last+1,unicode),&budgetGame),"exact provider budget boundary accepts last and rejects next");TableChannel c(unicode?"synthetic-arrival-unicode-boundary":"synthetic-arrival-ascii-boundary");auto candidate=table(last,unicode);c.state.project=[&](AgentUiState&out){out=candidate;return true;};c.read();c.events();}
}
}
int main(int argc,char**argv){
 check(argc==2,"source root");check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen SDL only");Options::baseXResolution=320;Options::baseYResolution=200;Options::keyOk=SDLK_RETURN;Options::keyCancel=SDLK_ESCAPE;Options::mousewheelSpeed=3;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}check(small.getHeight()==9&&small.getSpacing()==-1,"stock nine-pixel font and eight-pixel pitch");
 for(const std::string game:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+game+"/Language/en-US.yml");freshEmpty(big,small,lang,argv[1],game);arrivals(big,small,lang,argv[1],game);}budget();
 std::ofstream file(std::string(std::getenv("OPENXCOM_AGENT_TEST_PACKET_DIR"))+"/case-records.json");file<<"[";for(unsigned i=0;i<records.size();++i){if(i)file<<",";file<<records[i];}file<<"]";check(bool(file),"case references saved");
 check(agentPaintHealthy()&&!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"no full game or GUI executed");{Completion::Game g;Completion::GeoscapeState geo;Completion::Base base;Completion::ItemsArrivingState s(&g,&geo,&base);AgentUiState ui;agentObservePaint(-1);check(!s.agentUiState(ui),"paint failure rejects arrivals before widget or model access");}
 std::cout<<"AgentArrivalsProviderTestV20 PASS "<<checks<<" checks\n";
}
