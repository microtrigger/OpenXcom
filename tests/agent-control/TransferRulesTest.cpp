// SPDX-License-Identifier: GPL-3.0-or-later
// Actual callbacks extracted by runner; explicit domain/UI seams, NOT live proof.
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "fmath.h"
namespace OpenXcom {
namespace Options { bool storageLimitsEnforced=true, canTransferCraftsWhileAirborne=false; }
struct Action {}; struct Target {}; class Base; class Craft;
struct RuleItem {
    std::string id; double size; bool alien=false;
    RuleItem(std::string i,double s):id(i),size(s){}
    bool isAlien()const{return alien;} double getSize()const{return size;}
    const std::string &getType()const{return id;}
};
struct InterfaceElement { int color=0; };
struct RuleInterface { InterfaceElement element; InterfaceElement *getElement(const char*){return &element;} };
struct Mod { RuleInterface iface; RuleInterface *getInterface(const char*){return &iface;} };
struct ItemContainer {
    std::map<std::string,int> counts;
    void removeItem(const std::string &id,int n){counts[id]-=n;}
    double getTotalSize(Mod*)const{return 0;}
};
struct Soldier { void setPsiTraining(bool){} Craft *getCraft(){return nullptr;} };
struct Craft {
    ItemContainer items; int getNumSoldiers(){return 0;} ItemContainer *getItems(){return &items;}
    std::string getStatus(){return "STR_READY";} Target *getDestination(){return nullptr;}
    Base *getBase(){return nullptr;} void setBase(Base*,bool){} int getFuel(){return 100;}
    int getFuelLimit(Base*){return 0;} void setLowFuel(bool){} void returnToBase(){}
};
enum TransferType { TRANSFER_ITEM, TRANSFER_CRAFT, TRANSFER_SOLDIER, TRANSFER_SCIENTIST, TRANSFER_ENGINEER };
struct TransferRow { TransferType type; void *rule; std::string name; int cost,qtySrc,qtyDst,amount; };
struct Transfer {
    int hours,amount=0; std::string item;
    explicit Transfer(int h):hours(h){} void setItems(const std::string &id,int n){item=id;amount=n;}
    void setSoldier(Soldier*){} void setCraft(Craft*){} void setScientists(int){} void setEngineers(int){}
};
struct Base : Target {
    int capacity=100,scientists=0,engineers=0; double used=0; ItemContainer storage;
    std::vector<Soldier*> soldiers; std::vector<Craft*> crafts; std::vector<Transfer*> transfers;
    ~Base(){for(auto t:transfers)delete t;}
    int getAvailableStores(){return capacity;} double getUsedStores(){return used;} bool storesOverfull(double offset=0);
    int getAvailableQuarters(){return 100;} int getUsedQuarters(){return 0;}
    int getAvailableHangars(){return 10;} int getUsedHangars(){return 0;}
    int getAvailableContainment(){return 10;} int getUsedContainment(){return 0;}
    std::vector<Soldier*> *getSoldiers(){return &soldiers;} std::vector<Craft*> *getCrafts(){return &crafts;}
    std::vector<Transfer*> *getTransfers(){return &transfers;} ItemContainer *getStorageItems(){return &storage;}
    void removeCraft(Craft*,bool){} int getScientists(){return scientists;} void setScientists(int n){scientists=n;}
    int getEngineers(){return engineers;} void setEngineers(int n){engineers=n;}
};
struct SavedGame { long long funds=100000; long long getFunds(){return funds;} void setFunds(long long f){funds=f;} };
struct ErrorMessageState { std::string message; ErrorMessageState(std::string m,int,int,const char*,int):message(m){} };
struct Game {
    SavedGame save; Mod mod; int pops=0; std::vector<std::string> errors;
    SavedGame *getSavedGame(){return &save;} Mod *getMod(){return &mod;}
    void popState(){++pops;} void pushState(ErrorMessageState *e){errors.push_back(e->message);delete e;}
};
struct Timer { int stops=0; void stop(){++stops;} };
struct TransferItemsState {
    Base *_baseFrom,*_baseTo; Game *_game; Timer timer; Timer *_timerInc=&timer;
    int _palette=0,_total=0,_pQty=0,_cQty=0,_aQty=0; double _iQty=0,_distance=100;
    std::vector<TransferRow> _items; size_t sel=0; int refreshes=0;
    TransferItemsState(Base &f,Base &t,Game &g):_baseFrom(&f),_baseTo(&t),_game(&g){}
    TransferRow &getRow(){return _items.at(sel);} std::string tr(const char *s){return s;}
    void updateItemStrings(){++refreshes;} void increaseByValue(int); void decreaseByValue(int);
    void completeTransfer(); void btnCancelClick(Action*);
    void add(RuleItem &r,int stock,int cost=3){_items.push_back({TRANSFER_ITEM,&r,r.id,cost,stock,0,0});_baseFrom->storage.counts[r.id]=stock;}
};
#include "Stores.generated.inc"
#include "Transfer.generated.inc"
}
using namespace OpenXcom;
static int checks=0;
static void check(bool ok,const char *message){++checks;if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
int main(){
    // Source capacity intentionally irrelevant to this outbound controller.
    for(bool enforced:{false,true}) for(double sourceUsed:{0.,100.,150.})
    for(double destUsed:{0.,90.,100.,110.}) for(double size:{0.,0.5,1.,5.})
    for(int request:{0,1,3,1000,INT_MAX}) {
        Options::storageLimitsEnforced=enforced;
        Base source,dest; source.used=sourceUsed;dest.used=destUsed;
        Game game; RuleItem item("ITEM",size); TransferItemsState s(source,dest,game);s.add(item,37);
        s.increaseByValue(request);
        int expected=0;
        if(request>0 && destUsed+size<=100.) expected=std::min(request,std::min(37,size==0?INT_MAX:static_cast<int>((100.-destUsed+0.05)/size)));
        check(s.getRow().amount==expected,"selection capacity/source clamp");
        check(s._total==3*expected && AreSame(s._iQty,size*expected),"draft totals");
        check(source.storage.counts["ITEM"]==37 && dest.transfers.empty() && game.save.funds==100000,"draft no mutation");
        const bool rejected=request>0 && destUsed+size>100.;
        check(game.errors.size()==(rejected?1u:0u),"actual capacity error");
        if(rejected)check(game.errors.back()=="STR_NOT_ENOUGH_STORE_SPACE" && s.timer.stops==1,"visible error reason and timer stop");
        s.decreaseByValue(1);
        check(s.getRow().amount==std::max(0,expected-1),"decrease one");
        s.decreaseByValue(INT_MAX);
        check(s.getRow().amount==0 && s._total==0 && AreSame(s._iQty,0.),"clear clamps at zero");
        s.increaseByValue(request); s.btnCancelClick(nullptr);
        check(game.pops==2 && game.save.funds==100000 && dest.transfers.empty() && source.storage.counts["ITEM"]==37,"cancel no committed mutation");
    }
    // Combined rows compete for destination capacity; completion commits exactly the draft.
    {Base source,dest;source.used=150.;dest.used=96.;Game g; RuleItem a("A",.5),b("B",1.);
     TransferItemsState s(source,dest,g);s.add(a,20);s.add(b,20,7);
     s.increaseByValue(4);s.sel=1;s.increaseByValue(INT_MAX);
     check(s._items[0].amount==4 && s._items[1].amount==2 && s._total==26,"shared capacity across rows");
     s.increaseByValue(1);check(g.errors.back()=="STR_NOT_ENOUGH_STORE_SPACE","beyond shared capacity rejected");
     s.completeTransfer();check(g.save.funds==99974,"single commit debits once");
     check(source.storage.counts["A"]==16 && source.storage.counts["B"]==18,"commit removes selected source quantities");
     check(dest.transfers.size()==2 && dest.transfers[0]->item=="A" && dest.transfers[0]->amount==4 && dest.transfers[1]->item=="B" && dest.transfers[1]->amount==2,"commit preserves transit payload");
     check(dest.transfers[0]->hours==16 && dest.storage.counts.empty(),"arrival delayed, not immediate destination inventory");
    }
    // A genuinely large selection, not only a large request clamped to a tiny stock.
    {Base a,b;b.capacity=20000;Game g;RuleItem r("BULK",1);TransferItemsState s(a,b,g);s.add(r,10000,2);
     s.increaseByValue(1);s.increaseByValue(9);s.increaseByValue(9990);
     check(s.getRow().amount==10000 && s._total==20000 && s._iQty==10000.,"large repeated increments");
     s.increaseByValue(INT_MAX);check(s.getRow().amount==10000,"source exhausted clamps extra request");
     s.decreaseByValue(9997);check(s.getRow().amount==3 && s._total==6,"large decrease exact");
     s.decreaseByValue(INT_MAX);check(s.getRow().amount==0 && s._total==0,"large clear exact");}
    // Exact centi-unit truncation in current Base rule (not a naive floating point > comparison).
    {Base b;b.used=100.009;check(!b.storesOverfull(),"centi truncation below next centi");b.used=100.02;check(b.storesOverfull(),"overfull beyond centi boundary");}
    // Invalid/non-positive commands leave a populated draft untouched.
    {Base a,b;Game g;RuleItem r("R",1);TransferItemsState s(a,b,g);s.add(r,7);s.increaseByValue(3);
     s.increaseByValue(-1);s.decreaseByValue(0);s.decreaseByValue(-1);
     check(s.getRow().amount==3 && s._total==9,"nonpositive commands no-op");}
    std::cout<<checks<<" transfer callback/domain-seam checks passed; not game, UI or MCP live acceptance\n";
}
