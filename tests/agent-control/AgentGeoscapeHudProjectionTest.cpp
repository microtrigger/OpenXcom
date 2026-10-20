// SPDX-License-Identifier: GPL-3.0-or-later
// Actual stock-font regression. No Game, window, or video subsystem.
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/Text.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace OpenXcom;
namespace {
unsigned checks=0;
void check(bool value,const char *message){++checks;if(!value){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}}
}
int main(int argc,char **argv)
{
    check(argc==2,"fixture output directory supplied");
    check(SDL_Init(0)==0 && SDL_GetVideoSurface()==nullptr,"eventless SDL, no display");
    Options::baseXResolution=320;Options::baseYResolution=200;
    Font big,small;
    for(const auto &font:YAML::LoadFile("Language/Font.dat")["fonts"]){
        if(font["id"].as<std::string>()=="FONT_BIG")big.load(font);
        if(font["id"].as<std::string>()=="FONT_SMALL")small.load(font);
    }
    Language language;Surface canvas(320,200);SDL_Rect ink={};
    check(small.getHeight()==9 && small.getSpacing()==-1,"stock negative-spacing nine-pixel font");
    check(small.agentGlyphInk('0',ink) && ink.y+ink.h==8,"small digit actual ink fits eight rows");
    check(big.agentGlyphInk('1',ink) && ink.y+ink.h==13,"big digit actual ink ends before next HUD row");
    check(!small.agentGlyphInk(0x1f30c,ink),"missing original glyph has no ink proof");
    Text second(11,8,307,80),day(29,8,259,94),month(29,8,288,94),year(59,8,259,101);
    Text *dates[]={&second,&day,&month,&year};const char *dateValues[]={"05","1st","Jan","1999"};
    for(unsigned i=0;i<4;++i){dates[i]->initText(&big,&small,&language);dates[i]->setText(dateValues[i]);if(i)dates[i]->setAlign(ALIGN_CENTER);dates[i]->blit(&canvas);
        check(dates[i]->agentTextComplete(),"actual stock date/sec glyph ink fits 8px allocation");
        check(Observer::agentTextJson(dates[i])==Observer::agentJsonString(dateValues[i]),"complete 8px date/sec original text projects");
        check(dates[i]->agentPaintedBounds(ink) && ink.h<=8,"date/sec recorded bounds exclude ninth-row padding");
    }
    Text hour(20,16,259,74),minute(20,16,283,74),weekday(59,8,259,87);
    hour.initText(&big,&small,&language);hour.setBig();hour.setAlign(ALIGN_RIGHT);hour.setText("12");
    minute.initText(&big,&small,&language);minute.setBig();minute.setText("34");
    weekday.initText(&big,&small,&language);weekday.setAlign(ALIGN_CENTER);weekday.setText("FRIDAY");
    std::vector<Surface*> order={&hour,&minute,&weekday};for(auto *field:order)field->blit(&canvas);
    check(Observer::agentWidgetUncovered(&hour,order),"big hour is not covered by adjacent weekday ink");
    check(Observer::agentWidgetUncovered(&minute,order),"big minute is not covered by adjacent weekday ink");
    check(Observer::agentTextJson(&hour)=="\"12\"" && Observer::agentTextJson(&minute)=="\"34\"","complete big clock values project");
    Observer::AgentUiState regions;for(auto *field:order)Observer::agentIncludeWidget(regions,field);
    check(Observer::agentRegionsDisjoint(regions),"clock and adjacent weekday actual ink regions are disjoint");
    weekday.setY(86);weekday.blit(&canvas);
    check(!Observer::agentWidgetUncovered(&hour,order),"genuine overlapping later text still blocks hour");
    weekday.setY(87);weekday.setText("Friday");weekday.blit(&canvas);
    check(!weekday.agentTextComplete() && Observer::agentTextJson(&weekday)=="null","genuinely clipped stock descender remains suppressed");
    Text clipped(10,7,0,0);clipped.initText(&big,&small,&language);clipped.setText("0");clipped.blit(&canvas);
    check(!clipped.agentTextComplete() && Observer::agentTextJson(&clipped)=="null","nonzero glyph ink clipped at bottom remains suppressed");
    Text missing(80,10,0,20);missing.initText(&big,&small,&language);missing.setText("A\xF0\x9F\x8C\x8C");missing.blit(&canvas);
    check(!missing.agentTextComplete() && Observer::agentTextJson(&missing)=="null","missing original glyph cannot disclose substituted source string");
    Text tooWide(4,9,0,40);tooWide.initText(&big,&small,&language);tooWide.setText("W");tooWide.blit(&canvas);
    check(!tooWide.agentTextComplete() && Observer::agentTextJson(&tooWide)=="null","genuine horizontal ink clipping remains suppressed");

    // A real mapped printable glyph with an empty atlas cell must fail closed,
    // even when another glyph in the same string has valid visible ink.
    SDL_Surface *atlas=SDL_CreateRGBSurface(SDL_SWSURFACE,16,9,8,0,0,0,0);check(atlas!=nullptr,"blank-glyph test atlas");
    SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
    check(SDL_SetColors(atlas,palette,0,256)==1 && SDL_FillRect(atlas,nullptr,0)==0,"clear glyph atlas");
    SDL_Rect marker={9,1,3,6};check(SDL_FillRect(atlas,&marker,5)==0,"visible fallback cell beside blank A");
    const std::string atlasPath=std::string(argv[1])+"/Language/AgentBlankGlyph.bmp";
    check(SDL_SaveBMP(atlas,atlasPath.c_str())==0,"write generated glyph fixture");SDL_FreeSurface(atlas);
    FileMap::load("glyph-regression",argv[1],true);
    Font blank;blank.load(YAML::Load("width: 8\nheight: 9\nspacing: -1\nmonospace: true\nimages:\n  - file: AgentBlankGlyph.bmp\n    chars: 'A?'\n"));
    check(blank.agentHasGlyph('A') && !blank.agentGlyphInk('A',ink),"mapped blank printable has no ink proof");
    check(blank.agentGlyphInk('?',ink),"nonblank glyph in generated atlas has ink proof");
    Text blankText(40,10,0,60);blankText.initText(&blank,&blank,&language);blankText.setText("?A");blankText.blit(&canvas);
    check(!blankText.agentTextComplete() && Observer::agentTextJson(&blankText)=="null","mixed visible and blank mapped text cannot leak blank codepoint");
    check(Observer::agentPaintHealthy(),"off-screen paint operations succeeded");
    check(SDL_GetVideoSurface()==nullptr && !(SDL_WasInit(SDL_INIT_VIDEO)),"test never initialized a GUI");
    std::cout<<"AgentGeoscapeHudProjectionTest PASS "<<checks<<" checks\n";
}
