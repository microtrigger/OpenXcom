// SPDX-License-Identifier: GPL-3.0-or-later
// Real font/Text/Frame offscreen projection; no Game, display or simulated AI.
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Frame.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentDisplayedMessages.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <iostream>
using namespace OpenXcom;
static unsigned checks=0;
static void check(bool b,const char *m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main()
{
 Logger::logFile()="/tmp/openxcom-v6-report-widget.log";
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"no-video initialization");
 Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(const auto &font:YAML::LoadFile("Language/Font.dat")["fonts"]){if(font["id"].as<std::string>()=="FONT_BIG")big.load(font);if(font["id"].as<std::string>()=="FONT_SMALL")small.load(font);}
 Language language;Surface canvas(320,200);Frame frame(261,122,34,10);frame.setHighContrast(true);frame.setThickness(9);frame.blit(&canvas);
 Text report(251,112,39,15);report.initText(&big,&small,&language);report.setBig();report.setAlign(ALIGN_CENTER);report.setVerticalAlign(ALIGN_MIDDLE);report.setWordWrap(true);report.setHighContrast(true);
 Surface overlay(250,112,39,15);std::vector<Surface*> order={&frame,&report,&overlay};overlay.setVisible(false);
 for(const std::string suffix:{"is under alien control","is under Molecular Control","has Panicked","has gone Berserk"})
 {
  const std::string text="MCP Test\n"+suffix;
  report.setText(text);check(Observer::agentTextJson(&report)=="null","dirty text withheld before draw");report.blit(&canvas);
  check(Observer::agentTextJson(&report)==Observer::agentJsonString(text),"stock report fits complete actual glyph layout");
  check(Observer::agentWidgetUncovered(&report,order),"ordinary frame-before-text order leaves report visible");
  Observer::AgentUiState ui;Observer::agentIncludeWidget(ui,&report);check(!ui.regions.empty()&&Observer::agentRegionsDisjoint(ui),"proven report ink footprint");
  overlay.setVisible(true);overlay.blit(&canvas);check(!Observer::agentWidgetUncovered(&report,order),"covering overlay prevents report admission");overlay.setVisible(false);
  report.setVisible(false);check(Observer::agentTextJson(&report)=="null","hidden report withheld");report.setVisible(true);report.blit(&canvas);
 }
 report.setText("MCP Test\nis under alien control\xf0\x9f\x8c\x8c");report.blit(&canvas);check(Observer::agentTextJson(&report)=="null","missing glyph not reconstructed");
 report.setText(std::string(1000,'W'));report.blit(&canvas);check(Observer::agentTextJson(&report)=="null","clipped text withheld");
 report.setText("MCP Test\nis under alien control");report.setX(-1);report.blit(&canvas);check(Observer::agentTextJson(&report)=="null","out-of-viewport report withheld");
 report.setX(39);report.blit(&canvas);check(Observer::agentTextJson(&report)!="null","restored actual draw recovers text");
 // InfoboxOK's exact message allocation and paint order. The OK button itself
 // is covered by final-object provider/action tests after the parent's build.
 Text title(255,61,33,51);title.initText(&big,&small,&language);title.setBig();title.setAlign(ALIGN_CENTER);title.setVerticalAlign(ALIGN_MIDDLE);title.setWordWrap(true);title.setHighContrast(true);
 title.setText("MCP Test\nhas Panicked");title.blit(&canvas);check(Observer::agentTextJson(&title)!="null","stock persistent report title fits");
 Surface button(120,18,100,112);button.blit(&canvas);order={&frame,&button,&title};check(Observer::agentWidgetUncovered(&button,order),"persistent title actual ink does not cover normal OK rectangle");
 title.setY(108);title.blit(&canvas);check(!Observer::agentWidgetUncovered(&button,order),"later moved message ink blocks OK");
 check(Observer::agentPaintHealthy(),"constituent draw health");check(!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"never created video/window");
 std::cout<<"AgentReportWidgetTest PASS "<<checks<<" checks\n";
}
