// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace OpenXcom { namespace Observer {
// Bounded provenance of ordinary paint calls. No world objects or image recognition.
class AgentGlobeProjection {
public:
    struct Sample { int x, y; bool textured, uncertain, sphere; unsigned covered; };
    std::vector<Sample> samples;
    int width = 0, height = 0;
    void begin(int w, int h) {
        width = w; height = h; samples.clear();
        if (w <= 0 || h <= 0 || w > 512 || h > 400) return;
        for (int y = 8; y < h; y += 16) for (int x = 8; x < w; x += 16)
            samples.push_back({x, y, false, false, false, 0});
    }
    static double distance(int x, int y, double x1, double y1, double x2, double y2) {
        const double dx = x2-x1, dy = y2-y1, den = dx*dx+dy*dy;
        const double t = den ? std::max(0., std::min(1., ((x-x1)*dx+(y-y1)*dy)/den)) : 0.;
        const double px = x-x1-t*dx, py = y-y1-t*dy;
        return px*px+py*py;
    }
    template<class T> void polygon(const T *x, const T *y, int n, bool opaque) {
        if (n < 3 || n > 4) { for (auto &s : samples) s.uncertain = true; return; }
        for (auto &s : samples) {
            bool inside = false;
            for (int i=0,j=n-1; i<n; j=i++) {
                if (distance(s.x,s.y,x[j],y[j],x[i],y[i]) <= 4.) s.uncertain = true;
                if ((y[i]>s.y)!=(y[j]>s.y) && s.x < (double)(x[j]-x[i])*(s.y-y[i])/(y[j]-y[i])+x[i]) inside=!inside;
            }
            if (inside) { if (opaque) s.textured=true; else s.uncertain=true; }
        }
    }
    void shadow(int x, int y, bool survives) {
        if (x < 8 || y < 8 || (x-8)%16 || (y-8)%16 || x>=width || y>=height) return;
        const unsigned columns = (width+7)/16, index=(y-8)/16*columns+(x-8)/16;
        if (index<samples.size()) samples[index].sphere=survives;
    }
    void clearLayer(unsigned layer) { for(auto &s:samples) s.covered &= ~layer; }
    void line(unsigned layer,double x1,double y1,double x2,double y2) {
        for(auto &s:samples) if(distance(s.x,s.y,x1,y1,x2,y2)<=4.) s.covered|=layer;
    }
    void rect(unsigned layer,int x,int y,int w,int h) {
        for(auto &s:samples) if(s.x>=x && s.y>=y && s.x<x+w && s.y<y+h) s.covered|=layer;
    }
    bool visible(const Sample &s) const { return s.sphere && !s.uncertain && !s.covered; }
};
} }
