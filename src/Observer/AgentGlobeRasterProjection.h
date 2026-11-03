// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentGlobeProjection.h"
namespace OpenXcom { namespace Observer {
// Independent UI sidecar only. Inherits exact existing paint coverage algorithms;
// does not change the old16px instance, class layout, or geography DTO.
class AgentGlobeRasterProjection : public AgentGlobeProjection {
public:
    static const int step=4,offset=2,minimumY=32;
    bool valid=false;
    void begin(int w,int h) {
        valid=false;width=w;height=h;samples.clear();
        if(w<=0 || w>512 || h<48 || h>400)return;
        const int columns=(w+1)/4,rows=(h-32+1)/4;
        if(columns<=0 || rows<=0 || columns*rows>4096)return;
        for(int y=minimumY+offset;y<h;y+=step)for(int x=offset;x<w;x+=step)
            samples.push_back({x,y,false,false,false,0});
        valid=samples.size()==static_cast<std::size_t>(columns*rows);
    }
    void shadow(int x,int y,bool survives) {
        if(!valid || x<offset || y<minimumY+offset || (x-offset)%step || (y-minimumY-offset)%step || x>=width || y>=height)return;
        const unsigned columns=(width+1)/4,index=(y-minimumY-offset)/step*columns+(x-offset)/step;
        if(index<samples.size())samples[index].sphere=survives;
    }
    const Sample *at(int x,int y)const {
        if(!valid || x<offset || y<minimumY+offset || (x-offset)%step || (y-minimumY-offset)%step || x>=width || y>=height)return nullptr;
        const unsigned columns=(width+1)/4,index=(y-minimumY-offset)/step*columns+(x-offset)/step;
        return index<samples.size()?&samples[index]:nullptr;
    }
};
} }
