#include "../Source/Capture.h"
#include "../Source/XYSource.h"
#include "../Source/BandListen.h"
#include "../Source/Trajectory.h"
#include "../Source/TraceReduction.h"
#include "../Source/BandSelection.h"
#include <iostream>
#include <cstdlib>
void check(bool ok,const char* text) { if(!ok) {std::cerr<<text<<'\n';std::exit(1);} }
int main() {
    for(bool logarithmic:{false,true})for(float delta:{-100.f,-.1f,0.f,.1f,100.f}) {
        auto moved=fluid::shiftBand(1000,3000,delta,logarithmic,20000);
        check(moved[0]>=19.999f&&moved[1]<=20000.01f,"Shifted band stays inside the supported spectrum");
        check(logarithmic?std::abs(moved[1]/moved[0]-3)<1e-5:std::abs(moved[1]-moved[0]-2000)<.01f,"Band sweep preserves octave or Hz width, including at the bounds");
    }
    auto narrow=fluid::shiftBand(1000,1001,-100,true,20000);
    auto regrab=fluid::shiftBand(narrow[0],narrow[1],0,true,20000);
    check(std::abs(regrab[0]-narrow[0])<1e-6&&std::abs(regrab[1]-narrow[1])<1e-6,"Regrabbing a narrow logarithmic band never widens it");
    fluid::Capture c; c.settings.rate=48000;c.settings.seconds=.01;c.settings.holdoff=.001;c.settings.mode=1;
    double maxError=0;unsigned frames=0;
    for(int i=0;i<48000;++i) {
        float a=float(.8*std::sin(i*2*3.141592653589793*437.3/48000));
        auto gen=c.generation;c.ingest({a,-a});
        if(gen!=c.generation) {auto s=c.at(c.origin+int(c.span()*c.settings.pre));maxError=std::max(maxError,std::abs(double(s.l)));++frames;check(std::abs(s.l+s.r)<1e-6,"stereo alignment");}
    }
    check(frames>50,"trigger acquisition");check(maxError<1e-5,"subsample alignment");
    // A long window must not turn a 220-Hz signal into a 10-fps slideshow.
    c.settings.seconds=.1;c.settings.holdoff=0;c.resetTrigger();
    unsigned overlapping=0;long long last=0,maxGap=0;
    for(int i=0;i<48000;++i) {
        float a=float(.8*std::sin(i*2*3.141592653589793*220/48000));
        auto gen=c.generation;c.ingest({a,-a});
        if(gen!=c.generation) {++overlapping;if(last)maxGap=std::max(maxGap,c.position-last);last=c.position;}
    }
    check(overlapping>190,"overlapping acquisitions preserve every trigger across a long window");
    check(maxGap<=220,"no window-length dead time between acquisitions");
    const auto retainedPosition=c.position;
    const auto retainedWrite=c.crossingWrite;
    for(double window:{.5,.04,.2,.001,.35,.02})for(float pre:{0.f,.2f,.8f}) {
        c.settings.seconds=window;c.settings.pre=pre;
        check(c.reframeFromHistory(),"window change can reuse a complete buffered sweep immediately");
        check(c.frameAvailable(),"retargeted sweep has all real samples available");
        check(std::abs(c.at(c.origin+int(c.span()*pre)).l)<1e-5,"timebase retarget preserves fractional trigger alignment");
        check(c.position==retainedPosition && c.crossingWrite==retainedWrite,"timebase retarget neither consumes audio nor restarts triggers");
    }
    // Not enough history for a larger window: don't publish a zero-filled frame.
    fluid::Capture startup;startup.settings.mode=1;startup.settings.seconds=.001;startup.settings.holdoff=0;
    auto feed=[&]{float a=float(std::sin(6.283185307179586*1000*startup.position/48000));startup.ingest({a,a});};
    for(int i=0;i<2000;++i)feed();
    check(startup.frameAvailable(),"short initial sweep available");
    startup.settings.seconds=.5;
    check(!startup.reframeFromHistory() && !startup.ready,"insufficient history defers replacement instead of fabricating samples");
    for(int i=0;i<26000;++i)feed();
    check(startup.frameAvailable(),"capture continues after a timebase change without a reset");
    c.settings.seconds=.1;c.settings.pre=.2f;
    std::cout<<"Overlapping 100-ms sweeps: "<<overlapping<<"/s, maximum gap "<<maxGap<<" samples\n";
    c.resetTrigger();c.settings.hysteresis=.05;
    for(int i=0;i<2000;++i)c.ingest({float(.001*std::sin(i)),0});
    check(!c.ready,"hysteresis rejects small noise");
    c.settings.mode=0;for(int i=0;i<20000;++i)c.ingest({0,0});check(c.ready&&!c.triggered,"auto fallback on silence");
    fluid::Queue q;float l[8]={1,2,3,4,5,6,7,8};q.push(l,l,8);int n=0;q.drain([&](fluid::Stereo s){check(s.l==l[n++]&&s.l==s.r,"queue order");});check(n==8,"queue count");
    fluid::Capture xy;xy.settings.rate=48000;
    for(int i=0;i<48000;++i) {float s=float(std::sin(6.283185307179586*1000*i/48000));xy.ingest({s,-s});}
    fluid::XYSettings ys;ys.delayMs=.25;
    auto point=fluid::xyPoint(xy,24012,ys);
    check(std::abs(point.l-1)<1e-6 && std::abs(point.r)<1e-6,"quarter-cycle XY delay");
    ys.delayMs=.1;point=fluid::xyPoint(xy,24012.25,ys);
    check(std::abs(point.r-xy.at(24007.45).r)<1e-6,"fractional XY delay");
    ys.source=1;point=fluid::xyPoint(xy,24012.25,ys);
    check(std::abs(point.r-xy.at(24007.45).l)<1e-6,"mono / left XY source");
    ys.source=2;ys.delayMs=0;ys.frequency=1000;ys.phaseDegrees=90;ys.amplitude=.6f;
    point=fluid::xyPoint(xy,24000,ys);check(std::abs(point.r-.6f)<1e-6,"internal oscillator phase and level");
    ys.delayMs=.25;point=fluid::xyPoint(xy,24000,ys);check(std::abs(point.r)<1e-6,"internal oscillator delay");
    for(double rate:{44100.,48000.,96000.,192000.}) {
        fluid::Capture followed;followed.settings.rate=rate;
        for(int i=0;i<int(rate*.25);++i)followed.ingest({.8f,1.f});
        fluid::XYSettings follow;follow.source=2;follow.frequency=100;follow.amplitude=1;follow.follow=true;
        const double at=followed.position-4;follow.phaseDegrees=90-std::remainder(at*100/rate,1.)*360;
        check(std::abs(fluid::xyPoint(followed,at,follow).r-.8f)<1e-5,"Follower matches left/X peak, independent of the right channel");
        follow.amplitude=.5f;check(std::abs(fluid::xyPoint(followed,at,follow).r-.4f)<1e-5,"Follower oscillator level is a relative ratio");
        const auto end=followed.position;
        for(int i=0;i<int(rate*.12)+3;++i)followed.ingest({0,1.f});
        check(std::abs(followed.envelopeAt(double(end+int(rate*.12)-1))-.8f/float(std::exp(1.)))<.0003f,"Follower release has the same 120-ms timing at every sample rate");
        follow.delayMs=20;follow.amplitude=1;const auto t=double(end+100);
        follow.phaseDegrees=90-std::remainder((t-rate*.02)*100/rate,1.)*360;
        check(std::abs(fluid::xyPoint(followed,t,follow).r-.8f)<1e-5,"XY delay shifts the sine envelope with its carrier");
        for(int i=0;i<int(rate*4);++i)followed.ingest({0,1});
        check(followed.envelopeAt(double(followed.position-2))<1e-12f,"Silent left input fades the internal sine even with a loud right channel");
        followed.ingest({.6f,0});followed.ingest({0,0});
        check(std::abs(followed.envelopeAt(double(followed.position-2))-.6f)<1e-6,"Follower catches a one-sample transient immediately");
    }
    check(fluid::rearOpacity(.74f,25)==1 && fluid::rearOpacity(.75f,25)==1,"rear fade leaves the first three quarters untouched");
    check(std::abs(fluid::rearOpacity(.875f,25)-.5f)<1e-6 && fluid::rearOpacity(1,25)==0,"rear fade midpoint and transparent end");
    check(fluid::rearOpacity(1,0)==1,"rear fade can be disabled");
    auto trajectory=fluid::trajectory(xy,.2);
    check(trajectory && trajectory->points==9600 && trajectory->requested==9600,"3D XY preserves every sample in the requested time window");
    for(int i=0;i<trajectory->points-1;i+=37) {auto sample=xy.at(double(trajectory->base+trajectory->first+i));const auto packed=trajectory->data[size_t(trajectory->first+i)];check(sample.l==packed.l && sample.r==packed.r,"3D XY GPU data is faithful stereo audio, not decimated geometry");check(packed.envelope==xy.envelopeAt(double(trajectory->base+trajectory->first+i)),"3D GPU envelope matches the samplewise 2D follower");}
    fluid::Capture wrapped;wrapped.position=fluid::Capture::capacity+100;
    for(auto i=wrapped.position-16000;i<wrapped.position;++i)wrapped.ring[size_t(i)&(fluid::Capture::capacity-1)]={float(i%101)*.01f,-float(i%97)*.01f};
    trajectory=fluid::trajectory(wrapped,.2);
    for(int i=0;i<trajectory->points-1;i+=37) {auto sample=wrapped.at(double(trajectory->base+trajectory->first+i));const auto packed=trajectory->data[size_t(trajectory->first+i)];check(sample.l==packed.l&&sample.r==packed.r,"3D XY raw texture handles circular-buffer wrap");}
    {
        std::vector<fluid::Stereo> points;
        for(int i=0;i<4096;++i){double t=i*6.283185307179586/4095;points.push_back({float(200*std::cos(t)),float(200*std::sin(t))});}
        const auto reduced=fluid::reduceTrace(points);
        check(reduced.size()<points.size()/4,"Display reduction removes redundant vertices on a smooth XY curve");
        size_t segment=0;
        for(const auto p:points) {
            while(segment+2<reduced.size()) {
                const auto b=reduced[segment+1];if(p.l==b.l&&p.r==b.r){++segment;break;}
                // Advance only at the retained original point, preserving time order.
                break;
            }
            const auto a=reduced[segment],b=reduced[segment+1];const float dx=b.l-a.l,dy=b.r-a.r;
            const float t=std::clamp(((p.l-a.l)*dx+(p.r-a.r)*dy)/std::max(1e-20f,dx*dx+dy*dy),0.f,1.f);
            const float ex=p.l-a.l-t*dx,ey=p.r-a.r-t*dy;
            check(ex*ex+ey*ey<=.181f*.181f,"Reduced XY curve stays within its subpixel error bound");
        }
        std::vector<fluid::Stereo> impulse;for(int i=0;i<1000;++i)impulse.push_back({float(i),i==533?100.f:0.f});
        auto peak=fluid::reduceTrace(impulse);bool retained=false;for(auto p:peak)if(p.r==100)retained=true;
        check(retained,"XY display reduction preserves a one-sample transient");
    }
    auto response=[](double frequency) {
        fluid::BandListen filter;filter.prepare(48000);
        double in=0,out=0;float l[256],r[256];float* channels[]={l,r};
        for(int block=0;block<240;++block) {
            for(int i=0;i<256;++i)l[i]=r[i]=float(.2*std::sin(6.283185307179586*frequency*(block*256+i)/48000));
            filter.process(channels,2,256,true,800,1200);
            if(block>100)for(int i=0;i<256;++i) {double dry=.2*std::sin(6.283185307179586*frequency*(block*256+i)/48000);in+=dry*dry;out+=l[i]*l[i];check(std::isfinite(l[i])&&l[i]==r[i],"audition stability and stereo coherence");}
        }
        return std::sqrt(out/in);
    };
    const double pass=response(1000),low=response(100),high=response(8000);
    check(pass>.9&&pass<1.05,"selected band level");check(low<.001&&high<.001,"48-dB/oct audition rejects distant frequencies");
    fluid::BandListen bypass;bypass.prepare(48000);float raw[64];for(int i=0;i<64;++i)raw[i]=float(std::sin(i));float* mono[]={raw};bypass.process(mono,1,64,false,800,1200);
    for(int i=0;i<64;++i)check(raw[i]==float(std::sin(i)),"audition default bypass is bit exact");
    std::cout<<"Band listen: pass "<<pass<<", low rejection "<<20*std::log10(low)<<" dB, high rejection "<<20*std::log10(high)<<" dB\n";
    std::cout<<"PASS: XY quarter-cycle and fractional delays, mono source, internal sine frequency/phase/level\n";
    std::cout<<"PASS: "<<frames<<" triggers; maximum alignment error "<<maxError<<"; hysteresis, auto fallback, stereo and queue verified\n";
}
