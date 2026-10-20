// SPDX-License-Identifier: GPL-3.0-or-later
// Real UnitSprite, BattleUnit and Armor offscreen fixture, all synthetic frames.
// No Game, window, FOV, Map/AI, combat, saves, retail assets or RNG reseeding.
#include "../../src/Battlescape/UnitSprite.h"
#include "../../src/Engine/SurfaceSet.h"
#include "../../src/Engine/Options.h"
#include "../../src/Mod/Armor.h"
#include "../../src/Mod/Unit.h"
#include "../../src/Savegame/BattleUnit.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <iostream>
using namespace OpenXcom;using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool b,const char*m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(){
 Options::battleHairBleach=false;
 SDL_Color palette[256];for(int n=0;n<256;++n)palette[n]={Uint8(n),Uint8(n),Uint8(n),0};
 SurfaceSet frames(32,40),hands(32,40);
 for(int n=0;n<320;++n){frames.addFrame(n)->setPixel(2,2,3);hands.addFrame(n);}
 frames.setPalette(palette);hands.setPalette(palette);
 for(int routine:{0,10,13,14,15})for(bool helmet:{false,true}){
  Armor armor("SYNTHETIC");YAML::Node config;config["drawingRoutine"]=routine;armor.load(config);
  Unit rule("SYNTHETIC");BattleUnit unit(&rule,FACTION_PLAYER,1,&armor,nullptr,helmet?1:0);
  UnitSprite sprite(64,40,0,0,helmet);Surface cache(64,40);sprite.setPalette(palette);cache.setPalette(palette);
  auto draw=[&](){sprite.setBattleUnit(&unit);sprite.setSurfaces(&frames,&hands,&hands);cache.clear();sprite.blit(&cache);cache.agentCopyCollapsePaint(sprite);};
  draw();check(!sprite.agentCollapsePaint()&&!cache.agentCollapsePaint(),"standing ordinary draw is unmarked");
  unit.startFalling();draw();bool reviewed=routine!=15;
  check(sprite.agentCollapsePaint()==reviewed&&cache.agentCollapsePaint()==reviewed,"actual collapse branch transfers only reviewed routine");
  check(cache.getPixel(18,2)==3,"actual synthetic collapse torso copied at ordinary offset");
  unit.keepFalling();draw();check(cache.agentCollapsePaint()==reviewed,"next ordinary collapse frame marked");
  unit.keepFalling();unit.keepFalling();draw();check(!cache.agentCollapsePaint(),"out/corpse path clears cached marker");
 }
 check(agentPaintHealthy(),"all actual UnitSprite blits healthy");
 std::cout<<"AgentCollapseSpriteTest PASS "<<checks<<" checks\n";return 0;
}
