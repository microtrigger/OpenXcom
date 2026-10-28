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
#include "../../src/Observer/AgentOutfitting.h"
#include "../../src/Savegame/QuantityCommand.h"
#include "../../src/Interface/TextEdit.h"
#include "../../src/Interface/ComboBox.h"
#include "../../src/Interface/Window.h"
#include "../../src/Interface/Bar.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <sys/socket.h>
#include <sys/wait.h>
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
namespace Outfitting {
struct Craft { std::vector<void*> weapons;std::vector<void*>*getWeapons(){return &weapons;} std::string status="STR_READY",name="MCP Transport";const std::string &getStatus()const{return status;}std::string getName(Language*)const{return name;} };
struct Armor { static const std::string NONE;std::string type="STR_NONE_UC",store="STR_NONE";std::string getStoreItem()const{return store;}std::string getType()const{return type;} };const std::string Armor::NONE="STR_NONE";
struct Soldier {std::string name="MCP Healthy";Craft*craft=nullptr;Armor*armor=nullptr;int wound=0;Craft*getCraft()const{return craft;}void setCraft(Craft*c){craft=c;}Armor*getArmor()const{return armor;}void setArmor(Armor*a){armor=a;}std::string getName()const{return name;} };
struct ItemContainer {std::map<std::string,int>counts;void addItem(const std::string&k,int n=1){counts[k]+=n;}void removeItem(const std::string&k,int n=1){counts[k]-=n;}};
struct Base {std::vector<Soldier*>soldiers;std::vector<Craft*>crafts;ItemContainer items;std::vector<Soldier*>*getSoldiers(){return &soldiers;}std::vector<Craft*>*getCrafts(){return &crafts;}ItemContainer*getStorageItems(){return &items;}bool storesOverfull(){return false;}std::string getName(){return "MCP Outfitting";}};
struct SavedGame {int getMonthsPassed(){return 0;}std::string armor;void setLastSelectedArmor(const std::string&a){armor=a;}};
struct Screen {Surface*logical=nullptr;Surface*getSurface(){return logical;}};
struct Mod {struct Element {int color=1;} element;Mod*getInterface(const std::string&){return this;}Element*getElement(const std::string&){return &element;}};
struct Game {mutable Screen screen;SavedGame save;Mod mod;Language*lang=nullptr;const void*top=nullptr;int parent=0;unsigned pops=0,pushes=0,commands=0;std::string destination;Surface *_cursor=nullptr,*_fpsCounter=nullptr;
 Screen*getScreen()const{return &screen;}SavedGame*getSavedGame(){return &save;}Language*getLanguage(){return lang;}Mod*getMod(){return &mod;}
 bool isState(const void*p)const{return top==p;}void popState(){++pops;top=&parent;}template<class T>void pushState(T*p){++pushes;destination=p->kind;top=&parent;delete p;}
 bool agentOverlayClear(const AgentUiState&)const;
 bool agentNonCursorOverlayClear(const AgentUiState&)const;
};
#include "CompletionOverlay.generated.inc"
Game*active=nullptr;
struct State:OpenXcom::State {using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;Game*_game;Base*_base;std::string kind;State(Base*b=nullptr):_game(active),_base(b){if(_game)_game->top=this;}LocalizedText tr(const std::string&s)const{return _game->lang->getString(s);}void blit()override;};
#include "CompletionOrdinaryBlit.generated.inc"
struct SellState:State {using State::State;};
struct ErrorMessageState:State {template<class... T>ErrorMessageState(T...):State(){}};
struct SoldiersState;
struct SoldierInfoState;
struct CraftsState;
struct CraftInfoState;
struct CraftSoldiersState;
struct CraftEquipmentState;
struct CraftArmorState;
struct SoldierArmorState;
struct SoldiersState:State {
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull=false;
    std::uint64_t _agentUiRevision=0;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
#endif
	TextButton *_btnOk, *_btnPsiTraining, *_btnMemorial;
	Window *_window;
	Text *_txtTitle, *_txtName, *_txtRank, *_txtCraft;
	TextList *_lstSoldiers;
	

SoldiersState(Base*b=nullptr,size_t index=0):State(b){kind="SoldiersState";}
void blit() override;
bool agentUiState(AgentUiState&)const;
bool agentDispatch(const AgentRequest&);
void btnOkClick(Action*);
void lstSoldiersClick(Action*);
// Unexercised later-history navigation seams, not acceptance.
void btnMemorialClick(Action*){++_game->pushes;_game->top=&_game->parent;}
};
struct SoldierInfoState:State {
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull=false;
    std::uint64_t _agentUiRevision=0;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
#endif
	
	size_t _soldierId;
	Soldier *_soldier;
	std::vector<Soldier*> *_list;

	Surface *_bg, *_rank;
	TextButton *_btnOk, *_btnPrev, *_btnNext, *_btnArmor, *_btnSack, *_btnDiary;
	Text *_txtRank, *_txtMissions, *_txtKills, *_txtCraft, *_txtRecovery, *_txtPsionic, *_txtDead;
	TextEdit *_edtSoldier;

	Text *_txtTimeUnits, *_txtStamina, *_txtHealth, *_txtBravery, *_txtReactions, *_txtFiring, *_txtThrowing, *_txtMelee, *_txtStrength, *_txtPsiStrength, *_txtPsiSkill;
	Text *_numTimeUnits, *_numStamina, *_numHealth, *_numBravery, *_numReactions, *_numFiring, *_numThrowing, *_numMelee, *_numStrength, *_numPsiStrength, *_numPsiSkill;
	Bar *_barTimeUnits, *_barStamina, *_barHealth, *_barBravery, *_barReactions, *_barFiring, *_barThrowing, *_barMelee, *_barStrength, *_barPsiStrength, *_barPsiSkill;


SoldierInfoState(Base*b=nullptr,size_t index=0):State(b){kind="SoldierInfoState";_soldierId=index;_list=b?b->getSoldiers():nullptr;_soldier=b?b->getSoldiers()->at(index):nullptr;}
void blit() override;
bool agentUiState(AgentUiState&)const;
bool agentDispatch(const AgentRequest&);
void btnOkClick(Action*);
void btnPrevClick(Action*);
void btnNextClick(Action*);
void btnArmorClick(Action*);
bool agentDeadRosterValid()const{return false;}
void btnDiaryClick(Action*){++_game->pushes;_game->top=&_game->parent;}
void init();
};
struct CraftsState:State {
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull=false;
    std::uint64_t _agentUiRevision=0;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
#endif
	TextButton *_btnOk;
	Window *_window;
	Text *_txtTitle, *_txtBase, *_txtName, *_txtStatus, *_txtWeapon, *_txtCrew, *_txtHwp;
	TextList *_lstCrafts;
	

CraftsState(Base*b=nullptr,size_t index=0):State(b){kind="CraftsState";}
void blit() override;
bool agentUiState(AgentUiState&)const;
bool agentDispatch(const AgentRequest&);
void btnOkClick(Action*);
void lstCraftsClick(Action*);
};
struct CraftInfoState:State {
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull=false;
    std::uint64_t _agentUiRevision=0;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
#endif
	
