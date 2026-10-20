// SPDX-License-Identifier: GPL-3.0-or-later
// Pure synthetic actual-write records and final-pixel proof callback. This does
// not exercise SDL, Map, FOV, scaling, flip, game rules, AI or a real renderer.
#include "../../src/Observer/AgentProjectileWitness.h"
#include <cstdlib>
#include <iostream>
#include <climits>
using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool b,const char*m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(){
 typedef AgentProjectileFact Fact;
 AgentProjectileWitness::Palette palette;for(unsigned i=0;i<256;++i)palette[i]=i*0x010101;
 const AgentProjectileRect viewport={0,0,8,6};
 AgentProjectileWitness w;
 unsigned reads=0;
 const auto proof=[&](int x,int y,std::uint32_t rgb){++reads;check(x>=0&&x<8&&y>=0&&y<6,"proof visits only bounded candidate coordinates");check(rgb<=0xffffff,"proof only RGB");return true;};
 auto observe=[&](const std::vector<AgentProjectileRect>&covers=std::vector<AgentProjectileRect>()){return w.evaluate(true,viewport,covers,palette,proof);};
 check(observe()==Fact::Unknown,"no begun draw is unknown");
 for(const auto&p:std::vector<std::pair<int,int>>{{0,1},{1,0},{-1,2},{2,-1},{641,400},{640,401},{INT_MAX,INT_MAX}})
  check(!w.begin(p.first,p.second)&&w.storageCells()==0&&observe()==Fact::Unknown,"invalid extent never allocates or proves absence");
 check(w.begin(640,400)&&w.storageCells()==256000,"maximum storage bounded");
 check(w.begin(8,6)&&w.storageCells()==48,"ordinary bounded draw");
 check(observe()==Fact::Unknown,"unsealed draw unknown");w.seal(true);
 check(observe()==Fact::KnownAbsent&&reads==0,"no candidate does not scan final image");
 w.begin(8,6);w.wrote(3,2,1,2,true);w.seal(true);reads=0;
 check(observe()==Fact::Presented&&reads==1,"one changed body contribution proved");
 check(observe()==Fact::Presented,"cached unchanged draw remains provable");
 check(w.evaluate(false,viewport,{},palette,proof)==Fact::Unknown,"failed final flip or unsupported backend unknown");
 check(w.evaluate(true,viewport,{},palette,AgentProjectileWitness::FinalPixelProof())==Fact::Unknown,"missing final proof unknown");
 check(w.evaluate(true,viewport,{},palette,[](int,int,std::uint32_t){return false;})==Fact::Unknown,"final mismatch is not inferred absence");
 reads=0;check(observe({{3,2,1,1}})==Fact::KnownAbsent&&reads==0,"later covered candidate removed without reading pixels");
 check(observe()==Fact::Presented,"temporary overlay evaluation does not mutate cached Map witness");
 check(observe({{0,0,8,6}})==Fact::Unknown,"whole viewport cover unknown");
 check(observe({{0,0,4,6},{4,0,4,6}})==Fact::Unknown,"union whole viewport cover unknown");
 check(observe({{-1,-1,10,8}})==Fact::Unknown,"outside-extending whole cover unknown");
 check(observe({{INT_MIN,INT_MIN,INT_MAX,INT_MAX}})==Fact::Presented,"coverage arithmetic cannot overflow into witness");
 check(observe({{0,0,0,1}})==Fact::Unknown,"invalid cover fail closed");
 check(observe(std::vector<AgentProjectileRect>(129,{0,0,1,1}))==Fact::Unknown,"coverage count bound");
 check(w.evaluate(true,{0,0,2,2},{},palette,proof)==Fact::KnownAbsent,"candidate outside admitted viewport omitted");
 for(const auto&r:std::vector<AgentProjectileRect>{{-1,0,8,6},{0,-1,8,6},{0,0,0,6},{1,0,8,6},{0,1,8,6},{INT_MAX,0,1,1}})
  check(w.evaluate(true,r,{},palette,proof)==Fact::Unknown,"bad viewport cannot authorize a write");
 auto aliases=palette;aliases[2]=aliases[1];reads=0;
 check(w.evaluate(true,viewport,{},aliases,proof)==Fact::KnownAbsent&&reads==0,"palette aliases create no visible contribution");
 aliases[2]=0x1000000;check(w.evaluate(true,viewport,{},aliases,proof)==Fact::Unknown,"invalid RGB fails closed");
 w.begin(8,6);w.wrote(3,2,1,1,true);w.seal(true);check(observe()==Fact::KnownAbsent,"same palette index never creates contribution");
 w.begin(8,6);w.wrote(3,2,1,2,true);w.wrote(3,2,2,2,true);w.seal(true);check(observe()==Fact::Presented,"identical later body write retains actual earlier contribution");
 w.begin(8,6);w.wrote(3,2,1,2,true);w.wrote(3,2,2,1,true);w.seal(true);check(observe()==Fact::KnownAbsent,"body sequence restoring original RGB adds no net contribution");
 for(unsigned after:{2u,3u}){
  w.begin(8,6);w.wrote(3,2,1,2,true);w.wrote(3,2,2,after,false);w.seal(true);check(observe()==Fact::KnownAbsent,"later ordinary same/different write retires witness");
 }
 w.begin(8,6);w.wrote(3,2,1,2,true);w.wrote(3,2,2,3,false);w.wrote(3,2,3,4,true);w.seal(true);check(observe()==Fact::Presented,"new body contribution after ordinary write can be witnessed");
 w.begin(8,6);w.wrote(3,2,1,0,true);w.seal(true);check(observe()==Fact::KnownAbsent,"color-key zero cannot contribute to Map copy");
 w.begin(8,6);w.wrote(3,2,0,2,true);w.seal(true);check(observe()==Fact::Unknown,"transparent predecessor has unproven background");
 w.begin(8,6);w.wrote(3,2,1,2,true);w.wrote(3,2,9,3,true);w.seal(true);check(observe()==Fact::Unknown,"unrecorded intermediate write fails closed");
 w.begin(8,6);w.wrote(3,2,1,2,true);w.invalidate();w.seal(true);check(observe()==Fact::Unknown,"unknown writer sticky until new draw");
 w.begin(8,6);w.wrote(3,2,1,2,true);w.seal(false);check(observe()==Fact::Unknown,"failed draw is never absence");
 w.begin(8,6);w.wrote(3,2,1,2,true);w.seal(true);w.wrote(3,2,2,3,false);check(observe()==Fact::Unknown,"late write invalidates completed draw");
 for(const auto&p:std::vector<std::pair<int,int>>{{-1,0},{8,0},{0,-1},{0,6},{INT_MAX,INT_MIN}}){
  w.begin(8,6);w.wrote(p.first,p.second,1,2,true);w.seal(true);check(observe()==Fact::Unknown,"invalid write coordinates are not silently accepted");
 }
 // Transparent source is omitted by the ordinary writer, not fabricated as a
 // write in this API. Its lack of callback preserves an earlier contribution.
 w.begin(8,6);w.wrote(3,2,1,2,true);w.seal(true);check(observe()==Fact::Presented,"no transparent-source record erases opaque predecessor");
 // Every pixel is independent; partial clipping/occlusion does not introduce a
 // source/draw-call ID or an original full sprite box.
 for(int y=0;y<6;++y)for(int x=0;x<8;++x){
  w.begin(8,6);w.wrote(x,y,1,2,true);w.seal(true);check(observe()==Fact::Presented,"every bounded pixel address admitted");
  check(observe({{x,y,1,1}})==Fact::KnownAbsent,"each candidate can be independently occluded");
 }
 unsigned char buffer[96]={};AgentProjectileRaster raster;
 check(raster.begin(w,buffer,8,6,16),"padded raster binds bounded span");raster.body(true);raster.wrote(buffer+2*16+3,1,2);w.seal(true);check(observe()==Fact::Presented,"padded address maps to correct pixel");
 for(int pitch:{0,7,4097})check(!raster.begin(w,buffer,8,6,pitch)&&observe()==Fact::Unknown,"bad raster pitch rejected");
 check(!raster.begin(w,nullptr,8,6,16),"null raster pixels refused");
 raster.begin(w,buffer,8,6,16);raster.body(true);raster.wrote(buffer+8,1,2);w.seal(true);check(observe()==Fact::Unknown,"padding byte never becomes pixel");
 raster.begin(w,buffer,8,6,16);raster.body(true);raster.wrote(buffer+96,1,2);w.seal(true);check(observe()==Fact::Unknown,"past span rejected before indexing");
 raster.begin(w,buffer,8,6,16);raster.body(true);raster.wrote(buffer+3,1,2);raster.unknownWrite();w.seal(true);check(observe()==Fact::Unknown,"unknown raster writer sticky");
 AgentProjectileEpisodes episodes;
 check(!episodes.observe(Fact::Unknown),"unknown start emits nothing");
 check(episodes.observe(Fact::Presented),"first presented episode emits");
 for(int i=0;i<100;++i)check(!episodes.observe(Fact::Presented),"identical/moving presence does not count attacks");
 check(!episodes.observe(Fact::Unknown)&&!episodes.observe(Fact::Presented),"pause/unknown cannot manufacture reappearance");
 check(!episodes.observe(Fact::KnownAbsent)&&episodes.observe(Fact::Presented),"proven absent then reappearance emits");
 check(!episodes.observe(Fact::KnownAbsent)&&!episodes.observe(Fact::KnownAbsent),"repeated absence emits nothing");
 check(!episodes.observe(Fact::Unknown)&&episodes.observe(Fact::Presented),"unknown preserves prior absent status");
 std::cout<<"AgentProjectileWitnessTest PASS "<<checks<<" checks\n";
}
