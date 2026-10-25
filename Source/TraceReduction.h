#pragma once
#include "Capture.h"
namespace fluid {
// Bounded-error polyline reduction in display coordinates. Short chunks bound
// worst-case work on noisy/closed figures, without uniform sample skipping.
inline std::vector<Stereo> reduceTrace(const std::vector<Stereo>& points,float tolerance=.18f) {
    if(points.size()<3)return points;
    std::vector<unsigned char> keep(points.size(),0);keep.front()=keep.back()=1;
    struct Span{size_t a,b;};std::vector<Span> pending;pending.reserve(128);
    for(size_t begin=0;begin+1<points.size();) {
        const auto end=std::min(begin+128,points.size()-1);keep[begin]=keep[end]=1;pending.push_back({begin,end});
        while(!pending.empty()) {
            const auto span=pending.back();pending.pop_back();const auto a=points[span.a],b=points[span.b];
            const float dx=b.l-a.l,dy=b.r-a.r,denom=dx*dx+dy*dy;
            float worst=tolerance*tolerance;size_t split=span.a;
            for(size_t i=span.a+1;i<span.b;++i) {
                const float t=denom>1e-20f?std::clamp(((points[i].l-a.l)*dx+(points[i].r-a.r)*dy)/denom,0.f,1.f):0;
                const float x=points[i].l-a.l-t*dx,y=points[i].r-a.r-t*dy,dist=x*x+y*y;
                if(dist>worst){worst=dist;split=i;}
            }
            if(split!=span.a){keep[split]=1;pending.push_back({span.a,split});pending.push_back({split,span.b});}
        }
        begin=end;
    }
    std::vector<Stereo> result;result.reserve(points.size());for(size_t i=0;i<points.size();++i)if(keep[i])result.push_back(points[i]);return result;
}
}
