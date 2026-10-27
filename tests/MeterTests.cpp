#include "../Source/Metering.h"
#include <cstdlib>
#include <iostream>
#include <limits>
static void check(bool ok,const char* message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main() {
    fluid::MeterDSP meter;
    for(double rate:{44100.,48000.,96000.,192000.})for(int channels:{1,2}) {
        meter.prepare(rate,channels);
        for(int i=0;i<int(rate*4);++i){float v=float(.1*std::sin(6.283185307179586*1000*i/rate));meter.push(v,v);}
        auto frame=meter.frame();
        const double expected=-23.0+(channels==2?10*std::log10(2.):0);
        check(std::abs(frame.momentary-expected)<.08,"K-weighted 1-kHz momentary calibration across sample rates");
        check(std::abs(frame.shortTerm-expected)<.08,"3-second short-term loudness calibration");
        check(std::abs(frame.integrated-expected)<.08,"Gated integrated calibration and mono channel count");
        check(std::abs(frame.rms[0]+23.0103)<.005,"Unweighted 400-ms RMS calibration");
        check(std::abs(frame.maxTruePeak+20)<.05,"True Peak sine calibration");
        std::cout<<rate<<" Hz / "<<channels<<" channel: M "<<frame.momentary<<", S "<<frame.shortTerm<<", I "<<frame.integrated<<", TP "<<frame.maxTruePeak<<'\n';
    }
    meter.prepare(48000,2);
    for(int i=0;i<48000;++i) {
        float v=float(.9*std::sin(3.141592653589793*.5*i+3.141592653589793*.25));meter.push(v,-v);
    }
    check(meter.frame().maxTruePeak>meter.frame().hold[0]+2.8,"True Peak catches an intersample over missed by sample peak");
    check(std::abs(meter.frame().truePeak-20*std::log10(.9))<.1,"Intersample peak reconstruction accuracy");
    auto held=meter.frame().maxTruePeak;
    for(int i=0;i<48000;++i)meter.push(0,0);
    check(meter.frame().rms[0]==fluid::meterSilence,"RMS goes silent after its finite window");
    check(meter.frame().momentary==fluid::meterSilence,"Momentary has no artificial release tail");
    check(meter.frame().maxTruePeak==held,"True Peak maximum survives silence");
    check(meter.frame().shortTerm==fluid::meterSilence,"Short term is unavailable before three seconds");
    meter.reset();
    check(meter.frame().maxTruePeak==fluid::meterSilence&&meter.frame().integrated==fluid::meterSilence,"Reset clears retained measurements");
    for(int i=0;i<48000;++i)meter.push(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity());
    check(meter.frame().integrated==fluid::meterSilence&&meter.frame().peak[0]==fluid::meterSilence,"Nonfinite inputs cannot poison the meter");
    meter.prepare(48000,1);
    for(int i=0;i<48000*8;++i) {
        double amp=i<48000*2||i>=48000*6?.001:.1;
        meter.push(float(amp*std::sin(6.283185307179586*1000*i/48000)),0);
    }
    // Independent FFmpeg ebur128 reference: -23.3 LUFS. Overlapping blocks at
    // the two level transitions legitimately lower I below the steady -23.0.
    check(std::abs(meter.frame().integrated+23.3)<.06,"Relative gate matches the reference including transition blocks");
    std::cout<<"PASS: loudness, RMS, True Peak, gating, silence, mono/stereo and reset\n";
}
