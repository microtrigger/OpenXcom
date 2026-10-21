// SPDX-License-Identifier: GPL-3.0-or-later
// Offscreen stock-font test of constructor-extracted HUD geometry, not live gameplay.
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
// Linked ComboBox code references modal ownership; no combo is created here.
namespace OpenXcom { void State::setModal(InteractiveSurface *) { std::abort(); } }
using namespace OpenXcom;
unsigned checks=0;
void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL "<<why<<"\n";std::exit(1);}}
int main(int argc,char **argv){
 check(argc==2,"asset root supplied");
 check(SDL_Init(0)==0,"SDL without video");
 Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small,geoBig,geoSmall;
 for(const auto &n:YAML::LoadFile("Language/Font.dat")["fonts"]){
  auto id=n["id"].as<std::string>();
  if(id=="FONT_BIG")big.load(n);if(id=="FONT_SMALL")small.load(n);
  if(id=="FONT_GEO_BIG")geoBig.load(n);if(id=="FONT_GEO_SMALL")geoSmall.load(n);
 }
 Language language;
 for(bool funds:{false,true})for(int game:{0,1})for(int scale:{1,2}){
  const int screenWidth=320*scale,screenHeight=200*scale;
  Options::baseXResolution=screenWidth;Options::baseYResolution=screenHeight;
  Surface canvas(screenWidth,screenHeight);std::vector<Surface*> order;
  // Use the real UFO/TFTD background, not a solid-color stand-in: its baked
  // old frames must move too. Never write the original asset.
  std::ifstream input(std::string(argv[1])+(game?"/TFTD/GEOGRAPH/GEOBORD.SCR":"/UFO/GEOGRAPH/GEOBORD.SCR"),std::ios::binary);
  std::vector<unsigned char> original(64000);
  check(bool(input.read(reinterpret_cast<char*>(original.data()),original.size())),"original 320x200 GEOBORD available");
  Surface sidebar(64,200,screenWidth-64,screenHeight/2-100);Surface *_sidebar=&sidebar;
  for(int y=0;y<200;++y)for(int x=0;x<64;++x)sidebar.setPixel(x,y,original[y*320+256+x]);
  #include "CalendarBackground.generated.inc"
  for(int y=0;y<200;++y)for(int x=0;x<64;++x){
   int row=y;
   if(y==71)row=72;
   else if(y>=72&&y<115)row=108;
   else if(y==115)row=109;else if(y==116)row=110;
   else if(y>=117&&y<156)row=111;
   check(sidebar.getPixel(x,y)==original[row*320+256+x],"themed background reflow and untouched icons/top");
  }
  sidebar.blit(&canvas);
  #include "CalendarGeometry.generated.inc"
  Text *fields[]={_txtHour,_txtHourSep,_txtMin,_txtMinSep,_txtSec,_txtWeekday,_txtDay,_txtMonth,_txtYear,_txtFunds};
  TextButton *buttons[]={_btn5Secs,_btn1Min,_btn5Mins,_btn30Mins,_btn1Hour,_btn1Day};
  const char *labels[]={"5 Secs","1 Min","5 Mins","30 Mins","1 Hour","1 Day"};
  for(auto *t:fields){t->initText(&big,&small,&language);t->setColor(game?1:244);}
  _txtHour->setBig();_txtHour->setAlign(ALIGN_RIGHT);_txtHourSep->setBig();_txtHourSep->setText(":");
  _txtMin->setBig();_txtMinSep->setBig();_txtMinSep->setText(":");
  for(auto *t:{_txtWeekday,_txtDay,_txtMonth,_txtYear,_txtFunds})t->setAlign(ALIGN_CENTER);
  if(funds){
   #include "CalendarFunds.generated.inc"
  }
  _txtFunds->setVisible(funds);_txtFunds->setText("$999999");
  _txtHour->setText("23");_txtMin->setText("59");_txtSec->setText("59");_txtYear->setText(game?"2040":"1999");
  for(auto *t:fields)order.push_back(t);
  for(int i=0;i<6;++i){auto *b=buttons[i];b->initText(&geoBig,&geoSmall,&language);b->setBig();b->setColor(game?2:246);b->setTFTDMode(game);b->setGeoscapeButton(true);b->setText(labels[i]);order.push_back(b);}
  // All months, mixed-case descenders, and Cyrillic weekday examples. These
  // are text samples, not a claim to have tested every translated language.
  for(const char *month:{"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"})
  for(const char *weekday:{"WEDNESDAY","Friday","Суббота"}){
   _txtWeekday->setText(weekday);_txtDay->setText("23rd");_txtMonth->setText(month);
   for(auto *surface:order)surface->blit(&canvas);
   for(auto *t:fields){if(t==_txtFunds&&!funds)continue;
    if(!t->agentTextComplete())std::cerr<<"field="<<t->getText()<<" x="<<t->getX()<<" y="<<t->getY()<<" w="<<t->getWidth()<<" h="<<t->getHeight()<<" funds="<<funds<<"\n";
    check(t->agentTextComplete(),"entire calendar/clock text fits own viewport");
    if(!Observer::agentWidgetUncovered(t,order))std::cerr<<"covered field="<<t->getText()<<" funds="<<funds<<"\n";
    check(Observer::agentWidgetUncovered(t,order),"calendar/clock ink not overwritten by later widgets");
   }
   for(int i=0;i<6;++i){
    check(Observer::agentButtonLabel(buttons[i])==labels[i],"resized speed label remains complete");
    check(buttons[i]->getY()+buttons[i]->getHeight()<=screenHeight/2+56,"speed button stays above fixed zoom artwork");
   }
  }
  // Negative control: observer guards must still reject genuinely clipped ink.
  _txtMonth->setHeight(8);_txtMonth->setText("May");_txtMonth->blit(&canvas);
  check(!_txtMonth->agentTextComplete(),"ninth-row descender clipping remains fail closed");
  for(auto *t:fields)delete t;for(auto *b:buttons)delete b;
 }
 check(SDL_GetVideoSurface()==nullptr,"no live display claimed");
 std::cout<<"AgentCalendarLayoutTest PASS "<<checks<<" checks\n";
}
