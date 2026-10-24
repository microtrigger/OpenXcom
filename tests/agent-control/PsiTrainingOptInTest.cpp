// SPDX-License-Identifier: GPL-3.0-or-later
// Actual constructor/provider/callback bodies; explicit Game/Mod/Craft seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include <map>
#include <vector>
// Test-only access to actual private native cells; production headers unchanged.
#define private public
#include "../../src/Interface/TextList.h"
#undef private
#include <sstream>
#include "../../src/Interface/Window.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <iostream>
#include <cstdlib>
using namespace OpenXcom;
using namespace OpenXcom::Observer;
namespace OpenXcom {
#include "GameIdentity.generated.inc"
Game *State::_game = nullptr;
State::State():_screen(true),_modal(nullptr),_ruleInterface(nullptr),_ruleInterfaceParent(nullptr){}
State::~State(){}
void State::init(){} void State::handle(Action*){} void State::blit(){}
void State::resize(int&,int&){} void State::recenter(int,int){}
#include "StateBase.generated.inc"
}

int main(int argc,char **argv){
 if(argc!=2||SDL_Init(0)!=0)return 2;
 FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
 Surface canvas(320,200),background(320,200);canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 unsigned checks=0;auto check=[&](bool ok,const char*why){++checks;if(!ok){std::cerr<<"FAIL "<<why<<"\n";std::exit(1);}};
 for(const std::string master:{"xcom1","xcom2"}){
   Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");
   TextList list(290,112,8,52);list.setPalette(palette);list.initText(&big,&small,&language);list.setColumns(4,114,80,62,30);list.setSelectable(true);list.setBackground(&background);list.setMargin(2);
   const std::string unknown=language.getString("STR_UNKNOWN"),no=language.getString("STR_NO");
   list.addRow(4,"VISIBLE NAME",unknown.c_str(),"0/+0",no.c_str());list.blit(&canvas);
   std::vector<AgentOutfittingRow> rows;
   check(!list.agentPsiTrainingRows(rows)&&rows.empty(),"valid four-column native shape default off refuses capture");
   list.agentEnablePsiTrainingCells();
   check(!list.agentPsiTrainingRows(rows)&&rows.empty(),"opting in still requires new native paint");
   list.blit(&canvas);
   check(list.agentPsiTrainingRows(rows)&&rows.size()==1&&rows[0].cells.size()==4,"same object opt-in plus paint captures exactly four cells");
   const std::vector<std::string> expected={"VISIBLE NAME",unknown,"0/+0",no};
   for(size_t c=0;c<4;++c)check(rows[0].cells[c].known&&rows[0].cells[c].text==expected[c],"same unchanged native cell exact logical text");
   check(!SDL_GetVideoSurface(),"no game video");
 }
 std::cout<<"PASS "<<checks<<" paired four-column opt-in checks; actual native TextList\n";
}
