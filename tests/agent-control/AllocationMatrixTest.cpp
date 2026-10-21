// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit domain seams. Current command bodies are included, not reimplemented.
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include "ProductionCommands.h"
namespace OpenXcom {
struct RuleManufacture { std::string category="STR_EQUIPMENT"; const std::string &getCategory()const{return category;} };
class SavedGame {}; class Mod {};
class Base;
class Production {
public:
    int assigned=0,total=1,produced=0; bool infinite=false,sell=false; RuleManufacture rule;
    int getAssignedEngineers()const{return assigned;} void setAssignedEngineers(int n){assigned=n;}
    int getAmountTotal()const{return total;} void setAmountTotal(int n){total=n;}
    int getAmountProduced()const{return produced;} bool getInfiniteAmount()const{return infinite;}
    void setInfiniteAmount(bool b){infinite=b;} const RuleManufacture *getRules()const{return &rule;}
    void setSellItems(bool b){sell=b;} void startItem(Base*,SavedGame*,const Mod*){}
};
class Base {
public:
    int engineers=0,scientists=0,workshops=0,labs=0,hangars=0,usedHangars=0;
    Production *production=nullptr; int *researchAssigned=nullptr;
    int getAvailableEngineers()const{return engineers;} int getEngineers()const{return engineers;}
    void setEngineers(int n){engineers=n;} int getFreeWorkshops()const{return workshops-(production?production->assigned:0);}
    int getAvailableScientists()const{return scientists;} int getScientists()const{return scientists;}
    void setScientists(int n){scientists=n;} int getFreeLaboratories()const{return labs-(researchAssigned?*researchAssigned:0);}
    int getAvailableHangars()const{return hangars;} int getUsedHangars()const{return usedHangars;}
    void removeProduction(Production*){}
};
struct ResearchProject { int assigned=0; int getAssigned()const{return assigned;} void setAssigned(int n){assigned=n;} };
class ResearchInfoState {
public:
    Base *_base; ResearchProject *_project; unsigned refresh=0;
    ResearchInfoState(Base*b,ResearchProject*p):_base(b),_project(p){}
    void setAssignedScientist(){++refresh;} void moreByValue(int); void lessByValue(int);
};
#include "Research.generated.inc"
}
#include "Production.generated.inc"
using namespace OpenXcom;
static unsigned checks=0;
static void check(bool ok,const char *name){++checks;if(!ok){std::cerr<<"FAIL "<<name<<'\n';std::exit(1);}}
int main(){
    const int personnel[]={0,1,7,50,100000};
    const int capacity[]={0,1,3,30,100000};
    const int requests[]={0,1,5,1000,100000,INT_MAX};
    for(int staff:personnel)for(int space:capacity)for(int request:requests){
        const int expected=std::min(std::min(staff,space),request);
        Base b; Production p; b.engineers=staff;b.workshops=space;b.production=&p;
        ProductionCommands::rawEngineers(b,p,request);
        check(p.assigned==expected && b.engineers==staff-expected,"engineers clamp/conserve");
        ProductionCommands::rawEngineers(b,p,INT_MAX);
        check(p.assigned==std::min(staff,space),"engineers maximum");
        ProductionCommands::rawEngineers(b,p,1);
        check(p.assigned==std::min(staff,space),"engineers beyond maximum no-op");
        ProductionCommands::rawEngineers(b,p,INT_MIN);
        check(p.assigned==0 && b.engineers==staff,"engineers clear/INT_MIN");
        ProductionCommands::rawEngineers(b,p,-1);
        check(p.assigned==0 && b.engineers==staff,"engineers below zero no-op");
        ResearchProject r; b.scientists=staff;b.labs=space;b.researchAssigned=&r.assigned;ResearchInfoState ui(&b,&r);
        ui.moreByValue(request);
        check(r.assigned==expected && b.scientists==staff-expected,"scientists clamp/conserve");
        ui.moreByValue(INT_MAX);ui.moreByValue(1);
        check(r.assigned==std::min(staff,space),"scientists maximum/over-limit");
        ui.lessByValue(INT_MAX);ui.lessByValue(1);
        check(r.assigned==0 && b.scientists==staff,"scientists clear/below zero");
        ui.moreByValue(-1);ui.lessByValue(-1);
        check(r.assigned==0 && b.scientists==staff,"scientists invalid negative ignored");
    }
    for(int staff:personnel)for(int space:capacity){
        Base b;Production p;b.engineers=staff;b.workshops=space;b.production=&p;
        for(int i=0;i<12;++i)ProductionCommands::rawEngineers(b,p,1);
        check(p.assigned==std::min(12,std::min(staff,space)),"repeated engineer clicks");
        ProductionCommands::quantity(b,p,ProductionControl::Engineers,QuantityOperation::Set,100000);
        check(p.assigned==std::min(staff,space),"engineer set clamps");
        ProductionCommands::quantity(b,p,ProductionControl::Engineers,QuantityOperation::Clear);
        check(p.assigned==0&&b.engineers==staff,"engineer quantity clear");
    }
    for(int produced: {0,3,1000})for(int requested: {0,1,7,1000,100000,INT_MAX}){
        Base b;Production p;p.produced=produced;p.total=produced+1;
        ProductionCommands::quantity(b,p,ProductionControl::Units,QuantityOperation::Set,requested);
        check(p.total==std::max(requested,produced+1)&&!p.infinite,"finite set and produced floor");
        ProductionCommands::rawUnits(b,p,INT_MAX);
        check(p.total==INT_MAX,"finite raw increase saturates without overflow");
        ProductionCommands::rawUnits(b,p,1);
        check(p.total==INT_MAX,"finite over-max no wrap");
        ProductionCommands::quantity(b,p,ProductionControl::Units,QuantityOperation::Maximum);
        check(p.infinite,"non-craft maximum infinite");
        ProductionCommands::quantity(b,p,ProductionControl::Units,QuantityOperation::Maximum);
        check(p.infinite,"repeat maximum stable");
        ProductionCommands::quantity(b,p,ProductionControl::Units,QuantityOperation::Clear);
        check(!p.infinite&&p.total==produced+1,"clear means next unit not zero");
        ProductionCommands::rawUnits(b,p,INT_MIN);
        check(p.total==produced+1,"decrease retains produced floor");
        auto old=p.total;ProductionCommands::quantity(b,p,ProductionControl::Units,QuantityOperation::Set,-1);
        check(p.total==old,"negative set ignored");
    }
    for(int free: {0,1,3,12}){
        Base b;Production p;p.rule.category="STR_CRAFT";b.hangars=free;
        auto r=ProductionCommands::quantity(b,p,ProductionControl::Units,QuantityOperation::Maximum);
        check(!p.infinite&&p.total==1+free,"craft maximum bounded finite");
        check((r.warning==ProductionWarning::NoFreeHangars)==(free==0),"craft no-space warning");
    }
    std::cout<<checks<<" current-source arithmetic assertions passed; domain seams, no UI/MCP/live acceptance\n";
}
