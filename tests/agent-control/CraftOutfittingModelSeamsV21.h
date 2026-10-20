// SPDX-License-Identifier: GPL-3.0-or-later
// Narrow ordinary-model and presentation seams for the frozen-function oracle.
#pragma once
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
namespace OpenXcom {
typedef unsigned char Uint8;
enum { SDL_BUTTON_LEFT=1, SDL_BUTTON_RIGHT=3, SDL_BUTTON_WHEELUP=4, SDL_BUTTON_WHEELDOWN=5, BT_AMMO=2 };
struct LocalizedText {
    std::string text;
    LocalizedText(const std::string &s=""):text(s){}
    template<class T> LocalizedText &arg(const T &value){std::ostringstream s;s<<value;text+="|"+s.str();return *this;}
    operator std::string()const{return text;}
};
inline std::ostream &operator<<(std::ostream &s,const LocalizedText &t){return s<<t.text;}
struct Text {std::string value;void setText(const std::string &s){value=s;}void setText(const LocalizedText &s){value=s.text;}};
struct TextList {
    size_t selected=0;std::map<size_t,Uint8> colors;std::map<std::pair<size_t,int>,std::string> cells;
    size_t getSelectedRow()const{return selected;}int getArrowsLeftEdge()const{return 100;}int getArrowsRightEdge()const{return 120;}
    Uint8 getColor()const{return 1;}Uint8 getSecondaryColor()const{return 2;}
    void setRowColor(size_t row,Uint8 color){colors[row]=color;}
    void setCellText(size_t row,int col,const std::string &s){cells[{row,col}]=s;}
};
struct Timer {int stops=0,interval=250;bool running=false;void stop(){++stops;running=false;}void start(){running=true;}bool isRunning()const{return running;}void setInterval(int n){interval=n;}};
struct Action {struct Details{struct Button{int button=SDL_BUTTON_LEFT;}button;}details;double x=20;double getAbsoluteXMouse()const{return x;}Details *getDetails(){return &details;}};
namespace Options {extern int changeValueByMouseWheel;}
struct RuleItem {std::string type="item";bool fixed=false;int clip=0,battle=0;std::vector<std::string> ammo;bool isFixed()const{return fixed;}int getClipSize()const{return clip;}const std::string &getType()const{return type;}std::vector<std::string>*getCompatibleAmmo(){return &ammo;}int getBattleType()const{return battle;}};
struct Armor {int size=2;int getSize()const{return size;}};
struct Unit {std::string armor="armor";const std::string &getArmor()const{return armor;}};
struct RuleCraft {int vehicles=4,soldiers=14,maxItems=0;int getVehicles()const{return vehicles;}int getSoldiers()const{return soldiers;}int getMaxItems()const{return maxItems;}};
class ItemContainer {
public:std::map<std::string,int> _qty;
    void addItem(const std::string &,int=1);void removeItem(const std::string &,int=1);int getItem(const std::string &)const;
};
class Vehicle {RuleItem *_rules;int _ammo,_size;
public:Vehicle(RuleItem *r,int a,int s):_rules(r),_ammo(a),_size(s){}RuleItem *getRules()const{return _rules;}int getAmmo()const{return _ammo;}int getSize()const{return _size;}};
class Base;
class Craft {
public:Base *base;RuleCraft rule;std::string name,status="STR_READY";ItemContainer items;std::vector<Vehicle*> vehicles;
    Craft(Base *b,const std::string &s):base(b),name(s){}~Craft(){for(auto *v:vehicles)delete v;}
    const std::string &getStatus()const{return status;}std::string getName(void*)const{return name;}
    int getSpaceAvailable()const;int getSpaceUsed()const;int getNumSoldiers()const;int getNumVehicles()const{return vehicles.size();}
    int getVehicleCount(const std::string &)const;RuleCraft *getRules(){return &rule;}ItemContainer *getItems(){return &items;}std::vector<Vehicle*> *getVehicles(){return &vehicles;}
};
class Soldier {public:Craft *craft=nullptr;int wound=0;Craft *getCraft()const{return craft;}void setCraft(Craft *c){craft=c;}int getWoundRecovery()const{return wound;}};
class Base {public:ItemContainer storage;std::vector<Craft*> crafts;std::vector<Soldier*> soldiers;ItemContainer *getStorageItems(){return &storage;}std::vector<Craft*> *getCrafts(){return &crafts;}std::vector<Soldier*> *getSoldiers(){return &soldiers;}};
class SavedGame {public:int months=0;int getMonthsPassed()const{return months;}};
struct Element {int color=0;};struct RuleInterface {Element elem;Element *getElement(const std::string &){return &elem;}};
class Mod {
public:RuleItem item,ammo,other;Armor armor;Unit unit;bool hasUnit=true;RuleInterface iface;
    Mod(){ammo.type="ammo";other.type="other";}
    RuleItem *getItem(const std::string &s,bool=false){return s=="item"?&item:s=="ammo"?&ammo:&other;}
    Unit *getUnit(const std::string &){return hasUnit?&unit:nullptr;}Armor *getArmor(const std::string &,bool){return &armor;}
    RuleInterface *getInterface(const std::string &){return &iface;}
};
struct ErrorMessageState {std::string message;ErrorMessageState(const LocalizedText &s,void*,int,const char*,int):message(s.text){}};
struct SoldierInfoState {size_t row;SoldierInfoState(Base*,size_t n):row(n){}};
struct Game {
    Mod mod;SavedGame save;unsigned pops=0;std::vector<size_t> inspected;std::vector<std::string> warnings;std::vector<int> warningTotals;int *total=nullptr;
    Mod *getMod(){return &mod;}SavedGame *getSavedGame(){return &save;}void *getLanguage(){return nullptr;}
    void pushState(ErrorMessageState *e){warnings.push_back(e->message);warningTotals.push_back(total?*total:-1);delete e;}
    void pushState(SoldierInfoState *s){inspected.push_back(s->row);delete s;}void popState(){++pops;}
};
struct UiCommon {Game *_game=nullptr;Base *_base=nullptr;size_t _craft=0;void *_palette=nullptr;
    LocalizedText tr(const std::string &s){return LocalizedText(s);}LocalizedText tr(const std::string &s,int n){return LocalizedText(s).arg(n);}
};
struct EquipmentUi:UiCommon {
    size_t _sel=0;std::vector<std::string> _items={"item"};int _totalItems=0;Uint8 _ammoColor=4;
    TextList list,*_lstEquipment=&list;Text available,used,*_txtAvailable=&available,*_txtUsed=&used;Timer left,right,*_timerLeft=&left,*_timerRight=&right;
};
struct CrewUi:UiCommon {Uint8 _otherCraftColor=3;TextList list,*_lstSoldiers=&list;Text available,used,*_txtAvailable=&available,*_txtUsed=&used;};
}
