// SPDX-License-Identifier: GPL-3.0-or-later
// Literal ItemsArriving constructor/provider/callbacks; explicit domain/render/Game seams.
// Constructor transfer deletion/reuse is distinct from inventory delivery; Basescape is capture-only.
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
#include <vector>
#define private public
#include "../../src/Interface/TextList.h"
#undef private
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
static bool forbidModelRead=false;static unsigned modelReads=0,deletedTransfers=0,reuseCalls=0;
static void modelRead(){++modelReads;if(forbidModelRead){std::cerr<<"FAIL projection model getter\n";std::abort();}}
enum {TRANSFER_ITEM,TRANSFER_PERSONNEL,BT_NONE,BT_OTHER};
struct Transfer {int hours,quantity,type;std::string name;Transfer(int h,int q,const std::string&n,int t=TRANSFER_PERSONNEL):hours(h),quantity(q),type(t),name(n){}~Transfer(){++deletedTransfers;}
 int getHours(){modelRead();return hours;}int getQuantity(){modelRead();return quantity;}int getType(){modelRead();return type;}std::string getItems(){modelRead();return "ITEM";}std::string getName(Language*){modelRead();return name;}};
struct RuleItem {int getBattleType(){modelRead();return BT_NONE;}};
struct Craft {void reuseItem(const std::string&){++reuseCalls;}};
struct Base {std::string name;std::vector<Transfer*> transfers;std::vector<Craft*> crafts;std::vector<Transfer*> *getTransfers(){modelRead();return &transfers;}std::vector<Craft*> *getCrafts(){modelRead();return &crafts;}std::string getName(){modelRead();return name;}};
struct SavedGame {std::vector<Base*> bases;std::vector<Base*> *getBases(){modelRead();return &bases;}};
struct Globe{};
static std::vector<std::string> events;
struct GeoscapeState {Globe globe;void timerReset(){events.push_back("timer");}Globe *getGlobe(){return &globe;}};
struct BasescapeState {Base *base;Globe *globe;BasescapeState(Base*b,Globe*g):base(b),globe(g){}};
struct Screen {Surface *surface;Surface *getSurface(){return surface;}};
struct Mod {Surface *background;RuleItem item;Surface *getSurface(const std::string&){return background;}RuleItem *getItem(const std::string&,bool){modelRead();return &item;}};
struct Game {
 SavedGame saved;SavedGame *getSavedGame(){modelRead();return &saved;}Screen screen;Mod mod;Language *language;Font *big,*small;SDL_Color *palette;
 const void *top=nullptr;int pops=0,pushes=0;Base *opened=nullptr;Globe *openedGlobe=nullptr;bool clear=true,cursorVisible=false,fpsVisible=false;SDL_Rect cursor={},fps={};
 Screen *getScreen(){return &screen;}Mod *getMod(){return &mod;}Language *getLanguage(){return language;}bool isState(const void*s)const{return top==s;}
 void popState(){++pops;events.push_back("pop");top=nullptr;}void pushState(BasescapeState*s){++pushes;events.push_back("push");opened=s->base;openedGlobe=s->globe;delete s;}
 bool agentNonCursorOverlayClear(const AgentUiState&u)const{if(!clear)return false;for(const auto&r:u.regions)if(fpsVisible&&r.x<fps.x+fps.w&&r.x+r.width>fps.x&&r.y<fps.y+fps.h&&r.y+r.height>fps.y)return false;return true;}
 bool agentOverlayClear(const AgentUiState&u)const{if(!agentNonCursorOverlayClear(u))return false;for(const auto&r:u.regions)if(cursorVisible&&r.x<cursor.x+cursor.w&&r.x+r.width>cursor.x&&r.y<cursor.y+cursor.h&&r.y+r.height>cursor.y)return false;return true;}
};
static Game *currentGame;
struct State:OpenXcom::State {
 Game *_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;std::function<void()> paintHook;
 void setInterface(const std::string&){}void centerAllSurfaces(){}LocalizedText tr(const std::string&s)const{return _game->language->getString(s);}
 void add(Surface*s,const std::string&,const std::string&){s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);_surfaces.push_back(s);}
 void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);if(paintHook)paintHook();}
};
struct ItemsArrivingState:State {
 GeoscapeState *_state;Base *_base;TextButton *_btnOk,*_btnGotoBase;Window *_window;Text *_txtTitle,*_txtItem,*_txtQuantity,*_txtDestination;TextList *_lstTransfers;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 ItemsArrivingState(GeoscapeState*);~ItemsArrivingState();void btnOkClick(Action*);void btnGotoBaseClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void paint(){_window->think();for(int i=0;i<30;++i)_window->popup();blit();}
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "ItemsArriving.generated.inc"
}
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==3,"source and log arguments");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen native widgets");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string master:{"xcom1","xcom2"}){
  Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;Fixture::GeoscapeState geo;
  Fixture::Base a,b;a.name="FIRST BASE";b.name="SECOND BASE";Fixture::Craft craft;a.crafts={&craft};a.transfers={new Fixture::Transfer(0,3,"CURRENT ITEM",Fixture::TRANSFER_ITEM),new Fixture::Transfer(4,99,"PENDING SECRET")};b.transfers={new Fixture::Transfer(0,2,"CURRENT PERSON")};game.saved.bases={&a,&b};
  unsigned deleted=Fixture::deletedTransfers,reused=Fixture::reuseCalls;
  Fixture::ItemsArrivingState state(&geo);game.top=&state;
  check(Fixture::deletedTransfers==deleted+2&&Fixture::reuseCalls==reused+1&&a.transfers.size()==1&&b.transfers.empty(),"literal constructor deletes completed transfers and reuses item once, retains pending");
  check(state._base==&b,"native last processed base retained for callback");Fixture::forbidModelRead=true;unsigned reads=Fixture::modelReads;AgentUiState ui;
  check(!state.agentUiState(ui),"before presentation refuses");state.paint();check(state.agentUiState(ui),"actual stock constructor positive first");auto v=YAML::Load(ui.visible);
  check(v["headings"][2].IsNull(),"destination heading out-of-screen allocation remains null");
  check(v["rows"].size()==2&&v["rows"][0]["cells"][0].as<std::string>()=="CURRENT ITEM"&&v["rows"][0]["cells"][1].as<std::string>()=="3"&&v["rows"][0]["cells"][2].as<std::string>()=="FIRST BASE","native three-cell constructor order");
  check(ui.visible.find("PENDING SECRET")==std::string::npos,"pending transfer not visible");a.name="HIDDEN MODEL NAME";a.transfers[0]->name="CHANGED PENDING SECRET";
  for(int i=0;i<3;++i)check(state.agentUiState(ui)&&ui.visible.find("HIDDEN MODEL")==std::string::npos,"projection uses current widgets not changed models");
  auto*list=state._lstTransfers;std::vector<AgentArrivalRow> native;std::uint64_t epoch=0;
  check(list->agentArrivalLogicalRows(native,epoch)&&native.size()==2&&epoch>0,"logical capture epoch produced by draw");
  bool oldCoverage=list->_agentCoverageHealthy;list->_agentCoverageHealthy=false;check(list->agentArrivalLogicalRows(native,epoch),"legacy RGB coverage health does not gate logical observer");list->_agentCoverageHealthy=oldCoverage;
  check(native[0].bounds.h==8&&small.getHeight()==9&&small.getSpacing()==-1,"stock9 minus1 advance8");
  check(native[0].cells[2].known&&list->_texts[0][2]->getX()==198,"native partial final column known");int width=0;for(auto&r:native[0].cells[2].regions)width=std::max(width,int(r.w));check(width==73,"final25px clip retains73px allocation");
  AgentRequest q;q.action="dismiss_items_arriving";AgentFrameCommand frame;check(frame.stage(q,ui,&state,1,1,100,100),"stage authority before identical repaint");std::uint64_t oldEpoch=epoch;
  list->invalidate();state.blit();check(state.agentUiState(ui)&&list->agentArrivalLogicalRows(native,epoch)&&epoch!=oldEpoch&&!frame.matchesFrame(101,1,1,&state,ui),"actual redraw changes capture epoch and invalidates old action");
  oldEpoch=epoch;state.blit();check(list->agentArrivalLogicalRows(native,epoch)&&epoch==oldEpoch,"cached blit preserves capture epoch");
  auto*cell=list->_texts[0][0];cell->setText("HIDDEN CELL SENTINEL");list->invalidate();state.blit();check(state.agentUiState(ui),"paint sentinel before hide");cell->setVisible(false);check(!state.agentUiState(ui),"hideafterpaint stale");list->invalidate();state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull()&&ui.visible.find("HIDDEN CELL SENTINEL")==std::string::npos,"fresh hidden cell null not cached value");
  check(YAML::Load(ui.visible)["rows"][0]["cells"][1].as<std::string>()=="3","unhidden neighbor retained");cell->setVisible(true);list->invalidate();state.blit();cell->setVisible(false);cell->setVisible(true);check(!state.agentUiState(ui),"cell visibility ABA stale");list->invalidate();state.blit();
  int x=cell->getX();cell->setX(x+1);cell->setX(x);check(!state.agentUiState(ui),"cell geometry ABA stale");list->invalidate();state.blit();
  for(const std::string text:{std::string(500,'W'),std::string("LINE\nLINE"),std::string("LINE")+char(2)+"SMALL",std::string("\xF0\x9F\x99\x82")}){list->setCellText(0,0,text);check(!state.agentUiState(ui),"cell text stale beforepaint");state.blit();check(state.agentUiState(ui),"logical overflow/multiline/missingglyph admitted");auto rows=YAML::Load(ui.visible)["rows"];check(rows[0]["cells"][0].as<std::string>()==text&&rows[1]["cells"][0].as<std::string>()=="CURRENT PERSON","current logical text and following native row association");check(list->agentArrivalLogicalRows(native,epoch)&&native[1].bounds.y==list->getY()+list->_texts[1][0]->getY(),"following row uses actual allocated Y");}
  list->setCellText(0,0,std::string(1024,'X'));state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>()==std::string(1024,'X'),"1024byte boundary exact");list->setCellText(0,0,std::string(1025,'X'));state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull(),"overlength null not truncate");list->setCellText(0,0,"CURRENT ITEM");state.blit();
  SDL_FillRect(cell->getSurface(),nullptr,0);SDL_FillRect(list->getSurface(),nullptr,0);check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>()=="CURRENT ITEM","erased ink not logical admission");
  SDL_Rect tiny={0,0,1,1};SDL_SetClipRect(list->getSurface(),&tiny);state.blit();check(!state.agentUiState(ui),"local list clip refusal");SDL_SetClipRect(list->getSurface(),nullptr);check(!state.agentUiState(ui),"cliprestore not fresh");list->invalidate();state.blit();
  SDL_SetClipRect(cell->getSurface(),&tiny);list->invalidate();state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull()&&ui.visible.find("CURRENT ITEM")==std::string::npos&&YAML::Load(ui.visible)["rows"][0]["cells"][1].as<std::string>()=="3","fresh clippedcell null, priorvalue absent, neighbor retained");SDL_SetClipRect(cell->getSurface(),nullptr);check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull()&&ui.visible.find("CURRENT ITEM")==std::string::npos&&YAML::Load(ui.visible)["rows"][0]["cells"][1].as<std::string>()=="3","cliprestore without freshdraw retains safe null not oldvalue");list->invalidate();state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>()=="CURRENT ITEM","actual freshdraw restores current cell value");
  auto*later=list->_texts[0][1];int lx=later->getX();later->setX(cell->getX());list->invalidate();state.blit();check(!state.agentUiState(ui),"same-list later cell overlap structural refusal");later->setX(lx);list->invalidate();state.blit();
  Surface cover(5,4,list->getX()+cell->getX(),list->getY()+cell->getY());cover.setPalette(palette);cover.clear(90);state._surfaces.push_back(&cover);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull(),"real laterwidget occlusion withholds current cell");state._surfaces.pop_back();state.blit();
  game.fpsVisible=true;game.fps={Sint16(list->getX()+cell->getX()),Sint16(list->getY()+cell->getY()),5,4};check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull(),"FPS current cell occlusion");game.fpsVisible=false;game.cursorVisible=true;game.cursor=game.fps;check(state.agentUiState(ui)&&!YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull(),"cursor does not gate logical caption");game.cursorVisible=false;
  state._modal=state._btnOk;check(!state.agentUiState(ui),"modal refusal");state._modal=nullptr;game.top=nullptr;check(!state.agentUiState(ui),"inactive refusal");game.top=&state;
  Surface other(320,200);other.setPalette(palette);game.screen.surface=&other;check(!state.agentUiState(ui),"wrong destination refusal");q.action="dismiss_items_arriving";int beforeWrong=game.pops;check(!state.agentDispatch(q)&&game.pops==beforeWrong,"equalgeometry replacement destination cannot dispatch oldpaint");game.screen.surface=&canvas;check(state.agentUiState(ui),"restoring original still-current painted destination retains valid capture");game.screen.surface=&other;state.blit();check(state.agentUiState(ui),"actual statepaint on replacement destination admits current UI");game.screen.surface=&canvas;check(!state.agentUiState(ui)&&!state.agentDispatch(q)&&game.pops==beforeWrong,"old destination after newerpaint cannot reuse other target capture");state.blit();check(state.agentUiState(ui),"actual repaint original destination restores current UI");
  SDL_Rect clipped={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clipped);state.blit();check(!state.agentUiState(ui),"destinationclip refusal");SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();
  game.cursorVisible=true;game.cursor={20,170,5,5};q.action="dismiss_items_arriving";int untouched=game.pops;check(!state.agentDispatch(q)&&game.pops==untouched,"cursor on native OK hitbox refuses dispatch");game.cursorVisible=false;
  list->setCellText(0,0,std::string("COLOR")+char(1)+"FLIP");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>()=="COLORFLIP","only colorflip marker normalized from current logical text");
  list->setCellText(0,0,std::string(1,char(1)));state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>().empty(),"colorflip-only caption becomes known empty while other cells preserve row");
  check(Fixture::modelReads==reads&&Fixture::deletedTransfers==deleted+2&&Fixture::reuseCalls==reused+1,"all projection mutations remain UI only, no transfer replay");
  Fixture::events.clear();q.action="open_arrival_base";check(state.agentDispatch(q)&&game.pops==1&&game.pushes==1&&game.opened==&b&&game.openedGlobe==&geo.globe,"native go-to-base captures last native base and globe");check(Fixture::events==std::vector<std::string>({"timer","pop","push"}),"native timer pop push order");check(!state.agentDispatch(q)&&game.pushes==1,"inactive refusal not ledger replay qualification");state.release();Fixture::forbidModelRead=false;delete a.transfers[0];a.transfers.clear();
  game.saved.bases.clear();
  {
   Fixture::ItemsArrivingState report(&geo);game.top=&report;auto*l=report._lstTransfers;l->addRow(3,"",""," ");l->addRow(3,"NEIGHBOR","2","BASE");for(int i=0;i<30;++i)l->addRow(3,("OFFSCREEN SECRET "+std::to_string(i)).c_str(),"4","BASE");report.paint();
   check(report.agentUiState(ui)&&ui.visible.find("OFFSCREEN SECRET 29")==std::string::npos,"bounded current viewport excludes far sentinel");check(YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>().empty(),"known empty current cell is not null");
   check(ui.context.size()==7,"flags plus private epoch and list geometry context");
   l->setHeight(13);check(l->_visibleRows>=2,"native partialrow included in draw iteration");auto*off=l->_texts[1][1];int ox=off->getX();off->setX(0);l->invalidate();report.blit();check(!report.agentUiState(ui),"malformed drawn partialrow refuses before vertical omission");off->setX(ox);l->setHeight(112);l->invalidate();report.blit();check(report.agentUiState(ui),"restored partialrow allocation freshdraw");
   auto*old=l->_texts[1][0];auto*replacement=new Text(old->getWidth(),old->getHeight(),old->getX(),old->getY());replacement->setPalette(palette);replacement->initText(&big,&small,&language);replacement->setText("REPLACED CURRENT");l->_texts[1][0]=replacement;
   check(!report.agentUiState(ui),"cellpointer replacement refuses retained capture");l->invalidate();report.blit();check(report.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][1]["cells"][0].as<std::string>()=="REPLACED CURRENT","freshdraw captures replacement identity");delete old;
   q.action="dismiss_items_arriving";AgentFrameCommand prior;check(prior.stage(q,ui,&report,1,1,100,100),"stage capturetoken authority");l->invalidate();report.blit();check(report.agentUiState(ui)&&!prior.matchesFrame(101,1,1,&report,ui),"identical renderedvalues but newdrawtoken invalidates old authority");
   l->scrollDown(false);check(!report.agentUiState(ui),"scroll stale until repaint");report.blit();check(report.agentUiState(ui)&&ui.visible.find("OFFSCREEN SECRET 29")==std::string::npos,"fresh scroll remains viewportonly");
   l->scrollUp(true);l->setHeight(13);report.blit();check(l->agentArrivalLogicalRows(native,epoch),"short viewport accepts fullyvisible rows only");for(const auto&r:native)check(r.bounds.y+r.bounds.h<=l->getY()+l->getHeight(),"partialvertical row withheld");report.release();
  }
  {
   Fixture::ItemsArrivingState dirty(&geo);game.top=&dirty;dirty._lstTransfers->addRow(3,"NEVER PAINTED HIDDEN","2","BASE");dirty._lstTransfers->_texts[0][0]->setVisible(false);dirty.paint();check(!dirty.agentUiState(ui),"neverpainted dirtyhidden conservatively refuses");dirty.release();
  }
  Fixture::ItemsArrivingState empty(&geo);game.top=&empty;Fixture::forbidModelRead=true;empty.paint();check(empty.agentUiState(ui)&&YAML::Load(ui.visible)["rows"].size()==0,"empty actual constructor no invented rows");
  std::uint64_t emptyEpoch=0;check(empty._lstTransfers->agentArrivalLogicalRows(native,emptyEpoch)&&native.empty()&&emptyEpoch>0,"empty row capture still has currentdrawtoken");empty._lstTransfers->invalidate();empty.blit();check(empty._lstTransfers->agentArrivalLogicalRows(native,epoch)&&epoch!=emptyEpoch,"empty redraw changes private epoch");
  q.action="dismiss_items_arriving";int pops=game.pops;Fixture::events.clear();check(empty.agentDispatch(q)&&game.pops==pops+1&&Fixture::events==std::vector<std::string>({"pop"}),"native OK onlypop");empty.release();Fixture::forbidModelRead=false;
  // Valid same-object shape, only observer opt-in changes.
  TextList paired(271,112,14,50);paired.setPalette(palette);paired.initText(&big,&small,&language);paired.setColumns(3,155,41,98);paired.setSelectable(true);paired.setBackground(&background);paired.setMargin(2);paired.addRow(3,"PAIR","3","BASE");paired.blit(&canvas);epoch=999;
  check(!paired.agentArrivalLogicalRows(native,epoch)&&native.empty(),"valid3column defaultoff");paired.agentEnableArrivalLogicalCells();check(!paired.agentArrivalLogicalRows(native,epoch),"enable needs newdraw");paired.blit(&canvas);check(paired.agentArrivalLogicalRows(native,epoch)&&native.size()==1&&native[0].cells[0].text=="PAIR"&&native[0].cells[2].text=="BASE","sameobject freshoptin exact logical cells");
  {
   TextList narrow(190,112,14,50);narrow.setPalette(palette);narrow.initText(&big,&small,&language);narrow.setColumns(3,155,41,98);narrow.setSelectable(true);narrow.setBackground(&background);narrow.setMargin(2);narrow.agentEnableArrivalLogicalCells();narrow.addRow(3,"VISIBLE","2","WHOLLY OFFSCREEN SECRET");narrow.blit(&canvas);check(narrow.agentArrivalLogicalRows(native,epoch)&&native.size()==1&&!native[0].cells[2].known&&native[0].cells[2].text.empty(),"wholly horizontally offscreen thirdcell never releases sentinel");
  }
  paired.setWidth(641);paired.blit(&canvas);check(!paired.agentArrivalLogicalRows(native,epoch),"list width above640 refuses");paired.setWidth(271);paired.setHeight(401);paired.blit(&canvas);check(!paired.agentArrivalLogicalRows(native,epoch),"list height above400 refuses");paired.setHeight(112);paired.invalidate();paired.blit(&canvas);check(paired.agentArrivalLogicalRows(native,epoch),"native dimensions restored freshdraw");
  paired._agentDrawArrivalLogicalRows.clear();paired._agentArrivalLogicalCaptureHealthy=true;for(int i=0;i<128;++i)paired.agentCaptureArrivalLogicalRow(0,0);check(paired._agentDrawArrivalLogicalRows.size()==128&&paired._agentArrivalLogicalCaptureHealthy,"direct private capture budget128 boundary, not ordinary tallviewport claim");paired.agentCaptureArrivalLogicalRow(0,0);check(!paired._agentArrivalLogicalCaptureHealthy&&paired._agentDrawArrivalLogicalRows.size()==128,"capture budget129 failclosed");paired.invalidate();paired.blit(&canvas);
  auto drawEpoch=paired._agentArrivalLogicalDrawEpoch;paired._agentArrivalLogicalDrawEpoch=drawEpoch+1;check(!paired.agentArrivalLogicalRows(native,epoch)&&epoch==0,"mismatched drawtoken refuses");paired._agentArrivalLogicalDrawEpoch=drawEpoch;
  paired._agentArrivalLogicalDrawEpoch=UINT64_MAX;paired.invalidate();paired.blit(&canvas);check(paired._agentArrivalLogicalEpochExhausted&&!paired.agentArrivalLogicalRows(native,epoch)&&native.empty()&&epoch==0,"epochoverflow sticky failclosed no wrap");paired._agentArrivalLogicalDrawEpoch=1;paired.invalidate();paired.blit(&canvas);check(!paired.agentArrivalLogicalRows(native,epoch),"exhausted epoch cannot revive by anotherdraw");
 }
 std::cout<<"PASS "<<checks<<" arrival logical constructor/provider checks; explicit domain and rendering seams\n";
}
