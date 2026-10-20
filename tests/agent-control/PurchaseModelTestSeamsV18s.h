// Explicit test model seams. No simulation or hidden-state projection.
#pragma once
#include <string>
#include <vector>
namespace Purchases {
enum {BT_NONE,BT_AMMO}; enum TransferType{TRANSFER_SOLDIER,TRANSFER_SCIENTIST,TRANSFER_ENGINEER,TRANSFER_CRAFT,TRANSFER_ITEM};
struct RuleItem { double size=1; int time=24; double getSize()const{return size;} int getBattleType()const{return BT_NONE;} int getClipSize()const{return 0;} int getTransferTime()const{return time;} std::string getType()const{return "ITEM";} };
struct RuleSoldier { int time=24; int getTransferTime()const{return time;} std::string getType()const{return "SOLDIER";} };
struct RuleCraft { int time=24; int getTransferTime()const{return time;} std::string getType()const{return "CRAFT";} };
struct Soldier {};
struct Base; struct Craft {int id;Craft(RuleCraft*,Base*,int value):id(value){} void setStatus(const char*){} };
struct Transfer {int amount=0,hours,craftId=0;TransferType type=TRANSFER_ITEM;std::string item; explicit Transfer(int time):hours(time){} void setSoldier(Soldier*){type=TRANSFER_SOLDIER;amount=1;}void setScientists(int n){type=TRANSFER_SCIENTIST;amount=n;}void setEngineers(int n){type=TRANSFER_ENGINEER;amount=n;}void setCraft(Craft*c){type=TRANSFER_CRAFT;amount=1;craftId=c->id;delete c;}void setItems(const std::string&id,int n){type=TRANSFER_ITEM;item=id;amount=n;} };
struct TransferRow {TransferType type;void *rule;std::string name;int cost,qtySrc,qtyDst,amount;};
struct RuleInterface { int color=1; RuleInterface *getElement(const std::string&){return this;} };
struct SavedGame {long long funds=6000000;long long getFunds()const{return funds;}void setFunds(long long n){funds=n;}int nextId=1;int getId(const std::string&){return nextId++;} };
struct Mod {RuleInterface ui;unsigned generated=0;int getPersonnelTime()const{return 24;}Soldier *genSoldier(SavedGame*,const std::string&){++generated;static Soldier s;return &s;}RuleInterface *getInterface(const std::string&){return &ui;} };
struct Base {int quarters=50,usedQuarters=10,hangars=2,usedHangars=2;double stores=100,usedStores=10;std::vector<Transfer*>transfers;
 int getAvailableQuarters()const{return quarters;}int getUsedQuarters()const{return usedQuarters;}int getAvailableHangars()const{return hangars;}int getUsedHangars()const{return usedHangars;}double getAvailableStores()const{return stores;}double getUsedStores()const{return usedStores;}bool storesOverfull(double offset);std::vector<Transfer*>*getTransfers(){return &transfers;}~Base(){for(auto p:transfers)delete p;} };
}
