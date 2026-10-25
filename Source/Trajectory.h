#pragma once
#include "Capture.h"
#include <memory>
#include <cstring>
namespace fluid {
inline float rearOpacity(float age,float percent) {
    const float length=std::clamp(percent*.01f,0.f,.5f);
    if(length==0)return 1;
    const float t=std::clamp((age-(1-length))/length,0.f,1.f);
    return 1-t*t*(3-2*t);
}
struct TrajectoryFrame {
    static constexpr int textureWidth=2048;
    struct Sample {float l=0,r=0,envelope=0,padding=0;};
    std::vector<Sample> data;
    long long position=0,base=0;
    int count=0,first=0,points=0,requested=0;
    double rate=48000,seconds=.2;
};
inline std::shared_ptr<const TrajectoryFrame> trajectory(const Capture& c,double seconds) {
    if(c.position<2)return {};
    auto f=std::make_shared<TrajectoryFrame>();f->position=c.position;f->rate=c.settings.rate;f->seconds=seconds;
    f->requested=std::max(16,int(seconds*f->rate));
    const auto oldest=std::max(0LL,c.position-Capture::capacity);
    const auto first=std::max(oldest,c.position-f->requested);
    f->base=std::max(oldest,first-int(f->rate*.1)-2);f->count=int(c.position-f->base);f->first=int(first-f->base);f->points=int(c.position-first);
    const int padded=(f->count+TrajectoryFrame::textureWidth-1)/TrajectoryFrame::textureWidth*TrajectoryFrame::textureWidth;f->data.resize(size_t(padded));
    for(int i=0;i<f->count;++i){const auto index=size_t(f->base+i)&(Capture::capacity-1);const auto s=c.ring[index];f->data[size_t(i)]={s.l,s.r,c.envelope[index],0};}
    return f;
}
}
