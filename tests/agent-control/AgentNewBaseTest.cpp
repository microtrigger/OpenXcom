// SPDX-License-Identifier: GPL-3.0-or-later
// Native cost/name/lift lifecycle, model economy and read-only collector checks.
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
#include "../../src/Mod/RuleRegion.h"
#include "../../src/Savegame/Region.h"
#include "../../src/Observer/AgentPlayerBases.h"
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
 RuleBaseFacility *lift=nullptr;std::vector<std::string> facilities{"LIFT"};
 const std::vector<std::string>&getBaseFacilitiesList()const{return facilities;}RuleBaseFacility*getBaseFacility(const std::string&){return lift;}
 Surface *background;SurfaceSet *texture;RuleInterface interfaceRule{"placeFacility"};
 Resources(){interfaceRule.load(YAML::Load("elements: [{id: errorMessage, color: 32}, {id: errorPalette, color: -1}, {id: genericWindow, color: 32}, {id: palette, color: -1}]"));}
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
// Globe mouse-over registration only; geographical point selection is outside this fixture.
struct Globe {unsigned clears=0;void onMouseOver(ActionHandler){++clears;}};
struct BasescapeState:State {Base*base;BasescapeState(Base*b,Globe*):base(b){}};
struct SelectStartFacilityState:State {SelectStartFacilityState(Base*,BasescapeState*,Globe*){std::abort();}};
struct ConfirmNewBaseState:State {
 Base *_base;Globe *_globe;Window *_window;Text *_txtCost,*_txtArea;TextButton *_btnOk,*_btnCancel;int _cost;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 ConfirmNewBaseState(Base*,Globe*);~ConfirmNewBaseState();void btnOkClick(Action*);void btnCancelClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
};
struct BaseNameState:State {
 Base *_base;Globe *_globe;Window *_window;Text *_txtTitle;TextEdit *_edtName;TextButton *_btnOk;bool _first;
 BaseNameState(Base*,Globe*,bool);~BaseNameState();void applyNameChange(bool);void btnOkClick(Action*);void edtNameChange(Action*);
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
};
struct PlaceLiftState:State {
 Base *_base;Globe *_globe;BaseView *_view;Text *_txtTitle;bool _first;RuleBaseFacility *_lift;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 unsigned auditCalls=0;InteractiveSurface *auditSender=nullptr;int auditX=-1,auditY=-1;
 PlaceLiftState(Base*,Globe*,bool);~PlaceLiftState();void viewClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void auditClick(Action*a){++auditCalls;auditSender=a->getSender();auditX=int(a->getRelativeXMouse());auditY=int(a->getRelativeYMouse());}
 void enableAudit(){_view->onMouseClick((ActionHandler)&PlaceLiftState::auditClick,0);}
};
#include "NewBase.generated.inc"
#include "ErrorDismiss.generated.inc"
}
static unsigned checks=0;
static void check(bool value,const char*what){++checks;if(!value){std::cerr<<"FAIL "<<what<<'\n';std::exit(1);}}
static std::vector<YAML::Node> bases(const std::vector<std::string>&records){std::vector<YAML::Node> result;for(const auto&r:records){auto n=YAML::Load(r);if(n["kind"].as<std::string>()=="base")result.push_back(n);}return result;}
int main(int argc,char**argv){
 check(argc==3&&SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen native fixture only");FileMap::load("common","./",false);
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Options::baseXResolution=320;Options::baseYResolution=200;Options::allowBuildingQueue=false;Options::keyboardMode=KEYBOARD_ON;
 SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
 Surface canvas(640,400),background(320,200);canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 SurfaceSet texture(32,32);for(int i=0;i<64;++i){auto*f=texture.addFrame(i);f->setPalette(palette);f->clear(Uint8(16+i));}
 Mod nativeMod;RuleBaseFacility lift("STR_ACCESS_LIFT"),wide("STR_ACCESS_LIFT");
 for(auto*r:{&lift,&wide}){r->_spriteShape=1;r->_spriteFacility=4;r->_size=1;r->_lift=true;r->_buildCost=0;r->_buildTime=0;r->_monthlyCost=0;}wide._size=2;
 RuleRegion region("STR_EUROPE");region.load(YAML::Load("cost: 500000\nareas: [[0, 360, -90, 90]]"));
 for(const auto &variant:{"xcom1","xcom2"}){
  Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+variant+"/Language/en-US.yml");
  Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.mod.texture=&texture;game.mod.lift=&lift;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
  game.saved.getRegions()->push_back(new Region(&region));auto*first=new Base(&nativeMod);first->setName("FIRST");game.saved.getBases()->push_back(first);game.saved.setFunds(1000000);
  auto*pending=new Base(&nativeMod);pending->setLongitude(0.2);pending->setLatitude(0.1);
  AgentPlayerBases collector;std::vector<std::string> records,before;std::string reason;
  auto collect=[&](){check(collector.collect(&game.saved,&language,records,reason),"native own-base collector succeeds");};
  collect();check(bases(records).size()==1,"provisional base not exported before explicit cost confirmation");before=records;
  Fixture::Globe globe;Fixture::State previous,build;game.stack={&previous,&build};
  {
   Fixture::ConfirmNewBaseState popup(pending,&globe);game.stack.push_back(&popup);AgentRequest cancel;cancel.action="cancel_new_base_confirmation";AgentRequest confirm;confirm.action="confirm_new_base";AgentUiState ui;
   check(popup._cost==500000,"actual Region/RuleRegion computes native construction cost");check(!popup.agentUiState(ui)&&!popup.agentDispatch(confirm),"unpainted cost dialog grants nothing");popup.blit();check(popup.agentUiState(ui)&&ui.offers(confirm.action)&&ui.offers(cancel.action),"painted cost dialog offers confirm and cancel");std::ofstream(std::string(argv[2])+"/cost-"+variant+".json")<<ui.visible<<"\n";
   check(popup.agentDispatch(cancel)&&game.isState(&build)&&game.saved.getFunds()==1000000&&game.saved.getBases()->size()==1,"ordinary cost cancel charges nothing and registers no provisional base");collect();check(records==before,"cancel preserves complete own-base collector snapshot");check(!popup.agentDispatch(confirm),"popped canceled cost dialog refuses confirmation");
  }
  {
   Fixture::ConfirmNewBaseState popup(pending,&globe);game.stack.push_back(&popup);popup.blit();AgentRequest confirm;confirm.action="confirm_new_base";AgentUiState ui;
   game.saved.setFunds(100);check(popup.agentDispatch(confirm)&&game.saved.getBases()->size()==1&&game.saved.getFunds()==100,"native insufficient funds error registers no base and charges nothing");auto*error=dynamic_cast<Fixture::ErrorMessageState*>(game.stack.back());check(error&&error->message==std::string(language.getString("STR_NOT_ENOUGH_MONEY")),"ordinary insufficient-funds popup");check(!popup.agentDispatch(confirm),"error top state rejects duplicate confirmation");error->dismiss();check(game.isState(&popup),"ordinary error dismiss restores cost dialog");game.saved.setFunds(1000000);popup.blit();
   popup._modal=popup._btnCancel;check(!popup.agentDispatch(confirm),"cost modal blocks confirmation");popup._modal=nullptr;
   Surface cover(54,12,68,104);cover.setPalette(palette);cover.clear(99);popup._surfaces.push_back(&cover);popup.blit();check(!popup.agentDispatch(confirm),"real cost button overlay blocks confirmation");popup._surfaces.pop_back();popup.blit();
   popup._btnOk->setX(69);popup._btnOk->setX(68);check(!popup.agentDispatch(confirm),"cost button geometry ABA rejects stale confirmation");popup.blit();
   check(popup.agentDispatch(confirm)&&game.saved.getFunds()==500000&&game.saved.getBases()->size()==2&&game.saved.getBases()->back()==pending,"explicit ordinary confirmation charges once and appends second base");check(!popup.agentDispatch(confirm)&&game.saved.getFunds()==500000&&game.saved.getBases()->size()==2,"name top state prevents repeated charge or duplicate base");
   collect();const auto afterCost=bases(records);check(afterCost.size()==2,"registered base exported immediately on name window");std::string secondId;for(auto b:afterCost)if(b["longitude"].as<double>()==0.2)secondId=b["id"].as<std::string>();check(!secondId.empty(),"new base has observational stable identity");
   auto*name=dynamic_cast<Fixture::BaseNameState*>(game.stack.back());check(name!=nullptr,"ordinary cost transition enters native name screen");for(int n=0;n<20;++n)name->_window->popup();name->blit();AgentRequest replace;replace.action="replace_base_name";replace.text="SECOND";check(name->agentDispatch(replace)&&pending->getName(&language)=="SECOND","native editor name mutation through ordinary naming handler");name->blit();AgentRequest finish;finish.action="confirm_base_name";check(name->agentDispatch(finish),"native name confirm advances to lift selection");
   auto*place=dynamic_cast<Fixture::PlaceLiftState*>(game.stack.back());check(place!=nullptr&&!place->_first,"ordinary naming pops setup stack and opens nonfirst lift screen");collect();bool same=false;for(auto b:bases(records))if(b["name"].as<std::string>()=="SECOND")same=b["id"].as<std::string>()==secondId;check(same,"same base ID after naming and before lift construction");
   AgentRequest cell;cell.action="place_access_lift_cell";cell.x=2;cell.y=3;check(!place->agentDispatch(cell),"unpainted lift cannot mutate");place->enableAudit();place->blit();check(place->agentUiState(ui)&&ui.offers(cell.action)&&YAML::Load(ui.visible)["grid"]["cells"].size()==36,"painted lift offers all visible cells");std::ofstream(std::string(argv[2])+"/lift-"+variant+".json")<<ui.visible<<"\n";
   cell.x=6;check(!place->agentDispatch(cell),"lift offgrid request refused");cell.x=2;
   place->_view->setVisible(false);place->blit();check(!place->agentDispatch(cell),"hidden lift grid refused");place->_view->setVisible(true);check(!place->agentDispatch(cell),"restored unpainted lift grid stale");place->blit();
   Surface cellCover(32,32,64,104);cellCover.setPalette(palette);cellCover.clear(88);place->_surfaces.push_back(&cellCover);place->blit();check(!place->agentDispatch(cell),"real lift target overlay refused");place->_surfaces.pop_back();place->blit();
   Options::baseXResolution=640;Options::baseYResolution=400;place->_view->setX(40);place->_view->setY(208);place->blit();check(place->agentUiState(ui)&&YAML::Load(ui.visible)["grid"]["origin_x"].as<int>()==40,"translated lift grid projected");check(place->agentDispatch(cell)&&pending->getFacilities()->size()==1&&game.saved.getFunds()==500000,"ordinary lift click adds exactly one facility without second cost");auto*built=pending->getFacilities()->front();check(built->getRules()==&lift&&built->getX()==2&&built->getY()==3&&built->getBuildTime()==0&&place->auditSender==place->_view&&place->auditX==80&&place->auditY==112,"native lift model and real translated sender/cell center");check(!place->agentDispatch(cell)&&pending->getFacilities()->size()==1&&game.saved.getBases()->size()==2,"popped lift cannot replay construction");Options::baseXResolution=320;Options::baseYResolution=200;
   collect();same=false;unsigned ownLift=0;for(const auto&r:records){auto row=YAML::Load(r);if(row["kind"].as<std::string>()=="base"&&row["name"].as<std::string>()=="SECOND")same=row["id"].as<std::string>()==secondId;if(row["kind"].as<std::string>()=="facility"&&row["base_id"].as<std::string>()==secondId)++ownLift;}check(same&&ownLift==1,"collector retains same base identity and exactly one lift after UI transition");
   std::ofstream exported(std::string(argv[2])+"/records-"+variant+".json");exported<<"[";for(unsigned i=0;i<records.size();++i)exported<<(i?",":"")<<records[i];exported<<"]\n";
  }
  {Base empty(&nativeMod);Fixture::PlaceLiftState firstOnly(&empty,&globe,true);game.stack={&firstOnly};firstOnly.blit();AgentUiState ui;AgentRequest cell;cell.action="place_access_lift_cell";check(!firstOnly.agentUiState(ui)&&!firstOnly.agentDispatch(cell)&&empty.getFacilities()->empty(),"custom initial-base lift route remains closed");}
  {Base empty(&nativeMod);game.mod.lift=&wide;Fixture::PlaceLiftState oversized(&empty,&globe,false);game.stack={&oversized};oversized.blit();AgentUiState ui;AgentRequest cell;cell.action="place_access_lift_cell";check(!oversized.agentUiState(ui)&&!oversized.agentDispatch(cell)&&empty.getFacilities()->empty(),"non-size-one lift route remains closed");game.mod.lift=&lift;}
  for(auto*state:game.errors)delete state;game.errors.clear();game.stack.clear();
 }
 check(!SDL_GetVideoSurface(),"no game video opened");std::cout<<"PASS "<<checks<<" native new-base checks; explicit owning/resource/globe-registration/result-state seams\n";
}
