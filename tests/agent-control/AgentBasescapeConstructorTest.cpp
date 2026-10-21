// SPDX-License-Identifier: GPL-3.0-or-later
// Entire current constructor/provider; real widgets. Explicit owning/resource/map seams.
#define main retainedOutfittingFixtureMain
#include "AgentOutfittingProviderTestV21.cpp"
#undef main
extern "C" int __real_SDL_LockSurface(SDL_Surface*);
extern "C" int __wrap_SDL_LockSurface(SDL_Surface*p){return __real_SDL_LockSurface(p);}
extern "C" int __real_SDL_UpperBlit(SDL_Surface*,SDL_Rect*,SDL_Surface*,SDL_Rect*);
extern "C" int __wrap_SDL_UpperBlit(SDL_Surface*s,SDL_Rect*a,SDL_Surface*d,SDL_Rect*b){return __real_SDL_UpperBlit(s,a,d,b);}
namespace Constructor {
Paint *paintContext=nullptr;
struct Base{};struct Globe{};
struct BaseView:InteractiveSurface {using InteractiveSurface::InteractiveSurface;void setTexture(void*){}};
struct MiniBaseView:InteractiveSurface {using InteractiveSurface::InteractiveSurface;struct Slot{unsigned slot;bool selected;};void setTexture(void*){}void setBases(std::vector<Base*>*){}std::uint64_t agentRevision()const{return 1;}std::vector<Slot>agentSlots(const AgentProjectileOutput&)const{return {};} };
struct Screen {Surface*logical=nullptr;Surface*getSurface(){return logical;}bool agentCaptureOutput(const Surface*,AgentProjectileOutput&)const{return false;}};
struct Mod {void*getSurfaceSet(const std::string&){return nullptr;}};
struct Saved {std::vector<Base*>bases;std::vector<Base*>*getBases(){return &bases;}};
struct Game:Outfitting::Game {Screen screen;Mod mod;Saved save;Screen*getScreen(){return &screen;}Mod*getMod(){return &mod;}Saved*getSavedGame(){return &save;}Surface*getCursor(){return _cursor;}Surface*getFpsCounter(){return _fpsCounter;}};
Game *active=nullptr;
struct BasescapeState:OpenXcom::State {
 Game*_game=active;Base*_base;Globe*_globe;BaseView*_view;MiniBaseView*_mini;Text *_txtFacility,*_txtLocation,*_txtFunds;TextEdit*_edtBase;
 TextButton *_btnNewBase,*_btnBaseInfo,*_btnSoldiers,*_btnCrafts,*_btnFacilities,*_btnResearch,*_btnManufacture,*_btnTransfer,*_btnPurchase,*_btnSell,*_btnGeoscape;
 BasescapeState(Base*,Globe*);bool agentUiState(AgentUiState&)const;
 void setInterface(const std::string&name){check(name=="basescape","actual interface key");}
 void add(Surface*s,const std::string&id,const std::string&interface){check(interface=="basescape","actual add interface");paintContext->own(s);s->initText(&paintContext->big,&paintContext->small,&paintContext->lang);paintContext->colors(s,id);_surfaces.push_back(s);}
 void centerAllSurfaces(){check(Options::baseXResolution==320&&Options::baseYResolution==200,"zero offset centering seam");}
 LocalizedText tr(const std::string&id)const{return paintContext->lang.getString(id);}
#define CALLBACK(n) void n(Action*){}
 CALLBACK(viewLeftClick) CALLBACK(viewRightClick) CALLBACK(viewMouseOver) CALLBACK(viewMouseOut) CALLBACK(miniClick) CALLBACK(handleKeyPress) CALLBACK(edtBaseChange)
 CALLBACK(btnNewBaseClick) CALLBACK(btnBaseInfoClick) CALLBACK(btnSoldiersClick) CALLBACK(btnCraftsClick) CALLBACK(btnFacilitiesClick) CALLBACK(btnResearchClick) CALLBACK(btnManufactureClick) CALLBACK(btnTransferClick) CALLBACK(btnPurchaseClick) CALLBACK(btnSellClick) CALLBACK(btnGeoscapeClick)
#undef CALLBACK
};
#include "BasescapeNamed.generated.inc"
}
void constructorTests(Font&big,Font&small,Language&lang,const std::string&source,const std::string&master){
 Paint p(big,small,lang,source,master,"basescape");check(SDL_SetColorKey(p.canvas.getSurface(),0,0)==0,"opaque screen");Constructor::paintContext=&p;Constructor::Game g;Constructor::active=&g;g.screen.logical=&p.canvas;
 Surface cursor(9,13,0,180);cursor.setPalette(p.palette);cursor.clear(200);cursor.setVisible(false);g._cursor=&cursor;
 Constructor::Base base;Constructor::BasescapeState s(&base,nullptr);g.top=&s;
 auto offered=[&](){AgentUiState ui;return s.agentUiState(ui)&&ui.offers("open_transfer");};
 check(!offered(),"unpainted constructor has no transfer authority");p.draw();check(offered(),"actual constructor enabled Transfer");
 std::string label;SDL_Rect ink,point;check(s._btnTransfer->agentNamedControl(&p.canvas,label,ink,point)&&label=="TRANSFER","actual stock transfer label");
 cursor.setVisible(true);cursor.setX(ink.x);cursor.setY(ink.y);p.draw();cursor.blit(&p.canvas);check(!offered(),"cursor label overlap withholds transfer");
 cursor.setX(0);cursor.setY(180);p.draw();cursor.blit(&p.canvas);check(offered(),"unobstructed repaint restores transfer");
 std::cout<<master<<" actual constructor Transfer/overlay/restore PASS\n";
}
int main(int argc,char**argv){check(argc==2,"source root");check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen only");Options::baseXResolution=320;Options::baseYResolution=200;Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}for(const std::string master:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");constructorTests(big,small,lang,argv[1],master);}check(agentPaintHealthy()&&!SDL_GetVideoSurface(),"healthy offscreen paint");std::cout<<"PASS "<<checks<<" checks\n";}
