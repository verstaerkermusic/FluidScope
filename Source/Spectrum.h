#pragma once
#include <JuceHeader.h>
#include "Capture.h"
namespace fluid {
struct SpectrumFrame {
    static constexpr int bands=512, rows=384, columns=256;
    std::array<float,bands> db{},peak{};
    std::array<float,rows*columns> history{};
    std::array<float,rows> ages{};
    int rowCount=0;
    double rate=48000,seconds=6,time=0;
    float maxFrequency=20000;
    unsigned sequence=0;
    SpectrumFrame(){db.fill(-150);peak.fill(-150);history.fill(-150);}
};
class SpectrumAnalyser : private juce::Thread {
    static constexpr int maxSize=32768, historyCapacity=600;
    Queue queue;
    std::array<juce::dsp::FFT,3> transforms{{juce::dsp::FFT(13),juce::dsp::FFT(14),juce::dsp::FFT(15)}};
    std::array<Stereo,maxSize> samples{};
    std::array<float,maxSize> window{};
    std::array<float,maxSize*2> left{},right{};
    std::array<float,maxSize/2+1> bins{};
    std::array<float,SpectrumFrame::bands> columns{};
    std::array<float,SpectrumFrame::bands> power{},peaks{};
    struct Row {std::array<float,SpectrumFrame::bands> raw{},db{};double time=0;};
    float appliedDecay=-1;
    std::vector<Row> rows=std::vector<Row>(historyCapacity);
    unsigned rowWrite=0,rowCount=0,sequence=0;
    long long cursor=0,lastRow=0;
    int size=8192,hop=512,order=0;
    double sampleRate=0,windowSum=0;
    std::shared_ptr<const SpectrumFrame> published;
    void reset(double sr) {
        sampleRate=sr;order=sr>128000?2:sr>64000?1:0;size=8192<<order;hop=512<<order;
        cursor=lastRow=0;rowWrite=rowCount=0;power.fill(0);peaks.fill(-150);windowSum=0;
        for(int i=0;i<size;++i) {window[size_t(i)]=float(.5-.5*std::cos(6.283185307179586*i/(size-1)));windowSum+=window[size_t(i)];}
    }
    static float db(float p) {return p>1e-15f?10.f*std::log10(p):-150.f;}
    static float decayValue(float raw,float previous,double dt,float ms) {
        return raw>=previous || ms<=0 ? raw : raw+(previous-raw)*float(std::exp(-dt/(ms*.001)));
    }
    void refreshDecay(float ms) {
        if(ms==appliedDecay)return;
        const Row* previous=nullptr;
        for(unsigned i=0;i<rowCount;++i) {
            auto& row=rows[(rowWrite-rowCount+i)%historyCapacity];
            for(int b=0;b<SpectrumFrame::bands;++b)row.db[size_t(b)]=previous?decayValue(row.raw[size_t(b)],previous->db[size_t(b)],row.time-previous->time,ms):row.raw[size_t(b)];
            previous=&row;
        }
        appliedDecay=ms;
    }
    void analyse() {
        const int mask=maxSize-1;
        for(int i=0;i<size;++i) {auto s=samples[size_t(cursor-size+i)&mask];left[size_t(i)]=s.l*window[size_t(i)];right[size_t(i)]=s.r*window[size_t(i)];}
        std::fill(left.begin()+size,left.begin()+size*2,0);std::fill(right.begin()+size,right.begin()+size*2,0);
        transforms[size_t(order)].performRealOnlyForwardTransform(left.data(),true);
        transforms[size_t(order)].performRealOnlyForwardTransform(right.data(),true);
        const float norm=float(4./(windowSum*windowSum));const int channel=source.load();
        for(int k=0;k<=size/2;++k) {
            float l=left[size_t(k*2)]*left[size_t(k*2)]+left[size_t(k*2+1)]*left[size_t(k*2+1)];
            float r=right[size_t(k*2)]*right[size_t(k*2)]+right[size_t(k*2+1)]*right[size_t(k*2+1)];
            bins[size_t(k)]=norm*(channel==1?l:channel==2?r:.5f*(l+r));
        }
        const float maxFrequency=float(std::min(20000.,sampleRate*.47));
        const int smoothing=frequencySmooth.load();
        const double octave=smoothing==1?1./24:smoothing==2?1./12:smoothing==3?1./6:0;
        const float attack=float(std::exp(-double(hop)/(.008*sampleRate)));
        const float release=float(std::exp(-double(hop)/(std::max(10.f,smoothMs.load())*.001*sampleRate)));
        auto frame=std::make_shared<SpectrumFrame>();frame->rate=sampleRate;frame->maxFrequency=maxFrequency;frame->time=double(cursor)/sampleRate;frame->seconds=historySeconds.load();frame->sequence=++sequence;
        const double octaves=std::log2(maxFrequency/20.);
        for(int j=0;j<SpectrumFrame::bands;++j) {
            const double low=20*std::pow(maxFrequency/20.,(double(j)-.5)/(SpectrumFrame::bands-1));
            const double high=20*std::pow(maxFrequency/20.,(double(j)+.5)/(SpectrumFrame::bands-1));
            int a=std::clamp(int(std::ceil(low*size/sampleRate)),0,size/2),b=std::clamp(int(std::ceil(high*size/sampleRate)),a,size/2+1);
            if(b==a){a=std::clamp(int(std::round(std::sqrt(low*high)*size/sampleRate)),0,size/2);b=a+1;}
            float peak=0;for(int k=a;k<b;++k)peak=std::max(peak,bins[size_t(k)]);columns[size_t(j)]=peak;
        }
        const float sigma=float(octave*(SpectrumFrame::bands-1)/octaves/2.355);
        const int radius=std::min(16,int(std::ceil(sigma*2.5f)));
        std::array<float,SpectrumFrame::bands> rawDb{};
        for(int j=0;j<SpectrumFrame::bands;++j) {
            float p=columns[size_t(j)];
            if(sigma>.01f) {float sum=0,weight=0;for(int k=-radius;k<=radius;++k){float w=std::exp(-.5f*k*k/(sigma*sigma));sum+=w*columns[size_t(std::clamp(j+k,0,SpectrumFrame::bands-1))];weight+=w;}p=sum/weight;}
            rawDb[size_t(j)]=db(p);
            const float alpha=p>power[size_t(j)]?attack:release;
            power[size_t(j)]=std::max(0.f,alpha*power[size_t(j)]+(1-alpha)*p);
            frame->db[size_t(j)]=db(power[size_t(j)]);
            peaks[size_t(j)]=std::max(frame->db[size_t(j)],peaks[size_t(j)]-float(12.*hop/sampleRate));frame->peak[size_t(j)]=peaks[size_t(j)];
        }
        const float decay=historyDecayMs.load();refreshDecay(decay);
        Row current;current.time=frame->time;current.raw=rawDb;
        const Row* previous=rowCount?&rows[(rowWrite-1)%historyCapacity]:nullptr;
        for(int band=0;band<SpectrumFrame::bands;++band)current.db[size_t(band)]=previous?decayValue(rawDb[size_t(band)],previous->db[size_t(band)],current.time-previous->time,decay):rawDb[size_t(band)];
        if(cursor-lastRow>=sampleRate/30.) {
            rows[rowWrite++%historyCapacity]=current;rowCount=std::min(rowCount+1,unsigned(historyCapacity));lastRow=cursor;
        }
        // Keep measured rows and their timestamps intact. Resampling heights onto
        // a moving time grid makes short transients wobble on long histories.
        auto append=[&](const Row& row) {
            const int r=frame->rowCount++;frame->ages[size_t(r)]=float((frame->time-row.time)/frame->seconds);
            for(int col=0;col<SpectrumFrame::columns;++col) {
                float peak=-150;
                for(int band=col*SpectrumFrame::bands/SpectrumFrame::columns;band<(col+1)*SpectrumFrame::bands/SpectrumFrame::columns;++band)peak=std::max(peak,row.db[size_t(band)]);
                frame->history[size_t(r*SpectrumFrame::columns+col)]=peak;
            }
        };
        append(current);
        for(unsigned i=0;i<rowCount && frame->rowCount<SpectrumFrame::rows;++i) {
            const auto& row=rows[(rowWrite-1-i)%historyCapacity];
            if(row.time>=frame->time)continue;
            if(frame->time-row.time>frame->seconds)break;
            append(row);
        }
        std::atomic_store(&published,std::shared_ptr<const SpectrumFrame>(std::move(frame)));
    }
    void run() override {
        while(!threadShouldExit()) {
            if(!requested.load()) {queue.drain([](Stereo){});sampleRate=0;wait(10);continue;}
            if(sampleRate!=rate.load())reset(rate.load());
            bool consumed=false;
            queue.drain([&](Stereo s){if(threadShouldExit())return;consumed=true;samples[size_t(cursor++)&(maxSize-1)]=s;if(cursor>=size && cursor%hop==0)analyse();});
            if(!consumed)wait(3);
        }
    }
public:
    std::atomic<bool> requested{false};
    std::atomic<double> rate{48000};
    std::atomic<int> source{0},frequencySmooth{1};
    std::atomic<float> smoothMs{180},historySeconds{6},historyDecayMs{40};
    SpectrumAnalyser():Thread("FluidScope spectrum") {startThread(juce::Thread::Priority::low);}
    ~SpectrumAnalyser() override {stopThread(2000);}
    void push(const float* l,const float* r,int n) noexcept {if(requested.load(std::memory_order_relaxed))queue.push(l,r,n);}
    std::shared_ptr<const SpectrumFrame> snapshot() const {return std::atomic_load(&published);}
};
inline float spectrumFrequency(float x,bool log,float maxFrequency) {return log?20.f*std::pow(maxFrequency/20.f,std::clamp(x,0.f,1.f)):std::clamp(x,0.f,1.f)*maxFrequency;}
inline float spectrumX(float frequency,bool log,float maxFrequency) {return log?std::log(std::max(20.f,frequency)/20.f)/std::log(maxFrequency/20.f):frequency/maxFrequency;}
inline float spectrumValue(const std::array<float,SpectrumFrame::bands>& data,float frequency,float maxFrequency) {
    if(frequency<20 || frequency>maxFrequency)return -150;
    float index=std::clamp(spectrumX(frequency,true,maxFrequency),0.f,1.f)*(SpectrumFrame::bands-1);int a=std::min(int(index),SpectrumFrame::bands-2);
    const float mix=index-a;const double power=(1-mix)*std::pow(10.,data[size_t(a)]*.1)+mix*std::pow(10.,data[size_t(a+1)]*.1);return power>1e-15?float(10*std::log10(power)):-150.f;
}
}