	size_t _craftId;
	Craft *_craft;

	TextButton *_btnOk, *_btnW1, *_btnW2, *_btnCrew, *_btnEquip, *_btnArmor;
	Window *_window;
	TextEdit *_edtCraft;
	Text *_txtDamage, *_txtFuel;
	Text *_txtW1Name, *_txtW1Ammo, *_txtW2Name, *_txtW2Ammo;
	Surface *_sprite, *_weapon1, *_weapon2, *_crew, *_equip;
	/// Formats an amount of time.
	

CraftInfoState(Base*b=nullptr,size_t index=0):State(b){kind="CraftInfoState";_craftId=index;_craft=b?b->getCrafts()->at(index):nullptr;}
void blit() override;
bool agentUiState(AgentUiState&)const;
bool agentDispatch(const AgentRequest&);
void btnOkClick(Action*);
void btnCrewClick(Action*);
// Legacy transport-only fixture seams; armament callbacks have their own current-source oracle.
void btnW1Click(Action*){++_game->pushes;_game->top=&_game->parent;}
void btnW2Click(Action*){++_game->pushes;_game->top=&_game->parent;}
void btnEquipClick(Action*);
void btnArmorClick(Action*);
};
struct CraftSoldiersState:State {
	TextButton *_btnOk;
	Window *_window;
	Text *_txtTitle, *_txtName, *_txtRank, *_txtCraft, *_txtAvailable, *_txtUsed;
	ComboBox *_cbxSortBy;
	TextList *_lstSoldiers;

	
	size_t _craft;
	Uint8 _otherCraftColor;
	std::vector<Soldier *> _origSoldierOrder;
	

	/// initializes the display list based on the craft soldier's list
	

 bool _agentDestinationPaintFull=false;std::uint64_t _agentUiRevision=0;std::vector<std::uint64_t>_agentStoredPaintRevisions;

CraftSoldiersState(Base*b=nullptr,size_t index=0):State(b){kind="CraftSoldiersState";_craft=index;}
void blit() override;
bool agentUiState(AgentUiState&)const;
bool agentDispatch(const AgentRequest&);
void btnOkClick(Action*);
void setSoldierAssigned(size_t row,bool assigned);
void inspectSoldier(size_t row);
};
struct CraftEquipmentState:State {
	TextButton *_btnOk, *_btnClear, *_btnInventory;
	Window *_window;
	Text *_txtTitle, *_txtItem, *_txtStores, *_txtAvailable, *_txtUsed, *_txtCrew;
	TextList *_lstEquipment;
	Timer *_timerLeft, *_timerRight;
	size_t _sel, _craft;
	
	std::vector<std::string> _items;
	int _totalItems;
	Uint8 _ammoColor;
	/// Updates quantities of item.
	

 bool _agentDestinationPaintFull=false;std::uint64_t _agentUiRevision=0;std::vector<std::uint64_t>_agentStoredPaintRevisions;

CraftEquipmentState(Base*b=nullptr,size_t index=0):State(b){kind="CraftEquipmentState";_craft=index;}
void blit() override;
bool agentUiState(AgentUiState&)const;
bool agentDispatch(const AgentRequest&);
void btnOkClick(Action*);
void changeEquipmentQuantity(size_t row,QuantityOperation operation,int value=0);
};
struct CraftArmorState:State {
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull=false;
    std::uint64_t _agentUiRevision=0;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
#endif
	TextButton *_btnOk;
	Window *_window;
	Text *_txtTitle, *_txtName, *_txtCraft, *_txtArmor;
	TextList *_lstSoldiers;

	
	size_t _craft;

CraftArmorState(Base*b=nullptr,size_t index=0):State(b){kind="CraftArmorState";_craft=index;}
void blit() override;
bool agentUiState(AgentUiState&)const;
bool agentDispatch(const AgentRequest&);
void btnOkClick(Action*);
void openSoldierArmor(size_t row);
};
struct SoldierArmorState:State {
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull=false;
    std::uint64_t _agentUiRevision=0;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
#endif
	
	size_t _soldier;

