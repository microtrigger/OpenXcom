// SPDX-License-Identifier: GPL-3.0-or-later
// Current drawn report cells; no game/save/models. Inert state linker seams only.
#include "Engine/State.h"
#include "Engine/Game.h"
#include "Engine/Font.h"
#include "Engine/Language.h"
#include "Engine/Unicode.h"
#include <algorithm>
#include "Engine/Options.h"
#include "Engine/FileMap.h"
#include "Interface/Text.h"
#include "Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <cstdlib>
#define private public
#include "Interface/TextList.h"
#undef private
using namespace OpenXcom;
namespace OpenXcom {Game* State::_game=nullptr;bool Game::isState(State*)const{return false;}void State::setModal(InteractiveSurface*s){_modal=s;}}
unsigned checks=0;void check(bool ok,const char*why){++checks;if(!ok){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}}
int main(){
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");FileMap::load("report","./",false);Options::baseXResolution=320;Options::baseYResolution=200;Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}Language lang;Surface canvas(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);
 for(unsigned cols:{2,3,5}){
 TextList list(290,16,10,20);list.setPalette(palette);list.initText(&big,&small,&lang);list.setMargin(0);list.agentEnableReportCells(cols);
 if(cols==2)list.setColumns(2,254,64);else if(cols==3)list.setColumns(3,224,30,64);else list.setColumns(5,130,60,30,30,40);
 list.setDot(cols!=5);list.setFlooding(cols==5);
 for(int i=0;i<4;++i){if(cols==2)list.addRow(2,"Current report","123");else if(cols==3)list.addRow(3,"Current report","\x01" "2\x01","123");else if(i==1)list.addRow(5,"Current soldier","","","","");else list.addRow(5,"Current soldier","Rookie","1","2","3");}
 std::vector<AgentOutfittingRow> rows;std::uint64_t epoch=99;
 check(!list.agentReportRows(rows,epoch)&&rows.empty()&&epoch==0,"neverpaint refused");list.blit(&canvas);check(list.agentReportRows(rows,epoch)&&epoch>0&&rows.size()==2,"only two full drawn rows");
 for(const auto&r:rows){check(r.cells.size()==cols,"explicit report arity");for(unsigned c=0;c<cols;++c){std::string shown=list._texts[r.logicalIndex][c]->getText();shown.erase(std::remove(shown.begin(),shown.end(),Unicode::TOK_COLOR_FLIP),shown.end());check(r.cells[c].known&&r.cells[c].text==shown,"actual child text/dots, only nonvisible color toggle removed");}}
 check(rows.back().logicalIndex==1,"offscreen rows not emitted");if(cols==5)check(list._texts[0][0]->getWidth()==340&&rows[0].cells[0].known,"flooding actual allocation not nominal width");else check(rows[0].cells.back().regions[0].w==36,"clipped last allocation logical caption retained");
 list.draw();check(!list.agentReportRows(rows,epoch),"new draw before destination repaint refused");list.blit(&canvas);check(list.agentReportRows(rows,epoch),"destination repaint commits draw epoch");
 auto firstEpoch=epoch;list.setCellText(0,0,"Visible changed");check(!list.agentReportRows(rows,epoch),"dirty cell refused");list.blit(&canvas);check(list.agentReportRows(rows,epoch)&&epoch>firstEpoch&&rows[0].cells[0].text==list._texts[0][0]->getText(),"new draw epoch and current caption");
 Text*cell=list._texts[0][0];int x=cell->getX();cell->setX(x+1);cell->setX(x);check(!list.agentReportRows(rows,epoch),"geometry ABA refused");list.draw();list.blit(&canvas);check(list.agentReportRows(rows,epoch),"repaint recovers");
 list.setCellText(0,cols-1,std::string(100,'W'));list.blit(&canvas);check(list.agentReportRows(rows,epoch)&&rows[0].cells.back().known&&rows[0].cells.back().text==std::string(100,'W'),"active partial horizontal allocation retains logical caption not glyph-fit claim");
 Text*last=list._texts[0][cols-1];int lastX=last->getX();last->setX(320);list.draw();list.blit(&canvas);check(list.agentReportRows(rows,epoch)&&!rows[0].cells.back().known&&rows[0].cells.back().text.empty(),"wholly horizontal-offscreen current cell withheld");last->setX(lastX);list.draw();list.blit(&canvas);
 SDL_FillRect(cell->getSurface(),nullptr,0);check(list.agentReportRows(rows,epoch)&&rows[0].cells[0].known,"ink erasure not logical-caption rejection");
 cell->setVisible(false);check(!list.agentReportRows(rows,epoch),"hidden child stale refuses");cell->setVisible(true);list.draw();list.blit(&canvas);check(list.agentReportRows(rows,epoch),"restore redraw recovers");
 list.setVisible(false);check(!list.agentReportRows(rows,epoch),"hidden list refused");list.setVisible(true);list.blit(&canvas);
 SDL_Rect clip={10,24,290,12};SDL_SetClipRect(canvas.getSurface(),&clip);list.blit(&canvas);{bool admitted=list.agentReportRows(rows,epoch);check((!admitted&&rows.empty())||(rows.size()==1&&rows[0].logicalIndex==1),"partial vertical row withheld or destination refused");};SDL_SetClipRect(canvas.getSurface(),nullptr);list.blit(&canvas);
 Surface cover(6,8,12,20);cover.setPalette(palette);cover.clear(50);list._agentPaintOrder.push_back(&cover);check(list.agentReportRows(rows,epoch)&&!rows[0].cells[0].known,"real later overlay nulls affected cell");list.blit(&canvas);
 list.scrollTo(2);list.blit(&canvas);check(list.agentReportRows(rows,epoch)&&rows.size()==2&&rows[0].logicalIndex==2,"only current scrolled rows");
 list._agentReportDrawEpoch=UINT64_MAX;list.draw();list.blit(&canvas);check(!list.agentReportRows(rows,epoch)&&epoch==0,"epoch exhaustion sticky refusal");
 }
 check(!SDL_GetVideoSurface(),"no game/video");std::cout<<"PASS "<<checks<<" current drawn report cell checks\n";
}
