// SPDX-License-Identifier: GPL-3.0-or-later
// Actual article translation units and stock resources; explicit Game/Screen and
// resource-loader/availability seams. No game loop or display construction.
#include <bits/stdc++.h>
#include <dirent.h>
#define private public
#define protected public
#include "../../src/Mod/Mod.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Screen.h"
#include "../../src/Ufopaedia/ArticleState.h"
#include "../../src/Ufopaedia/Ufopaedia.h"
#include "../../src/Interface/Text.h"
#undef protected
#undef private
#define main retainedOutfittingFixtureMain
#include "ArticleHarnessBase.generated.inc"
#undef main
#include "../../src/Engine/SurfaceSet.h"
#include "../../src/Mod/ArticleDefinition.h"
#include "../../src/Mod/RuleCraft.h"
#include "../../src/Mod/RuleResearch.h"
#include "../../src/Mod/RuleCraftWeapon.h"
#include "../../src/Mod/RuleBaseFacility.h"
#include "../../src/Mod/RuleUfo.h"
#include "../../src/Mod/RuleItem.h"
#include "../../src/Mod/RuleInterface.h"
#include "../../src/Mod/Unit.h"
#include "../../src/Mod/Armor.h"
#include "../../src/Ufopaedia/ArticleStateCraft.h"
#include "../../src/Ufopaedia/ArticleStateCraftWeapon.h"
#include "../../src/Ufopaedia/ArticleStateVehicle.h"
#include "../../src/Ufopaedia/ArticleStateItem.h"
#include "../../src/Ufopaedia/ArticleStateArmor.h"
#include "../../src/Ufopaedia/ArticleStateBaseFacility.h"
#include "../../src/Ufopaedia/ArticleStateTextImage.h"
#include "../../src/Ufopaedia/ArticleStateText.h"
#include "../../src/Ufopaedia/ArticleStateUfo.h"
#include "../../src/Ufopaedia/ArticleStateTFTD.h"
#include "../../src/Ufopaedia/ArticleStateTFTDCraft.h"
#include "../../src/Ufopaedia/ArticleStateTFTDCraftWeapon.h"
#include "../../src/Ufopaedia/ArticleStateTFTDVehicle.h"
#include "../../src/Ufopaedia/ArticleStateTFTDItem.h"
#include "../../src/Ufopaedia/ArticleStateTFTDArmor.h"
#include "../../src/Ufopaedia/ArticleStateTFTDFacility.h"
#include "../../src/Ufopaedia/ArticleStateTFTDUso.h"

