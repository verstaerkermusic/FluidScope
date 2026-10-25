#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace fluid {
// Independent display audition path: 8th-order Butterworth HP + LP (48 dB/oct
// per edge). No analysis or memory allocation occurs in this audio-thread code.
class BandListen {
    struct Biquad {
        double b0=1,b1=0,b2=0,a1=0,a2=0;
        std::array<double,2> z1{},z2{};
        float tick(float x,int ch) noexcept {
            double y=b0*x+z1[size_t(ch)];z1[size_t(ch)]=b1*x-a1*y+z2[size_t(ch)];z2[size_t(ch)]=b2*x-a2*y;
            return float(y);
        }
        void reset() {z1.fill(0);z2.fill(0);}
    };
    std::array<Biquad,8> filters;
    double rate=48000,logLow=std::log(20.),logHigh=std::log(20000.);
    float mix=0;
    int counter=0;
    void coefficients(double low,double high) noexcept {
        constexpr double qs[]={.5097955791,.6013448869,.8999762231,2.562915448};
        for(int edge=0;edge<2;++edge) {
            const double w=6.283185307179586*std::clamp(edge?high:low,5.,rate*.47)/rate;
            const double c=std::cos(w),s=std::sin(w);
            for(int i=0;i<4;++i) {
                const double a=s/(2*qs[i]),den=1+a;
                auto& f=filters[size_t(edge*4+i)];
                const double num=edge?(1-c):(1+c);
                f.b0=num*.5/den;f.b1=(edge?num:-num)/den;f.b2=f.b0;
                f.a1=-2*c/den;f.a2=(1-a)/den;
            }
        }
    }
public:
    void prepare(double sr) noexcept {rate=sr;mix=0;counter=0;for(auto& f:filters)f.reset();coefficients(std::exp(logLow),std::exp(logHigh));}
    void process(float* const* data,int channels,int samples,bool enabled,float low,float high) noexcept {
        if(low>high)std::swap(low,high);
        low=std::clamp(low,5.f,float(rate*.45));high=std::clamp(high,low+1.f,float(rate*.47));
        if(!enabled && mix==0) return; // exact bypass, including normal plugin use
        const double targetL=std::log(double(low)),targetH=std::log(double(high));
        const double smoothing=1-std::exp(-32./(.025*rate));
        const float fade=float(1./(.012*rate));
        for(int i=0;i<samples;++i) {
            if(counter++%32==0) {logLow+=(targetL-logLow)*smoothing;logHigh+=(targetH-logHigh)*smoothing;coefficients(std::exp(logLow),std::exp(logHigh));}
            mix=enabled?std::min(1.f,mix+fade):std::max(0.f,mix-fade);
            for(int ch=0;ch<std::min(2,channels);++ch) {
                const float dry=data[ch][i];float wet=dry;for(auto& f:filters)wet=f.tick(wet,ch);
                data[ch][i]=mix==0?dry:dry+(wet-dry)*mix;
            }
        }
        if(!enabled && mix==0)for(auto& f:filters)f.reset();
    }
};
}
