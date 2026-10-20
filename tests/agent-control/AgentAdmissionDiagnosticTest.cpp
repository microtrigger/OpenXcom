// SPDX-License-Identifier: GPL-3.0-or-later
// Actual diagnostic method + actual Logger; only Game ownership and timestamp shim.
#include "../../src/Engine/Surface.h"
#include "../../src/Engine/Logger.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
namespace OpenXcom {
namespace CrossPlatform { std::string now(){return "fixture-time";} }
class Game { public: Surface *_cursor=nullptr,*_fpsCounter=nullptr;
 void agentRecordAdmissionFailure(bool,bool,bool)const; };
#include "AgentAdmissionDiagnostic.generated.inc"
}
using namespace OpenXcom;
unsigned checks=0;void check(bool x,const char*m){++checks;if(!x){std::cerr<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==2,"isolated output path");check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"no video initialization");
 Logger::logFile()=argv[1];Logger::reportingLevel()=LOG_INFO;
 auto lines=[&](){std::ifstream f(argv[1]);std::vector<std::string>out;std::string s;while(std::getline(f,s))out.push_back(s);return out;};
 Game game;Surface cursor(9,13,-3,198),fps(15,5,0,0);game._cursor=&cursor;
 unsetenv("OPENXCOM_AGENT_ADMISSION_DIAGNOSTICS");game.agentRecordAdmissionFailure(false,true,true);check(lines().empty(),"default off");
 setenv("OPENXCOM_AGENT_ADMISSION_DIAGNOSTICS","true",1);game.agentRecordAdmissionFailure(false,true,true);check(lines().empty(),"exact opt-in only");
 setenv("OPENXCOM_AGENT_ADMISSION_DIAGNOSTICS","1",1);game.agentRecordAdmissionFailure(true,true,true);check(lines().empty(),"success produces no diagnostic");
 game.agentRecordAdmissionFailure(false,true,true);check(lines().size()==1,"failed gate logged once");
 for(int i=0;i<100;++i)game.agentRecordAdmissionFailure(false,true,true);check(lines().size()==1,"unchanged failure deduplicated");
 game._fpsCounter=&fps;cursor.setVisible(false);game.agentRecordAdmissionFailure(true,true,false);check(lines().size()==2,"changed geometry/predicate recorded");
 for(int i=0;i<80;++i){cursor.setX(i);game.agentRecordAdmissionFailure(true,false,true);}check(lines().size()==32,"hard32 diagnostic-only limit");
 game.agentRecordAdmissionFailure(true,true,true);check(lines().size()==32,"cutoff does not alter success predicate");
 std::cout<<"AgentAdmissionDiagnosticTest PASS "<<checks<<" checks\n";
}
