// SPDX-License-Identifier: GPL-3.0-or-later
// Actual native containment draft/removal model and scoped visible UI providers.
// Explicit Game/resources, rule-subset and post-removal Sell/error construction seams.
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/LocalizedText.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Engine/SurfaceSet.h"
#include "../../src/Engine/Unicode.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Window.h"
#include "../../src/Basescape/BaseView.h"
#include "../../src/Savegame/Base.h"
#include "../../src/Savegame/BaseFacility.h"
#include "../../src/Savegame/SavedGame.h"
#define private public
#include "../../src/Mod/Mod.h"
#undef private
#include "../../src/Mod/RuleCraft.h"
#include "../../src/Mod/RuleItem.h"
#include "../../src/Mod/RuleResearch.h"
#include "../../src/Mod/Unit.h"
#include "../../src/Savegame/ItemContainer.h"
#include "../../src/Savegame/ResearchProject.h"
#include "../../src/Engine/Timer.h"
#include "../../src/Menu/OptionsBaseState.h"
#include "../../src/Mod/RuleSoldier.h"
#include "../../src/Mod/RuleCountry.h"
#include "../../src/Mod/Armor.h"
#include "../../src/Savegame/Craft.h"
#include "../../src/Savegame/Soldier.h"
#include "../../src/Savegame/Country.h"
// Test-only malformed arrow-vector setup; restore ownership before any assertion.
#define private public
#include "../../src/Interface/TextList.h"
#undef private
#include "../../src/Mod/RuleInterface.h"
// Test-only synthetic rule setup; native getters and model implementations used.
#define private public
#include "../../src/Mod/RuleBaseFacility.h"
#undef private
#include "../../src/Interface/TextEdit.h"
#include "../../src/Basescape/MiniBaseView.h"
#include "../../src/Observer/AgentProjectileOutput.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
using namespace OpenXcom;using namespace OpenXcom::Observer;
namespace Fixture {
struct State;
struct Screen {Surface *surface;Surface *getSurface(){return surface;}bool agentCaptureOutput(const Surface*,AgentProjectileOutput&){return false;}};
struct Resources {
 Mod *native=nullptr;
 const std::vector<std::string>&getCraftsList(){return native->getCraftsList();}RuleCraft*getCraft(const std::string&id){return native->getCraft(id);}
 const std::vector<std::string>&getSoldiersList(){return native->getSoldiersList();}RuleSoldier*getSoldier(const std::string&id){return native->getSoldier(id);}
 const std::vector<std::string>&getItemsList(){return native->getItemsList();}
 RuleItem*getItem(const std::string&id,bool error=false){return native->getItem(id,error);}Unit*getUnit(const std::string&id,bool error=false){return native->getUnit(id,error);}Armor*getArmor(const std::string&id,bool error=false){return native->getArmor(id,error);}
 int getEngineerCost(){return native->getEngineerCost();}int getScientistCost(){return native->getScientistCost();}
 Surface *background;SurfaceSet *texture;RuleInterface interfaceRule{"placeFacility"};
 Resources(){interfaceRule.load(YAML::Load("elements: [{id: errorMessage, color: 32}, {id: errorPalette, color: -1}]"));}
 Surface *getSurface(const std::string&){return background;}
 SurfaceSet *getSurfaceSet(const std::string&){return texture;}
 RuleInterface *getInterface(const std::string&){return &interfaceRule;}
};
struct Game {
 Screen screen;Resources mod;SavedGame saved;Language *language;Font *big,*small;SDL_Color *palette;
 std::vector<State*> stack;std::vector<State*> errors;Surface *cursor=nullptr,*fps=nullptr;unsigned pops=0,pushes=0;
 Screen *getScreen(){return &screen;}Resources *getMod(){return &mod;}SavedGame *getSavedGame(){return &saved;}
 Surface *getCursor(){return cursor;}Surface *getFpsCounter(){return fps;}
 bool isState(const void*s)const{return !stack.empty()&&stack.back()==s;}
 bool agentOverlayClear(const AgentUiState&ui)const{
  for(auto*s:{cursor,fps})if(s&&s->getVisible())for(const auto&r:ui.regions)
   if(r.x<s->getX()+s->getWidth()&&r.x+r.width>s->getX()&&r.y<s->getY()+s->getHeight()&&r.y+r.height>s->getY())return false;
  return true;
 }
 bool agentNonCursorOverlayClear(const AgentUiState&ui)const{return agentOverlayClear(ui);}
 void popState(){++pops;if(!stack.empty())stack.pop_back();}
 void pushState(State*s){++pushes;stack.push_back(s);errors.push_back(s);}
};
static Game *currentGame;
struct State:OpenXcom::State {
 Game *_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;
 void setInterface(const std::string&){std::copy(_game->palette,_game->palette+256,_palette);}
 void centerAllSurfaces(){}
 void init()override{}
 LocalizedText tr(const std::string&id)const{return _game->language->getString(id);}
 LocalizedText tr(const std::string&id,unsigned n)const{return _game->language->getString(id,n);}
 void add(Surface*s,const std::string&,const std::string&){
  s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);
  if(auto *view=dynamic_cast<BaseView*>(s)){view->setColor(32);view->setSecondaryColor(48);}
  _surfaces.push_back(s);
 }
 void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}
};
struct ErrorMessageState:State {
 std::string message;TextButton *button;
 ErrorMessageState(const std::string&m,SDL_Color*,int,const std::string&,int):message(m){
  button=new TextButton(80,16,120,92);add(button,"button","error");button->setText("OK");button->onMouseClick((ActionHandler)&ErrorMessageState::btnOkClick);
 }
 void btnOkClick(Action*);
 void dismiss(){SDL_Event event={};event.type=SDL_MOUSEBUTTONUP;event.button.button=SDL_BUTTON_LEFT;Action action(&event,1,1,0,0);action.setSender(button);action.setMouseAction(160,100,120,92);button->mouseClick(&action,this);}
};
struct SellState:State {Base *base;OptionsOrigin origin;SellState(Base*b,OptionsOrigin o=OPT_GEOSCAPE):base(b),origin(o){}};
struct ManageAlienContainmentState:State {
 Base *_base;OptionsOrigin _origin;TextButton *_btnOk,*_btnCancel;Window *_window;
 Text *_txtTitle,*_txtUsed,*_txtAvailable,*_txtItem,*_txtLiveAliens,*_txtDeadAliens,*_txtInterrogatedAliens;
 TextList *_lstAliens;Timer *_timerInc,*_timerDec;std::vector<int>_qtys;std::vector<std::string>_aliens;size_t _sel;int _aliensSold;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;
 ManageAlienContainmentState(Base*,OptionsOrigin);~ManageAlienContainmentState();void think()override;
 int getQuantity();void btnOkClick(Action*);void btnCancelClick(Action*);
 void lstItemsLeftArrowPress(Action*);void lstItemsLeftArrowRelease(Action*);void lstItemsLeftArrowClick(Action*);
 void lstItemsRightArrowPress(Action*);void lstItemsRightArrowRelease(Action*);void lstItemsRightArrowClick(Action*);void lstItemsMousePress(Action*);
 void increase();void increaseByValue(int);void decrease();void decreaseByValue(int);void updateStrings();
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
};
struct BasescapeState;
struct BaseInfoState:State {BaseInfoState(Base*,BasescapeState*){std::abort();}};
struct CraftsState:State {CraftsState(Base*){std::abort();}};
struct CraftInfoState:State {CraftInfoState(Base*,size_t){std::abort();}};
struct SoldiersState:State {SoldiersState(Base*){std::abort();}};
struct AllocatePsiTrainingState:State {AllocatePsiTrainingState(Base*){std::abort();}};
struct ResearchState:State {ResearchState(Base*){std::abort();}};
struct ManufactureState:State {ManufactureState(Base*){std::abort();}};
struct BasescapeState:State {
 Base *_base;BaseView *_view;unsigned rightCalls=0,leftCalls=0;InteractiveSurface *auditSender=nullptr;int auditX=-1,auditY=-1;
 BasescapeState(Base*base):_base(base){_view=new BaseView(192,192,0,8);add(_view,"view","basescape");_view->setTexture(_game->mod.texture);_view->setBase(base);_view->agentEnableFacilitySelection();_view->onMouseClick((ActionHandler)&BasescapeState::viewRightClick,SDL_BUTTON_RIGHT);_view->onMouseClick((ActionHandler)&BasescapeState::auditLeft,SDL_BUTTON_LEFT);_view->onMouseClick((ActionHandler)&BasescapeState::audit,0);}
 TextEdit *_edtBase=nullptr;MiniBaseView *_mini=nullptr;Text *_txtLocation=nullptr,*_txtFunds=nullptr;
 TextButton *_btnBaseInfo=nullptr,*_btnFacilities=nullptr,*_btnResearch=nullptr,*_btnManufacture=nullptr,*_btnPurchase=nullptr,*_btnSell=nullptr,*_btnTransfer=nullptr,*_btnSoldiers=nullptr,*_btnCrafts=nullptr,*_btnGeoscape=nullptr,*_btnNewBase=nullptr;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void miniClick(Action*){std::abort();}void btnBaseInfoClick(Action*){std::abort();}void btnFacilitiesClick(Action*){std::abort();}void btnResearchClick(Action*){std::abort();}void btnManufactureClick(Action*){std::abort();}void btnPurchaseClick(Action*){std::abort();}void btnSellClick(Action*){std::abort();}void btnTransferClick(Action*){std::abort();}void btnSoldiersClick(Action*){std::abort();}void btnCraftsClick(Action*){std::abort();}void btnGeoscapeClick(Action*){std::abort();}void btnNewBaseClick(Action*){std::abort();}
 void viewRightClick(Action*);void auditLeft(Action*){++leftCalls;}void audit(Action*a){if(a->getDetails()->button.button==SDL_BUTTON_RIGHT)++rightCalls;auditSender=a->getSender();auditX=int(a->getRelativeXMouse());auditY=int(a->getRelativeYMouse());}
};
#include "Containment.generated.inc"
#include "Basescape.generated.inc"
#include "ErrorDismiss.generated.inc"
}
static unsigned checks=0;
static void check(bool value,const char*what){++checks;if(!value){std::cerr<<"FAIL "<<what<<'\n';std::exit(1);}}
static YAML::Node stock(const std::string&path,const std::string&kind,const std::string&id){for(auto n:YAML::LoadFile(path)[kind])if(n["type"].as<std::string>()==id)return n;throw std::runtime_error("missing stock fixture rule");}
int main(int argc,char**argv){
 check(argc==3&&SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen native fixture only");FileMap::load("common","./",false);
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Options::baseXResolution=320;Options::baseYResolution=200;Options::storageLimitsEnforced=true;Options::canSellLiveAliens=false;
 SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};Surface canvas(640,400),background(320,200);canvas.setPalette(palette);background.setPalette(palette);background.clear(40);SurfaceSet texture(32,32);for(int i=0;i<64;++i){auto*f=texture.addFrame(i);f->setPalette(palette);f->clear(Uint8(16+i));}
 for(const auto&variant:{"xcom1","xcom2"}){
  const std::string dir=std::string(argv[1])+"/bin/standard/"+variant+"/";const std::string alien=std::string(variant)=="xcom1"?"STR_SECTOID_SOLDIER":"STR_AQUATOID_SOLDIER";
  Language language;language.loadFile(dir+"Language/en-US.yml");Mod native;
  auto itemNode=stock(dir+"items.rul","items",alien);auto unitNode=stock(dir+"units.rul","units",alien);const std::string armorId=unitNode["armor"].as<std::string>();auto armorNode=stock(dir+"armors.rul","armors",armorId);
  auto*armor=new Armor(armorId);armor->load(armorNode);native._armors[armorId]=armor;const auto corpse=armor->getCorpseGeoscape();
  auto*unit=new Unit(alien);YAML::Node u;u["armor"]=armorId;unit->load(u,&native);native._units[alien]=unit;
  for(const auto&id:{alien,corpse}){auto original=stock(dir+"items.rul","items",id);YAML::Node n;n["size"]=original["size"];n["costSell"]=original["costSell"].as<int>(0);n["liveAlien"]=id==alien;auto*r=new RuleItem(id);r->load(n,&native,0);native._items[id]=r;native._itemsIndex.push_back(id);}
  RuleBaseFacility containment("STR_ALIEN_CONTAINMENT"),stores("STR_GENERAL_STORES");for(auto*r:{&containment,&stores}){r->_spriteShape=1;r->_spriteFacility=4;r->_size=1;}containment._aliens=10;stores._storage=10;
  Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.mod.texture=&texture;game.mod.native=&native;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;Base base(&native);base.setName("TEST");auto*facility=new BaseFacility(&containment,&base);facility->setX(0);facility->setY(0);base.getFacilities()->push_back(facility);auto*store=new BaseFacility(&stores,&base);store->setX(1);store->setY(0);base.getFacilities()->push_back(store);base.getStorageItems()->addItem(alien,2);game.saved.setFunds(1000000);
  Fixture::BasescapeState bases(&base);game.stack={&bases};bases.blit();AgentRequest right;right.action="right_click_base_facility_cell";right.x=0;right.y=0;check(bases.agentDispatch(right)&&bases.rightCalls==1&&bases.leftCalls==0&&bases.auditSender==bases._view&&bases.auditX==16&&bases.auditY==16,"actual BaseView right button real sender and ordinary containment route");
  auto*state=dynamic_cast<Fixture::ManageAlienContainmentState*>(game.stack.back());check(state&&state->_base==&base,"ordinary completed-containment shortcut opens native manager");
  AgentRequest inc;inc.action="increase_containment_removal";inc.choice="r0";AgentRequest dec=inc;dec.action="decrease_containment_removal";AgentRequest cancel;cancel.action="cancel_containment";AgentRequest remove;remove.action="remove_selected_aliens";AgentUiState ui;
  check(!state->agentDispatch(inc),"unpainted containment cannot adjust draft");state->blit();check(state->agentUiState(ui)&&ui.offers(inc.action)&&ui.offers(cancel.action),"painted containment row controls and Cancel offered");std::ofstream(std::string(argv[2])+"/visible-"+variant+".json")<<ui.visible<<"\n";
  const auto visibleRow=YAML::Load(ui.visible)["rows"][0];check(visibleRow["cells"].size()==4&&visibleRow["cells"][0].as<std::string>()==std::string(language.getString(alien))&&visibleRow["cells"][1].as<std::string>()=="2"&&visibleRow["cells"][2].as<std::string>()=="0"&&visibleRow["cells"][3].as<std::string>()=="0","actual stock row retains all logical captions including clipped final allocation");
  state->_lstAliens->setCellText(0,3,"12345678");state->blit();check(state->agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][3].IsNull(),"long fourth caption clipped by actual viewport is null rather than exported suffix");
  state->_lstAliens->setCellText(0,3,"12");state->blit();check(state->agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][3].as<std::string>()=="12","fitting fourth-column digits remain visible logical data");state->_lstAliens->setCellText(0,3,"0");
  state->_lstAliens->setCellText(0,1,"12345678");state->blit();check(state->agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][1].IsNull(),"value fitting second column but overlapped by native arrow is null");state->_lstAliens->setCellText(0,1,"2");
  state->_lstAliens->setCellText(0,0,"A\nB");state->blit();check(state->agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull(),"vertically clipped multi-line caption is not serialized as complete");state->_lstAliens->setCellText(0,0,std::string(language.getString(alien)));state->blit();check(state->agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>()==std::string(language.getString(alien))&&YAML::Load(ui.visible)["rows"][0]["cells"][1].as<std::string>()=="2","ordinary captions restored by repaint after clipping regressions");
  state->_lstAliens->setMargin(-1);for(auto*cell:state->_lstAliens->_texts[0])cell->setX(cell->getX()-3);state->_lstAliens->invalidate();state->blit();check(state->agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull(),"caption starting left of viewport is null despite intersecting allocation");state->_lstAliens->setMargin(2);for(auto*cell:state->_lstAliens->_texts[0])cell->setX(cell->getX()+3);state->_lstAliens->invalidate();state->blit();
  for(auto *arrows:{&state->_lstAliens->_arrowLeft,&state->_lstAliens->_arrowRight}){auto*owned=arrows->back();arrows->pop_back();std::vector<AgentOutfittingRow> malformedRows;std::uint64_t malformedEpoch=0;const bool refused=!state->_lstAliens->agentContainmentRows(&canvas,malformedRows,malformedEpoch);arrows->push_back(owned);check(refused,"malformed native arrow vector fails closed before text-overlay indexing");}state->blit();
  state->_timerInc->start();check(!state->agentUiState(ui)&&!state->agentDispatch(inc),"running native increase timer closes MCP admission");state->_timerInc->stop();state->_timerDec->start();check(!state->agentDispatch(dec),"running native decrease timer closes MCP admission");state->_timerDec->stop();
  AgentRequest bad=inc;bad.choice="r99";check(!state->agentDispatch(bad)&&state->_qtys[0]==0,"unknown rendered row handle refused");state->_modal=state->_btnCancel;check(!state->agentDispatch(inc),"local modal blocks draft input");state->_modal=nullptr;
  state->_lstAliens->setVisible(false);state->blit();check(state->agentUiState(ui)&&!ui.offers(inc.action)&&!state->agentDispatch(inc),"hidden containment list grants no row mutation");state->_lstAliens->setVisible(true);check(!state->agentDispatch(inc),"list show without repaint stale");state->blit();
  std::vector<AgentOutfittingRow>captured;std::uint64_t epoch=0;check(state->_lstAliens->agentContainmentRows(&canvas,captured,epoch)&&captured.size()==1&&captured[0].increase,"real native containment row capture exposes visible arrow");
  Surface arrowCover(captured[0].increase->getWidth(),captured[0].increase->getHeight(),captured[0].increase->getX(),captured[0].increase->getY());arrowCover.setPalette(palette);arrowCover.clear(99);state->_surfaces.push_back(&arrowCover);state->blit();check(!state->agentDispatch(inc)&&state->_qtys[0]==0,"real arrow overlay refuses draft mutation");state->_surfaces.pop_back();state->blit();
  game.cursor=&arrowCover;arrowCover.blit(&canvas);check(!state->agentDispatch(inc),"real cursor over arrow refuses draft mutation");game.cursor=nullptr;state->blit();
  state->_lstAliens->setY(54);state->_lstAliens->setY(53);check(!state->agentDispatch(inc),"list geometry ABA refuses stale row grant");state->blit();
  check(state->agentDispatch(inc)&&state->_qtys[0]==1&&base.getStorageItems()->getItem(alien)==2&&game.saved.getFunds()==1000000,"native increase changes draft only");check(!state->_timerInc->isRunning()&&!state->_timerDec->isRunning(),"native press release sequence leaves no repeat timer armed");check(!state->agentDispatch(dec),"changed draft requires fresh repaint");state->blit();check(state->agentDispatch(dec)&&state->_qtys[0]==0,"ordinary decrement reverses draft");state->blit();check(state->agentDispatch(inc),"redraft one captive");state->blit();check(state->agentDispatch(cancel)&&game.isState(&bases)&&base.getStorageItems()->getItem(alien)==2&&base.getStorageItems()->getItem(corpse)==0&&game.saved.getFunds()==1000000,"Cancel discards draft without storage or funds mutation");check(!state->agentDispatch(remove),"popped manager rejects removal replay");
  auto open=[&](){bases.blit();check(bases.agentDispatch(right),"native right-click reopens manager");auto*s=dynamic_cast<Fixture::ManageAlienContainmentState*>(game.stack.back());check(s!=nullptr,"native manager top state");s->blit();return s;};
  state=open();check(state->agentDispatch(inc),"select one for default removal");state->blit();check(state->agentDispatch(remove)&&base.getStorageItems()->getItem(alien)==1&&base.getStorageItems()->getItem(corpse)==1&&game.saved.getFunds()==1000000,"default removal consumes one own captive and adds one native corpse without funds");check(!state->agentDispatch(remove)&&base.getStorageItems()->getItem(corpse)==1,"default removal cannot replay");
  Options::canSellLiveAliens=true;state=open();check(state->agentDispatch(inc),"select last captive for sale option");state->blit();check(state->agentDispatch(remove)&&base.getStorageItems()->getItem(alien)==0&&base.getStorageItems()->getItem(corpse)==1&&game.saved.getFunds()==1000000+native.getItem(alien)->getSellCost(),"native sale option credits stock live sale value without corpse");Options::canSellLiveAliens=false;
  base.getStorageItems()->getContents()->clear();base.getStorageItems()->addItem(alien,2);containment._aliens=1;state=open();check(state->agentUiState(ui)&&!ui.offers(cancel.action)&&!ui.offers(remove.action),"overcapacity hides Cancel and Remove until draft resolves excess");check(state->agentDispatch(inc),"overcapacity native draft remains usable");state->blit();check(state->agentUiState(ui)&&ui.offers(remove.action)&&!ui.offers(cancel.action),"enough drafted removal reveals ordinary Remove only");check(state->agentDispatch(remove)&&base.getStorageItems()->getItem(alien)==1&&base.getFreeContainment()==0,"native overcapacity removal restores containment capacity");containment._aliens=10;
  base.getStorageItems()->getContents()->clear();RuleResearch interrogation(alien);interrogation.load(YAML::Load("needItem: true\ndestroyItem: false\ncost: 100"),0);auto*project=new ResearchProject(&interrogation,100);base.addResearch(project);state=open();check(state->_qtys.size()==1&&state->_aliens[0]==alien&&base.getStorageItems()->getItem(alien)==0,"interrogation-only row native zero live quantity");check(state->agentDispatch(inc)&&state->_qtys[0]==0,"native interrogation-only increase clamps to zero live stock");state->blit();check(state->agentDispatch(cancel),"interrogation-only Cancel");base.removeResearch(project);
  base.getStorageItems()->addItem(alien,1);stores._storage=0;state=open();check(state->agentDispatch(inc),"select captive whose corpse will overfill stores");state->blit();check(state->agentDispatch(remove)&&base.getStorageItems()->getItem(alien)==0&&base.getStorageItems()->getItem(corpse)==1,"ordinary corpse conversion before stores-overfull transition");auto*error=dynamic_cast<Fixture::ErrorMessageState*>(game.stack.back());check(error!=nullptr&&base.storesOverfull(),"native stores-overfull condition pushes error");error->dismiss();auto*sell=dynamic_cast<Fixture::SellState*>(game.stack.back());check(sell&&sell->base==&base&&sell->origin==OPT_GEOSCAPE,"ordinary error dismiss exposes Sell construction seam");game.popState();stores._storage=10;
  Options::baseXResolution=640;Options::baseYResolution=400;bases._view->setX(40);bases._view->setY(208);bases.blit();check(bases.agentDispatch(right)&&bases.auditX==16&&bases.auditY==16&&bases.auditSender==bases._view,"translated real right click maps logical native facility");state=dynamic_cast<Fixture::ManageAlienContainmentState*>(game.stack.back());state->blit();check(state->agentDispatch(cancel),"translated route manager Cancel");check(bases._view->agentFacilityClick(&bases,0,0)&&bases.leftCalls==1&&game.isState(&bases),"existing native left-button helper remains left and does not open right-click route");Options::baseXResolution=320;Options::baseYResolution=200;
  for(auto*owned:game.errors)delete owned;game.errors.clear();game.stack.clear();
 }
 check(!SDL_GetVideoSurface(),"no game video opened");std::cout<<"PASS "<<checks<<" native containment checks; explicit owning/resource/rule-subset/post-removal-transition seams\n";
}
