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
struct AgentPurchaseCell { bool known=true;std::string text="3";std::vector<SDL_Rect> regions; };
struct AgentPurchaseRow {size_t logicalIndex=0;AgentPurchaseCell cells[4];SDL_Rect bounds={0,100,100,9};bool selected=false;const Surface *increase=nullptr,*decrease=nullptr;};
class TextList:public Surface {public:
 std::uint64_t revision=1;std::vector<AgentPurchaseRow> rows;const Surface *up=nullptr,*down=nullptr;bool healthy=true,selectable=true;size_t selected=0;int ups=0,downs=0;
 std::uint64_t agentListRevision()const{return revision;}
 void agentScrollControls(const Surface *&u,const Surface *&d)const{u=up;d=down;}
 bool agentPurchaseRows(std::vector<AgentPurchaseRow>&out)const{out=rows;return healthy;}
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
bool agentProjectList(const TextList*l,const Surface*,const std::vector<Surface*>&,unsigned,bool,AgentUiState&u,AgentListProjection&p,const std::function<bool(const AgentUiState&)>&allowed,bool,bool){
 if(!l->healthy)return false;
 AgentUiState a;a.regions={{l->x,l->y,l->w,l->h}};if(!allowed(a)||l->covered)return true;
 u.regions.insert(u.regions.end(),a.regions.begin(),a.regions.end());for(auto&r:l->rows)u.rows.push_back(r.logicalIndex);
 p.rows="[]";p.up=l->up;p.down=l->down;return true;
}
#include "Ui.generated.inc"
}
using namespace Observer;
struct Action {};
struct Screen {Surface logical;Surface *getSurface(){return &logical;} };
struct Game {Screen screen;Screen *getScreen(){return &screen;}const void*top=nullptr;bool clear=true;bool isState(const void*s)const{return top==s;}bool agentOverlayClear(const AgentUiState&)const{return clear;} };
struct State {std::function<void()> paintHook;void blit(){if(paintHook)paintHook();} };
struct Common:State {
 Game game;Game *_game=&game;void *_modal=nullptr;std::vector<Surface*>_surfaces;int token=0;void *_base=&token;
 Text title,funds,name,area,cost,total,quantity,amount,destination;TextButton ok,cancel;TextList list;ComboBox combo;Timer inc,dec;
 Text *_txtTitle=&title,*_txtFunds=&funds,*_txtName=&name,*_txtArea=&area,*_txtCost=&cost,*_txtTotal=&total,*_txtQuantity=&quantity,*_txtAmountTransfer=&amount,*_txtAmountDestination=&destination;
 TextButton *_btnOk=&ok,*_btnCancel=&cancel;TextList *_lstItems=&list,*_lstBases=&list;ComboBox *_cbxCategory=&combo;Timer *_timerInc=&inc,*_timerDec=&dec;
 int oks=0,cancels=0,clicks=0;
 Common(){int y=1;for(Surface*s:std::vector<Surface*>{&title,&funds,&name,&area,&cost,&total,&quantity,&amount,&destination,&ok,&cancel,&list,&combo}){s->y=y;y+=3;_surfaces.push_back(s);}}
 void btnOkClick(Action*){++oks;}void btnCancelClick(Action*){++cancels;}void lstBasesClick(Action*){++clicks;}
};
struct TransferBaseState:Common {std::vector<void*> _bases={nullptr,nullptr,nullptr};TransferBaseState(){game.top=this;blit();}bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit();bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);};
struct TransferConfirmState:Common {void *_state=&token;TransferConfirmState(){game.top=this;blit();}bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit();bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);};
struct TransferItemsState:Common {
 void *_baseFrom=&token,*_baseTo=&token;std::vector<int> _rows={0,1,2};size_t _sel=0;struct Row{int amount=7;};std::vector<Row> rows=std::vector<Row>(3);int incValue=0,decValue=0;
 TransferItemsState(){game.top=this;blit();}
 Row&getRow(){return rows.at(_rows.at(_sel));}void increaseByValue(int n){incValue=n;}void decreaseByValue(int n){decValue=n;}
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit();bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);
};
#include "TransferProviders.generated.inc"
}
using namespace OpenXcom;using namespace OpenXcom::Observer;
static int checks=0;static void check(bool b,const char*m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
static AgentRequest request(std::string a,std::string c="",std::string o="",int v=0){AgentRequest r;r.action=a;r.choice=c;r.operation=o;r.value=v;return r;}
template<class T>void gates(T&s){AgentUiState u;
 auto &clip=s.game.screen.logical.native.clip_rect;
 clip.w=100;s.blit();check(!s.agentUiState(u),"clipped destination render rejected");
 clip.w=320;check(!s.agentUiState(u),"restore clip without rerender rejected");s.blit();check(s.agentUiState(u),"full rerender recertifies");
 clip.x=1;check(!s.agentUiState(u),"clip mutation after render rejected");clip.x=0;
 s.paintHook=[&](){clip.w=100;};s.blit();check(!s.agentUiState(u),"clip changed during render rejected");s.paintHook=nullptr;clip.w=320;s.blit();
 ++s.title.revision;check(!s.agentUiState(u),"title mutation since blit rejected");s.blit();check(s.agentUiState(u),"title rerender certified");
 ++s.cancel.revision;check(!s.agentUiState(u),"button label mutation since blit rejected");s.blit();check(s.agentUiState(u),"button rerender certified");s.game.top=nullptr;check(!s.agentUiState(u),"not current top denied");s.game.top=&s;s._modal=&s;check(!s.agentUiState(u),"modal denied");s._modal=nullptr;healthy=false;check(!s.agentUiState(u),"unhealthy paint denied");healthy=true;s.game.clear=false;check(!s.agentUiState(u),"overlay all regions denied");s.game.clear=true;}
int main(){
 TransferItemsState s;Surface inc,dec;inc.x=150;inc.y=100;dec.x=155;dec.y=100;
 AgentPurchaseRow row;row.logicalIndex=1;row.increase=&inc;row.decrease=&dec;row.selected=true;for(int i=0;i<4;++i){row.cells[i].text="CELL"+std::to_string(i);row.cells[i].regions.push_back({i*20,100,4,1});}s.list.rows.push_back(row);AgentUiState u;
 check(s.agentUiState(u)&&u.screen=="transfer_items","items screen");check(u.offers("review_transfer")&&u.offers("cancel_transfer_items")&&u.offers("change_transfer_quantity"),"exact offered actions");check(u.visible.find("\"headings\":[null,")!=std::string::npos&&u.visible.find("\"cells\":[\"CELL0\"")!=std::string::npos,"visible shape/cells");
 auto saved=u;++s.list.revision;check(!s.agentUiState(u),"list mutation before repaint denied");s.blit();check(s.agentUiState(u)&&!saved.sameBinding(u),"list ABA revision binding");saved=u;++s.combo.revision;check(!s.agentUiState(u),"combo mutation before repaint denied");s.blit();check(s.agentUiState(u)&&!saved.sameBinding(u),"combo revision binding");
 gates(s);s.combo.popup=true;check(!s.agentUiState(u),"open dropdown denied");s.combo.popup=false;s.inc.running=true;check(!s.agentUiState(u),"increase timer denied");s.inc.running=false;s.dec.running=true;check(!s.agentUiState(u),"decrease timer denied");s.dec.running=false;
 for(int v:{-1000,-1,1,1000}){s.incValue=s.decValue=0;check(s.agentDispatch(request("change_transfer_quantity","r0","adjust",v)),"bounded adjust dispatch");check(s._sel==1&&(v>0?s.incValue==v:s.decValue==-v),"current mapped logical row/direction");}
 for(int v:{INT_MIN,-1001,0,1001,INT_MAX})check(!s.agentDispatch(request("change_transfer_quantity","r0","adjust",v)),"invalid adjust rejected");
 for(int v:{0,3,7,100000}){s.incValue=s.decValue=0;check(s.agentDispatch(request("change_transfer_quantity","r0","set",v)),"set admitted");check(v==7?(s.incValue==0&&s.decValue==0):v>7?s.incValue==v-7:s.decValue==7-v,"set delta delegates");}
 for(int v:{-1,100001})check(!s.agentDispatch(request("change_transfer_quantity","r0","set",v)),"set bounds");
 check(s.agentDispatch(request("change_transfer_quantity","r0","max"))&&s.incValue==INT_MAX,"max delegates");check(s.agentDispatch(request("change_transfer_quantity","r0","clear"))&&s.decValue==INT_MAX,"clear delegates");
 for(auto h:{"","r","r00","r01","r1","r32","r-1","r999","x0","ra"})check(!s.agentDispatch(request("change_transfer_quantity",h,"max")),"invalid/unobserved row rejected");
 s.list.rows[0].decrease=nullptr;check(s.agentUiState(u)&&u.context[2]==1,"single direction offered");check(!s.agentDispatch(request("change_transfer_quantity","r0","set",10))&&!s.agentDispatch(request("change_transfer_quantity","r0","clear")),"set requires both arrows; clear requires decrease");check(s.agentDispatch(request("change_transfer_quantity","r0","max")),"increase-only max works");s.list.rows[0].decrease=&dec;
 s.list.rows[0].cells[0].known=false;check(s.agentUiState(u)&&u.visible.find("\"cells\":[null,")!=std::string::npos,"unknown label not invented");s.list.rows[0].cells[0].known=true;
 inc.covered=dec.covered=true;check(s.agentUiState(u)&&!u.offers("change_transfer_quantity"),"covered arrows revoke quantity action");check(!s.agentDispatch(request("change_transfer_quantity","r0","max")),"covered quantity dispatch rejected");inc.covered=dec.covered=false;
 Surface up,down;up.x=200;up.y=150;down.x=210;down.y=150;s.list.up=&up;s.list.down=&down;
 check(s.agentDispatch(request("scroll_visible_list","up"))&&s.list.ups==1,"items scroll up ordinary callback");check(s.agentDispatch(request("scroll_visible_list","down"))&&s.list.downs==1,"items scroll down ordinary callback");check(!s.agentDispatch(request("scroll_visible_list","sideways")),"invalid scroll denied");s.list.up=nullptr;s.list.down=nullptr;
 s.list.rows[0].logicalIndex=99;check(!s.agentDispatch(request("change_transfer_quantity","r0","max")),"out of range mapped row rejected");s.list.rows[0].logicalIndex=1;
 check(s.agentDispatch(request("review_transfer"))&&s.oks==1,"review ordinary callback");check(s.agentDispatch(request("cancel_transfer_items"))&&s.cancels==1,"items cancel callback");s.ok.covered=true;check(!s.agentDispatch(request("review_transfer")),"covered review denied");
 TransferBaseState b;b.list.rows.push_back(row);check(b.agentUiState(u)&&u.screen=="transfer_destination"&&u.offers("choose_visible_row"),"destination screen");gates(b);check(b.agentDispatch(request("choose_visible_row","r0"))&&b.list.selected==1&&b.clicks==1,"destination selects observed mapped row");b.list.selectable=false;check(!b.agentDispatch(request("choose_visible_row","r0")),"failed native selection denied");b.list.selectable=true;for(auto h:{"r1","r8","r00","x0"})check(!b.agentDispatch(request("choose_visible_row",h)),"destination invalid handles");check(b.agentDispatch(request("cancel_transfer_destination"))&&b.cancels==1,"destination cancel");
 TransferConfirmState c;check(c.agentUiState(u)&&u.screen=="transfer_confirm"&&u.offers("confirm_transfer")&&u.offers("cancel_transfer_confirmation"),"confirm schema/actions");gates(c);check(c.agentDispatch(request("confirm_transfer"))&&c.oks==1,"confirm ordinary callback");check(c.agentDispatch(request("cancel_transfer_confirmation"))&&c.cancels==1,"confirm cancel callback");c.ok.covered=true;check(!c.agentDispatch(request("confirm_transfer")),"covered confirm denied");check(!c.agentDispatch(request("review_transfer")),"wrong-screen action denied");
 std::cout<<checks<<" actual provider/dispatch checks passed; synthetic paint/widgets, not final pixels or live MCP\n";
}
