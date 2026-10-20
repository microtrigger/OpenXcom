// SPDX-License-Identifier: GPL-3.0-or-later
// Same real renderer workload compiled with the agent feature both enabled and
// disabled. stdout is an exact pixel-byte inventory, not a text classifier.
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Engine/Palette.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Action.h"
#include "../../src/Interface/Text.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
using namespace OpenXcom;
static void require(bool yes){if(!yes)std::exit(2);}
static void emit(Surface&s){SDL_Surface*p=s.getSurface();require(SDL_LockSurface(p)==0);for(int y=0;y<p->h;++y)std::cout.write(static_cast<const char*>(p->pixels)+y*p->pitch,p->w);SDL_UnlockSurface(p);}
int main(int argc,char**argv){require(argc==2&&SDL_Init(0)==0&&!SDL_GetVideoSurface());Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}Options::baseXResolution=320;Options::baseYResolution=200;for(std::string master:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");Palette pal;pal.loadDat(std::string("../")+(master=="xcom1"?"UFO":"TFTD")+"/GEODATA/PALETTES.DAT",256,Palette::palOffset(master=="xcom1"?3:4));Surface canvas(320,200);require(SDL_SetColorKey(canvas.getSurface(),0,0)==0);canvas.setPalette(pal.getColors());for(bool large:{false,true})for(bool scroll:{false,true}){Text text(197,81,11,29);text.initText(&big,&small,&lang);text.setPalette(pal.getColors());text.setColor(2);text.setWordWrap(true);text.setScrollable(scroll);if(large)text.setBig();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
text.agentEnableVisibleLines();
#endif
for(std::string content:{std::string(),std::string("Visible line\nSecond line"),std::string("gjpqy gjpqy gjpqy\nAMWAMW\nAlpha\1 Beta\2Small text"),std::string(350,'W')}){text.setText(content);for(int step=0;step<24;++step){SDL_Event e={};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=step<12?SDL_BUTTON_WHEELDOWN:SDL_BUTTON_WHEELUP;Action a(&e,1,1,0,0);text.mousePress(&a,nullptr);canvas.clear(40);text.blit(&canvas);emit(text);emit(canvas);}}}}require(!SDL_GetVideoSurface());}
