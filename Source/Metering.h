#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace fluid {
constexpr float meterSilence = -150.f;
inline float meterDb(double amplitude) { return amplitude > 1.e-8 ? float(20 * std::log10(amplitude)) : meterSilence; }
inline float meterLoudness(double power) { return power > 1.e-15 ? float(-.691 + 10 * std::log10(power)) : meterSilence; }
struct MeterFrame {
    std::array<float,2> peak{{meterSilence,meterSilence}}, rms=peak, hold=peak;
    std::array<float,2> displayPeak{{meterSilence,meterSilence}};
    float truePeak=meterSilence, maxTruePeak=meterSilence;
    float momentary=meterSilence, shortTerm=meterSilence, integrated=meterSilence;
    double seconds=0;
    int channels=2;
};

// Mono/stereo BS.1770 K weighting and EBU time windows. This core has no GUI,
// allocations or locks while processing. Buffers are allocated at prepare.
// The plugin runs it on a dedicated analysis worker.
class MeterDSP {
    struct Biquad {
        double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
        double tick(double x) {
            double y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y; return y;
        }
    };
    std::array<Biquad,2> shelf,hp;
    static constexpr int taps=32, phases=4, histogramSize=12001;
    std::array<std::array<double,taps>,phases> fir{};
    std::array<std::array<double,taps>,2> delay{};
    std::array<double,30> energies{};
    std::vector<std::array<double,2>> rmsRing;
    std::array<double,2> rmsSum{}, envelope{};
    std::array<int,2> envelopeHold{};
    double peakRelease=1;
    std::uint64_t fastSlices=0, fastNext=240;
    size_t rmsIndex=0;
    // 0.01-LU bins retain the exact sum of block powers, bounding the error at
    // the relative gate without unbounded storage over long DAW sessions.
    std::array<double,histogramSize> sums{};
    std::array<std::uint64_t,histogramSize> counts{};
    double rate=48000, energy=0, absoluteSum=0;
    std::array<double,2> peak{}, tp{}, peakHold{};
    double tpHold=0;
    std::uint64_t absoluteCount=0, samples=0;
    int sliceSamples=4800,inSlice=0,slices=0,delayIndex=0,channels=2;
    MeterFrame result;
    static Biquad makeShelf(double sr) {
        const double k=std::tan(3.141592653589793*1681.974450955533/sr);
        const double vh=std::pow(10.,3.999843853973347/20.), vb=std::pow(vh,.4996667741545416);
        const double q=.7071752369554196, a=1+k/q+k*k;
        return {(vh+vb*k/q+k*k)/a,2*(k*k-vh)/a,(vh-vb*k/q+k*k)/a,2*(k*k-1)/a,(1-k/q+k*k)/a};
    }
    static Biquad makeHP(double sr) {
        const double k=std::tan(3.141592653589793*38.13547087602444/sr),q=.5003270373238773,a=1+k/q+k*k;
        return {1,-2,1,2*(k*k-1)/a,(1-k/q+k*k)/a};
    }
    void completeSlice() {
        energies[size_t(slices%30)]=energy/sliceSamples;
        ++slices;
        double m=0,s=0;
        for(int i=0;i<std::min(slices,30);++i) {
            double e=energies[size_t((slices-1-i)%30)];s+=e;if(i<4)m+=e;
        }
        result.momentary=slices>=4?meterLoudness(m/4):meterSilence;
        result.shortTerm=slices>=30?meterLoudness(s/30):meterSilence;
        if(slices>=4 && result.momentary>=-70) {
            int bin=std::clamp(int((result.momentary+70)*100),0,histogramSize-1);
            sums[size_t(bin)]+=m/4;++counts[size_t(bin)];absoluteSum+=m/4;++absoluteCount;
        }
        if(absoluteCount) {
            const float gate=std::max(-70.f,meterLoudness(absoluteSum/absoluteCount)-10);
            int first=std::clamp(int(std::ceil((gate+70)*100)),0,histogramSize-1);
            double sum=0;std::uint64_t count=0;
            for(int i=first;i<histogramSize;++i) {sum+=sums[size_t(i)];count+=counts[size_t(i)];}
            result.integrated=count?meterLoudness(sum/count):meterSilence;
        }
        result.seconds=double(samples)/rate;result.channels=channels;
        energy=0;inSlice=0;
    }
    void completeFastSlice() {
        for(int c=0;c<channels;++c) {
            size_t ch=size_t(c);
            result.peak[ch]=meterDb(peak[ch]);
            peakHold[ch]=std::max(peakHold[ch],peak[ch]);result.hold[ch]=meterDb(peakHold[ch]);
            result.rms[ch]=meterDb(std::sqrt(std::max(0.,rmsSum[ch])/rmsRing.size()));
            result.displayPeak[ch]=meterDb(envelope[ch]);
        }
        double t=std::max(tp[0],tp[1]);tpHold=std::max(tpHold,t);
        result.truePeak=meterDb(t);result.maxTruePeak=meterDb(tpHold);
        result.seconds=double(samples)/rate;result.channels=channels;
        peak.fill(0);tp.fill(0);
    }
public:
    MeterDSP() { prepare(48000,2); }
    void prepare(double sr,int channelCount) {
        rate=std::max(8000.,sr); channels=std::clamp(channelCount,1,2);
        sliceSamples=std::max(1,int(std::round(rate*.1)));
        for(int c=0;c<2;++c){shelf[size_t(c)]=makeShelf(rate);hp[size_t(c)]=makeHP(rate);}
        for(int p=0;p<phases;++p) {
            double sum=0;
            for(int i=0;i<taps;++i) {
                double x=i-(taps/2-1)-double(p)/phases;
                double sinc=std::abs(x)<1.e-10?1:std::sin(3.141592653589793*x)/(3.141592653589793*x);
                double window=.42-.5*std::cos(6.283185307179586*i/(taps-1))+.08*std::cos(12.566370614359172*i/(taps-1));
                fir[size_t(p)][size_t(i)]=sinc*window;sum+=sinc*window;
            }
            for(auto& coefficient:fir[size_t(p)])coefficient/=sum;
        }
        rmsRing.resize(size_t(std::round(rate*.4)));
        peakRelease=std::pow(10.,-48./(20*rate));
        reset();
    }
    void reset() {
        for(auto& f:shelf)f.z1=f.z2=0;for(auto& f:hp)f.z1=f.z2=0;
        for(auto& d:delay)d.fill(0);for(auto& e:rmsRing)e.fill(0);
        energies.fill(0);sums.fill(0);counts.fill(0);rmsSum.fill(0);envelope.fill(0);envelopeHold.fill(0);rmsIndex=0;fastSlices=0;fastNext=std::uint64_t(std::llround(rate*.005));peak.fill(0);tp.fill(0);peakHold.fill(0);
        samples=absoluteCount=0;energy=absoluteSum=tpHold=0;slices=inSlice=delayIndex=0;result=MeterFrame{};result.channels=channels;
    }
    // Publish fast peak/RMS frames every 5 ms; LUFS/gating stays at 100 ms.
    bool push(float left,float right) {
        const double input[]={std::isfinite(left)?left:0.f,std::isfinite(right)?right:0.f};
        for(int c=0;c<channels;++c) {
            size_t ch=size_t(c);double x=input[c];peak[ch]=std::max(peak[ch],std::abs(x));
            double power=x*x;rmsSum[ch]+=power-rmsRing[rmsIndex][ch];rmsRing[rmsIndex][ch]=power;
            if(std::abs(x)>=envelope[ch]) {envelope[ch]=std::abs(x);envelopeHold[ch]=int(rate*.015);}
            else if(envelopeHold[ch]>0)--envelopeHold[ch];
            else envelope[ch]*=peakRelease;
            double y=hp[ch].tick(shelf[ch].tick(x));energy+=y*y;
            delay[ch][size_t(delayIndex)]=x;tp[ch]=std::max(tp[ch],std::abs(x));
            for(int p=1;p<phases;++p) {
                double v=0;
                for(int i=0;i<taps;++i)v+=fir[size_t(p)][size_t(i)]*delay[ch][size_t((delayIndex-i+taps)%taps)];
                tp[ch]=std::max(tp[ch],std::abs(v));
            }
        }
        delayIndex=(delayIndex+1)%taps;++samples;
        rmsIndex=(rmsIndex+1)%rmsRing.size();
        if(++inSlice==sliceSamples)completeSlice();
        if(samples>=fastNext) {
            completeFastSlice();++fastSlices;
            fastNext=std::uint64_t(std::llround((fastSlices+1)*rate*.005));return true;
        }
        return false;
    }
    const MeterFrame& frame() const { return result; }
};
}
