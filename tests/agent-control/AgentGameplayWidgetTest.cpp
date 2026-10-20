// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/NumberText.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <cstdlib>
using namespace OpenXcom;
unsigned checks=0;void check(bool x,const char*m){++checks;if(!x){std::cerr<<m<<"\n";std::exit(1);}}
int main(){
 Logger::logFile()="/tmp/openxcom-v7-widget.log";check(SDL_Init(0)==0,"SDL no device");Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(const auto&f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Language lang;Surface canvas(320,200),parent(272,40,24,120),frame(272,40);Text label(200,20,10,13),acc(100,20,140,13),tu(80,20,210,13);
 for(Text*t:{&label,&acc,&tu}){t->initText(&big,&small,&lang);t->setBig();}
 label.setText("Snap Shot");acc.setText("Acc>65%");tu.setText("TUs>15");frame.blit(&parent);label.blit(&parent);acc.blit(&parent);tu.blit(&parent);parent.blit(&canvas);
 std::vector<Surface*> order={&frame,&label,&acc,&tu};
 check(Observer::agentCompositeTextJson(&label,272,40,order)!="null","full rendered mode label");
 check(Observer::agentCompositeTextJson(&tu,272,40,order)!="null","ordinary TU fits parent");
 tu.setText(std::string(100,'W'));tu.blit(&parent);check(Observer::agentCompositeTextJson(&tu,272,40,order)=="null","parent-clipped TU withheld");
 tu.setText("W");tu.setWidth(30);tu.setX(270);tu.blit(&parent);check(Observer::agentTextJson(&tu)!="null"&&Observer::agentCompositeTextJson(&tu,272,40,order)=="null","full child text still clipped by composite");
 tu.setX(210);tu.setWidth(80);tu.setText("TUs>15");tu.blit(&parent);label.setText(std::string(100,'W'));label.blit(&parent);check(Observer::agentCompositeTextJson(&label,272,40,order)=="null","long localized label withheld");
 label.setText("Snap Shot");label.blit(&parent);acc.setX(12);acc.blit(&parent);check(Observer::agentCompositeTextJson(&label,272,40,order)=="null","later column occludes earlier label");
 NumberText number(15,12,306,134);number.setValue(1);number.blit(&canvas);SDL_Rect ink;
 check(!Observer::agentWidgetVisible(&number),"stock padded allocation crosses right viewport");
 check(number.agentPaintedBounds(ink)&&Observer::agentRegionVisible({ink.x,ink.y,ink.w,ink.h}),"actual stock enemy digit fully visible");
 check(ink.w==3&&ink.h==5,"exact normal digit footprint");
 number.setX(318);number.blit(&canvas);check(number.agentPaintedBounds(ink)&&!Observer::agentRegionVisible({ink.x,ink.y,ink.w,ink.h}),"truly clipped digit refused");
 number.setX(306);number.setColor(255);number.blit(&canvas);check(!number.agentPaintedBounds(ink),"transparent digit refused");
 number.setColor(15);number.setValue(10);number.setX(304);number.blit(&canvas);check(number.agentPaintedBounds(ink)&&ink.w==7,"number10 footprint");
 Surface button(15,12,300,130);button.drawRect(0,0,15,12,32);button.blit(&canvas);NumberText above(15,12,306,121);above.setValue(2);above.blit(&canvas);
 std::vector<Observer::AgentUiRect> region={{300,130,15,12}};std::vector<Surface*> siblings={&button,&number,&above};
 check(Observer::agentRegionsUncovered(region,&number,siblings),"later padded number allocation alone does not cover enemy button");
 above.setY(130);above.blit(&canvas);check(!Observer::agentRegionsUncovered(region,&number,siblings),"actual later digits cover enemy button");
 check(Observer::agentPaintHealthy()&&!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"healthy offscreen render no GUI");
 std::cout<<"AgentGameplayWidgetTest PASS "<<checks<<" checks\n";
}
