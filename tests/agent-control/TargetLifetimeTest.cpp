// Actual Target layout, implicit copying and extracted constructor/destructor.
// Followers/domain behavior is an explicit seam; no game or transport.
#include "../../src/Savegame/Target.h"
#include "../../src/Observer/AgentUi.h"
#include <cstdlib>
#include <iostream>
#include <new>
using namespace OpenXcom;
using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
static std::weak_ptr<const void> dying;
static int callbacks=0;
namespace OpenXcom {
class Craft {public:void returnToBase(){check(dying.expired(),"token expires before follower callback");++callbacks;}};
static std::vector<Craft*> followers;
std::vector<Craft*> Target::getCraftFollowers()const{return followers;}
#include "TargetLifetime.generated.inc"
void Target::load(const YAML::Node&){} YAML::Node Target::save()const{return YAML::Node();}
std::string Target::getName(Language*)const{return _name;}
std::string Target::getDefaultName(Language*)const{return "";}
std::string Target::getMarkerName()const{return "";}
int Target::getMarkerId()const{return 0;}
}
class RealTarget:public Target {public:std::string getType()const override{return "TEST";}int getMarker()const override{return 0;}void values(){_lon=1.25;_lat=2.5;_id=77;_name="native";}bool unchanged()const{return _lon==1.25&&_lat==2.5&&_id==77&&_name=="native";}};
static bool same(std::weak_ptr<const void>a,std::weak_ptr<const void>b){return !a.owner_before(b)&&!b.owner_before(a);}
int main(){
 RealTarget a;a.values();auto first=a.agentLifetime();RealTarget b=a;check(b.unchanged()&&!same(first,b.agentLifetime()),"actual copy native values fresh identity");
 auto prior=b.agentLifetime();b=a;check(prior.expired()&&b.unchanged()&&!same(first,b.agentLifetime()),"actual assignment invalidates old identity preserves native values");
 auto self=b.agentLifetime();b=b;check(self.expired()&&b.unchanged(),"self assignment freshness without native mutation");
 AgentUiState u,v;check(u.sameBinding(v),"empty bindings legacy parity");u.targetLifetimes={first};v.targetLifetimes={first};check(u.sameBinding(v),"same control block");v.targetLifetimes={b.agentLifetime()};check(!u.sameBinding(v),"different control blocks");
 alignas(RealTarget) unsigned char storage[sizeof(RealTarget)];auto*p=new(storage)RealTarget;auto old=p->agentLifetime();AgentUiState cached;cached.targetLifetimes={old};p->~RealTarget();p=new(storage)RealTarget;AgentUiState fresh;fresh.targetLifetimes={p->agentLifetime()};check(old.expired()&&!cached.sameBinding(fresh),"same-address ABA refuses cached grant");p->~RealTarget();
 {RealTarget unrelated;auto other=unrelated.agentLifetime();(void)other;}v=u;check(u.sameBinding(v),"unrelated destruction no global invalidation");
 Craft follower;followers={&follower};{RealTarget end;dying=end.agentLifetime();}followers.clear();check(callbacks==1&&dying.expired(),"destructor callback ordering");
 std::cout<<"PASS "<<checks<<" actual Target lifetime checks; follower seam\n";
}