namespace ArticleHarness {
Font *big=nullptr,*small=nullptr;Language*language=nullptr;Surface*logical=nullptr;Screen*screen=nullptr;Mod*mod=nullptr;Game*game=nullptr;
SDL_Color palette[256];std::string data;std::map<std::string,std::string> paths;std::vector<ArticleDefinition*> available;std::vector<State*> retired;
void pathsFrom(const std::string&path){DIR*d=opendir(path.c_str());if(!d)return;while(auto*e=readdir(d)){std::string n=e->d_name;if(n=="."||n=="..")continue;const std::string p=path+"/"+n;if(e->d_type==DT_DIR)pathsFrom(p);else {std::transform(n.begin(),n.end(),n.begin(),::toupper);paths[n]=p;}}closedir(d);}
void clearResources(){for(auto&p:mod->_surfaces)delete p.second;mod->_surfaces.clear();for(auto&p:mod->_sets)delete p.second;mod->_sets.clear();}
}
namespace OpenXcom {
namespace Observer {AgentController::AgentController(Game*g):_game(g){} AgentController::~AgentController(){} }
Game::Game(const std::string&):_agentController(this),_screen(nullptr),_cursor(nullptr),_lang(nullptr),_save(nullptr),_mod(nullptr),_fpsCounter(nullptr){}
Game::~Game(){}
Mod::Mod():_startingTime(1,1,1,1999,0,0,0){}Mod::~Mod(){}
Screen::Screen(){}Screen::~Screen(){}Surface*Screen::getSurface(){return ArticleHarness::logical;}int Screen::getDX()const{return 0;}int Screen::getDY()const{return 0;}
Screen*Game::getScreen()const{return ArticleHarness::screen;}Mod*Game::getMod()const{return ArticleHarness::mod;}Language*Game::getLanguage()const{return ArticleHarness::language;}SavedGame*Game::getSavedGame()const{return nullptr;}
void Game::pushState(State*s){_states.push_back(s);}void Game::popState(){check(!_states.empty(),"ordinary nav pop has current state");ArticleHarness::retired.push_back(_states.back());_states.pop_back();}
bool Game::agentOverlayClear(const AgentUiState&ui)const{return Outfitting::active?Outfitting::active->agentOverlayClear(ui):true;}
void State::setGamePtr(Game*g){_game=g;}
void State::setPalette(const std::string&name,int back){check(back==-1,"article default palette input");Palette p;p.loadDat(ArticleHarness::data+"/GEODATA/PALETTES.DAT",256,Palette::palOffset(name=="PAL_UFOPAEDIA"?3:name=="PAL_BATTLEPEDIA"?4:0));std::copy(p.getColors(),p.getColors()+256,_palette);std::copy(_palette,_palette+256,ArticleHarness::palette);ArticleHarness::logical->setPalette(_palette);}
#include "ArticleStateOrdinary.generated.inc"
Font*Mod::getFont(const std::string&name,bool)const{return name=="FONT_BIG"?ArticleHarness::big:ArticleHarness::small;}
Surface*Mod::getSurface(const std::string&name,bool error){
 auto found=_surfaces.find(name);if(found!=_surfaces.end()){found->second->setPalette(ArticleHarness::palette);return found->second;}
 auto path=ArticleHarness::paths.find(name);if(path==ArticleHarness::paths.end()){check(!error,("missing stock surface "+name).c_str());return nullptr;}
 auto*s=new Surface(name=="INTERWIN.DAT"?160:320,name=="INTERWIN.DAT"?600:200);auto ext=name.substr(name.find_last_of('.'));
 if(ext==".SPK")s->loadSpk(path->second);else if(ext==".BDY")s->loadBdy(path->second);else s->loadScr(path->second);s->setPalette(ArticleHarness::palette);_surfaces[name]=s;return s;
}
SurfaceSet*Mod::getSurfaceSet(const std::string&name,bool){auto f=_sets.find(name);if(f!=_sets.end())return f->second;auto*set=new SurfaceSet(32,name=="BIGOBS.PCK"?48:40);const auto path=ArticleHarness::paths.at(name),tab=ArticleHarness::paths.at(name.substr(0,name.size()-4)+".TAB");set->loadPck(path,tab);for(auto&p:*set->getFrames())p.second->setPalette(ArticleHarness::palette);_sets[name]=set;return set;}
int Mod::getOffset(int id,int)const{return id;}
MapDataSet*Mod::getMapDataSet(const std::string&){return nullptr;} // Terrain resources are not consumed by article rendering.
void Mod::loadTransparencyOffset(const std::string&,int&v,const YAML::Node&n)const{if(n)v=n.as<int>();}
void Mod::loadSpriteOffset(const std::string&,int&v,const YAML::Node&n,const std::string&,size_t)const{if(n)v=n.as<int>();}
void Mod::loadSoundOffset(const std::string&,int&v,const YAML::Node&n,const std::string&)const{if(n)v=n.as<int>();}
void Mod::loadSoundOffset(const std::string&,std::vector<int>&v,const YAML::Node&n,const std::string&)const{if(n){if(n.IsSequence())v=n.as<std::vector<int>>();else v={n.as<int>()};}}
RuleCraft*Mod::getCraft(const std::string&id,bool error)const{auto f=_crafts.find(id);if(f==_crafts.end()){check(!error,("missing stock rule "+id).c_str());return nullptr;}return f->second;}
RuleCraftWeapon*Mod::getCraftWeapon(const std::string&id,bool error)const{auto f=_craftWeapons.find(id);if(f==_craftWeapons.end()){check(!error,("missing stock rule "+id).c_str());return nullptr;}return f->second;}
RuleItem*Mod::getItem(const std::string&id,bool error)const{auto f=_items.find(id);if(f==_items.end()){check(!error,("missing stock rule "+id).c_str());return nullptr;}return f->second;}
RuleUfo*Mod::getUfo(const std::string&id,bool error)const{auto f=_ufos.find(id);if(f==_ufos.end()){check(!error,("missing stock rule "+id).c_str());return nullptr;}return f->second;}
Unit*Mod::getUnit(const std::string&id,bool error)const{auto f=_units.find(id);if(f==_units.end()){check(!error,("missing stock rule "+id).c_str());return nullptr;}return f->second;}
Armor*Mod::getArmor(const std::string&id,bool error)const{auto f=_armors.find(id);if(f==_armors.end()){check(!error,("missing stock rule "+id).c_str());return nullptr;}return f->second;}
RuleBaseFacility*Mod::getBaseFacility(const std::string&id,bool error)const{auto f=_facilities.find(id);if(f==_facilities.end()){check(!error,("missing stock rule "+id).c_str());return nullptr;}return f->second;}
RuleInterface*Mod::getInterface(const std::string&id,bool error)const{auto f=_interfaces.find(id);if(f==_interfaces.end()){check(!error,("missing stock rule "+id).c_str());return nullptr;}return f->second;}
ArticleDefinition*Mod::getUfopaediaArticle(const std::string&id,bool error)const{auto f=_ufopaediaArticles.find(id);if(f==_ufopaediaArticles.end()){check(!error,("missing stock rule "+id).c_str());return nullptr;}return f->second;}

bool Ufopaedia::isArticleAvailable(SavedGame*,ArticleDefinition*){return true;}
ArticleDefinitionList Ufopaedia::getAvailableArticles(SavedGame*,Mod*){return ArticleHarness::available;}
#include "ArticleNavigation.generated.inc"

class ReportPopupHarness : public State {
 const RuleResearch *_research,*_bonus;
 public: ReportPopupHarness(const RuleResearch *main,const RuleResearch *bonus):_research(main),_bonus(bonus){}
 void btnReportClick(Action*);
};
#include "ReportCallback.generated.inc"
struct SuppressionSave {std::set<std::string>done;std::vector<std::string>queries;bool isResearched(const std::string&name,bool){queries.push_back(name);return done.count(name);}};
struct SuppressionGame {SuppressionSave*save;explicit SuppressionGame(SuppressionSave*s):save(s){}SuppressionSave*getSavedGame(){return save;}};
#include "ReportSuppression.generated.inc"

}
namespace {
ArticleDefinition*definition(unsigned type){switch(type){case 1:return new ArticleDefinitionCraft;case 2:return new ArticleDefinitionCraftWeapon;case 3:return new ArticleDefinitionVehicle;case 4:return new ArticleDefinitionItem;case 5:return new ArticleDefinitionArmor;case 6:return new ArticleDefinitionBaseFacility;case 7:return new ArticleDefinitionTextImage;case 8:return new ArticleDefinitionText;case 9:return new ArticleDefinitionUfo;default:return new ArticleDefinitionTFTD;}}
void loadRules(const std::string&source,const std::string&game){using namespace ArticleHarness;data=std::string("../")+(game=="xcom1"?"UFO":"TFTD");paths.clear();pathsFrom(data);available.clear();mod=new Mod;DIR*d=opendir((source+"/bin/standard/"+game).c_str());check(d,"stock rules directory");std::vector<std::string>files;while(auto*e=readdir(d)){std::string name=e->d_name;if(name.size()>4&&name.substr(name.size()-4)==".rul")files.push_back(name);}closedir(d);std::sort(files.begin(),files.end());std::map<unsigned,ArticleDefinition*>first;
 for(auto&file:files){auto root=YAML::LoadFile(source+"/bin/standard/"+game+"/"+file);
 for(auto n:root["crafts"]){auto id=n["type"].as<std::string>();auto*&v=mod->_crafts[id];if(!v)v=new RuleCraft(id);v->load(n,mod,0);}
 for(auto n:root["craftWeapons"]){auto id=n["type"].as<std::string>();auto*&v=mod->_craftWeapons[id];if(!v)v=new RuleCraftWeapon(id);v->load(n,mod);}
 for(auto n:root["items"]){auto id=n["type"].as<std::string>();auto*&v=mod->_items[id];if(!v)v=new RuleItem(id);v->load(n,mod,0);}
 for(auto n:root["ufos"]){auto id=n["type"].as<std::string>();auto*&v=mod->_ufos[id];if(!v)v=new RuleUfo(id);v->load(n,mod);}
 for(auto n:root["units"]){auto id=n["type"].as<std::string>();auto*&v=mod->_units[id];if(!v)v=new Unit(id);v->load(n,mod);}
 for(auto n:root["armors"]){auto id=n["type"].as<std::string>();auto*&v=mod->_armors[id];if(!v)v=new Armor(id);v->load(n);}
 for(auto n:root["facilities"]){auto id=n["type"].as<std::string>();auto*&v=mod->_facilities[id];if(!v)v=new RuleBaseFacility(id);v->load(n,mod,0);}
 for(auto n:root["interfaces"]){auto id=n["type"].as<std::string>();auto*&v=mod->_interfaces[id];if(!v)v=new RuleInterface(id);v->load(n);}
 for(auto n:root["ufopaedia"]){auto*def=definition(n["type_id"].as<unsigned>());def->load(n,0);mod->_ufopaediaArticles[def->id]=def;first.emplace(unsigned(def->getType()),def);}
 }
 for(auto&p:first)available.push_back(p.second);check(available.size()==(game=="xcom1"?9:8),"stock representative for every native article type");
}

ArticleState*currentArticle(){using namespace ArticleHarness;return game->_states.empty()?nullptr:dynamic_cast<ArticleState*>(game->_states.back());}
void paintArticle(){if(auto*s=currentArticle())s->blit();}
void wireArticle(TableChannel&c){
 c.state.project=[](AgentUiState&out){auto*s=currentArticle();return s&&s->agentUiState(out);};
 c.state.handler=[&](const AgentRequest&r){auto*s=currentArticle();check(s&&s->agentDispatch(r),"actual article dispatcher accepts fresh grant");if(!currentArticle())c.game.top=&c.unsupportedParent;};
}
void articleExecute(TableChannel&c,const std::string&action,const std::string&arg=""){
 c.read();c.queue(action,arg);c.frame();check(c.receive().empty(),"article dispatch waits for subsequent presentation");paintArticle();c.frame();const auto result=c.receive();check(!result.empty()&&YAML::Load(result)["status"].as<std::string>()=="dispatched","ordinary article callback dispatched");
 const auto retiredCount=ArticleHarness::retired.size();const auto*state=currentArticle();c.send(c.request);c.controller->poll();check(c.receive()==result&&state==currentArticle()&&retiredCount==ArticleHarness::retired.size(),"article duplicate exact and no repeated callback");c.send("S "+c.request.substr(2,32));c.controller->poll();check(c.receive()==result,"article status-only cached receipt");c.events();
}
// Scroll is always requalified from the latest public projection. Some stock
// redraws retain navigation but no surviving body-control witness. That is a
// bounded partial projection, not authority to manufacture an upward command.
void articleScrollPair(TableChannel &c,const std::string &master,const std::string &prefix)
{
 articleExecute(c,"scroll_article_text","down");record(c,master,"ufopaedia_article",prefix+"_down");
 AgentUiState current;check(currentArticle()&&currentArticle()->agentUiState(current),"post-scroll article projection");
 if(current.offers("scroll_article_text")) {articleExecute(c,"scroll_article_text","up");record(c,master,"ufopaedia_article",prefix+"_up");}
 else {
  auto shown=YAML::Load(c.read());const auto*before=currentArticle();const auto retiredCount=ArticleHarness::retired.size();
  c.request="E "+token(c.command++)+" "+c.epoch+" "+shown["capability"].as<std::string>()+" "+shown["revision"].as<std::string>()+" scroll_article_text up";
  c.send(c.request);c.controller->poll();const auto response=c.receive();
  check(!response.empty()&&YAML::Load(response)["status"].as<std::string>()=="unavailable"&&currentArticle()==before&&ArticleHarness::retired.size()==retiredCount,"unadvertised scroll after stock redraw is unavailable without dispatch");
  record(c,master,"ufopaedia_article",prefix+"_control_unavailable");
 }
}
// Real report-opening and article navigation bodies; owning popup and availability
// are explicit seams. No completion/RNG/eligibility or live-game claim.
void reportStackTests(const std::string &master)
{
 using namespace ArticleHarness;
 const auto previousAvailable=available;
 auto *bonusDef=mod->getUfopaediaArticle(master=="xcom1"?"STR_LASER_PISTOL":"STR_GAUSS_PISTOL");
 auto *mainDef=previousAvailable.front();check(bonusDef&&mainDef&&bonusDef!=mainDef,"two distinct stock report definitions");
 available={bonusDef,mainDef};
 RuleResearch bonus(bonusDef->id),main(mainDef->id),missing("MCP_MISSING_REPORT"),lookup("MCP_LOOKUP_TOPIC");
 YAML::Node node;node["lookup"]=mainDef->id;lookup.load(node,0);
 struct Case {const char *name;const RuleResearch *main,*bonus;std::vector<std::string> bottomToTop;};
 const std::vector<Case> cases={
  {"none",nullptr,nullptr,{}},{"missing",&missing,nullptr,{}},
  {"main",&main,nullptr,{mainDef->id}},{"bonus",nullptr,&bonus,{bonusDef->id}},
  {"both",&main,&bonus,{bonusDef->id,mainDef->id}},
  {"missing_main",&missing,&bonus,{bonusDef->id}},
  {"lookup",&lookup,&bonus,{bonusDef->id,mainDef->id}}
 };
 for(const auto &test:cases)
 {
  check(game->_states.empty(),"report case starts with isolated stack");
  auto *parent=new State;game->pushState(parent);
  auto *popup=new ReportPopupHarness(test.main,test.bonus);game->pushState(popup);
  popup->btnReportClick(nullptr);
  check(game->_states.size()==test.bottomToTop.size()+1&&game->_states.front()==parent,"ordinary callback pops popup and opens expected report count");
  for(size_t i=0;i<test.bottomToTop.size();++i){auto *article=dynamic_cast<ArticleState*>(*std::next(game->_states.begin(),i+1));check(article&&article->getId()==test.bottomToTop[i],"actual article stack order follows bonus then main");}
  if(currentArticle())
  {
   paintArticle();TableChannel c(master+"-report-stack-"+test.name);wireArticle(c);
   record(c,master,"ufopaedia_article","stack_open");
   for(auto it=test.bottomToTop.rbegin();it!=test.bottomToTop.rend();++it)
   {check(currentArticle()&&currentArticle()->getId()==*it,"close reveals exact lower article");articleExecute(c,"close_ufopaedia_article");if(currentArticle())record(c,master,"ufopaedia_article","lower_report_after_close");}
  }
  check(game->_states.size()==1&&game->_states.back()==parent,"all report exits preserve original parent");game->popState();
 }
 // A global index belongs to Ufopaedia, not to each stacked report. Closing
 // main does not restore bonus's index; ordinary next/prev wrap accordingly.
 auto *parent=new State;game->pushState(parent);auto *popup=new ReportPopupHarness(&main,&bonus);game->pushState(popup);popup->btnReportClick(nullptr);paintArticle();
 TableChannel c(master+"-report-stack-navigation");wireArticle(c);articleExecute(c,"close_ufopaedia_article");
 check(currentArticle()->getId()==bonusDef->id&&Ufopaedia::_current_index==1,"closing top leaves ordinary shared index unchanged");
 articleExecute(c,"next_ufopaedia_article");check(currentArticle()->getId()==bonusDef->id&&Ufopaedia::_current_index==0,"ordinary global next wraps after top close");
 articleExecute(c,"previous_ufopaedia_article");check(currentArticle()->getId()==mainDef->id&&Ufopaedia::_current_index==1,"ordinary previous wraps globally on remaining report");
 articleExecute(c,"close_ufopaedia_article");check(game->_states.size()==1&&game->_states.back()==parent,"stack navigation preserves parent");game->popState();
 // Exact daily-completion suppression block, with a declared researched-set seam.
 SuppressionSave saved;SuppressionGame context(&saved);
 check(selectReportResearch(&lookup,&context)==&lookup,"unseen lookup retains main report");
 saved.done.insert(mainDef->id);check(selectReportResearch(&lookup,&context)==nullptr,"previously researched lookup suppresses main report");
 check(selectReportResearch(&bonus,&context)==&bonus,"distinct unresearched name is not suppressed");
 saved.done.insert(bonusDef->id);check(selectReportResearch(&bonus,&context)==nullptr,"name fallback suppression uses exact research name");
 check(saved.queries==std::vector<std::string>({mainDef->id,mainDef->id,bonusDef->id,bonusDef->id}),"suppression consults actual lookup/name targets");
 available=previousAvailable;
}

void articleTests(Font&bigFont,Font&smallFont,Language&lang,const std::string&source,const std::string&master){using namespace ArticleHarness;big=&bigFont;small=&smallFont;language=&lang;loadRules(source,master);Surface canvas(320,200);logical=&canvas;Screen ownerScreen;screen=&ownerScreen;Game ownerGame("offscreen owning seam");game=&ownerGame;State::setGamePtr(game);Outfitting::Game overlays;overlays.lang=&lang;Outfitting::active=&overlays;
 unsigned index=0;
 for(auto*def:available){Ufopaedia::_current_index=index++;auto*s=Ufopaedia::createArticleState(def);check(s,"actual17 constructor switch");game->pushState(s);TableChannel c(master+"-article-type-"+std::to_string(unsigned(def->getType())));wireArticle(c);
 record(c,master,"ufopaedia_article","type_"+std::to_string(unsigned(def->getType()))+"_before_paint","unsupported");s->blit();auto stock=record(c,master,"ufopaedia_article","type_"+std::to_string(unsigned(def->getType()))+"_stock");AgentUiState ui;check(s->agentUiState(ui)&&ui.offers("close_ufopaedia_article")&&ui.offers("previous_ufopaedia_article")&&ui.offers("next_ufopaedia_article"),"every actual article has ordinary close and both nav controls");
 std::cout<<master<<" type"<<unsigned(def->getType())<<" "<<ui.visible<<"\n";
 if(ui.offers("scroll_article_text")){articleScrollPair(c,master,"actual_stock_scroll");c.read();c.queue("close_ufopaedia_article");s->_agentBody->scrollLine(true);s->_agentBody->scrollLine(false);s->blit();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable","ordinary scroll ABA rejects queued grant");}
 SDL_Rect clip={1,0,319,200};SDL_SetClipRect(canvas.getSurface(),&clip);s->blit();record(c,master,"ufopaedia_article","destination_clip","unsupported");SDL_SetClipRect(canvas.getSurface(),nullptr);record(c,master,"ufopaedia_article","clip_restore_no_repaint","unsupported");s->blit();
 const auto original=s->_agentBody->getText();s->_agentBody->setText("changed");s->_agentBody->setText(original);record(c,master,"ufopaedia_article","body_aba_no_paint","unsupported");s->_agentBody->blit(&canvas);record(c,master,"ufopaedia_article","child_only_repaint","unsupported");s->blit();
 c.read();c.queue("close_ufopaedia_article");s->_agentTitle->setText(s->_agentTitle->getText());s->blit();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable","title same setter revokes queued article close");
 SDL_Rect sourceClip={0,0,1,1};SDL_SetClipRect(s->_agentBody->getSurface(),&sourceClip);s->blit();auto partial=record(c,master,"ufopaedia_article","body_source_clip");check(partial["visible"]["body_lines"].IsNull()&&partial["actions"].size()>=3,"source clipped body withholds text independently of nav");SDL_SetClipRect(s->_agentBody->getSurface(),nullptr);partial=record(c,master,"ufopaedia_article","body_source_clip_restore");check(partial["visible"]["body_lines"].IsNull(),"source clip restore without body blit cannot revive");s->blit();
 Surface cover(320,200);cover.setVisible(true);overlays._fpsCounter=&cover;record(c,master,"ufopaedia_article","full_global_overlay","unsupported");overlays._fpsCounter=nullptr;s->blit();
 s->_modal=s->_btnOk;record(c,master,"ufopaedia_article","top_modal","unsupported");s->_modal=nullptr;
 // Callback navigation creates the actual next/previous constructor. Both
 // directions, wraparound, and return are proved for each of the 17 families.
 articleExecute(c,"next_ufopaedia_article");record(c,master,"ufopaedia_article","next_actual_constructor");articleExecute(c,"previous_ufopaedia_article");auto restored=record(c,master,"ufopaedia_article","previous_actual_constructor");check(restored["visible"]["title"].as<std::string>()==stock["visible"]["title"].as<std::string>(),"ordinary next then previous restores visible title");articleExecute(c,"close_ufopaedia_article");record(c,master,"ufopaedia_article","ordinary_close","unsupported");
 }
 // Specific stock completion reports required by the planned public route.
 for(const std::string id:master=="xcom1"?std::vector<std::string>{"STR_LASER_PISTOL"}:std::vector<std::string>{"STR_GAUSS_PISTOL"}){
 auto*def=mod->getUfopaediaArticle(id);check(def,"declared stock report definition");auto*s=Ufopaedia::createArticleState(def);game->pushState(s);s->blit();TableChannel c(master+"-completion-report-"+id);wireArticle(c);auto shown=record(c,master,"ufopaedia_article","stock_completion_report_"+id);check(!shown["visible"]["title"].IsNull(),"planned report complete shown title");s->_agentBody->setExtraLineSpacing(0);s->blit();shown=record(c,master,"ufopaedia_article","stock_overlap_runtime_text");check(shown["visible"]["body_lines"].IsSequence()&&shown["visible"]["body_lines"].size()>1,"legacy overlapping article exposes viewport text");std::cout<<"OVERLAP "<<master<<" "<<YAML::Dump(shown["visible"])<<std::endl;for(const auto &line:shown["visible"]["body_lines"])check(!line.IsNull(),"overlap readability does not gate runtime text");std::vector<Text::AgentVisibleLine> layout;check(s->_agentBody->agentViewportLines(&canvas,layout),"active article uses viewport mode");for(const auto &line:layout)check(line.pixels.empty(),"no runtime per-glyph pixel proof");AgentUiState ui;check(s->agentUiState(ui),"planned report provider");if(ui.offers("scroll_article_text")){articleScrollPair(c,master,"report_scroll");}articleExecute(c,"close_ufopaedia_article");
 }
 // Transparent recognized-owned-layer proof: synthetic text content uses the
 // actual Text article; only the deliberately registered test layer is synthetic.
 {auto*def=available.front();auto*s=Ufopaedia::createArticleState(def);game->pushState(s);Text*body=s->_agentBody;body->setText("Visible line");body->setWidth(180);body->setHeight(30);body->setX(10);body->setY(150);body->setColor(2);body->setScrollable(true);auto*layer=new Surface(180,30,10,150);layer->setPalette(palette);layer->clear();s->add(layer);s->agentRegisterTransparentLayer(layer);s->blit();TableChannel c(master+"-article-owned-layer");wireArticle(c);auto shown=record(c,master,"ufopaedia_article","transparent_owned_layer");check(shown["visible"]["body_lines"].IsSequence(),"transparent owned layer retains body scope");
 std::vector<Text::AgentVisibleLine> lines;check(body->agentViewportLines(&canvas,lines)&&!lines.empty()&&lines[0].pixels.empty(),"actual body viewport without runtime glyph pixels");layer->setPixel(lines[0].layout.x,lines[0].layout.y,40);s->blit();shown=record(c,master,"ufopaedia_article","opaque_owned_pixel");check(shown["visible"]["body_lines"].IsSequence()&&shown["visible"]["body_lines"][0].IsNull(),"one later opaque pixel withholds full line");layer->clear();layer->setX(11);layer->setX(10);record(c,master,"ufopaedia_article","owned_layer_geometry_aba_no_paint","unsupported");s->blit();record(c,master,"ufopaedia_article","owned_layer_fresh_paint");
 // Explicit oversized captured-layout seam: text rollback must not orphan scroll authority.
 const auto retainedLines=body->_agentVisibleLines;
 body->_agentVisibleLines.assign(32,retainedLines.front());
 for(auto &line:body->_agentVisibleLines)line.text=std::string(300,'A');
 AgentUiState oversized;check(s->agentUiState(oversized),"oversized body retains bounded controls");
 check(YAML::Load(oversized.visible)["body_lines"].IsNull(),"oversized body rolls back text");
 check(oversized.offers("scroll_article_text"),"independent scroll authority retained");
 bool scrollRegion=false;for(const auto&r:oversized.regions)if(r.x==body->getX()&&r.y==body->getY()&&r.width==body->getWidth()&&r.height==body->getHeight())scrollRegion=true;
 check(scrollRegion,"text fallback retains scroll control region");body->_agentVisibleLines=retainedLines;
 const pid_t child=fork();check(child>=0,"article failed paint child");if(!child){agentObservePaint(-1);AgentUiState fail;if(s->agentUiState(fail))_exit(2);s->blit();if(s->agentUiState(fail))_exit(3);_exit(0);}int status=0;check(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"failed article paint remains fail closed");articleExecute(c,"close_ufopaedia_article");}
 reportStackTests(master);
 for(auto*s:retired)delete s;retired.clear();clearResources();delete mod;mod=nullptr;game=nullptr;screen=nullptr;logical=nullptr;
}

}
int main(int argc,char**argv){check(argc==2,"source root");check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen only");Options::baseXResolution=320;Options::baseYResolution=200;Options::keyOk=SDLK_RETURN;Options::keyCancel=SDLK_ESCAPE;Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}for(const std::string game:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+game+"/Language/en-US.yml");articleTests(big,small,lang,argv[1],game);}std::string json="[";for(auto&r:records){if(json.size()>1)json+=",";json+=r;}const char*d=getenv("OPENXCOM_AGENT_TEST_PACKET_DIR");std::ofstream out(std::string(d)+"/case-records.json");out<<json<<"]";check(bool(out),"actual article case records saved");check(agentPaintHealthy()&&!SDL_GetVideoSurface(),"no GUI or paint failure");std::cout<<"PASS "<<checks<<" checks\n";}