	TextButton *_btnCancel;
	Window *_window;
	Text *_txtTitle, *_txtType, *_txtQuantity;
	TextList *_lstArmor;
	std::vector<Armor*> _armors;

SoldierArmorState(Base*b=nullptr,size_t index=0):State(b){kind="SoldierArmorState";_soldier=index;}
void blit() override;
bool agentUiState(AgentUiState&)const;
bool agentDispatch(const AgentRequest&);
void btnCancelClick(Action*);
void lstArmorClick(Action*);
};
#include "SoldiersStateMethods.generated.inc"
#include "SoldierInfoStateMethods.generated.inc"
#include "CraftsStateMethods.generated.inc"
#include "CraftInfoStateMethods.generated.inc"
#include "CraftSoldiersStateMethods.generated.inc"
#include "CraftEquipmentStateMethods.generated.inc"
#include "CraftArmorStateMethods.generated.inc"
#include "SoldierArmorStateMethods.generated.inc"
// Simulation/refresh seams are explicit; independent real-command oracle is separate.
void SoldierInfoState::init(){++_agentUiRevision;_soldier=_list->at(_soldierId);_edtSoldier->setText(_soldier->name);_txtRecovery->setText(_soldier->wound?"Wound recovery: 1 day":"");}
void CraftSoldiersState::setSoldierAssigned(size_t row,bool assigned){++_game->commands;auto*s=_base->getSoldiers()->at(row);auto*c=_base->getCrafts()->at(_craft);if(assigned){if(!s->wound)s->setCraft(c);}else if(s->getCraft()==c)s->setCraft(nullptr);_lstSoldiers->setCellText(row,2,s->getCraft()?s->getCraft()->name:"NONE");}
void CraftSoldiersState::inspectSoldier(size_t row){_game->pushState(new SoldierInfoState(_base,row));}
void CraftEquipmentState::changeEquipmentQuantity(size_t row,QuantityOperation,int){++_game->commands;_lstEquipment->setCellText(row,2,"2");}
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
  base.loadDat(dir+"PALETTES.DAT",256,Palette::palOffset(1));back.loadDat(dir+"BACKPALS.DAT",128);
  unsigned backIndex=0;for(auto e:rules["elements"])if(e["id"].as<std::string>()=="palette")if(e[hyper?"color2":"color"])backIndex=e[hyper?"color2":"color"].as<unsigned>();
  for(unsigned i=0;i<256;++i){palette[i]=base.getColors()[i];}
  for(unsigned i=0;i<16;++i){palette[Palette::backPos+i]=back.getColors(Palette::blockOffset(backIndex))[i];}
  canvas.setPalette(palette);background.setPalette(palette);background.clear(40);surfaces.push_back(&background);
 }
 void colors(Surface*widget,const std::string&id){for(auto e:rules["elements"])if(e["id"].as<std::string>()==id){if(e["color"])widget->setColor(e["color"].as<int>());if(e["color2"])widget->setSecondaryColor(e["color2"].as<int>());if(e["border"])widget->setBorderColor(e["border"].as<int>());if(e["TFTDMode"])widget->setTFTDMode(e["TFTDMode"].as<bool>());}}
 int color(const std::string&id)const{for(auto e:rules["elements"])if(e["id"].as<std::string>()==id)if(e["color"])return e["color"].as<int>();return 1;}
 template<class T>T*own(T*p){p->setPalette(palette);owned.emplace_back(p);surfaces.push_back(p);return p;}
 Text*text(int w,int h,int x,int y,const std::string&value,bool bigText=false,const std::string&id="text1"){
  auto*t=own(new Text(w,h,x,y));t->initText(&big,&small,&lang);colors(t,id);if(bigText)t->setBig();t->setText(value);return t;
 }
 TextButton*button(int w,int h,int x,int y,const std::string&key){auto*b=own(new TextButton(w,h,x,y));b->initText(&big,&small,&lang);colors(b,"button");b->setText(lang.getString(key));return b;}
 TextList*list(int w,int h,int x,int y,const std::string&id){auto*l=own(new CountedList(w,h,x,y));l->initText(&big,&small,&lang);colors(l,id);l->setBackground(&background);return l;}
 TextEdit*edit(OpenXcom::State*s,int w,int h,int x,int y,const std::string&value){auto*t=own(new TextEdit(s,w,h,x,y));t->initText(&big,&small,&lang);colors(t,"text1");t->setBig();t->setText(value);return t;}
 void draw(){if(render)render();else for(auto*s:surfaces)s->blit(&canvas);}
};
template<class T>void finishSetup(T&s,Paint&p){s._surfaces=p.surfaces;s._game->screen.logical=&p.canvas;p.render=[&](){s.blit();};p.draw();}
void setup(Outfitting::SoldiersState&s,Paint&p){
 s._btnOk=p.button(148,16,164,176,"STR_OK");s._txtTitle=p.text(310,17,5,8,p.lang.getString("STR_SOLDIER_LIST"),true);s._txtTitle->setAlign(ALIGN_CENTER);
 s._txtName=p.text(114,9,16,32,p.lang.getString("STR_NAME_UC"),false,"text2");s._txtRank=p.text(102,9,130,32,p.lang.getString("STR_RANK"),false,"text2");s._txtCraft=p.text(82,9,222,32,p.lang.getString("STR_CRAFT"),false,"text2");
 s._lstSoldiers=p.list(288,128,8,40,"list");s._lstSoldiers->setColumns(3,114,92,74);s._lstSoldiers->setSelectable(true);s._lstSoldiers->setMargin(8);s._lstSoldiers->agentEnableOutfitting(3);
 for(auto*person:s._base->soldiers)s._lstSoldiers->addRow(3,person->name.c_str(),p.lang.getString("STR_ROOKIE").c_str(),person->craft?person->craft->name.c_str():"NONE");finishSetup(s,p);
}
void setup(Outfitting::CraftsState&s,Paint&p){
 s._btnOk=p.button(288,16,16,176,"STR_OK");s._txtTitle=p.text(298,17,16,8,p.lang.getString("STR_INTERCEPTION_CRAFT"),true,"text");s._txtBase=p.text(298,17,16,24,p.lang.getString("STR_BASE_").arg("MCP Outfitting"),true,"text");
 s._txtName=p.text(94,9,16,40,p.lang.getString("STR_NAME_UC"),false,"text");s._txtStatus=p.text(50,9,110,40,p.lang.getString("STR_STATUS"),false,"text");s._txtWeapon=p.text(50,17,160,40,p.lang.getString("STR_WEAPON_SYSTEMS"),false,"text");s._txtWeapon->setWordWrap(true);s._txtCrew=p.text(58,9,210,40,p.lang.getString("STR_CREW"),false,"text");s._txtHwp=p.text(46,9,268,40,p.lang.getString("STR_HWPS"),false,"text");
 s._lstCrafts=p.list(288,118,8,58,"list");s._lstCrafts->setColumns(5,94,68,44,46,28);s._lstCrafts->setSelectable(true);s._lstCrafts->setMargin(8);s._lstCrafts->agentEnableOutfitting(5);s._lstCrafts->addRow(5,"MCP Transport",p.lang.getString("STR_READY").c_str(),"0/0","7","0");finishSetup(s,p);
}
void setup(Outfitting::CraftArmorState&s,Paint&p){
 s._btnOk=p.button(288,16,16,176,"STR_OK");s._txtTitle=p.text(300,17,16,7,p.lang.getString("STR_SELECT_ARMOR"),true,"text");s._txtName=p.text(114,9,16,32,p.lang.getString("STR_NAME_UC"),false,"text");s._txtCraft=p.text(76,9,130,32,p.lang.getString("STR_CRAFT"),false,"text");s._txtArmor=p.text(100,9,199,32,p.lang.getString("STR_ARMOR"),false,"text");s._lstSoldiers=p.list(292,128,8,40,"list");s._lstSoldiers->setColumns(3,114,69,101);s._lstSoldiers->setSelectable(true);s._lstSoldiers->setMargin(8);s._lstSoldiers->agentEnableOutfitting(3);
 for(auto*person:s._base->soldiers)s._lstSoldiers->addRow(3,person->name.c_str(),person->craft?person->craft->name.c_str():"NONE",p.lang.getString(person->armor->type).c_str());finishSetup(s,p);
}
void setup(Outfitting::SoldierArmorState&s,Paint&p){
 s._btnCancel=p.button(140,16,90,156,"STR_CANCEL_UC");s._txtTitle=p.text(182,16,69,28,p.lang.getString("STR_SELECT_ARMOR_FOR_SOLDIER").arg("MCP Healthy"),false,"text");s._txtTitle->setAlign(ALIGN_CENTER);s._txtType=p.text(90,9,80,52,p.lang.getString("STR_TYPE"),false,"text");s._txtQuantity=p.text(70,9,190,52,p.lang.getString("STR_QUANTITY_UC"),false,"text");s._lstArmor=p.list(160,80,73,68,"list");s._lstArmor->setColumns(2,132,21);s._lstArmor->setSelectable(true);s._lstArmor->setMargin(8);s._lstArmor->agentEnableOutfitting(2);
 s._lstArmor->addRow(1,p.lang.getString(s._armors[0]->type).c_str());s._lstArmor->addRow(2,p.lang.getString(s._armors[1]->type).c_str(),"1");finishSetup(s,p);
}
void setup(Outfitting::CraftSoldiersState&s,Paint&p){
 s._btnOk=p.button(148,16,164,176,"STR_OK");s._txtTitle=p.text(300,17,16,7,p.lang.getString("STR_SELECT_SQUAD_FOR_CRAFT").arg("MCP Transport"),true,"text");s._txtName=p.text(114,9,16,32,p.lang.getString("STR_NAME_UC"),false,"text");s._txtRank=p.text(102,9,122,32,p.lang.getString("STR_RANK"),false,"text");s._txtCraft=p.text(84,9,224,32,p.lang.getString("STR_CRAFT"),false,"text");s._txtAvailable=p.text(110,9,16,24,p.lang.getString("STR_SPACE_AVAILABLE").arg(7),false,"text");s._txtUsed=p.text(110,9,122,24,p.lang.getString("STR_SPACE_USED").arg(7),false,"text");
 s._lstSoldiers=p.list(288,128,8,40,"list");s._lstSoldiers->setColumns(3,106,102,72);s._lstSoldiers->setArrowColumn(192,ARROW_VERTICAL);s._lstSoldiers->setSelectable(true);s._lstSoldiers->setMargin(8);s._lstSoldiers->agentEnableOutfitting(3,false,true);
 for(auto*person:s._base->soldiers)s._lstSoldiers->addRow(3,person->name.c_str(),p.lang.getString("STR_ROOKIE").c_str(),person->craft?person->craft->name.c_str():"NONE");finishSetup(s,p);
}
void setup(Outfitting::CraftEquipmentState&s,Paint&p){
 s._btnOk=p.button(148,16,164,176,"STR_OK");s._txtTitle=p.text(300,17,16,7,p.lang.getString("STR_EQUIPMENT_FOR_CRAFT").arg("MCP Transport"),true,"text");s._txtItem=p.text(144,9,16,32,p.lang.getString("STR_ITEM"),false,"text");s._txtStores=p.text(150,9,160,32,p.lang.getString("STR_STORES"),false,"text");s._txtAvailable=p.text(110,9,16,24,p.lang.getString("STR_SPACE_AVAILABLE").arg(7),false,"text");s._txtUsed=p.text(110,9,130,24,p.lang.getString("STR_SPACE_USED").arg(7),false,"text");s._txtCrew=p.text(71,9,244,24,std::string(p.lang.getString("STR_SOLDIERS_UC"))+">\1"+"7",false,"text");s._lstEquipment=p.list(288,128,8,40,"list");s._lstEquipment->setArrowColumn(203,ARROW_HORIZONTAL);s._lstEquipment->setColumns(3,156,83,41);s._lstEquipment->setSelectable(true);s._lstEquipment->setMargin(8);s._lstEquipment->agentEnableOutfitting(3,true,false);
 s._items={p.master=="xcom1"?"STR_RIFLE":"STR_GAUSS_RIFLE",p.master=="xcom1"?"STR_TANK_CANNON":"STR_COELACANTH_GAS_CANNON"};for(auto&item:s._items)s._lstEquipment->addRow(3,p.lang.getString(item).c_str(),"10","1");finishSetup(s,p);
}
void setup(Outfitting::CraftInfoState&s,Paint&p){
 s._btnW1=p.button(24,32,14,48,"STR_OK");s._btnW2=p.button(24,32,282,48,"STR_OK");s._btnW1->setVisible(false);s._btnW2->setVisible(false);
 s._btnOk=p.button(64,24,128,168,"STR_OK");s._btnCrew=p.button(64,16,14,96,"STR_CREW");s._btnEquip=p.button(64,16,14,120,"STR_EQUIPMENT_UC");s._btnArmor=p.button(64,16,14,144,"STR_ARMOR");s._edtCraft=p.edit(&s,140,16,80,8,"MCP Transport");s._edtCraft->setAlign(ALIGN_CENTER);s._txtDamage=p.text(100,17,14,24,p.lang.getString("STR_DAMAGE_UC").arg(0),false,"text1");s._txtFuel=p.text(82,17,228,24,p.lang.getString("STR_FUEL").arg(100),false,"text1");
 s._txtW1Name=p.text(95,16,46,48,"",false,"text2");s._txtW1Ammo=p.text(75,24,46,64,"",false,"text3");s._txtW2Name=p.text(95,16,184,48,"",false,"text2");s._txtW2Ammo=p.text(75,24,204,64,"",false,"text3");for(Text*t:{s._txtW1Name,s._txtW1Ammo,s._txtW2Name,s._txtW2Ammo})t->setVisible(false);finishSetup(s,p);
}
void setup(Outfitting::SoldierInfoState&s,Paint&p){
 s._btnPrev=p.button(28,14,0,33,"STR_OK");s._btnPrev->setText("<<");s._btnOk=p.button(48,14,30,33,"STR_OK");s._btnNext=p.button(28,14,80,33,"STR_OK");s._btnNext->setText(">>");s._btnArmor=p.button(110,14,130,33,"STR_ARMOR");s._btnArmor->setText(p.lang.getString("STR_ARMOR_").arg(p.lang.getString(s._soldier->armor->type)));s._edtSoldier=p.edit(&s,210,16,40,9,s._soldier->name);
 s._txtRank=p.text(130,9,0,48,p.lang.getString("STR_RANK_").arg(p.lang.getString("STR_ROOKIE")));s._txtMissions=p.text(100,9,130,48,p.lang.getString("STR_MISSIONS").arg(0));s._txtKills=p.text(100,9,200,48,p.lang.getString("STR_KILLS").arg(0));s._txtCraft=p.text(130,9,0,56,p.lang.getString("STR_CRAFT_").arg("MCP Transport"));s._txtRecovery=p.text(180,9,130,56,"");s._txtPsionic=p.text(150,9,0,66,p.lang.getString("STR_IN_PSIONIC_TRAINING"),false,"text2");s._txtPsionic->setVisible(false);
 int y=80;const auto stat=[&](Text*&label,Text*&number,const std::string&key,bool hidden=false){label=p.text(120,9,6,y,p.lang.getString(key),false,"text2");number=p.text(18,9,131,y,hidden?"SECRET":"50",false,"numbers");label->setVisible(!hidden);number->setVisible(!hidden);y+=11;};
 stat(s._txtTimeUnits,s._numTimeUnits,"STR_TIME_UNITS");stat(s._txtStamina,s._numStamina,"STR_STAMINA");stat(s._txtHealth,s._numHealth,"STR_HEALTH");stat(s._txtBravery,s._numBravery,"STR_BRAVERY");stat(s._txtReactions,s._numReactions,"STR_REACTIONS");stat(s._txtFiring,s._numFiring,"STR_FIRING_ACCURACY");stat(s._txtThrowing,s._numThrowing,"STR_THROWING_ACCURACY");stat(s._txtMelee,s._numMelee,"STR_MELEE_ACCURACY");stat(s._txtStrength,s._numStrength,"STR_STRENGTH");stat(s._txtPsiStrength,s._numPsiStrength,"STR_PSIONIC_STRENGTH",true);stat(s._txtPsiSkill,s._numPsiSkill,"STR_PSIONIC_SKILL",true);finishSetup(s,p);
}
std::string pixels(Surface&s){SDL_Surface*p=s.getSurface();check(SDL_LockSurface(p)==0,"parity raster lock");std::string result;for(int y=0;y<p->h;++y)result.append(static_cast<const char*>(p->pixels)+y*p->pitch,p->w);SDL_UnlockSurface(p);return result;}
std::vector<std::string> records;
template<class T>void connect(TableChannel&c,T&s){c.state.project=[&](AgentUiState&out){return s.agentUiState(out);};c.state.handler=[&](const AgentRequest&r){check(s.agentDispatch(r),"actual current dispatcher accepts fresh granted command");if(s._game->top!=&s)c.game.top=&c.unsupportedParent;};}
YAML::Node record(TableChannel&c,const std::string&game,const std::string&screen,const std::string&name,const std::string&expected=""){
 auto packet=YAML::Load(c.read());const auto want=expected.empty()?screen:expected;check(packet["state"].as<std::string>()==want,(screen+"/"+name+" expected "+want).c_str());
 records.push_back("{\"name\":"+agentJsonString(name)+",\"game\":"+agentJsonString(game)+",\"provider\":"+agentJsonString(screen)+",\"expected_state\":"+agentJsonString(want)+",\"state_file\":"+agentJsonString(c.prefix+"-wire-"+std::to_string(c.schedule.size()-1)+".json")+"}");return packet;
}
template<class T>void execute(TableChannel&c,T&s,Paint&p,const std::string&action,const std::string&args=""){
 c.read();c.queue(action,args);c.frame();check(c.receive().empty(),"command receipt waits for subsequent presented frame");p.draw();c.frame();const auto result=c.receive();check(!result.empty()&&YAML::Load(result)["status"].as<std::string>()=="dispatched","ordinary dispatched receipt independent of success");
 const auto count=s._game->pops+s._game->pushes+s._game->commands;
 c.send(c.request);c.controller->poll();check(c.receive()==result,"exact duplicate receipt bytes");check(count==s._game->pops+s._game->pushes+s._game->commands,"duplicate never repeats mutation");
 auto command=c.request.substr(2,32);c.send("S 1 "+command);c.controller->poll();check(c.receive()==result,"status-only cached terminal receipt");c.events();s._game->top=&s;c.game.top=&c.state;p.draw();
}
template<class T>void common(T&s,Paint&p,Text*title,TextButton*button,TextList*list,const std::string&screen,const std::vector<std::pair<std::string,std::string>>&actions){
 TableChannel c(p.master+"-"+screen);connect(c,s);AgentUiState ui;check(s.agentUiState(ui),"native stock provider admitted");auto stock=record(c,p.master,screen,"stock");check(ui.visible.size()<=9000&&agentRegionsDisjoint(ui),"bounded disjoint native projection");
 s._agentDestinationPaintFull=false;record(c,p.master,screen,"before_completed_paint","unsupported");p.draw();
 SDL_Rect clip={1,0,319,200};SDL_SetClipRect(p.canvas.getSurface(),&clip);p.draw();record(c,p.master,screen,"clipped_destination","unsupported");SDL_SetClipRect(p.canvas.getSurface(),nullptr);record(c,p.master,screen,"restored_clip_without_repaint","unsupported");p.draw();record(c,p.master,screen,"full_repaint");
 if(title){auto old=title->getText();title->setText("Changed");title->setText(old);record(c,p.master,screen,"text_aba_unpainted","unsupported");title->blit(&p.canvas);record(c,p.master,screen,"child_only_repaint","unsupported");p.draw();}
 auto label=button->getText();button->setText("Changed");button->setText(label);record(c,p.master,screen,"button_aba_unpainted","unsupported");p.draw();
 c.read();c.queue(actions.back().first,actions.back().second);button->setText("Changed");button->setText(label);p.draw();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable","caption ABA rejects queued dispatch");
 c.send(c.request);c.controller->poll();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable","stale duplicate immutable");
 c.read();c.queue(actions.back().first,actions.back().second);Surface cover(button->getWidth(),button->getHeight(),button->getX(),button->getY());cover.setPalette(p.palette);cover.clear(1);cover.blit(&p.canvas);s._game->_cursor=&cover;c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable","overlay moved between read/dispatch rejects authority");s._game->_cursor=nullptr;p.draw();
 Surface full(320,200,0,0);full.setPalette(p.palette);full.clear(1);full.blit(&p.canvas);s._game->_cursor=&full;record(c,p.master,screen,"full_overlay","unsupported");s._game->_cursor=nullptr;p.draw();
 s._modal=button;record(c,p.master,screen,"top_modal","unsupported");s._modal=nullptr;p.draw();
 if(list){
  const auto opted=list->_agentOutfittingColumns;const auto originalPixels=pixels(p.canvas);list->_agentOutfittingColumns=0;list->invalidate();p.draw();check(pixels(p.canvas)==originalPixels,"ordinary GUI raster equal with scoped outfitting witness disabled");list->_agentOutfittingColumns=opted;list->invalidate();p.draw();
  auto original=list->getCellText(0,0);list->setCellText(0,0,std::string(1025,'W'));p.draw();auto unknown=record(c,p.master,screen,"unknown_first_label_control");check(unknown["visible"]["rows"].size()>0&&unknown["visible"]["rows"][0]["cells"][0].IsNull(),"unknown text keeps proven control");if(screen!="soldier_armor")execute(c,s,p,actions.front().first,actions.front().second);else execute(c,s,p,"select_soldier_armor","r0");list->setCellText(0,0,original);p.draw();
  auto&cell=list->_texts[0][0];cell->getCrop()->w=1;cell->getCrop()->h=cell->getHeight();list->invalidate();p.draw();auto clipped=record(c,p.master,screen,"source_crop");check(clipped["visible"]["rows"].size()>0&&clipped["visible"]["rows"][0]["cells"][0].IsNull(),"cropped cell never exposes complete text");cell->getCrop()->w=cell->getCrop()->h=0;list->invalidate();p.draw();
  if(list->_texts[0].size()>1){auto*other=list->_texts[0][1];const int oldX=other->getX();other->setX(list->_texts[0][0]->getX());list->invalidate();p.draw();auto overlap=record(c,p.master,screen,"overlapping_columns");check(overlap["visible"]["rows"].size()>0&&overlap["visible"]["rows"][0]["cells"][0].IsNull(),"changed opaque later column withholds full prior label");other->setX(oldX);list->invalidate();p.draw();}
  Surface rowCover(list->getWidth(),8,list->getX(),list->getY());rowCover.setPalette(p.palette);rowCover.clear(1);rowCover.blit(&p.canvas);s._game->_cursor=&rowCover;auto hidden=record(c,p.master,screen,"covered_first_row");check(hidden["visible"]["rows"].size()<stock["visible"]["rows"].size(),"covered row/control removed independently");s._game->_cursor=nullptr;p.draw();
  if(screen=="soldier_armor")check(stock["visible"]["rows"][0]["cells"][1].IsNull(),"default armor absent Text remains null");
  if(screen=="craft_equipment"){
   for(auto*t:list->_texts[0])t->setVisible(false);list->invalidate();p.draw();auto allNull=record(c,p.master,screen,"all_null_cells_shown_arrows");check(allNull["visible"]["rows"].size()>0,"equipment arrows retain independent authority");for(auto v:allNull["visible"]["rows"][0]["cells"])check(v.IsNull(),"no hidden label reconstructed");execute(c,s,p,"change_craft_equipment","r0 adjust 1");for(auto*t:list->_texts[0])t->setVisible(true);list->invalidate();p.draw();
  }
  else {
   for(auto*t:list->_texts[0])t->setVisible(false);list->invalidate();p.draw();auto erased=record(c,p.master,screen,"fully_erased_row");check(erased["visible"]["rows"].size()<stock["visible"]["rows"].size(),"fully erased personnel row has no control authority");for(auto*t:list->_texts[0])t->setVisible(true);list->invalidate();p.draw();
  }
  c.read();c.queue(actions.back().first,actions.back().second);list->setCellText(0,0,"Changed");list->setCellText(0,0,original);p.draw();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable","list content ABA rejects old authority");
  c.read();AgentUiState before;check(s.agentUiState(before),"restored row grant");auto events=YAML::Load(c.events());check(events["events"].size()>0,"V actual retained presentation");AgentUiState after;check(s.agentUiState(after)&&before.sameBinding(after),"V does not mutate row authority");
 }
 for(const auto&a:actions)execute(c,s,p,a.first,a.second);
 record(c,p.master,screen,"after_actions");
}
template<class T>void paintedFailure(T&s,Paint&p){const pid_t child=fork();check(child>=0,"bounded paint fault child");if(!child){agentObservePaint(-1);AgentUiState ui;if(s.agentUiState(ui))_exit(2);p.draw();if(s.agentUiState(ui))_exit(3);_exit(0);}int status=0;check(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"failed paint sticky fail closed in isolated child");}
void scalarProof(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game){
 Paint p(big,small,lang,source,game,"soldierList");Outfitting::Game g;g.lang=&lang;g.screen.logical=&p.canvas;auto*t=p.text(40,9,10,10,"A");AgentUiState ui;const auto value=[&](){ui=AgentUiState();AgentOutfittingView view(&g,p.surfaces,ui);return view.text(t);};
 check(value()=="null","nonempty text before paint withheld");p.draw();check(value()=="\"A\"","ordinary complete typed glyph known");
 t->setWidth(40);t->setHeight(1);t->setText("\nA");p.draw();SDL_Rect ink;check(!t->agentPaintedBounds(ink)&&agentTextJson(t)=="null"&&value()=="null","wholly clipped nonempty typed glyph has no text authority");
 t->setWidth(40);t->setHeight(9);t->setText("   ");p.draw();check(!t->agentPaintedBounds(ink)&&value()=="null","nonempty blank glyph range withheld by scoped helper");
 t->setText("");p.draw();check(value()=="\"\"","genuinely empty typed text remains empty string");t->setText("A");p.draw();t->getCrop()->w=1;t->getCrop()->h=1;check(value()=="null","non-full source crop does not revive full text");
}
void paging(Outfitting::SoldiersState&s,Paint&p){
 Outfitting::Soldier filler;filler.name="Visible filler";filler.armor=s._base->soldiers[0]->armor;auto*l=s._lstSoldiers;
 for(unsigned i=0;i<30;++i){s._base->soldiers.push_back(&filler);l->addRow(3,"Visible filler","Rookie","NONE");}p.draw();TableChannel c(p.master+"-personnel-page");connect(c,s);auto first=record(c,p.master,"soldiers","paged_first");check(first["visible"]["scroll"]["down"].as<bool>(),"actual scroll down arrow admitted");
 c.read();c.queue("select_soldier","r0");l->scrollDown(false,false);l->scrollUp(false,false);p.draw();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable","scroll away/back rejects old row grant");
 execute(c,s,p,"scroll_visible_list","down");record(c,p.master,"soldiers","paged_after_down");execute(c,s,p,"scroll_visible_list","up");record(c,p.master,"soldiers","paged_return");
 s._base->soldiers.resize(2);
}
void psiOverlay(Outfitting::SoldierInfoState&s,Paint&p){
 TableChannel c(p.master+"-psi-overlay");connect(c,s);
 s._txtPsiStrength->setVisible(true);s._numPsiStrength->setVisible(true);s._numPsiStrength->setText("50");s._txtPsiSkill->setVisible(true);s._numPsiSkill->setVisible(true);s._numPsiSkill->setText("50");p.draw();
 auto shown=record(c,p.master,"soldier_info","psi_shown");check(shown["visible"]["stats"].size()==11,"actual shown psi halves included");
 Surface cover(149,21,0,179);cover.setPalette(p.palette);cover.clear(1);cover.blit(&p.canvas);s._game->_cursor=&cover;
 auto hidden=record(c,p.master,"soldier_info","psi_pair_fully_covered");check(hidden["visible"]["stats"].size()==9,"fully covered psi pair does not reveal eligible count");
 cover.setWidth(130);cover.clear(1);p.draw();cover.blit(&p.canvas);
 auto half=record(c,p.master,"soldier_info","psi_label_covered_value_shown");check(half["visible"]["stats"].size()==11,"independently shown stat value retained");for(unsigned i=9;i<11;++i)check(half["visible"]["stats"][i]["label"].IsNull()&&!half["visible"]["stats"][i]["value"].IsNull(),"covered label remains null");
 s._game->_cursor=nullptr;s._txtPsiStrength->setVisible(false);s._numPsiStrength->setVisible(false);s._txtPsiSkill->setVisible(false);s._numPsiSkill->setVisible(false);p.draw();
}
void directions(Outfitting::CraftEquipmentState&s,Paint&p){
 TableChannel c(p.master+"-equipment-direction");connect(c,s);std::vector<AgentOutfittingRow> rows;check(s._lstEquipment->agentOutfittingRows(rows)&&!rows.empty()&&rows[0].increase&&rows[0].decrease,"both horizontal ordinary arrows witnessed");
 const auto *arrow=rows[0].increase;Surface cover(arrow->getWidth(),arrow->getHeight(),arrow->getX(),arrow->getY());cover.setPalette(p.palette);cover.clear(1);cover.blit(&p.canvas);s._game->_cursor=&cover;
 auto state=record(c,p.master,"craft_equipment","increase_arrow_covered");check(state["visible"]["rows"][0]["controls"].size()==1&&state["visible"]["rows"][0]["controls"][0].as<std::string>()=="decrease","only independently proved decrease remains");
 AgentRequest r;r.action="change_craft_equipment";r.choice="r0";r.operation="set";r.value=2;auto count=s._game->commands;check(!s.agentDispatch(r)&&count==s._game->commands,"absolute set requires both shown arrows");r.operation="max";check(!s.agentDispatch(r)&&count==s._game->commands,"maximum requires increase authority");r.operation="adjust";r.value=1;check(!s.agentDispatch(r)&&count==s._game->commands,"positive adjust denied under covered increase");
 execute(c,s,p,"change_craft_equipment","r0 clear");s._game->_cursor=nullptr;p.draw();record(c,p.master,"craft_equipment","both_arrows_restored");
}
void rasterLimits(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game){
 Paint p(big,small,lang,source,game,"craftSelect");auto*l=p.list(288,128,8,40,"list");l->setColumns(5,94,68,44,46,28);l->setSelectable(true);l->setMargin(8);l->agentEnableOutfitting(5);
 l->addRow(5,"A","B","C","D","E");l->addRow(5,"A","B","C","D","E");p.draw();std::vector<AgentOutfittingRow> rows;check(l->agentOutfittingRows(rows)&&rows.size()==2,"cap fixture starts with real admitted rows");
 for(auto&row:l->_texts)for(auto*cell:row){cell->setWidth(640);cell->setHeight(400);}l->invalidate();p.draw();check(!l->agentOutfittingRows(rows)&&!l->_agentCoverageHealthy,"actual bounded work exhaustion fails closed");
 auto*cell=l->_texts[0][0];cell->draw();cell->clear();check(SDL_LockSurface(cell->getSurface())==0,"synthetic run raster lock");for(int y=0;y<100;++y)for(int x=0;x<288;++x)if((x+y)%2)cell->setPixel(x,y,1);SDL_UnlockSurface(cell->getSurface());l->invalidate();p.draw();check(!l->agentOutfittingRows(rows)&&!l->_agentCoverageHealthy,"actual bounded run exhaustion fails closed");
}
void syntheticTransport(){
 Outfitting::Game g;AgentUiState ui;ui.screen="crafts";ui.actor=&g;ui.regions.push_back({0,0,1,1});
 const auto visible=[](int id,unsigned chars,bool unicode){std::string field;for(unsigned i=0;i<chars;++i)field+=unicode?"é":"W";std::string rows="[";for(unsigned r=0;r<32;++r){if(r)rows+=",";rows+="{\"handle\":\"r"+std::to_string(r)+"\",\"cells\":[";for(unsigned c=0;c<5;++c){if(c)rows+=",";rows+=agentJsonString(field);}rows+="],\"selected\":false}";}return "{\"title\":"+agentJsonString(std::to_string(id))+",\"base\":null,\"headings\":[null,null,null,null,null],\"rows\":"+rows+"],\"scroll\":{\"up\":false,\"down\":false},\"buttons\":[]}";};
 for(bool unicode:{false,true}){unsigned length=1;for(;length<1024;++length){ui.visible=visible(0,length,unicode);AgentOutfittingView view(&g,{},ui);if(!view.finish())break;}check(length>1,"synthetic producer accepts bounded semantic packet");const auto max=length-1;ui.visible=visible(0,max,unicode);AgentOutfittingView good(&g,{},ui);check(good.finish(),"actual scoped producer boundary accepted");ui.visible=visible(0,length,unicode);AgentOutfittingView bad(&g,{},ui);check(!bad.finish(),"next synthetic payload fails actual producer byte cap");
  TableChannel c(std::string("synthetic-outfitting-transport-")+(unicode?"unicode":"ascii"));c.state.project=[&](AgentUiState&out){out=ui;return true;};
  for(unsigned n=0;n<12;++n){ui.visible=visible(n,max,unicode);c.read();}std::string first;std::uint64_t cursor=0;unsigned found=0;for(;;){c.send("V "+token(c.challenge++)+" "+c.epoch+" "+std::to_string(cursor)+" 16");c.controller->poll();const auto bytes=c.receive();auto page=YAML::Load(bytes);if(first.empty())first=bytes;check(bytes.size()<=49152,"actual retained event page byte bounded");auto count=page["events"].size();if(!count)break;auto next=page["next_cursor"].as<std::uint64_t>();check(next>cursor,"partial byte page cursor progresses");found+=count;cursor=next;}check(found==12,"all large presentations retrieved without silent loss");
  // Identical request challenge yields exactly retained prefix bytes.
  auto old=YAML::Load(first);c.send("V "+old["challenge"].as<std::string>()+" "+c.epoch+" 0 16");c.controller->poll();check(c.receive()==first,"exact repeated first page stable");
 }
}
void protocolChecks(){
 const std::string prefix="E "+token(1)+" 1 "+token(2)+" "+token(3)+" 1 ";AgentRequest r;
 for(const std::string tail:{"set_craft_soldier r0 true","set_craft_soldier r31 false","change_craft_equipment r0 adjust -1000","change_craft_equipment r31 adjust 1000","change_craft_equipment r0 set 0","change_craft_equipment r0 set 100000","change_craft_equipment r0 max","change_craft_equipment r0 clear"})check(parseAgentRequest(prefix+tail,r),"strict canonical outfitting request accepted");
 for(const std::string tail:{"set_craft_soldier r0 1","set_craft_soldier r32 true","set_craft_soldier r00 false","change_craft_equipment r0 adjust 0","change_craft_equipment r0 adjust 1001","change_craft_equipment r0 adjust -1001","change_craft_equipment r0 set -1","change_craft_equipment r0 set 100001","change_craft_equipment r0 set 2147483648","change_craft_equipment r0 max 1","change_craft_equipment r0 hold 100","change_craft_equipment r0 clear extra","select_soldier r32","select_soldier r01"})check(!parseAgentRequest(prefix+tail,r),"invalid bounds or untyped gesture rejected");
 check(!parseAgentRequest("H openxcom-agent-control/23",r)&&parseAgentRequest("H openxcom-agent-control/24",r),"wire24 exact handshake");
}
void games(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game){
 scalarProof(big,small,lang,source,game);rasterLimits(big,small,lang,source,game);
 Outfitting::Game g;g.lang=&lang;Outfitting::active=&g;Outfitting::Craft craft;Outfitting::Armor plain,armor;plain.type=game=="xcom1"?"STR_NONE_UC":"STR_NONE_UC";armor.type=game=="xcom1"?"STR_PERSONAL_ARMOR_UC":"STR_PLASTIC_AQUA_ARMOR_UC";armor.store=game=="xcom1"?"STR_PERSONAL_ARMOR":"STR_PLASTIC_AQUA_ARMOR";
 Outfitting::Soldier healthy,wounded;healthy.craft=&craft;healthy.armor=&plain;wounded.name="MCP Wounded";wounded.wound=1;wounded.armor=&plain;Outfitting::Base base;base.soldiers={&healthy,&wounded};base.crafts={&craft};base.items.counts[armor.store]=1;
 {Outfitting::SoldiersState s(&base);Paint p(big,small,lang,source,game,"soldierList");setup(s,p);common(s,p,s._txtTitle,s._btnOk,s._lstSoldiers,"soldiers",{{"select_soldier","r0"},{"close_soldiers",""}});paintedFailure(s,p);paging(s,p);}
 {Outfitting::SoldierInfoState s(&base);Paint p(big,small,lang,source,game,"soldierInfo");setup(s,p);psiOverlay(s,p);AgentUiState ui;check(s.agentUiState(ui)&&ui.visible.find("SECRET")==std::string::npos&&YAML::Load(ui.visible)["stats"].size()==9,"undisclosed psi widgets never exported");common(s,p,s._txtRank,s._btnOk,nullptr,"soldier_info",{{"next_personnel",""},{"previous_personnel",""},{"open_soldier_armor",""},{"close_soldier_info",""}});}
 {Outfitting::CraftsState s(&base);Paint p(big,small,lang,source,game,"craftSelect");setup(s,p);common(s,p,s._txtTitle,s._btnOk,s._lstCrafts,"crafts",{{"select_craft","r0"},{"close_crafts",""}});}
 {Outfitting::CraftInfoState s(&base);Paint p(big,small,lang,source,game,"craftInfo");setup(s,p);common(s,p,s._txtDamage,s._btnOk,nullptr,"craft_info",{{"open_craft_crew",""},{"open_craft_equipment",""},{"open_craft_armor",""},{"close_craft_info",""}});}
 {Outfitting::CraftSoldiersState s(&base);Paint p(big,small,lang,source,game,"craftSoldiers");setup(s,p);common(s,p,s._txtTitle,s._btnOk,s._lstSoldiers,"craft_soldiers",{{"set_craft_soldier","r0 false"},{"set_craft_soldier","r0 true"},{"set_craft_soldier","r1 true"},{"open_craft_soldier_info","r1"},{"close_craft_crew",""}});check(!wounded.craft,"explicit model seam wounded no-effect, independent core oracle separate");}
 {Outfitting::CraftEquipmentState s(&base);Paint p(big,small,lang,source,game,"craftEquipment");setup(s,p);directions(s,p);common(s,p,s._txtTitle,s._btnOk,s._lstEquipment,"craft_equipment",{{"change_craft_equipment","r0 adjust 1"},{"change_craft_equipment","r0 adjust -1"},{"change_craft_equipment","r0 set 2"},{"change_craft_equipment","r0 max"},{"change_craft_equipment","r0 clear"},{"close_craft_equipment",""}});}
 {Outfitting::CraftArmorState s(&base);Paint p(big,small,lang,source,game,"craftArmor");setup(s,p);common(s,p,s._txtTitle,s._btnOk,s._lstSoldiers,"craft_armor",{{"select_craft_armor_soldier","r0"},{"close_craft_armor",""}});}
 {Outfitting::SoldierArmorState s(&base);s._armors={&plain,&armor};Paint p(big,small,lang,source,game,"soldierArmor");setup(s,p);common(s,p,s._txtTitle,s._btnCancel,s._lstArmor,"soldier_armor",{{"cancel_soldier_armor",""},{"select_soldier_armor","r1"},{"select_soldier_armor","r0"},{"cancel_soldier_armor",""}});check(healthy.armor==&plain&&base.items.counts[armor.store]==1,"ordinary extracted armor choice restores stores and equipped armor");}
}
}
// Source-extracted CraftInfo entry provider/dispatcher qualification only.
// Buttons use real widgets and stock palettes; navigation callbacks remain
// explicit counters here. The armament modal owns its native callback oracle.
void craftWeaponEntries(Font&big,Font&small,Language&lang,const std::string&source,const std::string&master)
{
 for (unsigned slots=0;slots<=2;++slots)
 {
  Outfitting::Game game;game.lang=&lang;Outfitting::active=&game;
  Outfitting::Craft craft;craft.weapons.resize(slots,nullptr);
  Outfitting::Base base;base.crafts={&craft};Outfitting::CraftInfoState state(&base);
  Paint paint(big,small,lang,source,master,"craftInfo");setup(state,paint);
  state._btnW1->setText("1");state._btnW2->setText("2");
  state._btnW1->setVisible(slots>0);state._btnW2->setVisible(slots>1);paint.draw();
  AgentUiState ui;check(state.agentUiState(ui),"slot entry native projection");
  check(ui.offers("open_craft_weapon_1")==bool(slots>0),"slot one capacity visibility");
  check(ui.offers("open_craft_weapon_2")==bool(slots>1),"slot two capacity visibility");
  TableChannel channel(master+"-craft-slot-"+std::to_string(slots));connect(channel,state);
  record(channel,master,"craft_info","slot_entry_"+std::to_string(slots));
  for (unsigned slot=0;slot<slots;++slot)
  {
   TextButton *button=slot?state._btnW2:state._btnW1;
   const std::string action=slot?"open_craft_weapon_2":"open_craft_weapon_1";
   button->setVisible(false);paint.draw();check(state.agentUiState(ui)&&!ui.offers(action),"hidden slot withheld");
   button->setVisible(true);paint.draw();check(state.agentUiState(ui)&&ui.offers(action),"visible slot restored");
   channel.read();channel.queue(action);const auto before=game.pushes;
   Surface cover(button->getWidth(),button->getHeight(),button->getX(),button->getY());
   cover.setPalette(paint.palette);cover.clear(1);cover.blit(&paint.canvas);game._fpsCounter=&cover;
   channel.frame();check(YAML::Load(channel.receive())["status"].as<std::string>()=="unavailable"&&game.pushes==before,"occlusion rejects queued slot entry");
   check(!state.agentUiState(ui)||!ui.offers(action),"covered slot withheld");game._fpsCounter=nullptr;paint.draw();
   channel.read();channel.queue(action);button->setText("changed");button->setText(slot?"2":"1");paint.draw();
   channel.frame();check(YAML::Load(channel.receive())["status"].as<std::string>()=="unavailable"&&game.pushes==before,"slot caption ABA rejects queued authority");
   execute(channel,state,paint,action);check(game.pushes==before+1,"slot dispatch enters callback seam exactly once");
  }
  Outfitting::Craft replacement;base.crafts[0]=&replacement;
  check(!state.agentUiState(ui),"craft replacement invalidates entry");base.crafts[0]=&craft;
 }
}

int main(int argc,char**argv){
 check(argc==2,"source root");check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen only");Options::baseXResolution=320;Options::baseYResolution=200;Options::keyOk=SDLK_RETURN;Options::keyCancel=SDLK_ESCAPE;Options::mousewheelSpeed=3;
 protocolChecks();syntheticTransport();
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 for(const std::string game:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+game+"/Language/en-US.yml");games(big,small,lang,argv[1],game);}
 std::ofstream out(std::string(std::getenv("OPENXCOM_AGENT_TEST_PACKET_DIR"))+"/case-records.json");out<<"[";for(unsigned i=0;i<records.size();++i){if(i)out<<",";out<<records[i];}out<<"]";check(bool(out),"case inventory persisted");check(agentPaintHealthy()&&!SDL_WasInit(SDL_INIT_VIDEO),"no game or GUI");std::cout<<"AgentOutfittingProviderTestV21 PASS "<<checks<<" checks\n";
}
