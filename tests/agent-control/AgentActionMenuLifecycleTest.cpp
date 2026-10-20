// SPDX-License-Identifier: GPL-3.0-or-later
// Actual ActionMenuItem methods with only constructor Game lookups supplied by
// the runner's explicit test-only font/language/style dependency shim.
#include "../../src/Battlescape/ActionMenuItem.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Mod/RuleInterface.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <cstdlib>
namespace OpenXcom { Font *agentTestBig, *agentTestSmall; Language *agentTestLanguage; Element agentTestStyle; }
using namespace OpenXcom;
static unsigned checks=0;
static void check(bool yes,const char *message) { ++checks;if(!yes){std::cerr<<message<<"\n";std::exit(1);} }
int main()
{
 Logger::logFile()="/tmp/openxcom-v7-action-menu-lifecycle.log";
 check(SDL_Init(0)==0,"offscreen SDL only");Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(const auto &f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Language language;agentTestBig=&big;agentTestSmall=&small;agentTestLanguage=&language;
 agentTestStyle=Element{0,0,0,0,16,48,32,false};Surface canvas(320,200);
 std::string label,acc,tu;ActionMenuItem row(0,nullptr,0,120);
 row.setAction(BA_SNAPSHOT,"Snap Shot","Acc>21%","TUs>12",12);
 check(!row.agentRow(label,acc,tu),"unpainted dirty row withheld");
 row.blit(&canvas);check(row.agentContextVisible(),"actual composite completed dirty lifecycle");
 check(row.agentRow(label,acc,tu)&&label=="Snap Shot"&&acc!="null"&&tu!="null","actual painted composite admitted");
 row.blit(&canvas);check(row.agentRow(label,acc,tu),"idle reblit stays admitted");
 row.setAction(BA_AIMEDSHOT,"Aimed Shot","Acc>40%","TUs>40",40);
 check(!row.agentRow(label,acc,tu),"new row content dirty until ordinary paint");
 row.blit(&canvas);check(row.agentRow(label,acc,tu)&&label=="Aimed Shot","new content painted normally");
 row.invalidate();check(!row.agentRow(label,acc,tu),"later invalidation revokes admission");
 row.blit(&canvas);check(row.agentRow(label,acc,tu),"repaint restores admission");
 row.setVisible(false);check(!row.agentRow(label,acc,tu),"invisible row withheld");row.setVisible(true);
 row.setHidden(true);check(!row.agentRow(label,acc,tu),"hidden row withheld");row.setHidden(false);
 row.setX(49);check(!row.agentRow(label,acc,tu),"viewport-clipped composite withheld");row.setX(24);
 row.getCrop()->w=100;row.getCrop()->h=40;check(!row.agentRow(label,acc,tu),"cropped composite withheld");row.resetCrop();
 row.setAction(BA_SNAPSHOT,std::string(100,'W'),"Acc>21%","TUs>12",12);row.blit(&canvas);
 check(!row.agentRow(label,acc,tu),"actually clipped label withheld");
 row.setAction(BA_SNAPSHOT,"Snap Shot","Acc>21%",std::string(100,'W'),12);row.blit(&canvas);
 check(row.agentRow(label,acc,tu)&&tu=="null","clipped optional TU remains null");
 Surface cover(272,40,24,120);cover.blit(&canvas);std::vector<Surface*> order={&row,&cover};
 check(!Observer::agentWidgetUncovered(&row,order),"later covering surface still blocks row");
 check(!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"no window or video device");
 row.invalidate();Observer::agentObservePaint(-1);row.blit(&canvas);
 check(!row.agentContextVisible()&&!row.agentRow(label,acc,tu),"failed paint health cannot complete dirty row");
 std::cout<<"AgentActionMenuLifecycleTest PASS "<<checks<<" checks\n";return 0;
}
