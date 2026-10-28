// SPDX-License-Identifier: GPL-3.0-or-later
// Native BaseView selection, literal Basescape ordinary click callback and
// DismantleFacilityState constructor/provider/dispatch/ordinary handlers.
// Seams: owning Game/State initialization, synthetic texture/interface resources,
// and error-popup construction (ordinary error dismiss callback remains literal).
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
#include "../../src/Mod/Mod.h"
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
struct DismantleFacilityState:State {
 Base *_base;BaseView *_view;BaseFacility *_fac;
 TextButton *_btnOk,*_btnCancel;Window *_window;Text *_txtTitle,*_txtFacility;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 DismantleFacilityState(Base*,BaseView*,BaseFacility*);~DismantleFacilityState();
 void btnOkClick(Action*);void btnCancelClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;
 void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
};
struct BasescapeState:State {
 Base *_base;BaseView *_view;unsigned auditCalls=0;InteractiveSurface *auditSender=nullptr;int auditX=-1,auditY=-1;
 BasescapeState(Base *base):_base(base){_view=new BaseView(192,192,0,8);add(_view,"view","basescape");_view->setTexture(_game->mod.texture);_view->setBase(base);_view->agentEnableFacilitySelection();_view->onMouseClick((ActionHandler)&BasescapeState::viewLeftClick,SDL_BUTTON_LEFT);_view->onMouseClick((ActionHandler)&BasescapeState::auditClick,0);}
 TextEdit *_edtBase=nullptr;MiniBaseView *_mini=nullptr;
 Text *_txtLocation=nullptr,*_txtFunds=nullptr;
 TextButton *_btnBaseInfo=nullptr,*_btnFacilities=nullptr,*_btnResearch=nullptr,*_btnManufacture=nullptr,*_btnPurchase=nullptr,*_btnSell=nullptr,*_btnTransfer=nullptr,*_btnSoldiers=nullptr,*_btnCrafts=nullptr,*_btnGeoscape=nullptr;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 // Unrelated actions cannot be offered: their widgets are absent from this bounded fixture.
 void miniClick(Action*){std::abort();}void btnBaseInfoClick(Action*){std::abort();}void btnFacilitiesClick(Action*){std::abort();}void btnResearchClick(Action*){std::abort();}void btnManufactureClick(Action*){std::abort();}void btnPurchaseClick(Action*){std::abort();}void btnSellClick(Action*){std::abort();}void btnTransferClick(Action*){std::abort();}void btnSoldiersClick(Action*){std::abort();}void btnCraftsClick(Action*){std::abort();}void btnGeoscapeClick(Action*){std::abort();}
 bool click(int x,int y){AgentRequest r;r.action="click_base_facility_cell";r.x=x;r.y=y;return agentDispatch(r);}
 void viewLeftClick(Action*);
 void auditClick(Action*a){++auditCalls;auditSender=a->getSender();auditX=int(a->getRelativeXMouse());auditY=int(a->getRelativeYMouse());}
};
#include "Dismantle.generated.inc"
#include "BaseClick.generated.inc"
#include "ErrorDismiss.generated.inc"
}
static unsigned checks=0;
static void check(bool value,const char*what){++checks;if(!value){std::cerr<<"FAIL "<<what<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==3&&SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen native fixture only");FileMap::load("common","./",false);
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Options::baseXResolution=320;Options::baseYResolution=200;Options::allowBuildingQueue=false;
 SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
 Surface canvas(640,400),background(320,200);canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 SurfaceSet texture(32,32);for(int i=0;i<64;++i){auto*f=texture.addFrame(i);f->setPalette(palette);f->clear(Uint8(16+i));}
 Mod nativeMod;RuleBaseFacility lift("STR_ACCESS_LIFT"),rule("STR_GENERAL_STORES"),quarters("STR_LIVING_QUARTERS");
 for(auto*r:{&lift,&rule,&quarters}){r->_spriteShape=1;r->_spriteFacility=4;r->_size=1;r->_buildCost=150000;r->_buildTime=10;r->_monthlyCost=5000;}lift._lift=true;quarters._personnel=10;
 for(const auto &variant:{"xcom1","xcom2"}){
  Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+variant+"/Language/en-US.yml");
  Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.mod.texture=&texture;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
  Base base(&nativeMod);auto add=[&](RuleBaseFacility*r,int x,int y,int time){auto*f=new BaseFacility(r,&base);f->setX(x);f->setY(y);f->setBuildTime(time);base.getFacilities()->push_back(f);return f;};
  auto*entry=add(&lift,0,0,0);auto*started=add(&rule,1,0,10);game.saved.setFunds(350000);
  Fixture::BasescapeState bases(&base);game.stack={&bases};bases.blit();
  AgentUiState grid;check(bases.agentUiState(grid)&&YAML::Load(grid.visible)["grid"]["cells"].size()==36,"actual Basescape provider offers all 36 visible cells");
  std::ofstream(std::string(argv[2])+"/basescape-"+variant+".json")<<grid.visible<<"\n";
  bases._view->setVisible(false);bases.blit();check(!bases.click(1,0),"hidden base grid blocks native selection");bases._view->setVisible(true);check(!bases.click(1,0),"unpainted restored base grid stale");bases.blit();
  Surface headerOverlap(160,9,0,0);headerOverlap.setPalette(palette);headerOverlap.clear(99);bases._surfaces.push_back(&headerOverlap);bases.blit();check(bases.agentUiState(grid)&&YAML::Load(grid.visible)["grid"]["cells"].size()==36,"ordinary one-pixel top header overlap preserves all visible native cell centers");bases._surfaces.pop_back();bases.blit();
  Surface cellCover(1,1,48,24);cellCover.setPalette(palette);cellCover.clear(99);bases._surfaces.push_back(&cellCover);bases.blit();check(!bases.click(1,0)&&bases.agentUiState(grid)&&YAML::Load(grid.visible)["grid"]["cells"].size()==35,"real one-pixel native click-center overlay removes only covered cell and rejects dispatch");bases._surfaces.pop_back();bases.blit();
  check(!bases.click(6,0)&&bases.auditCalls==0,"offgrid native helper refused");
  check(bases.click(5,5)&&game.isState(&bases)&&base.getFacilities()->size()==2,"empty visible cell ordinary click no popup");
  check(bases.click(1,0),"ordinary selected construction click");
  auto*popup=dynamic_cast<Fixture::DismantleFacilityState*>(game.stack.back());check(popup&&popup->_fac==started&&bases.auditSender==bases._view&&bases.auditX==48&&bases.auditY==16,"real sender cellcenter selects actual facility");
  AgentRequest cancel;cancel.action="cancel_dismantle_facility";AgentRequest confirm;confirm.action="confirm_dismantle_facility";AgentUiState ui;
  check(!popup->agentUiState(ui)&&!popup->agentDispatch(confirm),"unpainted popup refuses authority");popup->blit();check(popup->agentUiState(ui)&&ui.offers(confirm.action)&&ui.offers(cancel.action),"painted confirmation offers explicit confirm and cancel");
  std::ofstream(std::string(argv[2])+"/visible-"+variant+".json")<<ui.visible<<"\n";
  check(popup->agentDispatch(cancel)&&game.isState(&bases)&&base.getFacilities()->size()==2&&game.saved.getFunds()==350000,"cancel preserves construction and funds");check(!popup->agentDispatch(confirm),"popped canceled dialog cannot confirm");
  bases.blit();bases.click(1,0);popup=dynamic_cast<Fixture::DismantleFacilityState*>(game.stack.back());check(popup!=nullptr,"reopen confirmation through ordinary cell");popup->blit();
  for(Text*caption:{popup->_txtTitle,popup->_txtFacility}){const std::string original=caption->getText();caption->setText("");popup->blit();check(popup->agentUiState(ui)&&!ui.offers(confirm.action)&&ui.offers(cancel.action)&&!popup->agentDispatch(confirm),"empty repainted caption withholds confirm but preserves Cancel");caption->setText(original);popup->blit();}
  SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);popup->blit();check(!popup->agentDispatch(confirm),"clipped destination blocks confirmation");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!popup->agentDispatch(confirm),"clip restore without repaint stale");popup->blit();
  popup->_modal=popup->_btnCancel;check(!popup->agentDispatch(confirm),"modal blocks confirmation");popup->_modal=nullptr;
  popup->_btnOk->setVisible(false);popup->blit();check(popup->agentUiState(ui)&&!ui.offers(confirm.action)&&!popup->agentDispatch(confirm),"hidden confirmation refuses mutation");popup->_btnOk->setVisible(true);check(!popup->agentDispatch(confirm),"show without repaint does not restore stale grant");popup->blit();
  Surface cover(44,16,36,115);cover.setPalette(palette);cover.clear(99);popup->_surfaces.push_back(&cover);popup->blit();check(popup->agentUiState(ui)&&!ui.offers(confirm.action)&&!popup->agentDispatch(confirm),"real later overlay blocks confirm");popup->_surfaces.pop_back();popup->blit();
  game.cursor=&cover;cover.blit(&canvas);check(!popup->agentDispatch(confirm),"cursor overlay blocks confirm");game.cursor=nullptr;popup->blit();
  AgentFrameCommand frame;check(popup->agentUiState(ui)&&frame.stage(confirm,ui,popup,1,1,100,100),"confirmation frame staged");popup->_btnOk->setX(37);popup->_btnOk->setX(36);check(!popup->agentDispatch(confirm),"button geometry ABA invalidates grant");popup->blit();check(popup->agentUiState(ui)&&!frame.matchesFrame(101,1,1,popup,ui),"old grant cannot match fresh redraw");
  check(popup->agentDispatch(confirm)&&game.isState(&bases)&&base.getFacilities()->size()==1&&base.getFacilities()->front()==entry&&game.saved.getFunds()==350000,"explicit native confirm removes exactly started facility without refund");check(!popup->agentDispatch(confirm)&&base.getFacilities()->size()==1&&game.saved.getFunds()==350000,"popped confirmation cannot remove or charge twice");
  bases._view->setBase(&base);bases.blit();bases.click(0,0);popup=dynamic_cast<Fixture::DismantleFacilityState*>(game.stack.back());check(popup!=nullptr,"lift ordinary dialog opens");popup->blit();check(popup->agentUiState(ui)&&!ui.offers(confirm.action)&&ui.offers(cancel.action)&&!popup->agentDispatch(confirm),"lift has no MCP abandonment authority");std::ofstream(std::string(argv[2])+"/lift-"+variant+".json")<<ui.visible<<"\n";check(popup->agentDispatch(cancel)&&base.getFacilities()->size()==1&&game.saved.getFunds()==350000,"lift cancel preserves base");
  auto*occupied=add(&quarters,1,0,0);base.setScientists(1);bases._view->setBase(&base);bases.blit();bases.click(1,0);auto*error=dynamic_cast<Fixture::ErrorMessageState*>(game.stack.back());check(error&&error->message==std::string(language.getString("STR_FACILITY_IN_USE"))&&base.getFacilities()->size()==2,"native in-use rejects before confirmation");error->dismiss();check(game.isState(&bases),"ordinary in-use error dismiss returns base");base.setScientists(0);
  auto*tail=add(&rule,2,0,0);bases._view->setBase(&base);bases.blit();bases.click(1,0);error=dynamic_cast<Fixture::ErrorMessageState*>(game.stack.back());check(error&&error->message==std::string(language.getString("STR_CANNOT_DISMANTLE_FACILITY"))&&base.getFacilities()->size()==3,"native disconnected rejection preserves facilities");error->dismiss();check(game.isState(&bases),"ordinary disconnection error dismiss");
  Options::baseXResolution=640;Options::baseYResolution=400;bases._view->setX(40);bases._view->setY(208);bases.blit();bases.click(2,0);popup=dynamic_cast<Fixture::DismantleFacilityState*>(game.stack.back());check(popup&&popup->_fac==tail&&bases.auditX==80&&bases.auditY==16&&bases.auditSender==bases._view,"translated native cell selects tail facility");popup->blit();check(popup->agentDispatch(cancel)&&base.getFacilities()->size()==3&&game.saved.getFunds()==350000,"translated dialog cancel leaves model unchanged");Options::baseXResolution=320;Options::baseYResolution=200;
  (void)occupied;for(auto*state:game.errors)delete state;game.errors.clear();game.stack.clear();
 }
 check(!SDL_GetVideoSurface(),"no game video opened");std::cout<<"PASS "<<checks<<" native dismantle checks; explicit owning/resource/error-construction/base-constructor seams\n";
}
