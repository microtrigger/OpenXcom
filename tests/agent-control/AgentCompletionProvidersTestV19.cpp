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
struct ProductionCompleteState:Common {
 TextButton*_btnOk=nullptr,*_btnGotoBase=nullptr;Text*_txtMessage=nullptr;productionProgress_e _endType=PROGRESS_COMPLETE;
 bool _agentDestinationPaintFull=false;bool agentDestinationFull()const;std::vector<std::uint64_t> _agentStoredPaintRevisions;std::vector<std::uint64_t> agentPaintRevisions()const;void blit() override;
 using Common::Common;bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnOkClick(Action*);void btnGotoBaseClick(Action*);
};
struct UfoDetectedState:Common {
 int contactToken=0;void *_ufo=&contactToken;unsigned interceptCallbacks=0;void btnInterceptClick(Action*){++interceptCallbacks;} // explicit callback seam; real route tested separately

 TextButton*_btnIntercept=nullptr,*_btnCentre=nullptr,*_btnCancel=nullptr;Text*_txtUfo=nullptr,*_txtDetected=nullptr,*_txtHyperwave=nullptr;TextList*_lstInfo=nullptr,*_lstInfo2=nullptr;
 bool _agentDestinationPaintFull=false;bool agentDestinationFull()const;std::vector<std::uint64_t> _agentStoredPaintRevisions;std::vector<std::uint64_t> agentPaintRevisions()const;void blit() override;
 using Common::Common;bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnCancelClick(Action*);
};
#include "ItemsArrivingMethods.generated.inc"
#include "ProductionCompleteMethods.generated.inc"
#include "UfoDetectedMethods.generated.inc"
}
namespace {
#include "AgentManufactureChannel.inc"
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
 TextList*list(int w,int h,int x,int y,const std::string&id){auto*l=own(new TextList(w,h,x,y));l->initText(&big,&small,&lang);l->setColor(color(id));l->setBackground(&background);return l;}
 void draw(){if(render)render();else for(auto*s:surfaces)s->blit(&canvas);}
};
void setup(Completion::ItemsArrivingState&s,Paint&p){
 s._btnOk=p.button(142,16,16,166,"STR_OK");s._btnGotoBase=p.button(142,16,162,166,"STR_GO_TO_BASE");
 s._btnOk->onMouseClick((ActionHandler)&Completion::ItemsArrivingState::btnOkClick);s._btnGotoBase->onMouseClick((ActionHandler)&Completion::ItemsArrivingState::btnGotoBaseClick);
 s._txtTitle=p.text(310,17,5,18,p.lang.getString("STR_ITEMS_ARRIVING"),true);s._txtTitle->setAlign(ALIGN_CENTER);
 s._txtItem=p.text(114,9,16,34,p.lang.getString("STR_ITEM"));s._txtQuantity=p.text(54,9,152,34,p.lang.getString("STR_QUANTITY_UC"));s._txtDestination=p.text(112,9,212,34,p.lang.getString("STR_DESTINATION_UC"));
 s._lstTransfers=p.list(271,112,14,50,"text2");s._lstTransfers->setColumns(3,155,41,98);s._lstTransfers->setSelectable(true);s._lstTransfers->setMargin(2);
 for(unsigned i=0;i<25;++i){const std::string name=p.lang.getString(i%2?"STR_ENGINEER":"STR_SCIENTIST"),quantity=std::to_string(i+1);s._lstTransfers->addRow(3,name.c_str(),quantity.c_str(),"BASE");}
 s._surfaces=p.surfaces;s._game->screen.logical=&p.canvas;p.render=[&](){s.blit();};p.draw();
}
void setup(Completion::ProductionCompleteState&s,Paint&p,productionProgress_e end){
 s._endType=end;s._btnOk=p.button(118,18,40,154,"STR_OK");s._btnGotoBase=p.button(118,18,162,154,end==PROGRESS_CONSTRUCTION?"STR_GO_TO_BASE":"STR_ALLOCATE_MANUFACTURE");
 s._btnOk->onMouseClick((ActionHandler)&Completion::ProductionCompleteState::btnOkClick);s._btnGotoBase->onMouseClick((ActionHandler)&Completion::ProductionCompleteState::btnGotoBaseClick);
 s._txtMessage=p.text(246,110,37,35,"",true);s._txtMessage->setAlign(ALIGN_CENTER);s._txtMessage->setVerticalAlign(ALIGN_MIDDLE);s._txtMessage->setWordWrap(true);
 const char*key=end==PROGRESS_CONSTRUCTION?"STR_CONSTRUCTION_OF_FACILITY_AT_BASE_IS_COMPLETE":end==PROGRESS_COMPLETE?"STR_PRODUCTION_OF_ITEM_AT_BASE_IS_COMPLETE":end==PROGRESS_NOT_ENOUGH_MONEY?"STR_NOT_ENOUGH_MONEY_TO_PRODUCE_ITEM_AT_BASE":"STR_NOT_ENOUGH_SPECIAL_MATERIALS_TO_PRODUCE_ITEM_AT_BASE";
 s._txtMessage->setText(p.lang.getString(key).arg(p.lang.getString(end==PROGRESS_CONSTRUCTION?"STR_LABORATORY":p.master=="xcom1"?"STR_LASER_PISTOL":"STR_SONIC_PISTOL")).arg("Alpha"));s._surfaces=p.surfaces;s._game->screen.logical=&p.canvas;p.render=[&](){s.blit();};p.draw();
}
void setup(Completion::UfoDetectedState&s,Paint&p,bool hyper,bool detected){
 s._btnIntercept=p.button(200,12,28,hyper?136:118,"STR_INTERCEPT");s._btnCentre=p.button(200,12,28,hyper?152:134,"STR_CENTER_ON_UFO_TIME_5_SECONDS");s._btnCancel=p.button(200,12,28,hyper?168:150,"STR_CANCEL_UC");
 s._btnCancel->onMouseClick((ActionHandler)&Completion::UfoDetectedState::btnCancelClick);
 s._txtUfo=p.text(207,17,28,hyper?20:53,p.lang.getString("STR_UFO_").arg(7),true,"text");s._txtDetected=p.text(100,9,28,hyper?36:69,detected?std::string(p.lang.getString("STR_DETECTED")):"",false,"text");
 s._txtHyperwave=p.text(214,17,21,44,p.lang.getString("STR_HYPER_WAVE_TRANSMISSIONS_ARE_DECODED"),false,"text");s._txtHyperwave->setAlign(ALIGN_CENTER);s._txtHyperwave->setWordWrap(true);s._txtHyperwave->setVisible(hyper);
 s._lstInfo=p.list(217,32,28,hyper?60:80,"text");s._lstInfo2=p.list(217,32,28,96,"text");s._lstInfo2->setVisible(hyper);
 for(auto*l:{s._lstInfo,s._lstInfo2}){l->setColumns(2,77,140);l->setDot(true);for(unsigned i=0;i<4;++i)l->addRow(2,"SECRET", "Hidden detail");}s._surfaces=p.surfaces;s._game->screen.logical=&p.canvas;p.render=[&](){s.blit();};p.draw();
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
  for(unsigned i=0;i<v["rows"].size();++i){auto r=v["rows"][i];check(r.size()==3&&r["handle"].as<std::string>()=="r"+std::to_string(i)&&r["cells"].size()==3&&!r["selected"].as<bool>(),"read-only contiguous handles and three cells");for(auto cell:r["cells"])check(cell.IsScalar()&&cell.as<std::string>().size()<=1024,"bounded exact displayed row cell");}
 }else if(screen=="ufo_contact"){
  check(v["details_status"].as<std::string>()=="unavailable"&&ui.visible.find("Hidden")==std::string::npos&&ui.visible.find("SECRET")==std::string::npos,"UFO detail data is never exposed");
  for(auto a:ui.actions)check(a.id=="dismiss_ufo_contact"||a.id=="open_contact_interception","only explicit dismiss/intercept; center remains unsupported");
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
void arrivals(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game){
 Paint p(big,small,lang,source,game,"itemsArriving");Completion::Game g;Completion::GeoscapeState geo;Completion::Base base;Completion::ItemsArrivingState s(&g,&geo,&base);setup(s,p);AgentUiState ui;
 check(s.agentUiState(ui),"stock arrivals provider");validate(ui,"items_arriving");check(ui.rows.size()>=4,"stock arrivals proves meaningful positive rows");
 const auto stock=YAML::Load(ui.visible);check(stock["headings"][2].IsNull(),"native out-of-viewport destination heading stays null");
 for(unsigned i=0;i<ui.rows.size();++i)for(unsigned cell=0;cell<3;++cell)check(stock["rows"][i]["cells"][cell].as<std::string>()==s._lstTransfers->getCellText(ui.rows[i],cell),"all native stock arrival cells exact");
 negatives(s,p,s._txtTitle,s._btnOk,"title","dismiss_items_arriving","items_arriving",game,"arrivals");
 {TableChannel c(game+"-arrivals-table");connect(c,s);record(c,game,"items_arriving","stock_rows");
  AgentRequest forbidden;forbidden.action="choose_visible_row";forbidden.choice="r0";check(!s.agentDispatch(forbidden),"read-only arrivals cannot select rows");
  const auto oldCell=s._lstTransfers->getCellText(0,0);s._lstTransfers->setCellText(0,0,"ROW");s._lstTransfers->setCellText(0,0,oldCell);record(c,game,"items_arriving","list_aba_without_state_repaint","unsupported");s._lstTransfers->blit(&p.canvas);check(!s.agentUiState(ui),"isolated list repaint cannot certify the popup");p.draw();record(c,game,"items_arriving","list_aba_full_repaint");
  auto initial=s._lstTransfers->getScroll();c.queue("scroll_visible_list","down");receipt(c,p);check(s._lstTransfers->getScroll()==initial+1&&g.pops==0&&geo.resets==0,"arrival scroll changes only visible page");
  c.read();c.queue("scroll_visible_list","down");s._lstTransfers->scrollTo(3);s._lstTransfers->scrollTo(initial+1);p.draw();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable","arrival page ABA rejects stale authority");
  s._lstTransfers->scrollTo(0);p.draw();c.read();c.queue("dismiss_items_arriving");const auto old=s._lstTransfers->getCellText(0,0);s._lstTransfers->setCellText(0,0,"ROW");s._lstTransfers->setCellText(0,0,old);p.draw();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable"&&g.pops==0,"arrival row ABA invalidates dismissal too");
  s._lstTransfers->setCellText(0,2,std::string(80,'W'));p.draw();auto clipped=record(c,game,"items_arriving","clipped_destination_row");check(s.agentUiState(ui)&&std::find(ui.rows.begin(),ui.rows.end(),0)==ui.rows.end(),"truly clipped cell drops complete row");s._lstTransfers->setCellText(0,2,"BASE");p.draw();
  std::vector<AgentListRow> rows;check(s._lstTransfers->agentRows(rows)&&rows.size()>=4&&!rows[0].regions.empty(),"native row overlay witness");auto ink=rows[0].regions[0];Surface overlay(ink.w,ink.h,ink.x,ink.y);overlay.setPalette(p.palette);overlay.clear(1);overlay.blit(&p.canvas);g._cursor=&overlay;
  record(c,game,"items_arriving","full_row_overlay");check(s.agentUiState(ui)&&std::find(ui.rows.begin(),ui.rows.end(),rows[0].logicalIndex)==ui.rows.end()&&ui.rows.size()>=3,"only overlaid row omitted");g._cursor=nullptr;p.draw();
  const Surface*up=nullptr,*down=nullptr;s._lstTransfers->agentScrollControls(up,down);check(up&&down,"painted scroll arrows");overlay.setX(down->getX());overlay.setY(down->getY());overlay.setWidth(down->getWidth());overlay.setHeight(down->getHeight());overlay.blit(&p.canvas);g._fpsCounter=&overlay;auto arrow=record(c,game,"items_arriving","down_arrow_overlay");check(!arrow["visible"]["scroll"]["down"].as<bool>(),"covered down arrow omitted");AgentRequest request;request.action="scroll_visible_list";request.choice="down";check(!s.agentDispatch(request),"only admitted arrow can dispatch");g._fpsCounter=nullptr;p.draw();
  // Hover selection is ordinary GUI state, deliberately absent publicly.
  SDL_Event e={};e.type=SDL_MOUSEMOTION;Action a(&e,1,1,0,0);a.setMouseAction(20,54,s._lstTransfers->getX(),s._lstTransfers->getY());s._lstTransfers->mouseOver(&a,&s);p.draw();auto hovered=record(c,game,"items_arriving","hover_read_only");for(auto row:hovered["visible"]["rows"])check(!row["selected"].as<bool>(),"read-only selected is always false");
 }
 {TableChannel c(game+"-arrivals-single");connect(c,s);s._lstTransfers->clearList();s._lstTransfers->setHeight(112);s._lstTransfers->addRow(3,lang.getString("STR_SCIENTIST").c_str(),"1","MCP Manufacture");p.draw();auto one=record(c,game,"items_arriving","single_scientist_mcp_manufacture");check(one["visible"]["rows"].size()==0,"live fixture destination string clips Scientist arrival row");std::cout<<game<<" single_scientist_MCP_Manufacture_rows="<<one["visible"]["rows"].size()<<"\n";
 s._lstTransfers->setCellText(0,0,lang.getString("STR_MEDI_KIT"));p.draw();one=record(c,game,"items_arriving","single_medikit_mcp_manufacture");check(one["visible"]["rows"].size()==0,"live fixture destination string clips Medi-Kit arrival row");std::cout<<game<<" single_medikit_MCP_Manufacture_rows="<<one["visible"]["rows"].size()<<"\n";
 s._lstTransfers->setCellText(0,2,"MCP19");p.draw();one=record(c,game,"items_arriving","single_medikit_mcp19");check(one["visible"]["rows"].size()==1,"short approved MCP19 destination preserves Medi-Kit row");s._lstTransfers->setCellText(0,0,lang.getString("STR_SCIENTIST"));p.draw();one=record(c,game,"items_arriving","single_scientist_mcp19");check(one["visible"]["rows"].size()==1,"short MCP19 destination preserves Scientist row");s._lstTransfers->setCellText(0,2,"Alpha");p.draw();one=record(c,game,"items_arriving","single_scientist_alpha_descender");check(one["visible"]["rows"].size()==1,"single Alpha descender fits when no next row overpaints it");std::cout<<game<<" single_scientist_Alpha_rows="<<one["visible"]["rows"].size()<<"\n";
 s._lstTransfers->clearList();s._lstTransfers->setHeight(112);for(unsigned i=0;i<25;++i){const auto q=std::to_string(i+1);s._lstTransfers->addRow(3,lang.getString("STR_SCIENTIST").c_str(),q.c_str(),"Alpha");}p.draw();one=record(c,game,"items_arriving","dense_alpha_descender_negative");check(one["visible"]["rows"].size()==0,"dense Alpha descenders remain clipped read-only rows");
 for(unsigned i=0;i<25;++i){s._lstTransfers->setCellText(i,2,"MCP19");}p.draw();one=record(c,game,"items_arriving","dense_scientist_mcp19");check(one["visible"]["rows"].size()>=4,"dense MCP19 Scientist rows remain meaningfully visible");
 s._lstTransfers->clearList();s._lstTransfers->setHeight(112);s._lstTransfers->addRow(3,lang.getString("STR_SCIENTIST").c_str(),"1","BASE");p.draw();record(c,game,"items_arriving","single_stock_restore");}
 lifecycle(s,p,s._txtTitle,s._btnOk,"dismiss_items_arriving","items_arriving",game,"dismiss");g.top=&s;p.draw();lifecycle(s,p,s._txtTitle,s._btnGotoBase,"open_arrival_base","items_arriving",game,"destination",true);check(g.destination=="base"&&g.destinationBase==&base&&g.destinationGlobe==&geo.globe,"arrival destination ordinary callback receives correct test-seam base/globe");
 g.top=&s;p.draw();auto before=g.pops;auto reset=geo.resets;click(s._btnOk,&s);check(g.pops==before+1&&geo.resets==reset,"ordinary arrival OK one-pop without timer reset");g.top=&s;p.draw();before=g.pops;reset=geo.resets;click(s._btnGotoBase,&s);check(g.pops==before+1&&geo.resets==reset+1&&g.destination=="base","ordinary arrival Go To Base timer reset preserved");
 std::cout<<game<<" arrivals positive_rows="<<stock["rows"].size()<<" PASS\n";
}
void production(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game){
 const productionProgress_e types[]={PROGRESS_CONSTRUCTION,PROGRESS_COMPLETE,PROGRESS_NOT_ENOUGH_MONEY,PROGRESS_NOT_ENOUGH_MATERIALS};const char*variants[]={"construction","complete","money","materials"};
 for(unsigned i=0;i<4;++i){Paint p(big,small,lang,source,game,"geoManufactureComplete");Completion::Game g;Completion::GeoscapeState geo;Completion::Base base;Completion::ProductionCompleteState s(&g,&geo,&base);setup(s,p,types[i]);AgentUiState ui;check(s.agentUiState(ui),"production stock provider");validate(ui,"production_complete");
  {TableChannel c(game+"-production-"+variants[i]+"-caption");connect(c,s);auto state=record(c,game,"production_complete",std::string(variants[i])+"_stock_caption");check(!state["visible"]["message"].IsNull()&&state["visible"]["message"].as<std::string>()==s._txtMessage->getText(),"actual complete wrapped production message");check(state["visible"]["buttons"].size()==2&&state["visible"]["buttons"][1]["label"].as<std::string>()==std::string(lang.getString(i==0?"STR_GO_TO_BASE":"STR_ALLOCATE_MANUFACTURE")),"actual correct destination caption variant");}
  if(i<2)negatives(s,p,s._txtMessage,s._btnOk,"message","dismiss_production_complete","production_complete",game,variants[i]);
  lifecycle(s,p,s._txtMessage,s._btnGotoBase,"open_production_destination","production_complete",game,variants[i],true);check(g.destination==(i==0?"base":"manufacture")&&g.destinationBase==&base,"production destination policy in unchanged ordinary callback");
  g.top=&s;p.draw();auto before=g.pops,reset=geo.resets;click(s._btnOk,&s);check(g.pops==before+1&&geo.resets==reset,"ordinary production OK one-pop no timer reset");g.top=&s;p.draw();before=g.pops;reset=geo.resets;click(s._btnGotoBase,&s);check(g.pops==before+1&&geo.resets==reset+1,"ordinary production destination timer reset once");
  if(i==1){g.top=&s;p.draw();lifecycle(s,p,s._txtMessage,s._btnOk,"dismiss_production_complete","production_complete",game,"dismiss");}
 }
 std::cout<<game<<" all four production caption/message variants PASS\n";
}
void ufo(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game){
 for(unsigned i=0;i<3;++i){bool hyper=i==2,detected=i!=1;std::string variant=hyper?"hyperwave":detected?"detected":"redisplay";Paint p(big,small,lang,source,game,"UFOInfo",hyper);Completion::Game g;Completion::GeoscapeState geo;Completion::Base base;Completion::UfoDetectedState s(&g,&geo,&base);setup(s,p,hyper,detected);AgentUiState ui;check(s.agentUiState(ui),"stock UFO provider");validate(ui,"ufo_contact");
  {TableChannel c(game+"-ufo-"+variant+"-caption");connect(c,s);auto state=record(c,game,"ufo_contact",variant+"_stock_caption");check(state["visible"]["buttons"].size()==3,"all three actual UFO captions visible");check(state["visible"]["detected_label"].as<std::string>()==(detected?std::string(lang.getString("STR_DETECTED")):""),"detected/empty label exact");check(hyper?!state["visible"]["hyperwave_label"].IsNull():state["visible"]["hyperwave_label"].IsNull(),"hyperwave drawn only in its actual layout");AgentRequest r;for(const char*name:{"intercept","center","open_intercept","center_ufo"}){r.action=name;check(!s.agentDispatch(r),"unrecognized aliases and center remain unsupported");}}
  negatives(s,p,s._txtUfo,s._btnCancel,"title","dismiss_ufo_contact","ufo_contact",game,variant);lifecycle(s,p,s._txtUfo,s._btnCancel,"dismiss_ufo_contact","ufo_contact",game,variant);check(geo.resets==0&&geo.globeReads==0&&g.pushes==0,"UFO dismissal never reads globe or resets timer");
  g.top=&s;p.draw();auto before=g.pops;click(s._btnCancel,&s);check(g.pops==before+1&&geo.resets==0,"ordinary UFO Cancel exact one-pop path");
 }
 std::cout<<game<<" UFO normal/redisplay/hyperwave PASS\n";
}
void pendingDiagnostic(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game){
 Paint p(big,small,lang,source,game,"geoManufactureComplete");Completion::Game g;Completion::GeoscapeState geo;Completion::Base base;Completion::ProductionCompleteState s(&g,&geo,&base);setup(s,p,PROGRESS_COMPLETE);
 TableChannel c(game+"-pending-diagnostic");connect(c,s);c.read();c.queue("dismiss_production_complete");const auto action=c.request;
 std::vector<std::string> log=c.schedule;log.push_back("{\"request\":"+agentJsonString(action)+",\"response_file\":null}");
 auto pending=status(c);check(YAML::Load(pending)["status"].as<std::string>()=="pending"&&g.pops==0,"S pending does not execute callback");log.push_back(c.schedule.back());
 c.events();log.push_back(c.schedule.back());check(g.pops==0&&geo.resets==0,"pending V is mutation-neutral");
 const auto completed=receipt(c,p);log.push_back(c.schedule.back());check(g.pops==1&&geo.resets==0,"S/V interleaving preserves exactly one ordinary close");
 c.send(action);c.controller->poll();check(c.receive()==completed&&g.pops==1,"interleaved duplicate never repeats close");log.push_back(c.schedule.back());
 std::string json="[";for(const auto&v:log){if(json.size()>1)json+=",";json+=v;}c.save(game+"-pending-diagnostic-all-sends.json",json+"]");
}
#include "CompletionBudget.generated.inc"
void budget(){
 Completion::Game budgetGame;
 const auto table=[](unsigned count,bool unicode){AgentUiState candidate;candidate.screen="items_arriving";std::string cell;for(unsigned i=0;i<count;++i)cell+=unicode?"\xE2\x82\xAC":"A";std::string rows="[";for(unsigned r=0;r<3;++r){if(r)rows+=",";rows+="{\"handle\":\"r"+std::to_string(r)+"\",\"cells\":["+agentJsonString(cell)+","+agentJsonString(cell)+","+agentJsonString(cell)+"],\"selected\":false}";}rows+="]";candidate.visible="{\"title\":null,\"headings\":[null,null,null],\"rows\":"+rows+",\"scroll\":{\"up\":false,\"down\":false},\"buttons\":[{\"id\":\"ok\",\"label\":\"OK\"}]}";candidate.actions.emplace_back("dismiss_items_arriving","OK","visible_text");return candidate;};
 for(bool unicode:{false,true}){unsigned last=0;for(unsigned n=1;n<=(unicode?341u:1024u);++n){const auto candidate=table(n,unicode);if(completionBudgetGate(candidate,&budgetGame))last=n;else break;}check(last>0&&completionBudgetGate(table(last,unicode),&budgetGame)&&!completionBudgetGate(table(last+1,unicode),&budgetGame),"exact extracted provider budget accepts boundary and fails next increment");TableChannel bound(unicode?"synthetic-arrival-unicode-boundary":"synthetic-arrival-ascii-boundary");auto candidate=table(last,unicode);bound.state.project=[&](AgentUiState&out){out=candidate;return true;};check(YAML::Load(bound.read())["state"].as<std::string>()=="items_arriving","bounded maximum synthetic arrival reaches Controller");bound.events();}

 // Transport-only seam: adversarial valid public UTF-8 payload near the
 // provider ceiling. It is NOT claimed as stock font/layout admission.
 TableChannel c("synthetic-completion-budget");AgentUiState ui;ui.screen="production_complete";std::string message;for(unsigned i=0;i<1024/3;++i)message+="\xE2\x82\xAC";std::string buttons="[";
 for(unsigned i=0;i<2;++i){if(i)buttons+=",";buttons+="{\"id\":\""+std::string(i?"destination":"ok")+"\",\"label\":"+agentJsonString(message)+"}";ui.actions.emplace_back(i?"open_production_destination":"dismiss_production_complete",message,"visible_text");}buttons+="]";ui.visible="{\"message\":"+agentJsonString(message)+",\"buttons\":"+buttons+"}";check(ui.payload().size()<=9000,"max-string production synthetic payload below9000");c.state.project=[&](AgentUiState&out){out=ui;return true;};
 for(unsigned i=0;i<12;++i){ui.visible="{\"message\":"+agentJsonString(std::to_string(i)+message.substr(3))+",\"buttons\":"+buttons+"}";auto state=c.read();check(state.size()<=32768,"large whole state within wire cap");}
 std::uint64_t after=0;unsigned total=0,pages=0;bool more=true;std::string firstRequest,firstResponse;
 while(more){const std::string request="V "+token(c.challenge++)+" "+c.epoch+" "+std::to_string(after)+" 16";c.send(request);c.controller->poll();auto raw=c.receive();check(!raw.empty()&&raw.size()<=49152,"actual large V page bounded49152");if(!pages){firstRequest=request;firstResponse=raw;}auto events=YAML::Load(raw);check(events["events"].size()==2&&events["next_cursor"].as<std::uint64_t>()==2*(pages+1),"exact budget cursor chain2,4,6,8,10,12");for(auto e:events["events"]){check(e["seq"].as<std::uint64_t>()==after+1,"retained event sequence contiguous");after=e["seq"].as<std::uint64_t>();++total;}more=events["next_cursor"].as<std::uint64_t>()<events["newest"].as<std::uint64_t>();++pages;check(pages<=12,"bounded pagination reaches terminal");}
 check(total==12&&pages==6,"all12 large presentations retained across six actual pages");
 c.send("V "+token(c.challenge++)+" "+c.epoch+" "+std::to_string(after)+" 16");c.controller->poll();const auto empty=YAML::Load(c.receive());check(empty["events"].size()==0&&empty["after"].as<std::uint64_t>()==12&&empty["next_cursor"].as<std::uint64_t>()==12&&empty["newest"].as<std::uint64_t>()==12&&!empty["gap"].as<bool>(),"explicit empty remainder preserves cursor12");
 c.send(firstRequest);c.controller->poll();check(c.receive()==firstResponse&&c.state.calls==0,"exact first-page replay is byte-identical and V is mutation-neutral");
}
}
int main(int argc,char**argv){
 check(argc==2,"source root");check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen SDL only");Options::baseXResolution=320;Options::baseYResolution=200;Options::keyOk=SDLK_RETURN;Options::keyCancel=SDLK_ESCAPE;Options::mousewheelSpeed=3;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}check(small.getHeight()==9&&small.getSpacing()==-1,"actual nine-pixel stock font eight-pixel row pitch");
 for(const std::string game:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+game+"/Language/en-US.yml");arrivals(big,small,lang,argv[1],game);production(big,small,lang,argv[1],game);ufo(big,small,lang,argv[1],game);pendingDiagnostic(big,small,lang,argv[1],game);}budget();
 std::ofstream file(std::string(std::getenv("OPENXCOM_AGENT_TEST_PACKET_DIR"))+"/case-records.json");file<<"[";for(unsigned i=0;i<records.size();++i){if(i)file<<",";file<<records[i];}file<<"]";check(bool(file),"case references saved");
 check(agentPaintHealthy()&&!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"no full game or GUI executed");{Completion::Game g;Completion::GeoscapeState geo;Completion::Base base;Completion::ItemsArrivingState arrival(&g,&geo,&base);Completion::ProductionCompleteState production(&g,&geo,&base);Completion::UfoDetectedState ufo(&g,&geo,&base);AgentUiState ui;agentObservePaint(-1);g.top=&arrival;check(!arrival.agentUiState(ui),"failed paint rejects arrivals before widget/model access");g.top=&production;check(!production.agentUiState(ui),"failed paint rejects production before widget/model access");g.top=&ufo;check(!ufo.agentUiState(ui),"failed paint rejects UFO before widget/model access");}
 std::cout<<"AgentCompletionProvidersTestV19 PASS "<<checks<<" checks\n";
}
