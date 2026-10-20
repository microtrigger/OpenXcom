// SPDX-License-Identifier: GPL-3.0-or-later
// Exact provider/dispatch/projection bodies + real offscreen widgets/font/SDL.
// Owning Game/actor identities and normal action bodies are recorded shims;
// the separate routing fixture executes those normal bodies verbatim.
#include "../../src/Observer/AgentInventoryPaint.h"
#include "../../src/Observer/AgentInventoryBody.h"
#include "../../src/Engine/Options.h"
#include "../../src/Mod/RuleInventory.h"
#include "../../src/Mod/RuleInterface.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/SurfaceSet.h"
#include "../../src/Interface/Text.h"
#include "../../src/Battlescape/WarningMessage.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <cstdlib>
#include <memory>
using namespace OpenXcom; using namespace OpenXcom::Observer;
static unsigned checks=0;static void check(bool b,const char*m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
namespace InventoryHarness {
using namespace OpenXcom;
struct Unit {std::uint64_t revision=0;std::uint64_t stanceControlRevision()const{return revision;}};
struct Saved {Unit*selected=nullptr;std::uint64_t revision=0;std::uint64_t stanceControlRevision()const{return revision;}Unit*getSelectedUnit(){return selected;}};
struct Mod {std::map<std::string,RuleInventory*> invs;Font*big=nullptr,*small=nullptr;RuleInterface*interface=nullptr;
 std::map<std::string,RuleInventory*>*getInventories(){return &invs;}
 Font*getFont(const std::string&id){return id=="FONT_BIG"?big:small;}RuleInterface*getInterface(const std::string&){return interface;}};
struct Game {const void*top=nullptr;Mod*mod=nullptr;Language*language=nullptr;bool isState(const void*s)const{return top==s;}Mod*getMod(){return mod;}Language*getLanguage(){return language;}};
class Inventory : public Surface {
public:
 struct AgentTarget {AgentUiRect rect;std::string area,label="null";AgentUiRect labelInk={0,0,0,0};std::vector<std::pair<int,unsigned>>labelPixels;};
 Surface grid{320,200},items{320,200},selection{32,48};WarningMessage warning{224,24,48,176};
 Surface *_grid=&grid,*_items=&items,*_selection=&selection;WarningMessage*_warning=&warning;
 std::vector<AgentTarget>_agentTargets;std::vector<unsigned char>_agentShownItemInk,_agentShownGridInk;
 std::uint64_t _agentInventoryRevision=0,_agentShownRevision=0;
 bool _agentFrameReady=false,_agentGridReady=false,_agentGridUnambiguous=false,_agentHeldPresented=false;
 Game*_game=nullptr;void drawGrid();
 Unit*_selUnit=nullptr;const void*_selItem=nullptr;std::vector<std::string>calls;
 Inventory():Surface(320,200){warning.setVisible(false);}
 bool agentProjectionReady()const;bool agentOverlayClear(const AgentUiRect&,bool=true)const;
 bool agentItemsVisible(const AgentUiRect&)const;bool agentGridVisible(const AgentTarget&)const;
 void agentCaptureTarget(int,int,int,int,const std::string&);void agentCaptureItems(const Surface*);
 bool movePointer(int,int){calls.push_back("hover");++_agentInventoryRevision;return true;}
 bool activateButton(Uint8 b,int,int,bool ctrl){check(b==SDL_BUTTON_LEFT&&!ctrl,"semantic primary has no inherited modifiers");calls.push_back("primary");++_agentInventoryRevision;return true;}
 bool returnHeldItem(){calls.push_back("held-return");++_agentInventoryRevision;return true;}
 void compose(Surface*canvas){clear();grid.blit(this);items.blit(this);selection.blit(this);warning.blit(this);agentCaptureItems(canvas);Surface::blit(canvas);_agentFrameReady=Observer::agentPaintHealthy();_agentShownRevision=_agentInventoryRevision;}
};
#include "AgentInventoryPaintMethods.generated.inc"
#include "AgentInventoryDrawGrid.generated.inc"
class InventoryState {
public:
 Game*_game;Inventory*_inv;Saved*_battleGame;Surface*_modal=nullptr;
 Surface*_btnOk,*_btnUnload,*_btnGround;Text*_txtName,*_txtTus,*_txtWeight,*_txtItem,*_txtAmmo;
 bool _tu=true;std::vector<Surface*>_surfaces;std::vector<std::string>calls;
 bool agentUiState(AgentUiState&)const;bool agentBindingCurrent(const AgentUiState&)const;
 bool agentDispatch(const AgentRequest&);bool agentDispatch(const std::string&);
 void invMouseOver(Action*){_inv->calls.push_back("hover-callback");}
 void invClick(Action*){_inv->calls.push_back("click-callback");}
 void btnOkClick(Action*){calls.push_back("ok");}void btnUnloadClick(Action*){calls.push_back("unload");}void btnGroundClick(Action*){calls.push_back("page");}
};
#include "AgentInventoryProviderMethods.generated.inc"
}
int main(int argc,char**argv){
 Logger::logFile()="/tmp/openxcom-v11-inventory-widget.log";
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"no display initialization");Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(const auto&f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}Language language;
 Surface canvas(320,200),background(320,200),ok(35,22,237,1),unload(32,25,288,32),page(32,15,289,137);
 Text name(210,17,28,6),tus(40,9,245,24),weight(70,9,245,24),item(160,9,128,140),ammo(66,24,254,64);
 for(Text*t:{&name,&tus,&weight,&item,&ammo}){t->initText(&big,&small,&language);t->setColor(40);}
 weight.setVisible(false);name.setBig();name.setText("MCP Test");tus.setText("TUs> 60");weight.setText("Weight> 20");item.setText("Rifle");ammo.setText("Rounds> 20");
 InventoryHarness::Inventory inv;inv.warning.initText(&big,&small,&language);inv.warning.setColor(16);inv.warning.setTextColor(40);
 SDL_Color colors[256];for(int i=0;i<256;++i)colors[i]={Uint8(i),Uint8(i),Uint8(i),0};
 for(Surface*s:{&canvas,&background,&ok,&unload,&page,static_cast<Surface*>(&inv),&inv.grid,&inv.items,&inv.selection,static_cast<Surface*>(&inv.warning),static_cast<Surface*>(&name),static_cast<Surface*>(&tus),static_cast<Surface*>(&weight),static_cast<Surface*>(&item),static_cast<Surface*>(&ammo)})s->setPalette(colors);
 check(SDL_SetColorKey(canvas.getSurface(),0,0)==0,"actual Screen logical destination has no color key");
 background.clear(1);ok.clear(30);unload.clear(30);page.clear(30);for(int y=0;y<15;++y)page.setPixel(31,y,0);
 inv.agentCaptureTarget(0,63,32,48,"hand");inv.agentCaptureTarget(128,63,32,48,"hand");inv.agentCaptureTarget(0,152,16,16,"ground");
 auto grids=[&](){inv.grid.clear();for(const auto&t:inv._agentTargets){auto&r=t.rect;SDL_Rect q={Sint16(r.x),Sint16(r.y),Uint16(r.width+(t.area!="hand")),Uint16(r.height+(t.area!="hand"))};inv.grid.drawRect(&q,20);++q.x;++q.y;q.w-=2;q.h-=2;inv.grid.drawRect(&q,0);}inv._agentGridReady=true;};grids();inv.items.setPixel(10,75,30);
 InventoryHarness::Game game;InventoryHarness::Saved saved;InventoryHarness::Unit unit,other;saved.selected=&unit;inv._selUnit=&unit;
 InventoryHarness::InventoryState state;state._game=&game;state._inv=&inv;state._battleGame=&saved;game.top=&state;
 state._btnOk=&ok;state._btnUnload=&unload;state._btnGround=&page;state._txtName=&name;state._txtTus=&tus;state._txtWeight=&weight;state._txtItem=&item;state._txtAmmo=&ammo;
 state._surfaces={&background,&name,&tus,&weight,&item,&ammo,&ok,&unload,&page,&inv};
 auto draw=[&](){for(auto*s:state._surfaces)if(s==&inv)inv.compose(&canvas);else s->blit(&canvas);};
 AgentUiState ui;check(!state.agentUiState(ui),"unpresented provider withheld");draw();check(state.agentUiState(ui)&&agentRegionsDisjoint(ui),"painted inventory fields coexist");
 check(ui.visible.find("\"item_name\":\"Rifle\"")!=std::string::npos&&ui.visible.find("\"ammo\":\"Rounds> 20\"")!=std::string::npos,"actual displayed hover strings only");
 check(ui.points.size()==3&&ui.visible.find("\"item\":\"presented\"")!=std::string::npos,"painted local controls and item-layer contribution");
 check(ui.offers("close_inventory")&&ui.offers("unload_inventory")&&ui.offers("next_inventory_ground_page")&&ui.offers("inventory_hover")&&ui.offers("inventory_activate")&&!ui.offers("cancel_inventory_hold"),"only observed prerequisites, unload ordinary eligibility not predicted");
 // Same normal formatted widget, with fully drawn ink, loses only renderer tokens.
 tus.setText(std::string("TU>")+char(1)+"50");draw();check(state.agentUiState(ui)&&ui.visible.find("\"time_units\":\"TU>50\"")!=std::string::npos,"painted color token normalized to public text");
 Text multiline(80,40,0,0);multiline.initText(&big,&small,&language);multiline.setPalette(colors);multiline.setColor(40);multiline.setBig();multiline.setText(std::string("A")+char(2)+"B");multiline.draw();
 check(agentInventoryTextJson(&multiline)=="\"A\\u000aB\"","drawn small-font newline becomes visible linebreak");
 multiline.setText(std::string(100,'W')+char(1)+"50");multiline.draw();check(agentInventoryTextJson(&multiline)=="null","normalization cannot rescue genuinely clipped ink");
 std::string shown;for(const std::string raw:{std::string("A")+char(3)+"B",std::string("A\tB"),std::string("A")+char(127)+"B",std::string("A\302\205B")})check(!agentInventoryRenderedText(raw,shown),"unknown non-rendered control refused");
 tus.setText("TUs> 60");draw();check(state.agentUiState(ui),"ordinary unformatted text retained");
 const auto original=ui;draw();check(state.agentUiState(ui)&&original.sameBinding(ui),"idle paint retains action binding");
 inv._agentInventoryRevision+=2;draw();check(state.agentUiState(ui)&&!original.sameBinding(ui)&&original.payload()==ui.payload(),"held/page ABA private generation cannot restore grant or leak to payload");
 saved.selected=&other;check(!state.agentBindingCurrent(ui),"selected actor replacement invalidates");saved.selected=&unit;check(state.agentBindingCurrent(ui),"matching private actor binding current");
 game.top=&other;check(!state.agentUiState(ui)&&!state.agentDispatch("close_inventory"),"covered top state cannot dispatch");game.top=&state;
 state._modal=&ok;check(!state.agentUiState(ui),"same-state modal blocks all controls");state._modal=nullptr;
 inv.selection.setPixel(10,12,40);inv.selection.setX(0);inv.selection.setY(63);draw();check(state.agentUiState(ui)&&ui.offers("cancel_inventory_hold")&&ui.points.size()==2,"visibly held sprite enables only held-return and omits covered target");
 AgentRequest request;request.action="cancel_inventory_hold";inv.calls.clear();check(state.agentDispatch(request)&&inv.calls==std::vector<std::string>({"held-return","click-callback"}),"cancel no prime or close fallback");
 inv.selection.clear();draw();check(!state.agentDispatch(request),"no shown held sprite cannot cancel");
 request.action="inventory_activate";request.choice="v1";inv.calls.clear();check(state.agentDispatch(request)&&inv.calls==std::vector<std::string>({"hover","hover-callback","primary","click-callback"}),"atomic ordinary hover-primary tails once");draw();
 request.action="inventory_hover";inv.calls.clear();check(state.agentDispatch(request)&&inv.calls==std::vector<std::string>({"hover","hover-callback"}),"standalone inspection does not activate");draw();
 request.choice="v63";inv.calls.clear();check(!state.agentDispatch(request)&&inv.calls.empty(),"unlisted target cannot invoke handler");
 check(state.agentDispatch("unload_inventory")&&state.calls.back()=="unload","visible unload delegates exact ordinary handler");check(state.agentDispatch("next_inventory_ground_page")&&state.calls.back()=="page","page invokes normal rearrangement handler");check(state.agentDispatch("close_inventory")&&state.calls.back()=="ok","normal close outcome remains handler-owned");
 inv.items.setPixel(238,2,40);draw();check(state.agentUiState(ui)&&!ui.offers("close_inventory"),"later item-layer ink blocks underlying OK");inv.items.setPixel(238,2,0);
 inv.items.setPixel(130,143,40);draw();check(state.agentUiState(ui)&&ui.visible.find("\"item_name\":null")!=std::string::npos,"later composite ink obscures hover text");inv.items.setPixel(130,143,0);
 item.setText(std::string(100,'W'));draw();check(state.agentUiState(ui)&&ui.visible.find("\"item_name\":null")!=std::string::npos,"clipped item name withheld");item.setText("Rifle");
 ok.setVisible(false);draw();check(state.agentUiState(ui)&&!ui.offers("close_inventory"),"hidden ordinary OK omitted");ok.setVisible(true);
 Surface appended(1,1);state._surfaces.push_back(&appended);draw();check(!state.agentUiState(ui),"unreviewed appended layer rejects composite");state._surfaces.pop_back();
 state._tu=false;draw();check(state.agentUiState(ui)&&ui.actions.size()==1&&ui.offers("close_inventory")&&ui.points.empty()&&ui.visible.find("\"held\":null")!=std::string::npos,"prebattle legacy close-only scope preserved");state._tu=true;
 inv.warning.showMessage("Not enough Time Units");draw();check(state.agentUiState(ui)&&ui.displayedMessage=="Not enough Time Units"&&ui.visible.find("\"warning\":\"Not enough Time Units\"")!=std::string::npos,"ordinary painted warning feeds text event with no private cause");
 inv.warning.setVisible(false);draw();
 page.setPixel(31,2,30);draw();check(state.agentUiState(ui)&&!ui.offers("next_inventory_ground_page"),"actual clipped arrow ink rejected although padded transparent allocation was accepted");page.setPixel(31,2,0);
 check(argc==3,"private existing runtime asset root and public source rules supplied");
 // Read existing local assets only. No image/crop or retail byte is written.
 // Reconstruct the ordinary drawItems/drawHandSprite placement using the public
 // stock rule width/height and real PCK frame, without a live BattleItem/model.
 for(const std::string gameName:{"UFO","TFTD"}) {
  const bool tftd=gameName=="TFTD";const std::string ruleName=tftd?"STR_GAUSS_RIFLE":"STR_RIFLE";
  YAML::Node gun,mag;for(const auto&n:YAML::LoadFile(std::string(argv[2])+"/bin/standard/"+(tftd?"xcom2":"xcom1")+"/items.rul")["items"]){if(n["type"].as<std::string>()==ruleName)gun=YAML::Clone(n);if(n["type"].as<std::string>()==ruleName+"_CLIP")mag=YAML::Clone(n);}
  check(gun&&mag,"public stock firearm and magazine rules");
  SurfaceSet sprites(32,48);sprites.loadPck(std::string(argv[1])+"/"+gameName+"/UNITS/BIGOBS.PCK",std::string(argv[1])+"/"+gameName+"/UNITS/BIGOBS.TAB");sprites.setPalette(colors);
  auto paintItem=[&](const YAML::Node&r,Surface&target,int x,int y){Surface*f=sprites.getFrame(r["bigSprite"].as<int>());check(f,"stock sprite exists");f->setX(x+(2-r["invWidth"].as<int>(1))*8);f->setY(y+(3-r["invHeight"].as<int>(1))*8);f->blit(&target);};
  auto handPoint=[&](int x)->std::string{for(unsigned n=0;n<ui.points.size();++n)if(ui.points[n].first==x&&ui.points[n].second==87)return "v"+std::to_string(n);return "";};
  inv.items.clear();inv.selection.clear();inv.selection.setX(0);inv.selection.setY(63);paintItem(gun,inv.items,0,63);draw();
  check(state.agentUiState(ui)&&!handPoint(16).empty(),"actual loaded weapon icon does not hide its normal hand target");
  inv.items.clear();paintItem(gun,inv.selection,0,0);draw();
  check(state.agentUiState(ui)&&ui.offers("cancel_inventory_hold")&&ui.offers("unload_inventory"),"actual held weapon remains visible and normal unload control admitted");
  // Normal unload produces right gun/left magazine. Routing fixture separately
  // executes that mutation and its TU/refusal logic verbatim.
  inv.selection.clear();paintItem(gun,inv.items,0,63);paintItem(mag,inv.items,128,63);draw();
  check(state.agentUiState(ui)&&!handPoint(16).empty()&&!handPoint(144).empty(),"normal unloaded gun and magazine both have painted hand targets");
  inv.items.clear();paintItem(gun,inv.items,0,63);inv.selection.clear();inv.selection.setX(128);inv.selection.setY(63);paintItem(mag,inv.selection,0,0);draw();
  check(state.agentUiState(ui)&&ui.offers("cancel_inventory_hold")&&!handPoint(16).empty(),"held real magazine leaves receiving weapon target freshly available");
  AgentRequest activate;activate.action="inventory_activate";activate.choice=handPoint(16);inv.calls.clear();
  check(state.agentDispatch(activate)&&inv.calls==std::vector<std::string>({"hover","hover-callback","primary","click-callback"}),"fresh real weapon target consumes once before semantic held move can obscure it");
  std::cout<<"private offscreen "<<gameName<<" weapon/magazine presentation path PASS\n";
 }
 // Real drawGrid body + actual RuleInventory YAML and widgets, never item/model queries.
 inv.items.clear();inv.selection.clear();inv.warning.setVisible(false);weight.setText("");
 InventoryHarness::Mod mod;RuleInterface interface("inventory");
 interface.load(YAML::Load("elements: [{id: textSlots, color: 40}, {id: grid, color: 20}]"));
 mod.big=&big;mod.small=&small;mod.interface=&interface;game.mod=&mod;game.language=&language;inv._game=&game;
 for(const std::string master:{"xcom1","xcom2"}) {
  language.loadFile(std::string(argv[2])+"/bin/standard/"+master+"/Language/en-US.yml");
  std::vector<std::unique_ptr<RuleInventory>> rules;
  for(const auto&n:YAML::LoadFile(std::string(argv[2])+"/bin/standard/"+master+"/inventories.rul")["invs"]){
   rules.emplace_back(new RuleInventory(n["id"].as<std::string>()));rules.back()->load(n,0);mod.invs[rules.back()->getId()]=rules.back().get();}
  inv.drawGrid();draw();
  check(inv._agentTargets.size()==63,"real stock draw captures exactly 63 bounded candidates");
  unsigned body=0;for(const auto&t:inv._agentTargets)if(t.area=="body")++body;
  check(body==23&&inv._agentGridReady,"both games have 23 supported body cells and unambiguous grid");
  check(state.agentUiState(ui)&&ui.visible.find("\"area\":\"body\"")!=std::string::npos&&ui.points.size()<=63,"actual body grid publishes admitted local geometry");
  check(agentRegionsDisjoint(ui),"adjacent shared borders do not produce overlapping target interiors");
  check(ui.visible.find("\"label\":\"BACK PACK\"")!=std::string::npos,"group label is the actual rendered stock translation");
  const auto bodyFresh=ui;draw();check(state.agentUiState(ui)&&bodyFresh.sameBinding(ui),"body idle paint keeps grant binding");
  inv._agentInventoryRevision+=2;draw();check(state.agentUiState(ui)&&!bodyFresh.sameBinding(ui)&&bodyFresh.payload()==ui.payload(),"body context ABA rejected without leaking a generation");
  const auto count=inv._agentTargets.size();inv.agentCaptureTarget(8,8,16,16,"body");inv.agentCaptureTarget(192,37,17,16,"body");inv.agentCaptureTarget(192,37,16,16,"unknown");check(inv._agentTargets.size()==count,"unsupported body geometry and kind cannot create controls");
  auto*back=mod.invs["STR_BACK_PACK"];auto slots=*back->getSlots();back->getSlots()->push_back(slots[0]);inv.drawGrid();draw();check(!inv._agentGridReady&&!state.agentUiState(ui),"duplicate body cell invalidates malformed layout");*back->getSlots()=slots;
  RuleInventory custom("CUSTOM");custom.load(YAML::Load("x: 192\ny: 37\nslots: [[0,0]]"),0);mod.invs["CUSTOM"]=&custom;inv.drawGrid();draw();check(!inv._agentGridReady&&!state.agentUiState(ui),"unknown overlapping body control blocks presentation");mod.invs.erase("CUSTOM");
  inv.drawGrid();draw();check(state.agentUiState(ui),"restored ordinary body layout recovers freshly");
  // A real stock 1x1 magazine has a visible body contribution without assuming fit.
  const bool tftd=master=="xcom2";YAML::Node mag;for(const auto&n:YAML::LoadFile(std::string(argv[2])+"/bin/standard/"+master+"/items.rul")["items"])if(n["type"].as<std::string>()==(tftd?"STR_GAUSS_RIFLE_CLIP":"STR_RIFLE_CLIP"))mag=YAML::Clone(n);
  SurfaceSet sprites(32,48);sprites.loadPck(std::string(argv[1])+(tftd?"/TFTD":"/UFO")+"/UNITS/BIGOBS.PCK",std::string(argv[1])+(tftd?"/TFTD":"/UFO")+"/UNITS/BIGOBS.TAB");sprites.setPalette(colors);
  Surface*icon=sprites.getFrame(mag["bigSprite"].as<int>());check(icon,"real body item sprite exists");icon->setX(192);icon->setY(37);icon->blit(&inv.items);draw();
  check(state.agentUiState(ui)&&inv.agentItemsVisible({192,37,16,16}),"actual stock magazine contributes painted ink inside body cell");
  check(std::find(ui.points.begin(),ui.points.end(),std::make_pair(200,45))!=ui.points.end(),"actual occupied magazine body target remains inspectable and activatable");
  inv.selection.setPixel(0,0,60);inv.selection.setX(192);inv.selection.setY(37);draw();check(state.agentUiState(ui)&&ui.offers("cancel_inventory_hold"),"held sprite over body offers ordinary return");
  bool covered=false;for(const auto&p:ui.points)if(p.first==200&&p.second==45)covered=true;check(!covered,"held ink blocks covered body target");
  inv.selection.clear();inv.items.clear();mod.invs.clear();
 }
 check(agentPaintHealthy()&&!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"offscreen rendering healthy without game/window");std::cout<<"AgentInventoryProviderTest PASS "<<checks<<" checks\n";
}
