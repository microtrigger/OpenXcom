// SPDX-License-Identifier: GPL-3.0-or-later
// Actual constructor/provider/callback bodies; explicit Game/Mod/Craft seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/ToggleTextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Window.h"
#include "../../src/Interface/Bar.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <sstream>
#include "../../src/Engine/Unicode.h"
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
enum {FACTION_PLAYER=0,SIDE_FRONT=0,SIDE_LEFT=1,SIDE_RIGHT=2,SIDE_REAR=3,SIDE_UNDER=4};
struct Stats{int tu=60,stamina=70,health=40,bravery=50,reactions=55,firing=65,throwing=66,melee=67,strength=35,psiSkill=0,psiStrength=93;};
struct BattleUnit {Stats stats;int faction=0,id=1;int getFaction(){return faction;}int getTimeUnits(){return 43;}int getEnergy(){return 51;}int getHealth(){return 30;}int getStunlevel(){return 7;}int getFatalWounds(){return 2;}int getMorale(){return 83;}Stats*getBaseStats(){return &stats;}std::string getType(){return "SOLDIER";}std::string getRankString(){return "STR_ROOKIE";}std::string getName(Language*,bool){return "Test Unit "+std::to_string(id);}int getArmor(int side){return 11+side;}int getMaxArmor(int side){return 20+side;}};
struct BattleGame{std::vector<BattleUnit*>units;unsigned index=0;std::vector<BattleUnit*>*getUnits(){return &units;}BattleUnit*getSelectedUnit(){return units[index];}void selectPreviousPlayerUnit(bool,bool,bool){index=(index+units.size()-1)%units.size();}void selectNextPlayerUnit(bool,bool,bool){index=(index+1)%units.size();}};
struct Screen{Surface*surface;Surface*getSurface(){return surface;}void resetDisplay(bool){}static void updateScale(int,int,int,bool){}};
struct Mod{int getPsiRequirements(){return 0;}};
struct SavedGame{BattleGame*battle=nullptr;BattleGame*getSavedBattle(){return battle;}bool researched=false;bool isResearched(int){return researched;}};
struct Game{Screen screen;Mod mod;SavedGame save;Language*language;Font*big,*small;SDL_Color*palette;const void*top=nullptr;bool clear=true;int pops=0;Screen*getScreen(){return &screen;}Mod*getMod(){return &mod;}SavedGame*getSavedGame(){return &save;}Language*getLanguage(){return language;}bool isState(const void*s)const{return top==s;}bool agentOverlayClear(const AgentUiState&)const{return clear;}bool agentNonCursorOverlayClear(const AgentUiState&)const{return clear;}void popState(){++pops;top=nullptr;}};
static Game*currentGame;
struct State:OpenXcom::State{Game*_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;LocalizedText tr(const std::string&s)const{return _game->language->getString(s);}void add(Surface*s){s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);_surfaces.push_back(s);}void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}};
struct BattlescapeState{BattleGame*b;void selectPreviousPlayerUnit(bool a,bool c,bool d){b->selectPreviousPlayerUnit(a,c,d);}void selectNextPlayerUnit(bool a,bool c,bool d){b->selectNextPlayerUnit(a,c,d);}};
struct BattlescapeGame{static const bool _debugPlay=false;};
struct UnitInfoState:State {
 BattleGame*_battleGame;BattleUnit*_unit;BattlescapeState*_parent=nullptr;bool _fromInventory=true,_mindProbe=false;
 Surface*_bg;InteractiveSurface*_exit;Text*_txtName;TextButton*_btnPrev,*_btnNext;
 bool _agentPaintFull=false;const BattleUnit*_agentPaintUnit=nullptr;std::vector<std::uint64_t>_agentPaintRevisions;
 Text *_txtTimeUnits,*_numTimeUnits; Bar *_barTimeUnits;
 Text *_txtEnergy,*_numEnergy; Bar *_barEnergy;
 Text *_txtHealth,*_numHealth; Bar *_barHealth;
 Text *_txtFatalWounds,*_numFatalWounds; Bar *_barFatalWounds;
 Text *_txtBravery,*_numBravery; Bar *_barBravery;
 Text *_txtMorale,*_numMorale; Bar *_barMorale;
 Text *_txtReactions,*_numReactions; Bar *_barReactions;
 Text *_txtFiring,*_numFiring; Bar *_barFiring;
 Text *_txtThrowing,*_numThrowing; Bar *_barThrowing;
 Text *_txtMelee,*_numMelee; Bar *_barMelee;
 Text *_txtStrength,*_numStrength; Bar *_barStrength;
 Text *_txtPsiStrength,*_numPsiStrength; Bar *_barPsiStrength;
 Text *_txtPsiSkill,*_numPsiSkill; Bar *_barPsiSkill;
 Text *_txtFrontArmor,*_numFrontArmor; Bar *_barFrontArmor;
 Text *_txtLeftArmor,*_numLeftArmor; Bar *_barLeftArmor;
 Text *_txtRightArmor,*_numRightArmor; Bar *_barRightArmor;
 Text *_txtRearArmor,*_numRearArmor; Bar *_barRearArmor;
 Text *_txtUnderArmor,*_numUnderArmor; Bar *_barUnderArmor;
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;void init()override;void btnPrevClick(Action*);void btnNextClick(Action*);void exitClick(Action*);
 UnitInfoState(BattleGame*b):_battleGame(b),_unit(b->getSelectedUnit()){
 _bg=new Surface(320,200);add(_bg);_bg->clear(40);_exit=new InteractiveSurface(320,180,0,20);add(_exit);_txtName=new Text(288,17,16,4);add(_txtName);
 _txtTimeUnits=new Text(140,9,8,38);_numTimeUnits=new Text(18,9,150,38);_barTimeUnits=new Bar(150,5,170,39);add(_txtTimeUnits);add(_numTimeUnits);add(_barTimeUnits);_txtTimeUnits->setText("TimeUnits");_barTimeUnits->setColor(90);_barTimeUnits->setSecondaryColor(100);_barTimeUnits->setScale(1);
 _txtEnergy=new Text(140,9,8,47);_numEnergy=new Text(18,9,150,47);_barEnergy=new Bar(150,5,170,48);add(_txtEnergy);add(_numEnergy);add(_barEnergy);_txtEnergy->setText("Energy");_barEnergy->setColor(90);_barEnergy->setSecondaryColor(100);_barEnergy->setScale(1);
 _txtHealth=new Text(140,9,8,56);_numHealth=new Text(18,9,150,56);_barHealth=new Bar(150,5,170,57);add(_txtHealth);add(_numHealth);add(_barHealth);_txtHealth->setText("Health");_barHealth->setColor(90);_barHealth->setSecondaryColor(100);_barHealth->setScale(1);
 _txtFatalWounds=new Text(140,9,8,65);_numFatalWounds=new Text(18,9,150,65);_barFatalWounds=new Bar(150,5,170,66);add(_txtFatalWounds);add(_numFatalWounds);add(_barFatalWounds);_txtFatalWounds->setText("FatalWounds");_barFatalWounds->setColor(90);_barFatalWounds->setSecondaryColor(100);_barFatalWounds->setScale(1);
 _txtBravery=new Text(140,9,8,74);_numBravery=new Text(18,9,150,74);_barBravery=new Bar(150,5,170,75);add(_txtBravery);add(_numBravery);add(_barBravery);_txtBravery->setText("Bravery");_barBravery->setColor(90);_barBravery->setSecondaryColor(100);_barBravery->setScale(1);
 _txtMorale=new Text(140,9,8,83);_numMorale=new Text(18,9,150,83);_barMorale=new Bar(150,5,170,84);add(_txtMorale);add(_numMorale);add(_barMorale);_txtMorale->setText("Morale");_barMorale->setColor(90);_barMorale->setSecondaryColor(100);_barMorale->setScale(1);
 _txtReactions=new Text(140,9,8,92);_numReactions=new Text(18,9,150,92);_barReactions=new Bar(150,5,170,93);add(_txtReactions);add(_numReactions);add(_barReactions);_txtReactions->setText("Reactions");_barReactions->setColor(90);_barReactions->setSecondaryColor(100);_barReactions->setScale(1);
 _txtFiring=new Text(140,9,8,101);_numFiring=new Text(18,9,150,101);_barFiring=new Bar(150,5,170,102);add(_txtFiring);add(_numFiring);add(_barFiring);_txtFiring->setText("Firing");_barFiring->setColor(90);_barFiring->setSecondaryColor(100);_barFiring->setScale(1);
 _txtThrowing=new Text(140,9,8,110);_numThrowing=new Text(18,9,150,110);_barThrowing=new Bar(150,5,170,111);add(_txtThrowing);add(_numThrowing);add(_barThrowing);_txtThrowing->setText("Throwing");_barThrowing->setColor(90);_barThrowing->setSecondaryColor(100);_barThrowing->setScale(1);
 _txtMelee=new Text(140,9,8,119);_numMelee=new Text(18,9,150,119);_barMelee=new Bar(150,5,170,120);add(_txtMelee);add(_numMelee);add(_barMelee);_txtMelee->setText("Melee");_barMelee->setColor(90);_barMelee->setSecondaryColor(100);_barMelee->setScale(1);
 _txtStrength=new Text(140,9,8,128);_numStrength=new Text(18,9,150,128);_barStrength=new Bar(150,5,170,129);add(_txtStrength);add(_numStrength);add(_barStrength);_txtStrength->setText("Strength");_barStrength->setColor(90);_barStrength->setSecondaryColor(100);_barStrength->setScale(1);
 _txtPsiStrength=new Text(140,9,8,137);_numPsiStrength=new Text(18,9,150,137);_barPsiStrength=new Bar(150,5,170,138);add(_txtPsiStrength);add(_numPsiStrength);add(_barPsiStrength);_txtPsiStrength->setText("PsiStrength");_barPsiStrength->setColor(90);_barPsiStrength->setSecondaryColor(100);_barPsiStrength->setScale(1);
 _txtPsiSkill=new Text(140,9,8,146);_numPsiSkill=new Text(18,9,150,146);_barPsiSkill=new Bar(150,5,170,147);add(_txtPsiSkill);add(_numPsiSkill);add(_barPsiSkill);_txtPsiSkill->setText("PsiSkill");_barPsiSkill->setColor(90);_barPsiSkill->setSecondaryColor(100);_barPsiSkill->setScale(1);
 _txtFrontArmor=new Text(140,9,8,155);_numFrontArmor=new Text(18,9,150,155);_barFrontArmor=new Bar(150,5,170,156);add(_txtFrontArmor);add(_numFrontArmor);add(_barFrontArmor);_txtFrontArmor->setText("FrontArmor");_barFrontArmor->setColor(90);_barFrontArmor->setSecondaryColor(100);_barFrontArmor->setScale(1);
 _txtLeftArmor=new Text(140,9,8,164);_numLeftArmor=new Text(18,9,150,164);_barLeftArmor=new Bar(150,5,170,165);add(_txtLeftArmor);add(_numLeftArmor);add(_barLeftArmor);_txtLeftArmor->setText("LeftArmor");_barLeftArmor->setColor(90);_barLeftArmor->setSecondaryColor(100);_barLeftArmor->setScale(1);
 _txtRightArmor=new Text(140,9,8,173);_numRightArmor=new Text(18,9,150,173);_barRightArmor=new Bar(150,5,170,174);add(_txtRightArmor);add(_numRightArmor);add(_barRightArmor);_txtRightArmor->setText("RightArmor");_barRightArmor->setColor(90);_barRightArmor->setSecondaryColor(100);_barRightArmor->setScale(1);
 _txtRearArmor=new Text(140,9,8,182);_numRearArmor=new Text(18,9,150,182);_barRearArmor=new Bar(150,5,170,183);add(_txtRearArmor);add(_numRearArmor);add(_barRearArmor);_txtRearArmor->setText("RearArmor");_barRearArmor->setColor(90);_barRearArmor->setSecondaryColor(100);_barRearArmor->setScale(1);
 _txtUnderArmor=new Text(140,9,8,191);_numUnderArmor=new Text(18,9,150,191);_barUnderArmor=new Bar(150,5,170,192);add(_txtUnderArmor);add(_numUnderArmor);add(_barUnderArmor);_txtUnderArmor->setText("UnderArmor");_barUnderArmor->setColor(90);_barUnderArmor->setSecondaryColor(100);_barUnderArmor->setScale(1);
_btnPrev=new TextButton(14,18,2,2);_btnNext=new TextButton(14,18,304,2);add(_btnPrev);add(_btnNext);_btnPrev->setText("<<");_btnNext->setText(">>");for(auto*w:_surfaces)w->agentTrackPresentation();init();}
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "UnitInfo.generated.inc"
}
static unsigned checks=0;static void check(bool ok,const char*m){++checks;if(!ok){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){check(argc==3,"args");check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;Options::maximizeInfoScreens=false;Options::psiStrengthEval=false;
Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}Surface canvas(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);
for(std::string master:{"xcom1","xcom2"}){Language language;language.loadFile(std::string(argv[1])+"/standard/"+master+"/Language/en-US.yml");Fixture::Game game;game.screen.surface=&canvas;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;Fixture::BattleUnit a,b;b.id=2;Fixture::BattleGame battle;battle.units={&a,&b};game.save.battle=&battle;Fixture::UnitInfoState state(&battle);game.top=&state;AgentUiState ui;AgentRequest r;
check(!state.agentUiState(ui),"prepaint");state.blit();check(state.agentUiState(ui),"painted");auto v=YAML::Load(ui.visible);check(v["rows"].size()==18,"18rows");check(v["rows"][11]["value"].IsNull()&&v["rows"][12]["bar"].IsNull(),"psiunknownhidden");check(v["rows"][3]["value"].as<std::string>()=="2","wounds");check(v["rows"][13]["value"].as<std::string>()=="11","frontarmor");check(v["rows"][17]["value"].as<std::string>()=="15","underarmor");check(v["rows"][2]["bar"]["runs"].size()>0,"paintedhealthbar");
state._numHealth->setText("29");check(!state.agentUiState(ui),"stale text");state.blit();check(state.agentUiState(ui),"repaint");game.clear=false;r.action="next_unit_info";check(!state.agentDispatch(r),"overlaynext");r.action="close_unit_info";check(!state.agentDispatch(r),"overlayclose");game.clear=true;state._modal=state._exit;check(!state.agentUiState(ui),"modal");state._modal=nullptr;game.save.battle=nullptr;check(!state.agentUiState(ui),"retiredbattle");game.save.battle=&battle;auto saved=battle.units;battle.units={&b};check(!state.agentUiState(ui),"retiredunit");battle.units=saved;a.faction=1;check(!state.agentUiState(ui),"enemydenied");a.faction=0;state._mindProbe=true;check(!state.agentUiState(ui),"probenotsupported");state._mindProbe=false;
a.stats.psiSkill=10;state.init();state.blit();check(state.agentUiState(ui),"knownpsipainted");v=YAML::Load(ui.visible);check(v["rows"][11]["value"].as<std::string>()=="93"&&v["rows"][12]["value"].as<std::string>()=="10","knownpsiUI");a.stats.psiSkill=0;Options::psiStrengthEval=true;game.save.researched=true;state.init();state.blit();check(state.agentUiState(ui),"researchedpsi");v=YAML::Load(ui.visible);check(!v["rows"][11]["value"].IsNull()&&v["rows"][12]["value"].IsNull(),"skillstillhidden");Options::psiStrengthEval=false;game.save.researched=false;
r.action="next_unit_info";check(state.agentDispatch(r)&&battle.index==1,"actualnext");check(!state.agentUiState(ui),"newunitunpainted");state.blit();check(state.agentUiState(ui),"nextpaint");r.action="previous_unit_info";check(state.agentDispatch(r)&&battle.index==0,"actualprev");state.blit();check(state.agentUiState(ui),"prevpaint");r.action="close_unit_info";check(state.agentDispatch(r)&&game.pops==1,"actualclose");check(!state.agentDispatch(r),"wrongtop");state.release();}
std::cout<<"PASS "<<checks<<" actual UnitInfo init/provider/prev/next/close with synthetic player-unit/layout seams\n";}
