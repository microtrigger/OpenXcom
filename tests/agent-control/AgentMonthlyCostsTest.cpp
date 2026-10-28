// SPDX-License-Identifier: GPL-3.0-or-later
// Native MonthlyCosts constructor/provider/dispatch/OK and BaseInfo entry callback.
// Seams: owning Game/State initialization, minimal BaseInfo constructor, synthetic
// palette/background/interface resources and stock cost-only rule subsets.
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
#include "../../src/Mod/RuleSoldier.h"
#include "../../src/Mod/RuleCountry.h"
#include "../../src/Mod/Armor.h"
#include "../../src/Savegame/Craft.h"
#include "../../src/Savegame/Soldier.h"
#include "../../src/Savegame/Country.h"
#include "../../src/Interface/TextList.h"
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
struct MonthlyCostsState:State {
 Base *_base;TextButton *_btnOk;Window *_window;
 Text *_txtTitle,*_txtCost,*_txtQuantity,*_txtTotal,*_txtRental,*_txtSalaries,*_txtIncome,*_txtMaintenance;
 TextList *_lstCrafts,*_lstSalaries,*_lstMaintenance,*_lstTotal;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 MonthlyCostsState(Base*);~MonthlyCostsState();void btnOkClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
};
struct BaseInfoState:State {Base *_base;BaseInfoState(Base*b):_base(b){}void btnMonthlyCostsClick(Action*);};
#include "MonthlyCosts.generated.inc"
#include "BaseInfoClick.generated.inc"
}
static unsigned checks=0;
static void check(bool value,const char*what){++checks;if(!value){std::cerr<<"FAIL "<<what<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==3&&SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen native fixture only");FileMap::load("common","./",false);
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Options::baseXResolution=320;Options::baseYResolution=200;
 SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
 Surface canvas(320,200),background(320,200);canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const auto &variant:{"xcom1","xcom2"}){
  const std::string directory=std::string(argv[1])+"/bin/standard/"+variant+"/";Language language;language.loadFile(directory+"Language/en-US.yml");Mod native;
  const auto vars=YAML::LoadFile(directory+"vars.rul");native._costEngineer=vars["costEngineer"].as<int>();native._costScientist=vars["costScientist"].as<int>();
  for(auto row:YAML::LoadFile(directory+"crafts.rul")["crafts"])if(row["costRent"]&&row["costRent"].as<int>()){const auto id=row["type"].as<std::string>();auto*r=new RuleCraft(id);YAML::Node n;n["costRent"]=row["costRent"];r->load(n,&native,0);native._crafts[id]=r;native._craftsIndex.push_back(id);}
  for(auto row:YAML::LoadFile(directory+"soldiers.rul")["soldiers"]){const auto id=row["type"].as<std::string>();auto*r=new RuleSoldier(id);YAML::Node n;n["costSalary"]=row["costSalary"];r->load(n,&native);native._soldiers[id]=r;native._soldiersIndex.push_back(id);}
  Armor armor("FIXTURE_ARMOR");RuleBaseFacility facilityRule("STR_GENERAL_STORES");facilityRule._monthlyCost=5000;facilityRule._size=1;RuleCountry countryRule("FIXTURE_COUNTRY");
  Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.mod.native=&native;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
  auto*base=new Base(&native);base->setName("FIRST");base->setEngineers(2);base->setScientists(3);game.saved.getBases()->push_back(base);auto*other=new Base(&native);other->setEngineers(1);game.saved.getBases()->push_back(other);game.saved.setFunds(7654321);
  unsigned quantity=0;long craftTotal=0;for(const auto&id:native.getCraftsList()){++quantity;for(unsigned n=0;n<quantity;++n)base->getCrafts()->push_back(new Craft(native.getCraft(id),base,n+1));craftTotal+=quantity*native.getCraft(id)->getRentCost();}
  auto*sr=native.getSoldier(native.getSoldiersList().front());for(unsigned i=0;i<3;++i)base->getSoldiers()->push_back(new Soldier(sr,&armor,0));base->getFacilities()->push_back(new BaseFacility(&facilityRule,base));auto*country=new Country(&countryRule,false);country->getFunding().push_back(1234000);game.saved.getCountries()->push_back(country);
  const long expected=craftTotal+3*sr->getSalaryCost()+2*native.getEngineerCost()+3*native.getScientistCost()+5000;
  check(base->getMonthlyMaintenace()==expected&&game.saved.getBaseMaintenance()==expected+native.getEngineerCost(),"actual native base and campaign totals differ by other base staff");
  Fixture::BaseInfoState entry(base);game.stack={&entry};entry.btnMonthlyCostsClick(nullptr);auto*state=dynamic_cast<Fixture::MonthlyCostsState*>(game.stack.back());check(state&&state->_base==base,"literal BaseInfo ordinary entry opens native report for current base");AgentRequest close;close.action="close_monthly_costs";AgentUiState ui;
  check(!state->agentUiState(ui)&&!state->agentDispatch(close),"unpainted report no authority");state->blit();check(state->agentUiState(ui)&&ui.offers(close.action),"painted report offers ordinary close");std::ofstream(std::string(argv[2])+"/visible-"+variant+".json")<<ui.visible<<"\n";
  check(state->_txtIncome->getText().find(Unicode::formatFunding(1234000))!=std::string::npos&&state->_txtMaintenance->getText().find(Unicode::formatFunding(expected+native.getEngineerCost()))!=std::string::npos,"ordinary constructor paints campaign income and campaign maintenance");
  const auto rendered=ui.visible;check(rendered.find(Unicode::formatFunding(expected))!=std::string::npos,"visible report includes actual base total");
  auto report=YAML::Load(ui.visible);check(!report["income"].IsNull()&&report["income"].as<std::string>().find(Unicode::formatFunding(1234000))!=std::string::npos,"visible campaign income survives native adjacent-label allocation overlap");check(report["craft_rental_label"].as<std::string>()==std::string(language.getString("STR_CRAFT_RENTAL")),"stock rental heading visible in both games");
  check(report["craft_rows"].size()==native.getCraftsList().size()&&report["salary_rows"].size()==3&&report["maintenance_rows"].size()==1&&report["total_rows"].size()==1,"all stock-sized report logical rows retained");
  for(unsigned i=0;i<native.getCraftsList().size();++i){auto cells=report["craft_rows"][i]["cells"];auto*r=native.getCraft(native.getCraftsList()[i]);check(cells.size()==4&&cells[0].as<std::string>().find(std::string(language.getString(r->getType())))==0&&cells[1].as<std::string>().find(Unicode::formatFunding(r->getRentCost()))==0&&cells[2].as<std::string>().find(std::to_string(i+1))==0&&cells[3].as<std::string>()==Unicode::formatFunding((i+1)*r->getRentCost()),"native rental cost quantity and total complete including column extending one pixel beyond list");}
  const long salaryTotals[]={3*sr->getSalaryCost(),2*native.getEngineerCost(),3*native.getScientistCost()};for(unsigned i=0;i<3;++i)check(report["salary_rows"][i]["cells"].size()==4&&report["salary_rows"][i]["cells"][3].as<std::string>()==Unicode::formatFunding(salaryTotals[i]),"native salary totals match stock unit costs and actual personnel");
  check(report["maintenance_rows"][0]["cells"].size()==2&&report["maintenance_rows"][0]["cells"][1].as<std::string>().find(Unicode::formatFunding(5000))!=std::string::npos&&report["total_rows"][0]["cells"][1].as<std::string>()==Unicode::formatFunding(expected),"native two-column upkeep and current-base total");
  base->setEngineers(99);check(state->agentUiState(ui)&&ui.visible==rendered,"provider returns captured report not hidden model refresh or fallback");base->setEngineers(2);
  Surface cellCover(49,8,249,48);cellCover.setPalette(palette);cellCover.clear(99);state->_surfaces.push_back(&cellCover);state->blit();check(state->agentUiState(ui)&&YAML::Load(ui.visible)["craft_rows"][0]["cells"][3].IsNull()&&!YAML::Load(ui.visible)["craft_rows"][0]["cells"][0].IsNull(),"actual cell overlay yields null only for covered cell without model fallback");state->_surfaces.pop_back();state->blit();
  Surface incomeCover(1,1,20,148);incomeCover.setPalette(palette);incomeCover.clear(77);state->_surfaces.push_back(&incomeCover);state->blit();check(state->agentUiState(ui)&&YAML::Load(ui.visible)["income"].IsNull(),"real interior income overlay remains nullable");state->_surfaces.pop_back();state->blit();
  state->_lstCrafts->setVisible(false);state->blit();check(state->agentUiState(ui)&&YAML::Load(ui.visible)["craft_rows"].size()==0,"hidden report table not serialized from model");state->_lstCrafts->setVisible(true);check(!state->agentUiState(ui),"restored table without repaint is stale");state->blit();
  state->_modal=state->_btnOk;check(!state->agentDispatch(close),"modal blocks report close");state->_modal=nullptr;game.stack.pop_back();check(!state->agentUiState(ui)&&!state->agentDispatch(close),"inactive report refuses stale close");game.stack.push_back(state);
  state->_btnOk->setVisible(false);state->blit();check(state->agentUiState(ui)&&!ui.offers(close.action)&&!state->agentDispatch(close),"hidden close not offered");state->_btnOk->setVisible(true);check(!state->agentDispatch(close),"show without repaint remains stale");state->blit();
  Surface cover(300,20,10,170);cover.setPalette(palette);cover.clear(99);state->_surfaces.push_back(&cover);state->blit();check(!state->agentDispatch(close),"real close overlay blocks dispatch");state->_surfaces.pop_back();state->blit();
  SDL_Rect clip={0,0,150,200};SDL_SetClipRect(canvas.getSurface(),&clip);state->blit();check(!state->agentUiState(ui)&&!state->agentDispatch(close),"clipped current destination invalidates report authority");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state->agentDispatch(close),"clip restore without paint does not revive authority");state->blit();
  state->_btnOk->setX(11);state->_btnOk->setX(10);check(!state->agentDispatch(close),"button geometry ABA rejects old report authority");state->blit();
  check(state->agentDispatch(close)&&game.isState(&entry)&&game.saved.getFunds()==7654321&&base->getMonthlyMaintenace()==expected&&game.saved.getBases()->size()==2,"native OK only pops report without economic mutation");check(!state->agentDispatch(close),"popped report cannot replay OK");
  for(auto*owned:game.errors)delete owned;game.errors.clear();game.stack.clear();
 }
 check(!SDL_GetVideoSurface(),"no game video opened");std::cout<<"PASS "<<checks<<" native monthly-cost checks; explicit owning/resource/rule-subset seams\n";
}
