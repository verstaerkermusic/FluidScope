#pragma once
#include <JuceHeader.h>
#include "Metering.h"
namespace fluid {
class MeterAnalyser : private juce::Thread {
    struct Sample {float l=0,r=0;double rate=48000;unsigned epoch=0;int channels=2;};
    static constexpr unsigned capacity=1u<<18;
    std::vector<Sample> queue=std::vector<Sample>(capacity);
    std::atomic<unsigned> read{0},write{0},epoch{1};
    MeterDSP dsp;
    juce::CriticalSection lock;
    MeterFrame published;
    double publishedAt=0;
    unsigned publishedEpoch=0;
    void publish(unsigned generation) {
        const juce::ScopedLock scoped(lock);published=dsp.frame();publishedEpoch=generation;publishedAt=juce::Time::getMillisecondCounterHiRes();
    }
    void run() override {
        unsigned generation=0;double rate=0;int channels=0;
        while(!threadShouldExit()) {
            if(generation!=epoch.load()) {generation=epoch.load();dsp.reset();publish(generation);}
            auto r=read.load(std::memory_order_relaxed),w=write.load(std::memory_order_acquire);
            while(r!=w && !threadShouldExit()) {
                auto sample=queue[r&(capacity-1)];++r;
                if(sample.epoch!=epoch.load())continue;
                if(rate!=sample.rate||channels!=sample.channels||generation!=sample.epoch) {
                    rate=sample.rate;channels=sample.channels;generation=sample.epoch;dsp.prepare(rate,channels);publish(generation);
                }
                if(dsp.push(sample.l,sample.r))publish(generation);
            }
            read.store(r,std::memory_order_release);wait(5);
        }
    }
public:
    std::atomic<unsigned> dropped{0};
    MeterAnalyser():Thread("FluidScope meters") {startThread();}
    ~MeterAnalyser() override {stopThread(-1);}
    void reset() {epoch.fetch_add(1);}
    void push(const float* l,const float* r,int n,double rate,int channels) noexcept {
        auto w=write.load(std::memory_order_relaxed);
        if(unsigned(n)>capacity-(w-read.load(std::memory_order_acquire))) {
            ++dropped;reset();return;
        }
        auto generation=epoch.load();
        for(int i=0;i<n;++i)queue[(w++)&(capacity-1)]={l[i],r[i],rate,generation,channels};
        write.store(w,std::memory_order_release);
    }
    MeterFrame snapshot() {
        const juce::ScopedLock scoped(lock);
        if(publishedEpoch!=epoch.load())return {};
        auto frame=published;
        // A stopped DAW may stop calling processBlock entirely. Retained maxima
        // and integrated loudness remain available, while live meters go idle.
        if(juce::Time::getMillisecondCounterHiRes()-publishedAt>350) {
            frame.peak.fill(meterSilence);frame.rms.fill(meterSilence);frame.truePeak=frame.momentary=frame.shortTerm=meterSilence;
        }
        return frame;
    }
};
}
