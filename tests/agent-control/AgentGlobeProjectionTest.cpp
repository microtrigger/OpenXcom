// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentGlobeProjection.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
unsigned checks=0;
void check(bool v,const char *why) { ++checks; if(!v){std::cerr<<why<<'\n';std::exit(1);} }
int main() {
    AgentGlobeProjection p; p.begin(512,400); check(p.samples.size()==800,"bounded grid");
    p.begin(513,400); check(p.samples.empty(),"overwidth closed");
    p.begin(32,32); check(p.samples.size()==4,"grid centers");
    check(!p.visible(p.samples[0]),"no shadow proof");
    p.shadow(8,8,true); check(p.visible(p.samples[0]),"final shader survivor");
    p.shadow(8,8,false); check(!p.visible(p.samples[0]),"final transparent clipped");
    p.shadow(9,8,true); check(!p.visible(p.samples[0]),"unlisted point cannot alias sample");
    p.shadow(8,8,true);
    int x[]={0,16,16,0}, y[]={0,0,16,16}; p.polygon(x,y,4,true);
    check(p.samples[0].textured && p.visible(p.samples[0]),"opaque primitive interior");
    check(!p.samples[1].textured,"outside stays untextured");
    p.line(1,0,7,16,7); check(!p.visible(p.samples[0]),"radar line exclusion");
    p.clearLayer(1); check(p.visible(p.samples[0]),"layer redraw clear");
    p.rect(2,7,7,2,2); p.rect(4,7,7,2,2); p.clearLayer(2);
    check(!p.visible(p.samples[0]),"independent marker exclusion");
    p.clearLayer(4); p.rect(8,0,0,16,16); check(!p.visible(p.samples[0]),"later sibling exclusion");
    p.clearLayer(8); check(p.visible(p.samples[0]),"sibling redraw clear");
    p.polygon(x,y,4,false); check(!p.visible(p.samples[0]),"nonopaque primitive uncertain");
    p.begin(32,32); p.shadow(8,8,true); int nearX[]={7,16,16,7}; p.polygon(nearX,y,4,true);
    check(!p.visible(p.samples[0]),"edge tolerance rejects raster ambiguity");
    p.begin(32,32); check(!p.samples[0].textured && !p.samples[0].sphere && !p.samples[0].covered,"draw reset loses proof");
    p.begin(25,25); p.shadow(24,24,true); check(p.visible(p.samples[3]),"nonmultiple viewport indexing");
    p.begin(8,8); check(p.samples.empty(),"no partial edge sample");
    std::cout<<"AgentGlobeProjectionTest PASS "<<checks<<" checks\n";
}
