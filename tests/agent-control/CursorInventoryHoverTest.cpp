// SPDX-License-Identifier: GPL-3.0-or-later
// Source-extracted Inventory mouseOver/movePointer. Explicit lookup/render/domain seams;
// proves routing and held-item invariance, not full inventory or live cursor admission.
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
using std::floor;
namespace OpenXcom {
struct Item {int slot=3,x=2,y=1,quantity=7;};
using BattleItem=Item;
struct State {int hover=0;};
struct Action {double x,y;double getAbsoluteXMouse(){return x;}double getAbsoluteYMouse(){return y;}};
struct InteractiveSurface {void mouseOver(Action*,State*s){++s->hover;}int getX(){return 7;}int getY(){return 11;}};
const int INV_GROUND=2;
struct RuleInventory {int type=INV_GROUND;int getType(){return type;}};
struct Selection {int x=0,y=0;int getWidth(){return 32;}int getHeight(){return 48;}void setX(int n){x=n;}void setY(int n){y=n;}};
struct Unit {Item item;int tu=50,calls=0;Item*getItem(RuleInventory*,int,int){++calls;return &item;}};
struct Inventory:InteractiveSurface {Selection selection;Selection*_selection=&selection;Unit unit;Unit*_selUnit=&unit;Item held;Item*_selItem=&held,*_mouseOverItem=nullptr;RuleInventory slot;int _groundOffset=4;std::uint64_t _agentInventoryRevision=0;int lookups=0;RuleInventory*getSlotInPosition(int*x,int*y){++lookups;return *x>=0&&*y>=0&&*x<320&&*y<200?&slot:nullptr;}void setMouseOverItem(Item*i){_mouseOverItem=i;}bool movePointer(int,int);void mouseOver(Action*,State*);};
#include "CursorInventoryHover.generated.inc"
}
static int checks=0;static void check(bool b){++checks;if(!b)std::exit(1);}
int main(){using namespace OpenXcom;Inventory i;State s;for(int x:{0,16,160,319,400})for(int y:{0,24,100,199,300}){Action a{double(x+7)+.75,double(y+11)+.25};int calls=s.hover;auto rev=i._agentInventoryRevision;i.mouseOver(&a,&s);check(s.hover==calls+1);check(i._selItem==&i.held&&i.held.slot==3&&i.held.x==2&&i.held.y==1&&i.held.quantity==7&&i.unit.tu==50);check(i.selection.x==x-16&&i.selection.y==y-24);check(i._agentInventoryRevision==rev+1);check(i._mouseOverItem==((x<320&&y<200)?&i.unit.item:nullptr));}i._selUnit=nullptr;int calls=s.hover;Action a{10,20};i.mouseOver(&a,&s);check(s.hover==calls);check(i._selItem==&i.held);std::cout<<checks<<" extracted inventory hover checks passed; lookup/render/domain seams, no live acceptance\n";}
