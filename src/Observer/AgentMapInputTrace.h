// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../Engine/Logger.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sstream>
namespace OpenXcom { namespace Observer {
// Opt-in owner-local diagnostics of already-public admitted input only.
// No source actor, path, terrain, action token or game-model lookup.
class AgentMapInputTrace {
    AgentMapInputTrace *&_slot; AgentMapInputTrace *_previous;
    bool _enabled=false, _recorded=false, _known;
    unsigned _cell; int _expected[3],_point[2],_origin[2];
public:
    AgentMapInputTrace(AgentMapInputTrace *&slot,unsigned cell,bool known,int ex,int ey,int ez,
        int px,int py,int ox,int oy) noexcept : _slot(slot),_previous(slot),_known(known),_cell(cell),
        _expected{ex,ey,ez},_point{px,py},_origin{ox,oy} {
        const int saved=errno; const char *v=std::getenv("OPENXCOM_AGENT_MAP_INPUT_DIAGNOSTICS");
        _enabled=v && std::strcmp(v,"1")==0; if(_enabled)_slot=this; errno=saved;
    }
    ~AgentMapInputTrace() noexcept { if(_enabled&&!_recorded)emit("no_dispatch"); _slot=_previous; }
    void emit(const char *outcome,bool hasPosition=false,int x=0,int y=0,int z=0) noexcept {
        const int saved=errno;
        try {
            if(_enabled&&!_recorded) {
                _recorded=true; static unsigned count=0;
                if(count<32) { ++count; std::ostringstream out;
                    out<<"{\"schema\":\"agent-map-input/1\",\"ordinal\":"<<count<<",\"cell_index\":"<<_cell
                       <<",\"expected\":";
                    if(_known)out<<"["<<_expected[0]<<","<<_expected[1]<<","<<_expected[2]<<"]";else out<<"null";
                    out<<",\"canvas_point\":["<<_point[0]<<","<<_point[1]<<"],\"map_origin\":["<<_origin[0]<<","<<_origin[1]
                       <<"],\"outcome\":\""<<outcome<<"\",\"passed_position\":";
                    if(hasPosition)out<<"["<<x<<","<<y<<","<<z<<"]";else out<<"null";
                    out<<"}"; Log(LOG_INFO)<<"AGENT_MAP_INPUT "<<out.str();
                }
            }
        } catch(...) {} errno=saved;
    }
};
} }
