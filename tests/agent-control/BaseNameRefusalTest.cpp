// SPDX-License-Identifier: GPL-3.0-or-later
// Actual provider/dispatch/applyNameChange; synthetic owning-state and inert linker seams.
#include "Engine/Action.h"
#include "Engine/State.h"
#include "Engine/Game.h"
// Inert linker seams; no state, timer loop, or modal focus is used here.
namespace OpenXcom { Game *State::_game=nullptr; bool Game::isState(State*)const{return false;} void State::setModal(InteractiveSurface *surface){_modal=surface;} }
#include "Engine/Font.h"
#include "Engine/Language.h"
#include "Engine/Options.h"
#include "Engine/FileMap.h"
#include "Engine/Surface.h"
#include "Engine/Unicode.h"
#include "Interface/TextEdit.h"
#include "Interface/Text.h"
#include "Interface/TextButton.h"
#include "Observer/AgentUi.h"
#include "Observer/AgentProtocol.h"
#include "Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
using namespace OpenXcom;
using namespace OpenXcom::Observer;
namespace Fixture {
struct Base {std::string name;int changes=0;void setName(const std::string &value){name=value;++changes;}};
struct BaseNameState {
 TextEdit *_edtName;Text *_txtTitle;TextButton *_btnOk;Base *_base;InteractiveSurface *_modal=nullptr;std::vector<Surface*>_surfaces;int confirms=0;
 bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void applyNameChange(bool);void btnOkClick(Action*){++confirms;}
};
#include "BaseName.generated.inc"
}
unsigned checks=0;void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}}
int main(){
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;Options::keyboardMode=KEYBOARD_ON;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Language language;Surface canvas(320,200);TextEdit edit(nullptr,127,16,59,94);edit.initText(&big,&small,&language);edit.setBig();edit.setFocus(true,false);Text title(182,17,37,70);title.initText(&big,&small,&language);title.setBig();title.setText("Base Name");TextButton ok(162,12,47,118);ok.initText(&big,&small,&language);ok.setText("OK");ok.setVisible(false);
 Fixture::Base base;Fixture::BaseNameState state;state._edtName=&edit;state._txtTitle=&title;state._btnOk=&ok;state._base=&base;state._surfaces={&title,&ok,&edit};auto paint=[&](){for(auto*w:state._surfaces)w->blit(&canvas);};paint();AgentUiState ui;AgentRequest req;req.action="replace_base_name";
 check(state.agentUiState(ui)&&ui.offers(req.action),"blank native editor offers replacement");const auto revision=edit.agentInputRevision();req.text="MCP Second Campaign";check(state.agentDispatch(req),"native width refusal reports dispatched no-op");check(edit.getText().empty()&&base.name.empty()&&base.changes==0&&!ok.getVisible()&&state.confirms==0&&edit.agentInputRevision()==revision,"width refusal commits no name/callback/OK/confirmation");
 req.text=std::string(1,'\n');check(!state.agentDispatch(req)&&base.changes==0,"invalid typed text remains unavailable");
 edit.setVisible(false);paint();req.text="MCP Two";check(!state.agentDispatch(req)&&base.changes==0,"hidden editor cannot dispatch");edit.setVisible(true);paint();
 state._modal=&ok;check(!state.agentDispatch(req)&&base.changes==0,"unrelated modal cannot dispatch");state._modal=nullptr;
 Surface cover(10,10,60,95);state._surfaces.push_back(&cover);paint();check(!state.agentDispatch(req)&&base.changes==0,"covering widget cannot dispatch");state._surfaces.pop_back();paint();
 check(state.agentUiState(ui)&&ui.offers(req.action),"restored fresh offered action");// Frame stores/compares opaque owner identity only; never dereferences this fixture.
 AgentFrameCommand frame;check(frame.stage(req,ui,reinterpret_cast<OpenXcom::State*>(&state),1,1,100,100),"frame staged");
 check(state.agentDispatch(req),"valid short replacement dispatched");check(edit.getText()=="MCP Two"&&base.name=="MCP Two"&&base.changes==1&&ok.getVisible()&&state.confirms==0,"accepted short name invokes one ordinary model update only");paint();AgentUiState changed;check(state.agentUiState(changed)&&!frame.matchesFrame(101,1,1,reinterpret_cast<OpenXcom::State*>(&state),changed),"changed input revision invalidates previous frame");
 req.text="MCP Second Campaign";check(state.agentDispatch(req)&&edit.getText()=="MCP Two"&&base.name=="MCP Two"&&base.changes==1&&ok.getVisible()&&state.confirms==0,"overwidth preserves previously committed short name without callback");
 check(!SDL_GetVideoSurface(),"no live video");std::cout<<"PASS "<<checks<<" BaseName native dispatch/width/fresh-frame checks; synthetic owning state only\n";
}
