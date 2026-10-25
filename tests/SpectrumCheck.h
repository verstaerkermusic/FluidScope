#pragma once
#include "../Source/Spectrum.h"
inline void checkSpectrum() {
    fluid::SpectrumAnalyser analyser;analyser.rate.store(48000);analyser.frequencySmooth.store(0);analyser.smoothMs.store(180);analyser.requested.store(true);
    float l[512],r[512];int cursor=0;
    for(int block=0;block<110;++block) {
        for(int i=0;i<512;++i){double t=double(cursor++)/48000;l[i]=float(.5*std::sin(6.283185307179586*1000*t)+.1*std::sin(6.283185307179586*5000*t));r[i]=-l[i];}
        analyser.push(l,r,512);
    }
    std::shared_ptr<const fluid::SpectrumFrame> frame;
    for(int i=0;i<400;++i){frame=analyser.snapshot();if(frame && frame->time>1.1)break;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    check(frame && frame->time>1.1,"FFT worker publishes frames independently of the GUI");
    float fundamental=fluid::spectrumValue(frame->db,1000,frame->maxFrequency),harmonic=fluid::spectrumValue(frame->db,5000,frame->maxFrequency);
    std::cout<<"Measured FFT: "<<fundamental<<" / "<<harmonic<<std::endl;
    check(fundamental>-11 && fundamental<-4,"FFT amplitude calibration / antiphase stereo must not cancel");
    check(harmonic<fundamental-7,"FFT identifies unequal tone levels");
    check(fluid::spectrumValue(frame->db,10000,frame->maxFrequency)<-90,"FFT numerical noise floor");
    for(bool log:{false,true})for(float f:{20.f,60.f,1000.f,19000.f})check(std::abs(fluid::spectrumFrequency(fluid::spectrumX(f,log,20000),log,20000)-f)<f*.0001f,"linear/log frequency mapping");
    int band=juce::roundToInt(fluid::spectrumX(1000,true,frame->maxFrequency)*(fluid::SpectrumFrame::bands-1));
    int column=juce::roundToInt(float(band)*(fluid::SpectrumFrame::columns-1)/(fluid::SpectrumFrame::bands-1));
    check(frame->history[size_t(column)]>-25,"FFT history contains the measured current tone");
    check(frame->history.back()<-140,"FFT history does not invent past audio");
    auto before=frame;
    analyser.historySeconds.store(12);
    analyser.smoothMs.store(400);std::fill(l,l+512,0);std::fill(r,r+512,0);
    const double target=frame->time+.4;
    for(int block=0;block<40;++block)analyser.push(l,r,512);
    for(int i=0;i<400;++i){frame=analyser.snapshot();if(frame && frame->time>=target)break;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    float tail=fluid::spectrumValue(frame->db,1000,frame->maxFrequency);check(tail<fundamental-1 && tail>fundamental-30,"FFT release is smooth rather than an instantaneous drop");
    check(frame->history[size_t(column)]<-130,"3D short decay follows silence independently of the slower 2D release");
    int matched=0;
    for(int old=1;old<before->rowCount;++old) {
        const double timestamp=before->time-before->ages[size_t(old)]*before->seconds;
        for(int now=1;now<frame->rowCount;++now)if(std::abs(frame->time-frame->ages[size_t(now)]*frame->seconds-timestamp)<1e-5) {
            ++matched;
            for(int c=0;c<fluid::SpectrumFrame::columns;++c)check(before->history[size_t(old*fluid::SpectrumFrame::columns+c)]==frame->history[size_t(now*fluid::SpectrumFrame::columns+c)],"Captured spectrum heights remain fixed while long history advances");
            break;
        }
    }
    check(matched>10,"3D history retains timestamped measurements across changes in history length");
    analyser.historyDecayMs.store(0);
    for(int block=0;block<4;++block)analyser.push(l,r,512);
    const auto previousSequence=frame->sequence;
    for(int i=0;i<400;++i){frame=analyser.snapshot();if(frame->sequence>previousSequence+2)break;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    check(frame->history[size_t(column)]<-140,"Zero visual decay introduces no release tail");
    std::cout<<"Spectrum PASS: 1k "<<fundamental<<" dBFS; 5k "<<harmonic<<" dBFS; release "<<tail<<" dBFS; stereo, axes and history verified\n";
}
