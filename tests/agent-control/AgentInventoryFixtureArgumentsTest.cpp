// SPDX-License-Identifier: GPL-3.0-or-later
// Calls prepare() only. No fixture start(), Game, SDL, assets or battle.
#include "../agent-report-fixture/AgentReportFixture.h"
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
using namespace OpenXcom;
static unsigned checks=0;
static void check(bool value) { ++checks; if(!value) std::exit(1); }
int main(int argc,char **argv)
{
    check(argc==2);
    const std::string alias=argv[1];
    check(alias=="alien-control-v1" || alias=="gameplay-v1" || alias=="gameplay-armored-v1" ||
        alias=="gameplay-vehicle-v1" || alias=="gameplay-mission-v1" ||
        alias=="inventory-body-v1" || alias=="inventory-paging-v1");
    char app[]="fixture-test",flag[]="--agent-report-fixture",tail[]="-user",path[]="/tmp/unused";
    char *normal[]={app,nullptr}; int n=1;
    AgentReportFixture::prepare(n,normal); check(n==1 && !AgentReportFixture::waiting());
    char wrong[]="inventory-unrecognized-v1";
    char *invalid[]={app,flag,wrong,nullptr}; n=3; bool rejected=false;
    try { AgentReportFixture::prepare(n,invalid); } catch(const std::exception &) { rejected=true; }
    check(rejected && n==3 && !AgentReportFixture::waiting());
    char *missing[]={app,flag,nullptr}; n=2; rejected=false;
    try { AgentReportFixture::prepare(n,missing); } catch(const std::exception &) { rejected=true; }
    check(rejected && n==2 && !AgentReportFixture::waiting());
    for (const char *mode : {"", "0", "true"})
    {
        if (*mode) setenv("OPENXCOM_AGENT_CONTROL",mode,1); else unsetenv("OPENXCOM_AGENT_CONTROL");
        char *disabled[]={app,flag,argv[1],nullptr}; n=3; bool refused=false;
        try { AgentReportFixture::prepare(n,disabled); } catch(const std::exception &) { refused=true; }
        check(refused && n==3 && !AgentReportFixture::waiting());
    }
    setenv("OPENXCOM_AGENT_CONTROL","1",1);
    char *valid[]={app,flag,argv[1],tail,path,nullptr}; n=5;
    AgentReportFixture::prepare(n,valid);
    check(n==3 && valid[1]==tail && valid[2]==path && valid[3]==nullptr && AgentReportFixture::waiting());
    char *repeat[]={app,flag,argv[1],nullptr}; n=3; bool refused=false;
    try { AgentReportFixture::prepare(n,repeat); } catch(const std::exception &) { refused=true; }
    check(refused && n==3 && AgentReportFixture::waiting());
    std::cout<<"AgentInventoryFixtureArgumentsTest PASS "<<checks<<" checks: "<<alias<<'\n';
}
