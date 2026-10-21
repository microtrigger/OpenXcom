// SPDX-License-Identifier: GPL-3.0-or-later
// Exact native provider/dispatch bodies; explicit synthetic paint/widget seams.
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iostream>
#include "Observer/AgentUi.h"
namespace OpenXcom {
struct SDL_Rect { int x,y,w,h; };
struct SDL_Surface {int w=320,h=200;SDL_Rect clip_rect={0,0,320,200};};
class Surface {public:SDL_Surface native;SDL_Surface *getSurface()const{return const_cast<SDL_Surface*>(&native);}int x=0,y=0,w=2,h=1;bool visible=true,covered=false;virtual ~Surface(){} int getX()const{return x;}int getY()const{return y;}int getWidth()const{return w;}int getHeight()const{return h;}bool getVisible()const{return visible;} };
class Text:public Surface {public:std::uint64_t revision=1;std::uint64_t agentTextRevision()const{return revision;}std::uint64_t agentLabelRevision()const{return revision;}std::string value="Text";const std::string &getText()const{return value;} };
class TextButton:public Text {};
struct AgentListRow {size_t logicalIndex=0;std::vector<std::string> cells={"CRAFT","READY","BASE","2/0/0"};bool selected=false,completeText=true;std::vector<SDL_Rect> regions,labelRegions;SDL_Rect bounds={0,100,200,9};};
class TextList:public Surface {public:
 std::uint64_t revision=1;std::vector<AgentListRow> rows;const Surface *up=nullptr,*down=nullptr;bool healthy=true,selectable=true;size_t selected=0;int ups=0,downs=0;
 std::uint64_t agentListRevision()const{return revision;}
 void agentScrollControls(const Surface *&u,const Surface *&d)const{u=up;d=down;}
 bool agentRows(std::vector<AgentListRow>&out)const{out=rows;return healthy;}
 size_t getSelectedRow()const{return selected;}bool agentControlRows(std::vector<AgentListRow>&out)const{return agentRows(out);}
 void scrollUp(bool,bool){++ups;}void scrollDown(bool,bool){++downs;}
 bool agentSelectProjectRow(size_t n){selected=n;return selectable;}
};
struct ComboBox:Surface {bool popup=false;std::uint64_t revision=1;bool agentPopupOpen()const{return popup;}std::uint64_t agentUiRevision()const{return revision;} };
struct Timer {bool running=false;bool isRunning()const{return running;} };
namespace Observer {
static bool healthy=true, uncovered=true;
std::string agentJsonString(const std::string&s){std::string o="\"";for(char c:s){if(c=='"'||c=='\\')o+='\\';o+=c;}return o+'"';}
bool agentPaintHealthy(){return healthy;}
std::vector<Surface*> agentFlattenPaintOrder(const std::vector<Surface*>&s){return s;}
bool agentWidgetUncovered(const Surface*s,const std::vector<Surface*>&){return s&&s->visible&&!s->covered&&uncovered;}
bool agentRegionsUncovered(const std::vector<AgentUiRect>&,const Surface*s,const std::vector<Surface*>&p){return agentWidgetUncovered(s,p);}
void agentIncludeWidget(AgentUiState&u,const Surface*s){if(s&&s->visible&&!s->covered)u.regions.push_back({s->x,s->y,s->w,s->h});}
std::string agentTextJson(const Text*t){return t&&t->visible?agentJsonString(t->value):"null";}
std::string agentButtonLabel(const TextButton*t){return t&&t->visible?t->value:"";}
bool agentWidgetVisible(const Surface*s){return s&&s->visible&&!s->covered;}
#include "Ui.generated.inc"
}
using namespace Observer;
struct Action {};
struct State {std::function<void()> paintHook;virtual ~State(){} void blit(){if(paintHook)paintHook();} };
struct Globe {double x=0,y=0;int centers=0;void center(double a,double b){x=a;y=b;++centers;}};struct GeoscapeState {Globe globe;int resets=0;void timerReset(){++resets;}Globe *getGlobe(){return &globe;}};
struct Target {virtual ~Target(){} };struct Ufo:Target {double getLongitude(){return 1.25;}double getLatitude(){return 2.5;}};struct Waypoint:Target{int id=0;static int deleted;~Waypoint(){++deleted;}int getId(){return id;}void setId(int n){id=n;}};int Waypoint::deleted=0;
struct Craft {std::string status="STR_READY";bool low=false,complete=false;Target*destination=nullptr;std::string getStatus(){return status;}bool getLowFuel(){return low;}bool getMissionComplete(){return complete;}void setDestination(Target*t){destination=t;}void setStatus(std::string s){status=s;}};
namespace Options {bool craftLaunchAlways=false;}
struct SavedGame {int ids=0;std::vector<Waypoint*> waypoints;int getId(const char*){return ++ids;}std::vector<Waypoint*>*getWaypoints(){return &waypoints;}};
struct Screen {Surface logical;Surface *getSurface(){return &logical;} };
struct Game {Screen screen;Screen *getScreen(){return &screen;}const void*top=nullptr;bool clear=true;int pops=0;SavedGame save;std::vector<State*> pushed;
 ~Game(){for(auto*s:pushed)delete s;}bool isState(const void*s)const{return top==s;}bool agentOverlayClear(const AgentUiState&)const{return clear;}void popState(){++pops;}void pushState(State*s){pushed.push_back(s);}SavedGame*getSavedGame(){return &save;}};
struct Common:State {
 Game game;Game *_game=&game;void *_modal=nullptr;std::vector<Surface*>_surfaces;int token=0;void *_base=&token,*_globe=&token;
 Target target;Target *_target=&target;Craft craft;Craft *_craft=&craft;
 Text title,status,base,weapons,craftName;TextButton ok,cancel,go;TextList list;
 Text *_txtTitle=&title,*_txtTarget=&title,*_txtCraft=&craftName,*_txtStatus=&status,*_txtBase=&base,*_txtWeapons=&weapons;
 TextButton *_btnOk=&ok,*_btnCancel=&cancel,*_btnGotoBase=&go;TextList *_lstCrafts=&list;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;
 Common(){int y=1;for(Surface*s:std::vector<Surface*>{&title,&status,&base,&weapons,&craftName,&ok,&cancel,&go,&list}){s->y=y;y+=3;_surfaces.push_back(s);}}
};
struct SelectDestinationState:State {Craft*craft;void*globe;SelectDestinationState(Craft*c,void*g):craft(c),globe(g){}};
struct BasescapeState:State {BasescapeState(void*,void*){}};
struct ConfirmDestinationState:Common {
 ConfirmDestinationState(Craft*c=nullptr,Target*t=nullptr){if(c)_craft=c;if(t)_target=t;game.top=this;blit();}
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit();bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnOkClick(Action*);void btnCancelClick(Action*);
};
struct InterceptState:Common {
 std::vector<Craft*>_crafts;InterceptState(void*g=nullptr,void*b=nullptr,Target*t=nullptr){if(g)_globe=g;if(b)_base=b;if(t)_target=t;_crafts.push_back(&craft);game.top=this;blit();}
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit();bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnCancelClick(Action*);void btnGotoBaseClick(Action*);void lstCraftsLeftClick(Action*);
};
struct UfoDetectedState {Game game;Game*_game=&game;GeoscapeState geo;GeoscapeState*_state=&geo;Ufo ufo;Ufo*_ufo=&ufo;void btnInterceptClick(Action*);};
#include "TransferProviders.generated.inc"
}
using namespace OpenXcom;using namespace OpenXcom::Observer;
static int checks=0;static void check(bool b,const char*m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
static AgentRequest req(std::string a,std::string c=""){AgentRequest r;r.action=a;r.choice=c;return r;}
template<class T>void guards(T&s){AgentUiState u;auto&clip=s.game.screen.logical.native.clip_rect;
 clip.w=100;s.blit();check(!s.agentUiState(u),"clipped render rejected");clip.w=320;check(!s.agentUiState(u),"restore clip needs render");s.blit();check(s.agentUiState(u),"rerender accepted");
 clip.x=1;check(!s.agentUiState(u),"clip after render rejected");clip.x=0;s.paintHook=[&](){clip.w=100;};s.blit();check(!s.agentUiState(u),"clip during render rejected");s.paintHook=nullptr;clip.w=320;s.blit();
 ++s.title.revision;check(!s.agentUiState(u),"text revision pending denied");s.blit();++s.cancel.revision;check(!s.agentUiState(u),"button revision pending denied");s.blit();
 s.game.top=nullptr;check(!s.agentUiState(u),"not top denied");s.game.top=&s;s._modal=&s;check(!s.agentUiState(u),"modal denied");s._modal=nullptr;healthy=false;check(!s.agentUiState(u),"paint unhealthy denied");healthy=true;s.game.clear=false;check(!s.agentUiState(u),"overlay denied");s.game.clear=true;}
int main(){
 InterceptState s;AgentListRow row;row.regions={{0,100,20,1}};s.list.rows={row};s.go.visible=false;AgentUiState u;
 check(s.agentUiState(u)&&u.screen=="intercept_craft"&&u.offers("choose_visible_row"),"four column interception rows admitted");check(u.visible.find("\"cells\":[\"CRAFT\",\"READY\",\"BASE\",\"2/0/0\"]")!=std::string::npos,"exact four displayed columns");check(!u.offers("open_intercept_base"),"hidden optional base button omitted");guards(s);
 auto saved=u;s.agentUiState(saved);++s.list.revision;check(!s.agentUiState(u),"list mutation before paint denied");s.blit();check(s.agentUiState(u)&&!saved.sameBinding(u),"list generation changed binding");
 s._target=nullptr;check(!s.agentUiState(u)&&!s.agentDispatch(req("choose_visible_row","r0")),"no implicit target route");s._target=&s.target;
 for(auto h:{"r","r00","r01","r1","r99","x0","r-1"})check(!s.agentDispatch(req("choose_visible_row",h)),"invalid unobserved handles denied");
 s.list.selectable=false;check(!s.agentDispatch(req("choose_visible_row","r0")),"native selection failure denied");s.list.selectable=true;
 s.list.rows[0].cells.pop_back();check(s.agentUiState(u)&&!u.offers("choose_visible_row"),"three columns not accepted");s.list.rows[0]=row;s.list.rows[0].cells[0]="";check(s.agentUiState(u)&&!u.offers("choose_visible_row"),"empty craft name not accepted");s.list.rows[0]=row;
 s.list.covered=true;check(!s.agentUiState(u)||!u.offers("choose_visible_row"),"covered row withheld");s.list.covered=false;
 Surface up,down;up.x=200;up.y=150;down.x=210;down.y=150;s.list.up=&up;s.list.down=&down;check(s.agentDispatch(req("scroll_visible_list","up"))&&s.list.ups==1,"scroll up");check(s.agentDispatch(req("scroll_visible_list","down"))&&s.list.downs==1,"scroll down");check(!s.agentDispatch(req("scroll_visible_list","bad")),"scroll invalid denied");
 s.cancel.covered=true;check(!s.agentDispatch(req("cancel_interception")),"covered cancel denied");s.cancel.covered=false;check(s.agentDispatch(req("cancel_interception"))&&s.game.pops==1,"ordinary cancel pops once");
 // All ordinary eligibility branches, tested through provider dispatch with a real displayed target seam.
 for(auto status:{"STR_READY","STR_OUT","STR_REPAIRS"})for(bool always:{false,true})for(bool low:{false,true})for(bool complete:{false,true}){
  InterceptState t;t.list.rows={row};t.craft.status=status;t.craft.low=low;t.craft.complete=complete;Options::craftLaunchAlways=always;
  bool eligible=t.craft.status=="STR_READY"||((t.craft.status=="STR_OUT"||always)&&!low&&!complete);
  check(t.agentDispatch(req("choose_visible_row","r0")),"visible action dispatches even if ordinary rules decline");check(t.game.pops==(eligible?1:0)&&t.game.pushed.size()==(eligible?1u:0u),"ordinary eligibility exact");
  if(eligible){auto*c=dynamic_cast<ConfirmDestinationState*>(t.game.pushed[0]);check(c&&c->_craft==&t.craft&&c->_target==&t.target,"preselected target preserved");}
 }
 {InterceptState t;t._target=nullptr;t.lstCraftsLeftClick(nullptr);check(dynamic_cast<SelectDestinationState*>(t.game.pushed[0])!=nullptr,"ordinary no target branch preserved outside MCP");}
 ConfirmDestinationState c;check(c.agentUiState(u)&&u.screen=="interception_confirm"&&u.offers("confirm_interception"),"confirmation provider");guards(c);c.ok.covered=true;check(!c.agentDispatch(req("confirm_interception")),"covered confirm denied");c.ok.covered=false;check(c.agentDispatch(req("confirm_interception"))&&c.craft.destination==&c.target&&c.craft.status=="STR_OUT"&&c.game.pops==2,"ordinary target confirmation commit");
 {ConfirmDestinationState t;check(t.agentDispatch(req("cancel_interception_confirmation"))&&t.game.pops==1&&t.craft.destination==nullptr&&t.craft.status=="STR_READY","cancel existing target no mutation");}
 {Waypoint w;ConfirmDestinationState t(nullptr,&w);t.btnOkClick(nullptr);check(w.id==1&&t.game.save.waypoints.size()==1&&t.craft.destination==&w,"new waypoint registers once");}
 {Waypoint w;w.id=7;ConfirmDestinationState t(nullptr,&w);t.btnOkClick(nullptr);check(w.id==7&&t.game.save.waypoints.empty(),"existing waypoint not reregistered");}
 {int deleted=Waypoint::deleted;ConfirmDestinationState t(nullptr,new Waypoint);t.btnCancelClick(nullptr);check(Waypoint::deleted==deleted+1&&t.game.pops==1,"cancel deletes only draft waypoint");}
 {Waypoint w;w.id=7;int deleted=Waypoint::deleted;ConfirmDestinationState t(nullptr,&w);t.btnCancelClick(nullptr);check(Waypoint::deleted==deleted,"cancel preserves registered waypoint");}
 {UfoDetectedState t;t.btnInterceptClick(nullptr);auto*i=dynamic_cast<InterceptState*>(t.game.pushed.at(0));check(t.geo.resets==1&&t.geo.globe.centers==1&&t.geo.globe.x==1.25&&t.geo.globe.y==2.5&&t.game.pops==0,"ordinary detected contact intercept timer/center/push");check(i&&i->_target==&t.ufo&&i->_globe==&t.geo.globe,"detected contact target passed unchanged");}
 std::cout<<checks<<" interception helper/provider/controller seam checks passed; no real font capture or live game\n";
}
