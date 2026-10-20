// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentUi.h"
#include "../../src/Battlescape/CameraProjection.h"
#include <iostream>
#include <cstdlib>
using namespace OpenXcom;
unsigned checks=0;void check(bool x,const char*m){++checks;if(!x){std::cerr<<m<<"\n";std::exit(1);}}
int main(){
 for(int width:{320,640})for(int height:{144,344})for(int z=0;z<4;++z)for(int x=1;x<15;++x)for(int y=1;y<15;++y){
  CameraProjectionMath p(32,40,0,0,z);int sx,sy;p.mapToScreen(x,y,z,&sx,&sy);
  p.offsetX=width/2-sx;p.offsetY=height/2-sy;
  AgentMapSnapshot s=cameraMapSnapshot(p,0,0,width,height,false,"");int tx,ty;
  p.screenToMapUnclamped(s.focus.x,s.focus.y+10,&tx,&ty);
  check(tx==x&&ty==y,"center focus is ordinary selector inverse including level");
 }
 Observer::AgentUiState a;a.screen="battlescape";a.visible="{public}";a.controlVisible="{controls}";
 a.points={{16,20},{32,28}};a.context={0,12,4};a.rows={2};a.actions.emplace_back("map_primary","Primary map activation","visible_control_semantics","grid_cell");
 auto b=a;check(a.sameBinding(b),"idle repeat stable");b.visible="{animated public frame}";check(a.sameBinding(b),"observation-only animation independent");
 b=a;b.context[1]++;check(!a.sameBinding(b),"camera ABA generation denies old grant");b=a;b.context[2]++;check(!a.sameBinding(b),"actor pose ABA denies old grant");
 b=a;b.rows[0]=3;check(!a.sameBinding(b),"same numbered slot new mapping denied");b=a;b.points[0].first++;check(!a.sameBinding(b),"grid geometry changes denied");
 b=a;b.context[0]++;check(!a.sameBinding(b),"weapon targeting context changes denied");
 std::cout<<"AgentGameplayTest PASS "<<checks<<" checks\n";
}
