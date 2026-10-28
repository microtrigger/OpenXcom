// SPDX-License-Identifier: GPL-3.0-or-later
// Native owned soldier diary overview/performance/mission widgets and callbacks.
// Explicit owning Game/resource/minimal SoldierInfo and seeded history/medal seams.
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
#include "../../src/Savegame/SoldierDiary.h"
#include "../../src/Savegame/MissionStatistics.h"
#include "../../src/Savegame/BattleUnitStatistics.h"
#include "../../src/Mod/RuleCommendations.h"
#include "../../src/Savegame/Country.h"
// Test-only row-shape corruption, restored before ownership cleanup.
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
 const std::map<std::string,RuleCommendations*>&getCommendationsList(){return native->getCommendationsList();}
 RuleCommendations*getCommendation(const std::string&id){return native->getCommendation(id);}
 const std::vector<std::string>&getPsiRequirements(){return native->getPsiRequirements();}
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
 Language*getLanguage(){return language;}Screen *getScreen(){return &screen;}Resources *getMod(){return &mod;}SavedGame *getSavedGame(){return &saved;}
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
 void add(Surface*s,const std::string&element="",const std::string&interface=""){
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
enum SoldierDiaryDisplay{DIARY_KILLS,DIARY_MISSIONS,DIARY_COMMENDATIONS};
struct SoldierDiaryOverviewState;struct SoldierDiaryPerformanceState;
struct SoldierInfoState:State {Base*_base;size_t _soldierId;SoldierInfoState(Base*b,size_t id):_base(b),_soldierId(id){}void setSoldierId(size_t id){_soldierId=id;}void btnDiaryClick(Action*);};
struct SoldierDiaryMissionState:State {
 Soldier*_soldier;TextButton *_btnOk,*_btnPrev,*_btnNext;Window*_window;
 Text *_txtTitle,*_txtUFO,*_txtScore,*_txtKills,*_txtLocation,*_txtRace,*_txtDaylight,*_txtDaysWounded,*_txtNoRecord;TextList*_lstKills;int _rowEntry;
 int _agentMissionRowEntry=-1;std::vector<int>_agentMissionIds;std::vector<const MissionStatistics*>_agentMissionRecords;bool _agentMissionValid=false,_agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;
 SoldierDiaryMissionState(Soldier*,int);~SoldierDiaryMissionState();void init()override;void btnOkClick(Action*);void btnPrevClick(Action*);void btnNextClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
};
struct SoldierDiaryOverviewState:State {
 Base*_base;size_t _soldierId;SoldierInfoState*_soldierInfoState;Soldier*_soldier=nullptr;std::vector<Soldier*>*_list;
 TextButton *_btnOk,*_btnPrev,*_btnNext,*_btnKills,*_btnMissions,*_btnCommendations;Window*_window;
 Text *_txtTitle,*_txtMission,*_txtRating,*_txtDate;TextList*_lstDiary;std::vector<MissionStatistics*>_agentMissionRows;bool agentMissionRowValid(size_t)const;InteractiveSurface*auditSender=nullptr;size_t auditRow=999;void audit(Action*a){auditSender=a->getSender();auditRow=_lstDiary->getSelectedRow();}
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;
 SoldierDiaryOverviewState(Base*,size_t,SoldierInfoState*);~SoldierDiaryOverviewState();void init()override;void setSoldierId(size_t);
 void btnOkClick(Action*);void btnPrevClick(Action*);void btnNextClick(Action*);void btnKillsClick(Action*);void btnMissionsClick(Action*);void btnCommendationsClick(Action*);void lstDiaryInfoClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
};
struct SoldierDiaryPerformanceState:State {
 Base*_base;size_t _soldierId;SoldierDiaryOverviewState*_soldierDiaryOverviewState;Soldier*_soldier=nullptr;std::vector<Soldier*>*_list;
 TextButton *_btnOk,*_btnPrev,*_btnNext,*_btnKills,*_btnMissions,*_btnCommendations;Window*_window;
 Text *_txtTitle,*_txtMedalName,*_txtMedalLevel,*_txtMedalInfo;TextList *_lstPerformance,*_lstKillTotals,*_lstMissionTotals,*_lstCommendations;
 std::vector<std::string>_commendationsListEntry;std::vector<Surface*>_commendations,_commendationDecorations;SurfaceSet*_commendationSprite,*_commendationDecoration;
 SoldierDiaryDisplay _display;int _lastScrollPos;TextButton*_group;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;
 SoldierDiaryPerformanceState(Base*,size_t,SoldierDiaryOverviewState*,SoldierDiaryDisplay);~SoldierDiaryPerformanceState();void init()override;void drawSprites();
 void btnOkClick(Action*);void btnPrevClick(Action*);void btnNextClick(Action*);void btnKillsToggle(Action*);void btnMissionsToggle(Action*);void btnCommendationsToggle(Action*);void lstInfoMouseOver(Action*);void lstInfoMouseOut(Action*);void think()override;
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
};
#include "Diary.generated.inc"
#include "SoldierInfoEntry.generated.inc"
}
static unsigned checks=0;
static void check(bool value,const char*what){++checks;if(!value){std::cerr<<"FAIL "<<what<<'\n';std::exit(1);}}
static void closedActions(const AgentUiState&ui){for(const auto&a:ui.actions)check(a.argumentKind=="none"||a.argumentKind=="project_row"||a.argumentKind=="scroll_direction","diary action arguments limited to native navigation or visible row/scroll");}
int main(int argc,char**argv){
 check(argc==3&&SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen native fixture only");FileMap::load("common","./",false);Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}Options::baseXResolution=320;Options::baseYResolution=200;
 SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};Surface canvas(320,200),background(320,200);canvas.setPalette(palette);background.setPalette(palette);background.clear(40);SurfaceSet medalTexture(31,8);auto*medalFrame=medalTexture.addFrame(0);medalFrame->setPalette(palette);medalFrame->clear(20);
 for(const auto&variant:{"xcom1","xcom2"}){
  const std::string dir=std::string(argv[1])+"/bin/standard/"+variant+"/";Language language;language.loadFile(std::string(argv[1])+"/bin/common/Language/en-US.yml");language.loadFile(dir+"Language/en-US.yml");Mod native;RuleSoldier rule("STR_SOLDIER");Armor armor("FIXTURE_ARMOR");
  Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.mod.texture=&medalTexture;game.mod.native=&native;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;Base base(&native);auto*alpha=new Soldier(&rule,&armor,0);auto*bravo=new Soldier(&rule,&armor,0);alpha->setName("ALPHA");bravo->setName("BRAVO");base.getSoldiers()->push_back(alpha);base.getSoldiers()->push_back(bravo);game.saved.setFunds(1000000);
  std::vector<std::string>regions,ufos;for(auto r:YAML::LoadFile(dir+"regions.rul")["regions"])regions.push_back(r["type"].as<std::string>());for(auto r:YAML::LoadFile(dir+"ufos.rul")["ufos"])ufos.push_back(r["type"].as<std::string>());
  YAML::Node diary;for(int i=0;i<25;++i){auto*mission=new MissionStatistics;mission->id=i;mission->region=regions[i%regions.size()];mission->ufo=ufos[i%ufos.size()];mission->type="STR_BASE_DEFENSE";mission->rating="STR_GOOD";mission->success=true;mission->score=10;mission->time.load(YAML::Load("year: 1999\nmonth: 1\nday: 1\nweekday: 1"));game.saved.getMissionStatistics()->push_back(mission);diary["missionIdList"].push_back(i);}alpha->getDiary()->load(diary,&native);
  auto*medalRule=new RuleCommendations;medalRule->load(YAML::Load("description: STR_MEDAL_NAME\nsprite: 0"));native._commendations["STR_MEDALS"]=medalRule;alpha->getDiary()->getSoldierCommendations()->push_back(new SoldierCommendations("STR_MEDALS"));
  for(int i=0;i<6;++i){auto*receipt=new BattleUnitKills;receipt->name="Own history";receipt->rank="STR_LIVE_SOLDIER";receipt->race=std::string(variant)=="xcom1"?"STR_SECTOID":"STR_AQUATOID";receipt->weapon=std::string(variant)=="xcom1"?"STR_RIFLE":"STR_GAUSS_RIFLE";receipt->status=STATUS_DEAD;receipt->mission=0;receipt->id=i;alpha->getDiary()->getKills().push_back(receipt);}game.saved.getMissionStatistics()->front()->alienRace=std::string(variant)=="xcom1"?"STR_SECTOID":"STR_AQUATOID";
  const auto alphaBefore=YAML::Dump(alpha->getDiary()->save()),bravoBefore=YAML::Dump(bravo->getDiary()->save());
  {TextList strict(100,32,0,0);strict.setPalette(palette);strict.initText(&big,&small,&language);strict.setColumns(2,60,40);strict.addRow(1,"Header");strict.agentEnableReportCells(2);strict.blit(&canvas);std::vector<AgentOutfittingRow>captured;std::uint64_t epoch=0;check(!strict.agentReportRows(captured,epoch),"default strict report rejects ragged row");SDL_Rect target={};check(!strict.agentReportRowTarget(0,target),"report without selection opt-in has no native row action target");}
  {TextList optional(100,32,0,0);optional.setPalette(palette);optional.initText(&big,&small,&language);optional.setColumns(2,60,40);optional.addRow(1,"Header");optional.agentEnableReportCells(2,true);optional.blit(&canvas);std::vector<AgentOutfittingRow>captured;std::uint64_t epoch=0;check(optional.agentReportRows(captured,epoch)&&captured.size()==1&&captured[0].cells.size()==2&&captured[0].cells[0].known&&!captured[0].cells[1].known,"explicit missing-cell report retains heading and null absent column");auto*extra=new Text(40,9);optional._texts[0].push_back(extra);bool refused=!optional.agentReportRows(captured,epoch);optional._texts[0].pop_back();delete extra;check(refused,"postpaint row cell count change invalidates captured report");extra=new Text(40,9);auto*more=new Text(40,9);optional._texts[0].push_back(extra);optional._texts[0].push_back(more);refused=!optional.agentReportRows(captured,epoch);optional._texts[0].resize(1);delete extra;delete more;check(refused,"extra malformed columns rejected despite missing-cell opt-in");}
  Fixture::SoldierInfoState info(&base,1);game.stack={&info};info.btnDiaryClick(nullptr);auto*overview=dynamic_cast<Fixture::SoldierDiaryOverviewState*>(game.stack.back());check(overview&&overview->_soldierId==1,"literal SoldierInfo entry opens current living soldier diary");AgentUiState ui;AgentRequest ok;ok.action="close_soldier_diary";AgentRequest prev;prev.action="previous_personnel";AgentRequest next;next.action="next_personnel";AgentRequest performance;performance.action="open_diary_performance";
  check(!overview->agentDispatch(ok),"unpainted diary cannot dispatch");overview->blit();check(overview->agentUiState(ui)&&YAML::Load(ui.visible)["rows"].size()==0,"empty living diary visible without fabricated rows");closedActions(ui);std::ofstream(std::string(argv[2])+"/empty-"+variant+".json")<<ui.visible<<"\n";
  check(overview->agentDispatch(prev)&&overview->_soldierId==0,"native previous soldier navigation");check(!overview->agentDispatch(next),"navigation requires repaint before new grant");overview->blit();check(overview->agentUiState(ui)&&YAML::Load(ui.visible)["rows"].size()>0,"populated native owned mission diary rows");closedActions(ui);std::ofstream(std::string(argv[2])+"/overview-"+variant+".json")<<ui.visible<<"\n";
  for(auto row:YAML::Load(ui.visible)["rows"])check(row["cells"].size()==5,"overview report has five bounded logical cells");
  AgentRequest scroll;scroll.action="scroll_visible_list";scroll.choice="down";check(ui.offers(scroll.action)&&overview->agentDispatch(scroll)&&overview->_lstDiary->getScroll()>0,"native overview report scroll");overview->blit();scroll.choice="up";check(overview->agentDispatch(scroll),"native overview scroll up");overview->blit();
  const auto originalYear=overview->_lstDiary->getCellText(0,4);overview->_lstDiary->setCellText(0,4,"199919991999");overview->blit();check(overview->agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][4].IsNull(),"clipped diary date suffix is null rather than exported whole model caption");overview->_lstDiary->setCellText(0,4,originalYear);overview->blit();
  Surface rowCover(100,8,16,44);rowCover.setPalette(palette);rowCover.clear(99);overview->_surfaces.push_back(&rowCover);overview->blit();check(overview->agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull(),"real row caption occlusion projects null");overview->_surfaces.pop_back();overview->blit();
  SDL_Rect clipped={0,0,150,200};SDL_SetClipRect(canvas.getSurface(),&clipped);overview->blit();check(!overview->agentUiState(ui)&&!overview->agentDispatch(performance),"clipped destination invalidates diary authority");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!overview->agentDispatch(performance),"restored clip needs fresh painting");overview->blit();
  overview->_modal=overview->_btnOk;check(!overview->agentDispatch(performance),"diary modal blocks performance");overview->_modal=nullptr;
  Surface cover(70,16,86,176);cover.setPalette(palette);cover.clear(99);overview->_surfaces.push_back(&cover);overview->blit();check(!overview->agentDispatch(performance),"real performance-button overlay prevents dispatch");overview->_surfaces.pop_back();overview->blit();
  overview->_lstDiary->setVisible(false);overview->blit();check(overview->agentUiState(ui)&&YAML::Load(ui.visible)["rows"].size()==0,"hidden mission list has no model fallback");overview->_lstDiary->setVisible(true);check(!overview->agentUiState(ui),"unpainted list restore remains stale");overview->blit();
  AgentRequest select;select.action="select_diary_mission";select.choice="r0";check(overview->agentUiState(ui)&&ui.offers(select.action)&&YAML::Load(ui.visible)["rows"][0]["handle"].as<std::string>()=="r0","populated current report grants dense visible mission handle");AgentFrameCommand selectionFrame;check(selectionFrame.stage(select,ui,overview,1,1,100,100),"mission row frame authority staged");
  const auto firstCaption=overview->_lstDiary->getCellText(0,0);overview->_lstDiary->setCellText(0,0,"");check(!overview->agentDispatch(select),"caption mutation invalidates unpainted mission row grant");overview->blit();check(overview->agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["handle"].as<std::string>()=="r0"&&!selectionFrame.matchesFrame(100,1,1,overview,ui),"visible native row retains handle despite nullable caption while old frame grant fails");overview->_lstDiary->setCellText(0,0,firstCaption);overview->blit();
  Surface targetCover(288,8,8,44);targetCover.setPalette(palette);targetCover.clear(99);overview->_surfaces.push_back(&targetCover);overview->blit();check(overview->agentUiState(ui)&&std::find(ui.rows.begin(),ui.rows.end(),0)==ui.rows.end(),"real selected-row occlusion removes its native mission authority");overview->_surfaces.pop_back();overview->blit();
  auto*stats=game.saved.getMissionStatistics();std::swap((*stats)[0],(*stats)[1]);check(overview->agentUiState(ui)&&std::find(ui.rows.begin(),ui.rows.end(),0)==ui.rows.end(),"statistics reordered since native list population withholds mismatched row");std::swap((*stats)[0],(*stats)[1]);
  YAML::Node malformed;for(unsigned i=0;i<25;++i)malformed["missionIdList"].push_back(999);alpha->getDiary()->load(malformed,&native);check(overview->agentUiState(ui)&&!ui.offers(select.action)&&!overview->agentDispatch(select),"malformed mission mapping grants no dispatch and never constructs unsafe native detail");YAML::Node restored;for(unsigned i=0;i<25;++i)restored["missionIdList"].push_back(i);alpha->getDiary()->load(restored,&native);overview->blit();
  overview->_lstDiary->onMouseClick((ActionHandler)&Fixture::SoldierDiaryOverviewState::audit,0);check(overview->agentDispatch(select),"ordinary list hover click opens native mapped mission");auto*mission=dynamic_cast<Fixture::SoldierDiaryMissionState*>(game.stack.back());check(mission&&mission->_rowEntry==0&&overview->auditSender==overview->_lstDiary&&overview->auditRow==0,"visible native row maps owned mission ordinal zero with real non-null list sender");AgentRequest mok;mok.action="close_diary_mission";check(!mission->agentDispatch(mok),"opening mission popup animation blocks authority");for(int i=0;i<32&&mission->_window->agentPopupActive();++i)mission->_window->popup();mission->blit();{bool admitted=mission->agentUiState(ui);if(!admitted||YAML::Load(ui.visible)["rows"].size()==0)std::cerr<<"Mission diagnostic: admitted="<<admitted<<" list="<<mission->_lstKills->getRows()<<" model="<<alpha->getDiary()->getKills().size()<<" popup="<<mission->_window->agentPopupActive()<<" visible="<<ui.visible<<"\n";check(admitted&&YAML::Load(ui.visible)["rows"].size()>0,"native populated mission historical receipt rows visible");};std::ofstream(std::string(argv[2])+"/mission-"+variant+".json")<<ui.visible<<"\n";for(auto row:YAML::Load(ui.visible)["rows"])check(row["cells"].size()==3,"native mission record has three bounded caption cells");check((*stats)[0]->injuryList.empty(),"missing soldier injury lookup is pure find/default zero");AgentRequest mn;mn.action="next_diary_mission";check(mission->agentDispatch(mn)&&mission->_rowEntry==1,"native next mission updates ordinal");mission->blit();check(mission->agentUiState(ui)&&YAML::Load(ui.visible)["rows"].size()==0,"mission with no historical kill record has empty rows");std::ofstream(std::string(argv[2])+"/mission-empty-"+variant+".json")<<ui.visible<<"\n";
  YAML::Node reordered;reordered["missionIdList"].push_back(1);reordered["missionIdList"].push_back(0);for(int i=2;i<25;++i)reordered["missionIdList"].push_back(i);alpha->getDiary()->load(reordered,&native);mission->blit();check(!mission->agentUiState(ui)&&!mission->agentDispatch(mn),"valid mission ID reorder after init and repaint is refused");alpha->getDiary()->load(restored,&native);
  auto*originalRecord=(*stats)[0];MissionStatistics replacement=*originalRecord;(*stats)[0]=&replacement;mission->blit();check(!mission->agentUiState(ui)&&!mission->agentDispatch(mn),"same ID with replaced statistics object after repaint is refused");(*stats)[0]=originalRecord;
  mission->_rowEntry=0;mission->blit();check(!mission->agentUiState(ui)&&!mission->agentDispatch(mn),"ordinal changed without native init cannot reuse old mission captions after repaint");mission->_rowEntry=1;mission->blit();
  alpha->getDiary()->load(malformed,&native);mission->blit();check(!mission->agentUiState(ui)&&!mission->agentDispatch(mn)&&!mission->agentDispatch(mok),"history mutation after mission init closes all stale authority");alpha->getDiary()->load(restored,&native);check(mission->agentDispatch(mok)&&game.isState(overview),"native mission OK returns to overview");overview->blit();check(overview->agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["selected"].as<bool>(),"actual native row hover plus repaint exposes selected highlight");overview->_surfaces.push_back(&targetCover);overview->blit();check(overview->agentUiState(ui),"overview remains observable with selected target covered");for(auto row:YAML::Load(ui.visible)["rows"])check(!row["selected"].as<bool>(),"covered native selected row does not export highlight state");overview->_surfaces.pop_back();overview->blit();
  check(overview->agentDispatch(performance),"ordinary performance button opens mission aggregate screen");auto*totals=dynamic_cast<Fixture::SoldierDiaryPerformanceState*>(game.stack.back());check(totals&&totals->_display==Fixture::DIARY_MISSIONS,"only native missions performance mode reached");check(!overview->agentDispatch(ok),"inactive overview refuses input");totals->blit();check(totals->agentUiState(ui)&&YAML::Load(ui.visible)["rows"].size()>0&&YAML::Load(ui.visible)["totals"].size()==1,"native aggregate categories and totals displayed");closedActions(ui);std::ofstream(std::string(argv[2])+"/performance-"+variant+".json")<<ui.visible<<"\n";
  for(auto row:YAML::Load(ui.visible)["rows"])check(row["cells"].size()==2,"performance logical two-cell rows including section headings");check(YAML::Load(ui.visible)["totals"][0]["cells"].size()==4,"performance native mission win score wounded four-cell total");scroll.choice="down";check(ui.offers(scroll.action)&&totals->agentDispatch(scroll)&&totals->_lstPerformance->getScroll()>0,"native performance scroll");totals->blit();
  AgentRequest combat;combat.action="open_diary_combat";AgentRequest awards;awards.action="open_diary_awards";check(totals->agentDispatch(combat)&&totals->_display==Fixture::DIARY_KILLS&&totals->_group==totals->_btnKills,"ordinary combat tab transitions native display");totals->blit();check(totals->agentUiState(ui)&&YAML::Load(ui.visible)["mode"].as<std::string>()=="combat"&&YAML::Load(ui.visible)["rows"].size()>0,"native own historical combat aggregate report");std::ofstream(std::string(argv[2])+"/combat-"+variant+".json")<<ui.visible<<"\n";
  check(totals->agentDispatch(awards)&&totals->_display==Fixture::DIARY_COMMENDATIONS&&totals->_group==totals->_btnCommendations,"ordinary awards tab transitions native display");totals->blit();check(totals->agentUiState(ui)&&YAML::Load(ui.visible)["mode"].as<std::string>()=="awards"&&YAML::Load(ui.visible)["rows"].size()==1,"native seeded commendation displays report-only row");for(const auto&a:ui.actions)check(a.argumentKind!="project_row","awards report exposes no row tooltip mutation action");std::ofstream(std::string(argv[2])+"/awards-"+variant+".json")<<ui.visible<<"\n";check(totals->agentDispatch(performance)&&totals->_display==Fixture::DIARY_MISSIONS&&totals->_group==totals->_btnMissions,"ordinary performance tab returns mission aggregate");totals->blit();
  AgentRequest pnext;pnext.action="next_personnel";AgentRequest pok;pok.action="close_diary_performance";check(totals->agentDispatch(pnext)&&totals->_soldierId==1,"native performance next soldier");totals->blit();check(totals->agentUiState(ui),"empty soldier performance report supported");check(totals->agentDispatch(pok)&&game.isState(overview)&&overview->_soldierId==1,"native performance OK returns selected soldier to overview");overview->init();overview->blit();check(overview->agentDispatch(ok)&&game.isState(&info)&&info._soldierId==1,"native overview OK returns selection to SoldierInfo");

  {Fixture::SoldierDiaryOverviewState blocked(nullptr,0,&info);game.stack={&blocked};blocked.blit();check(!blocked.agentUiState(ui)&&!blocked.agentDispatch(ok),"memorial overview outside living diary scope");}
  check(YAML::Dump(alpha->getDiary()->save())==alphaBefore&&YAML::Dump(bravo->getDiary()->save())==bravoBefore&&game.saved.getFunds()==1000000,"view navigation scrolling and OK leave owned diaries and funds unchanged");for(auto*mission:*game.saved.getMissionStatistics())check(mission->injuryList.empty(),"native mission viewing does not insert missing injury keys");
  for(auto*owned:game.errors)delete owned;game.errors.clear();game.stack.clear();
 }
 check(!SDL_GetVideoSurface(),"no game video opened");std::cout<<"PASS "<<checks<<" native diary checks; explicit owning/resource/minimal-SoldierInfo seams\n";
}
