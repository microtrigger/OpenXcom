// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Engine/Palette.h"
#include "../../src/Engine/Options.h"
#include "../../src/Interface/TextButton.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <cstdlib>
using namespace OpenXcom;
static void require(bool ok){if(!ok)std::exit(2);}static void emit(Surface&s){auto*p=s.getSurface();require(SDL_LockSurface(p)==0);for(int y=0;y<p->h;++y)std::cout.write(static_cast<const char*>(p->pixels)+y*p->pitch,p->w);SDL_UnlockSurface(p);}
int main(int argc,char**argv){require(argc==2&&SDL_Init(0)==0&&!SDL_GetVideoSurface());Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}Options::baseXResolution=320;Options::baseYResolution=200;for(std::string master:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");Palette pal;pal.loadDat(std::string("../")+(master=="xcom1"?"UFO":"TFTD")+"/GEODATA/PALETTES.DAT",256,Palette::palOffset(1));Surface canvas(320,200);require(SDL_SetColorKey(canvas.getSurface(),0,0)==0);canvas.setPalette(pal.getColors());for(bool large:{false,true})for(bool contrast:{false,true}){TextButton button(128,12,192,188);button.initText(&big,&small,&lang);button.setPalette(pal.getColors());button.setColor(160);button.setHighContrast(contrast);if(large)button.setBig();
#if defined(OPENXCOM_EXPERIMENTAL_AGENT_CONTROL) && defined(OPENXCOM_NAMED_BUTTON_TEST)
button.agentEnableNamedControl();
#endif
TextButton*group=nullptr;button.setGroup(&group);for(std::string key:{"STR_GEOSCAPE_UC","STR_BASE_INFORMATION","STR_PURCHASE_RECRUIT","STR_SOLDIERS_UC"})for(int variant=0;variant<8;++variant){button.setText(lang.getString(key));button.setWidth(variant%2?127:128);button.setX(variant%2?193:192);button.setVisible(true);button.setHidden(false);if(variant&2)button.selectRadio();else group=nullptr;button.setGeoscapeButton(variant&4);button.invalidate();canvas.clear(40);button.blit(&canvas);emit(button);emit(canvas);}}}require(!SDL_GetVideoSurface());}
