// SPDX-License-Identifier: GPL-3.0-or-later
// No Game, resources, battle, SDL video/audio or active save is created.
#include "../agent-report-fixture/AgentReportFixture.h"
#include <cstdlib>
#include <exception>
#include <iostream>
using namespace OpenXcom;
unsigned checks=0;
void check(bool b){++checks;if(!b)std::exit(1);}
int main(int argc, char **argv)
{
 char app[]="fixture-test",flag[]="--agent-report-fixture",oldAlias[]="alien-control-v1",newAlias[]="gameplay-v1",armoredAlias[]="gameplay-armored-v1",vehicleAlias[]="gameplay-vehicle-v1",wrong[]="bad",tail[]="-user",path[]="/tmp/unused";
 char *alias = argc == 2 && std::string(argv[1]) == "gameplay-vehicle-v1" ? vehicleAlias :
     argc == 2 && std::string(argv[1]) == "gameplay-armored-v1" ? armoredAlias :
     argc == 2 && std::string(argv[1]) == "gameplay-v1" ? newAlias : oldAlias;
 char *normal[]={app,nullptr};int n=1;AgentReportFixture::prepare(n,normal);check(n==1&&!AgentReportFixture::waiting());
 char *bad[]={app,flag,wrong,nullptr};n=3;bool caught=false;try{AgentReportFixture::prepare(n,bad);}catch(const std::exception&){caught=true;}check(caught&&!AgentReportFixture::waiting());
 char *missing[]={app,flag,nullptr};n=2;caught=false;try{AgentReportFixture::prepare(n,missing);}catch(const std::exception&){caught=true;}check(caught);
 unsetenv("OPENXCOM_AGENT_CONTROL");char *noMode[]={app,flag,alias,nullptr};n=3;caught=false;try{AgentReportFixture::prepare(n,noMode);}catch(const std::exception&){caught=true;}check(caught&&!AgentReportFixture::waiting());
 setenv("OPENXCOM_AGENT_CONTROL","1",1);char *valid[]={app,flag,alias,tail,path,nullptr};n=5;AgentReportFixture::prepare(n,valid);check(n==3&&valid[1]==tail&&valid[2]==path&&valid[3]==nullptr&&AgentReportFixture::waiting());
 char *repeat[]={app,flag,alias,nullptr};n=3;caught=false;try{AgentReportFixture::prepare(n,repeat);}catch(const std::exception&){caught=true;}check(caught);
 std::cout<<"AgentReportFixtureArgumentsTest PASS "<<checks<<" checks\n";
}
