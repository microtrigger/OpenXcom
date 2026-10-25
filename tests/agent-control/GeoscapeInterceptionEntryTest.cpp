// SPDX-License-Identifier: GPL-3.0-or-later
// Exact native provider/dispatch bodies; explicit synthetic paint/widget seams.
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iostream>
#include "Observer/AgentUi.h"
#include "Observer/AgentTargetLifetime.h"
namespace OpenXcom {
struct SDL_Rect { int x,y,w,h; };
struct SDL_Surface {int w=320,h=200;SDL_Rect clip_rect={0,0,320,200};};
class Surface {public:std::uint64_t agentPresentationRevision()const{return 1;}bool agentPresentationPaintedTo(const Surface*)const{return visible&&!covered;}SDL_Surface native;SDL_Surface *getSurface()const{return const_cast<SDL_Surface*>(&native);}int x=0,y=0,w=2,h=1;bool visible=true,covered=false;virtual ~Surface(){} int getX()const{return x;}int getY()const{return y;}int getWidth()const{return w;}int getHeight()const{return h;}bool getVisible()const{return visible;} };
class Text:public Surface {public:std::uint64_t revision=1;std::uint64_t agentTextRevision()const{return revision;}std::uint64_t agentLabelRevision()const{return revision;}std::string value="Text";const std::string &getText()const{return value;} };
class TextButton:public Text {public:bool pressed=false;std::uint64_t agentSelectionRevision()const{return revision;}bool agentPressed(bool &v)const{v=pressed;return visible&&!covered;} };
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
bool agentRegionVisible(const AgentUiRect&r){return r.x>=0&&r.y>=0&&r.width>0&&r.height>0&&r.x+r.width<=320&&r.y+r.height<=200;}
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
struct Navigation {virtual ~Navigation(){};};struct Globe {};
struct InterceptState:Navigation {Globe*globe;explicit InterceptState(Globe*g):globe(g){}};
struct Game {bool clear=true;std::vector<Navigation*>stack;~Game(){for(auto*p:stack)delete p;}bool agentOverlayClear(const AgentUiState&)const{return clear;}void pushState(Navigation*n){stack.push_back(n);}};
struct DogfightState {bool agentUiState(AgentUiState&)const{return false;}};
struct GeoscapeState {
 Text texts[8];TextButton buttons[12];Game game;Game*_game=&game;Globe globe;Globe*_globe=&globe;void*_modal=nullptr;std::vector<DogfightState*>_dogfights;std::vector<Surface*>_surfaces;bool disabled=false;
 Text *_txtHour=&texts[0],*_txtMin=&texts[1],*_txtSec=&texts[2],*_txtWeekday=&texts[3],*_txtDay=&texts[4],*_txtMonth=&texts[5],*_txtYear=&texts[6],*_txtFunds=&texts[7];
 TextButton *_btn5Secs=&buttons[0],*_btn1Min=&buttons[1],*_btn5Mins=&buttons[2],*_btn30Mins=&buttons[3],*_btn1Hour=&buttons[4],*_btn1Day=&buttons[5],*_btnIntercept=&buttons[6],*_btnBases=&buttons[7],*_btnGraphs=&buttons[8],*_btnUfopaedia=&buttons[9],*_btnOptions=&buttons[10],*_btnFunding=&buttons[11];
 GeoscapeState(){for(unsigned i=0;i<8;++i){texts[i].y=i*2;_surfaces.push_back(&texts[i]);}for(unsigned i=0;i<12;++i){buttons[i].y=30+i*3;_surfaces.push_back(&buttons[i]);}buttons[0].pressed=true;buttons[6].value="INTERCEPT";}
 DogfightState*agentActiveDogfight()const{return nullptr;}bool buttonsDisabled(){return disabled;}
 bool agentUiState(AgentUiState&)const;bool agentDispatch(const std::string&);void btnInterceptClick(Action*);
 void btnFundingClick(Action*){}void btnBasesClick(Action*){}void btnOptionsClick(Action*){}
};
#include "GeoEntry.generated.inc"
}
using namespace OpenXcom;using namespace OpenXcom::Observer;
static int checks=0;static void check(bool b,const char*m){++checks;if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
 GeoscapeState g;AgentUiState u,old;check(g.agentUiState(u)&&u.offers("open_interception"),"visible INTERCEPT action");check(g.agentDispatch("open_interception")&&g.game.stack.size()==1,"native entry pushes once");check(dynamic_cast<InterceptState*>(g.game.stack.back())->globe==g._globe,"ordinary globe operand");
 g._btnIntercept->visible=false;check(g.agentUiState(u)&&!u.offers("open_interception")&&!g.agentDispatch("open_interception"),"hidden INTERCEPT not admitted");g._btnIntercept->visible=true;
 g._btnIntercept->covered=true;check(g.agentUiState(u)&&!u.offers("open_interception")&&!g.agentDispatch("open_interception"),"covered INTERCEPT not admitted");g._btnIntercept->covered=false;
 g._modal=&g;check(!g.agentUiState(u)&&!g.agentDispatch("open_interception"),"modal blocks entry");g._modal=nullptr;
 g.agentUiState(old);++g._btnIntercept->revision;g.agentUiState(u);check(!old.sameBinding(u),"button revision changes binding");
 g.disabled=true;check(g.agentDispatch("open_interception")&&g.game.stack.size()==1,"native zoom disabled no-op preserved, receipt not screen success");
 std::cout<<"GeoscapeInterceptionEntryTest PASS "<<checks<<" synthetic widget/native-body checks; not live/render/constructor proof\n";
}
