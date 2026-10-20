// SPDX-License-Identifier: GPL-3.0-or-later
#define main retainedOutfittingFixtureMain
#include "AgentOutfittingProviderTestV21.cpp"
#undef main
namespace { SDL_Surface *failLock=nullptr,*failBlit=nullptr; }
extern "C" int __real_SDL_LockSurface(SDL_Surface*);
extern "C" int __wrap_SDL_LockSurface(SDL_Surface*p){return p==failLock?-1:__real_SDL_LockSurface(p);}
extern "C" int __real_SDL_UpperBlit(SDL_Surface*,SDL_Rect*,SDL_Surface*,SDL_Rect*);
extern "C" int __wrap_SDL_UpperBlit(SDL_Surface*s,SDL_Rect*a,SDL_Surface*d,SDL_Rect*b){return s==failBlit?-1:__real_SDL_UpperBlit(s,a,d,b);}
namespace NamedButton {
struct Mini:Surface {struct Slot{unsigned slot;bool selected;};std::uint64_t agentRevision()const{return 1;}std::vector<Slot>agentSlots(const AgentProjectileOutput&)const{return {};}void hoverAt(int){}};
struct Screen {Surface*logical=nullptr;Surface*getSurface(){return logical;}bool agentCaptureOutput(const Surface*,AgentProjectileOutput&)const{return false;}};
struct Game:Outfitting::Game {Screen screen;Screen*getScreen(){return &screen;}Surface*getCursor(){return _cursor;}Surface*getFpsCounter(){return _fpsCounter;}};
struct BasescapeState {Game*_game;const void*_base=nullptr;Surface*_modal=nullptr;Mini*_mini=nullptr;TextEdit*_edtBase;Text*_txtLocation,*_txtFunds;TextButton*_btnBaseInfo,*_btnResearch,*_btnManufacture,*_btnPurchase,*_btnSoldiers,*_btnCrafts,*_btnGeoscape;std::vector<Surface*>_surfaces;
 bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnGeoscapeClick(Action*);
 void miniClick(Action*){++_game->commands;}void btnBaseInfoClick(Action*){++_game->commands;}void btnResearchClick(Action*){++_game->commands;}void btnManufactureClick(Action*){++_game->commands;}void btnPurchaseClick(Action*){++_game->commands;}void btnSoldiersClick(Action*){++_game->commands;}void btnCraftsClick(Action*){++_game->commands;}
};
#include "BasescapeNamed.generated.inc"
}
void namedTests(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game)
{
 Paint p(big,small,lang,source,game,"basescape");NamedButton::Game g;NamedButton::BasescapeState s;s._game=&g;g.top=&s;g.screen.logical=&p.canvas;
 s._edtBase=p.edit(nullptr,127,17,193,0,"MCP Outfitting");s._txtLocation=p.text(126,9,194,16,"Europe");s._txtFunds=p.text(126,9,194,24,"$4 145 000");s._btnBaseInfo=p.button(128,12,192,71,"STR_BASE_INFORMATION");s._btnSoldiers=p.button(128,12,192,84,"STR_SOLDIERS_UC");s._btnCrafts=p.button(128,12,192,97,"STR_EQUIP_CRAFT");s._btnResearch=p.button(128,12,192,123,"STR_RESEARCH");s._btnManufacture=p.button(128,12,192,136,"STR_MANUFACTURE");s._btnPurchase=p.button(128,12,192,162,"STR_PURCHASE_RECRUIT");s._btnGeoscape=p.button(128,12,192,188,"STR_GEOSCAPE_UC");s._surfaces=p.surfaces;
 Surface cursor(9,13,318,198);cursor.setPalette(p.palette);cursor.clear(200);cursor.setVisible(false);g._cursor=&cursor;cursor.agentTrackPresentation();
 for(auto*w:s._surfaces)w->agentTrackPresentation();for(auto*w:{s._btnBaseInfo,s._btnSoldiers,s._btnCrafts,s._btnResearch,s._btnManufacture,s._btnPurchase,s._btnGeoscape})w->agentEnableNamedControl();
 auto paint=[&](){p.draw();cursor.blit(&p.canvas);};auto offered=[&](){AgentUiState ui;return s.agentUiState(ui)&&ui.offers("return_geoscape");};
 TableChannel c(game+"-named-basescape");connect(c,s);check(!offered(),"before first actual button paint no named authority");record(c,game,"basescape","before_first_paint","unsupported");paint();check(offered(),"stock named return admitted");record(c,game,"basescape","stock");
 std::string label;SDL_Rect ink,point;check(s._btnGeoscape->agentNamedControl(&p.canvas,label,ink,point)&&label=="GEOSCAPE","complete stock label and private point");
 cursor.setVisible(true);paint();check(offered(),"reported decorative corner cursor permits named control");record(c,game,"basescape","corner_cursor");
 cursor.setX(ink.x);cursor.setY(ink.y);paint();check(!offered(),"cursor intersects proved label withholds named control");record(c,game,"basescape","label_cursor");
 cursor.setX(point.x);cursor.setY(point.y);paint();check(!offered(),"cursor covers fixed ordinary interior point");record(c,game,"basescape","point_cursor");cursor.setVisible(false);paint();
 s._modal=s._btnBaseInfo;record(c,game,"basescape","top_modal","unsupported");s._modal=nullptr;g.top=&g.parent;record(c,game,"basescape","wrong_top","unsupported");g.top=&s;
 Surface cover(320,200);cover.setPalette(p.palette);cover.clear(40);g._fpsCounter=&cover;cover.agentTrackPresentation();cover.blit(&p.canvas);check(!offered(),"full global overlay");record(c,game,"basescape","full_overlay","unsupported");g._fpsCounter=nullptr;paint();
 auto*button=s._btnGeoscape;const auto name=button->getText();button->setText(std::string(200,'W'));paint();check(!offered(),"partial label withholds action");record(c,game,"basescape","partial_label");button->setText(name);paint();
 auto stale=[&](const char*name,const std::function<void()>&mutate){c.read();c.queue("return_geoscape");const auto n=g.pops;mutate();paint();c.frame();auto r=c.receive();check(!r.empty()&&YAML::Load(r)["status"].as<std::string>()=="unavailable"&&g.pops==n,"ABA queued command never dispatched");record(c,game,"basescape",name);};
 stale("label_aba",[&](){button->setText("Other");button->setText(name);});
 stale("same_label_reset",[&](){button->setText(name);});
 stale("position_aba",[&](){Surface*w=button;w->setX(191);w->setX(192);});
 stale("hidden_aba",[&](){Surface*w=button;w->setHidden(true);w->setHidden(false);});
 stale("visible_aba",[&](){Surface*w=button;w->setVisible(false);w->setVisible(true);});
 stale("resize_aba",[&](){Surface*w=button;w->setWidth(127);w->setWidth(128);});
 stale("overlay_geometry_aba",[&](){cursor.setX(ink.x);cursor.setY(ink.y);cursor.setX(318);cursor.setY(198);});
 SDL_Color palette[256];std::copy(p.palette,p.palette+256,palette);palette[2].r^=1;stale("palette_aba",[&](){button->setPalette(palette);button->setPalette(p.palette);});
 Surface*w=button;w->setX(191);w->setX(192);check(!offered(),"geometry restore without actual blit no authority");record(c,game,"basescape","geometry_restore_no_paint");paint();
 SDL_Rect clip={0,0,319,199};SDL_SetClipRect(p.canvas.getSurface(),&clip);paint();SDL_SetClipRect(p.canvas.getSurface(),nullptr);check(!offered(),"destination clip paint restore without repaint");record(c,game,"basescape","destination_clip_restore");paint();check(offered(),"fresh full destination restores");
 SDL_Rect sourceClip={0,0,100,12};SDL_SetClipRect(button->getSurface(),&sourceClip);paint();SDL_SetClipRect(button->getSurface(),nullptr);check(!offered(),"source clip paint restore without repaint");record(c,game,"basescape","source_clip_restore");paint();
 *button->getCrop()={0,0,100,12};paint();button->resetCrop();check(!offered(),"source crop restore requires repaint");record(c,game,"basescape","source_crop_restore");paint();
 SDL_SetAlpha(button->getSurface(),SDL_SRCALPHA,128);paint();SDL_SetAlpha(button->getSurface(),0,255);check(!offered(),"unsupported source alpha paint restore");record(c,game,"basescape","source_alpha_restore");paint();
 SDL_SetColorKey(button->getSurface(),SDL_SRCCOLORKEY,1);paint();SDL_SetColorKey(button->getSurface(),SDL_SRCCOLORKEY,0);check(!offered(),"unsupported source color key paint restore");record(c,game,"basescape","source_colorkey_restore");paint();
 SDL_SetAlpha(p.canvas.getSurface(),SDL_SRCALPHA,128);paint();SDL_SetAlpha(p.canvas.getSurface(),0,255);check(!offered(),"unsupported destination alpha paint restore");record(c,game,"basescape","destination_alpha_restore");paint();
 SDL_SetColorKey(p.canvas.getSurface(),SDL_SRCCOLORKEY,1);paint();SDL_SetColorKey(p.canvas.getSurface(),SDL_SRCCOLORKEY,0);check(!offered(),"unsupported destination color key paint restore");record(c,game,"basescape","destination_colorkey_restore");paint();
 p.canvas.setPixel(ink.x,ink.y,199);check(!offered(),"changed final label rectangle pixel");record(c,game,"basescape","changed_label_pixel");paint();
 for(bool lock:{true,false}){const auto pid=fork();check(pid>=0,"paint fault child");if(!pid){if(lock)failLock=button->getSurface();else failBlit=button->getSurface();button->setText(name);paint();if(offered())_exit(2);failLock=failBlit=nullptr;paint();if(offered())_exit(3);_exit(0);}int status=0;check(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0,lock?"actual failed SDL lock remains fail closed":"actual failed SDL blit remains fail closed");}
 {TextButton large(300,190);large.initText(&big,&small,&lang);large.setPalette(p.palette);large.setColor(160);large.agentEnableNamedControl();std::string words;for(int i=0;i<12;++i)words+=std::string("A A A A A A A A A A A A A A A A\n");large.setText(words);large.blit(&p.canvas);check(!large.agentLabel().empty(),"large complete typed label precedes cap check");std::string label;SDL_Rect a,b;check(!large.agentNamedControl(&p.canvas,label,a,b),"named label8192 pixel cap fails closed");paint();}
 cursor.setX(318);cursor.setY(198);cursor.setVisible(true);paint();check(offered(),"corner after negatives still admitted");record(c,game,"basescape","fresh_corner_return");const auto before=g.pops;execute(c,s,p,"return_geoscape");check(g.pops==before+1,"ordinary return callback exactly once");record(c,game,"basescape","after_fixture_reentry");
}
int main(int argc,char**argv){check(argc==2,"source root");check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen only");Options::baseXResolution=320;Options::baseYResolution=200;Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}for(const std::string game:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+game+"/Language/en-US.yml");namedTests(big,small,lang,argv[1],game);}std::string json="[";for(auto&r:records){if(json.size()>1)json+=",";json+=r;}const char*d=getenv("OPENXCOM_AGENT_TEST_PACKET_DIR");std::ofstream out(std::string(d)+"/case-records.json");out<<json<<"]";check(bool(out),"named cases saved");check(agentPaintHealthy()&&!SDL_GetVideoSurface(),"no unobserved paint failure or GUI");std::cout<<"PASS "<<checks<<" checks\n";}
