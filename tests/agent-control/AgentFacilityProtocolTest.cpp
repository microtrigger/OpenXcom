// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cassert>
#include <iostream>
using namespace OpenXcom::Observer;
int main(){AgentRequest r;const std::string t(32,'1'),p="E "+t+" "+t+" "+t+" 1 ";
assert(parseAgentRequest("H openxcom-agent-control/32",r));assert(!parseAgentRequest("H openxcom-agent-control/31",r));
for(const std::string a:{"open_build_facilities","close_build_facilities","cancel_place_facility"}){assert(parseAgentRequest(p+a,r));assert(!parseAgentRequest(p+a+" extra",r));}
for(const std::string h:{"r0","r31"}){assert(parseAgentRequest(p+"select_build_facility "+h,r));assert(r.choice==h);}
for(const std::string h:{"r32","r00","LAB","0 0","r0 extra",""})assert(!parseAgentRequest(p+"select_build_facility "+h,r));
for(const std::string a:{"place_facility","build_facility","construct_facility"})assert(!parseAgentRequest(p+a+" r0",r));
std::cout<<"PASS facility protocol: select/details/cancel, no construction authority\n";}
