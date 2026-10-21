// SPDX-License-Identifier: GPL-3.0-or-later
// Actual offscreen TextList/font capture, no Game/window or gameplay fixture.
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <iostream>
using namespace OpenXcom;
namespace OpenXcom { Game *State::_game=nullptr; bool Game::isState(State *) const { std::abort(); } void State::setModal(InteractiveSurface *) { std::abort(); } }
unsigned checks=0;
void check(bool good,const char *why){++checks;if(!good){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}}
int main(){
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen initialization");
 Options::baseXResolution=320;Options::baseYResolution=200;
 FileMap::load("test-fonts",".",true);
 Font big,small;for(const auto &f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Language lang;Surface canvas(320,200),bg(320,200);SDL_Color pal[256]={};for(int i=0;i<256;++i)pal[i].r=pal[i].g=pal[i].b=i;canvas.setPalette(pal);bg.setPalette(pal);bg.drawRect(0,0,320,200,40);
 TextList list(288,64,8,78);list.initText(&big,&small,&lang);list.setPalette(pal);list.setColumns(4,86,70,80,46);list.setSelectable(true);list.setBackground(&bg);list.setMargin(6);list.setColor(20);list.setSecondaryColor(30);
 list.addRow(4,"INTERCEPTOR","READY","BASE","2/0/0");
 list.addRow(4,"SKYRANGER","REARMING","BASE","0/14/0");
 list.blit(&canvas);std::vector<AgentListRow> rows;check(list.agentRows(rows)&&rows.size()==2,"actual four-column stockfont capture");
 Observer::AgentUiState ui;Observer::AgentListProjection projection;std::vector<Surface*> paint={&list};
 check(Observer::agentProjectInterceptList(&list,&list,paint,ui,projection,{}),"dedicated capture helper");check(ui.rows.size()==2,"two actual captured rows admitted");check(projection.rows.find("INTERCEPTOR")!=std::string::npos,"actual visible craft label");check(list.agentSelectProjectRow(ui.rows[1]),"native row selection");check(list.getSelectedRow()==1,"ordinary logical row selected");
 list.blit(&canvas);
 Observer::AgentUiState legacy;Observer::AgentListProjection lp;check(Observer::agentProjectList(&list,&list,paint,4,true,legacy,lp,{},true),"legacy projection returns");check(legacy.rows.empty(),"legacy fourcolumn authority remains denied");
 list.setCellText(0,0,"CHANGED");check(!list.agentRows(rows),"changed unpainted row rejected");list.blit(&canvas);ui=Observer::AgentUiState();check(Observer::agentProjectInterceptList(&list,&list,paint,ui,projection,{})&&projection.rows.find("CHANGED")!=std::string::npos,"repaint reflects actual changed label");
 ui=Observer::AgentUiState();check(Observer::agentProjectInterceptList(&list,&list,paint,ui,projection,[](const Observer::AgentUiState&){return false;})&&ui.rows.empty(),"external overlay forbids rows");
 Surface cover(288,64,8,78);cover.setPalette(pal);paint.push_back(&cover);ui=Observer::AgentUiState();check(Observer::agentProjectInterceptList(&list,&list,paint,ui,projection,{})&&ui.rows.empty(),"later surface forbids rows");
 check(Observer::agentPaintHealthy(),"actual paints healthy");check(!SDL_GetVideoSurface(),"no live game or window");std::cout<<"PASS "<<checks<<" real offscreen interception list checks\n";
}
