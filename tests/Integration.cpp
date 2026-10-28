#include <JuceHeader.h>
#include <iostream>
#include <ctime>
#include <thread>
#include <chrono>
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
void check(bool b,const char* text) { if(!b){std::cerr<<text<<'\n';std::exit(1);} }
#include "SpectrumCheck.h"
#include "../Source/DisplayWindow.h"
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if(argc==1)checkSpectrum();
    if(argc==1) {
        fluid::MeterAnalyser analyser;
        std::vector<float> tone(192000);
        for(size_t i=0;i<tone.size();++i)tone[i]=float(.1*std::sin(6.283185307179586*1000*i/48000));
        analyser.push(tone.data(),tone.data(),int(tone.size()),48000,1);
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+2000;
        while(analyser.snapshot().seconds<4 && juce::Time::getMillisecondCounterHiRes()<deadline)juce::Thread::sleep(5);
        auto f=analyser.snapshot();check(f.seconds>=4&&std::abs(f.integrated+23)<.1,"Meter worker measures mono without duplicating its loudness");
        analyser.reset();check(analyser.snapshot().maxTruePeak==fluid::meterSilence,"Meter reset immediately invalidates queued results");
        analyser.push(tone.data(),tone.data(),int(tone.size()),48000,2);
        const auto next=juce::Time::getMillisecondCounterHiRes()+2000;
        while(analyser.snapshot().seconds<4 && juce::Time::getMillisecondCounterHiRes()<next)juce::Thread::sleep(5);
        check(std::abs(analyser.snapshot().integrated+19.99)<.1,"Worker reset and stereo channel changes start a fresh measurement");
        juce::Thread::sleep(400);f=analyser.snapshot();
        check(f.truePeak==fluid::meterSilence&&f.maxTruePeak>-21&&f.integrated>-21,"Stopped-host live meters idle while maxima and integrated loudness remain");
        check(analyser.dropped.load()==0,"Normal meter worker input is lossless");
    }
    std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
    for(int channels:{1,2}) for(double rate:{44100.,48000.,96000.,192000.}) {
        auto layout=p->getBusesLayout();layout.inputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));layout.outputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));
        check(p->setBusesLayout(layout),"bus layout");p->prepareToPlay(rate,1024);
        for(int n:{1,17,64,512,1024}) {
            juce::AudioBuffer<float> b(channels,n),original(channels,n);
            for(int ch=0;ch<channels;++ch)for(int i=0;i<n;++i)b.setSample(ch,i,float(std::sin(i*.1+ch)*.9));
            original.makeCopyOf(b);juce::MidiBuffer midi;p->processBlock(b,midi);
            for(int ch=0;ch<channels;++ch)for(int i=0;i<n;++i)check(b.getSample(ch,i)==original.getSample(ch,i),"bit exact audio passthrough");
        }
        p->releaseResources();
    }
    auto parameter=[&](const char* id)->juce::RangedAudioParameter* {for(auto* p0:p->getParameters())if(auto* rp=dynamic_cast<juce::RangedAudioParameter*>(p0))if(rp->getParameterID()==id)return rp;std::exit(1);};
    auto set=[&](const char* id,float v){auto* rp=parameter(id);rp->setValueNotifyingHost(rp->convertTo0to1(v));};
    auto* param=p->getParameters()[0];param->setValueNotifyingHost(.73f);juce::MemoryBlock state;p->getStateInformation(state);param->setValueNotifyingHost(.1f);p->setStateInformation(state.getData(),int(state.getSize()));check(std::abs(param->getValue()-.73f)<.0001f,"state restore");
    p->prepareToPlay(48000,512);
    auto* time=dynamic_cast<juce::RangedAudioParameter*>(param);time->setValueNotifyingHost(time->convertTo0to1(12.f));
    std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());
    juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
    if(argc>1 && juce::String(argv[1])=="--benchmark") {
        set("time",argc>3?juce::String(argv[3]).getFloatValue():40);set("hold",0);set("persistence",argc>2?juce::String(argv[2]).getFloatValue():120);
        const juce::String mode=argc>4?argv[4]:"scope";
        const bool meterMotion=mode=="meters";
        if(mode=="xy") {set("view",1);set("xySource",2);set("xyFreq",330);set("xyDelay",.25f);set("xyPhase",45);}
        const bool xy3d=mode.startsWith("xy3d");const double benchRate=mode=="xy3d-stress"?192000.:48000.;p->prepareToPlay(benchRate,512);
        if(xy3d){set("view",1);set("xy3D",1);set("xyFollow",1);set("xyAmp",1);set("xy3DTime",mode=="xy3d"?.2f:2.f);set("xySource",2);set("xyFreq",220);set("xyPhase",90);}
        const bool longHistory=mode=="depth-long";const int durationMs=longHistory?16000:6000;
        if(longHistory){set("view",3);set("history",12);}
        if(mode.startsWith("fft")){set("view",2);set("fftGain",12);}
        if(mode=="fft-band"){set("bandLow",1000);set("bandHigh",3000);}if(mode.startsWith("depth")&&mode!="depth-long"){set("view",3);set("history",2);set("fftGain",12);}if(mode.startsWith("quad"))set("view",4);
        if(mode.contains("clean")){set("clean",1);set("historyOrbitYaw",130);set("historyOrbitPitch",-110);set("xyOrbitYaw",120);set("xyOrbitPitch",110);}
        if(mode.contains("wire"))set("fftWireframe",1);
        const bool full=argc>5 && juce::String(argv[5])=="full";
        if(argc>6){set("historyZoom",juce::String(argv[6]).getFloatValue());set("xyZoom",juce::String(argv[6]).getFloatValue());}
        struct Window : juce::DocumentWindow {
            Window():DocumentWindow("FluidScope live rendering check",juce::Colour(0xff10161c),closeButton){}
            void closeButtonPressed() override {setVisible(false);}
        } window;
        window.setUsingNativeTitleBar(true);window.setContentNonOwned(editor.get(),true);
        window.centreWithSize(1180,900);window.setAlwaysOnTop(full);window.setVisible(true);window.toFront(true);
        if(mode=="depth-wire") {
            auto clickClean=[&]{for(int i=0;i<editor->getNumChildComponents();++i)if(auto* b=dynamic_cast<juce::ToggleButton*>(editor->getChildComponent(i)))if(b->getButtonText()=="Clean") {
                auto* hit=editor->getComponentAt(b->getBounds().getCentre());check(hit==b||b->isParentOf(hit),"Visible normal-window Clean toggle receives mouse hits");b->triggerClick();return;
            }check(false,"Visible normal-window Clean toggle exists");};
            juce::Timer::callAfterDelay(1200,clickClean);
            juce::Timer::callAfterDelay(1500,[&,clickClean]{check(parameter("clean")->getValue()==1,"Visible normal-window Clean toggle turns on");clickClean();});
            juce::Timer::callAfterDelay(1800,[&]{check(parameter("clean")->getValue()==0,"Visible normal-window Clean toggle turns off");});
        }
        if(full)juce::Timer::callAfterDelay(400,[&]{editor->keyPressed(juce::KeyPress(juce::KeyPress::F11Key));});
        auto presentation=[&]()->fluid::DisplayWindow*{for(int i=0;i<juce::Desktop::getInstance().getNumComponents();++i)if(auto* w=dynamic_cast<fluid::DisplayWindow*>(juce::Desktop::getInstance().getComponent(i)))return w;return nullptr;};
        if(full) {
            // Reproduce a DAW's floating editor being raised after fullscreen.
            juce::Timer::callAfterDelay(1100,[&]{window.toFront(true);});
            juce::Timer::callAfterDelay(1500,[&]{auto* w=presentation();check(w&&fluid::presentationAboveOwner(*w,window),"fullscreen stays above a raised floating host editor");check(window.isAlwaysOnTop(),"fullscreen preserves the host window's floating setting");});
            juce::Timer::callAfterDelay(1800,[&]{auto* w=presentation();check(w!=nullptr,"fullscreen exists before Escape");w->display.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));});
            juce::Timer::callAfterDelay(2000,[&]{check(!bool(editor->getProperties()["fullScreenOpen"]),"Escape returns to the original editor");check(window.isShowing()&&window.isAlwaysOnTop(),"Escape preserves the floating host window");editor->keyPressed(juce::KeyPress(juce::KeyPress::F11Key));});
            juce::Timer::callAfterDelay(2300,[&]{auto* w=presentation();check(w&&fluid::presentationAboveOwner(*w,window),"fullscreen reopens correctly after Escape");});
        }
        std::function<void(juce::Component&,bool)> captureGL;
        captureGL=[&](juce::Component& component,bool request){if(auto* panel=dynamic_cast<fluid::SpectrumPanel*>(&component)) {if(panel->graphicsReady()&&request)panel->requestCapture();}for(int i=0;i<component.getNumChildComponents();++i)captureGL(*component.getChildComponent(i),request);};
        juce::Timer::callAfterDelay(longHistory?15000:3000,[&]{if(full){for(int i=0;i<juce::Desktop::getInstance().getNumComponents();++i)if(auto* w=dynamic_cast<fluid::DisplayWindow*>(juce::Desktop::getInstance().getComponent(i)))captureGL(w->display,true);}else captureGL(*editor,true);});
        const auto start=juce::Time::getMillisecondCounterHiRes();const auto cpuStart=std::clock();long long sample=0;
        std::atomic<bool> running{true};
        std::thread producer([&]{
            unsigned noiseState=0x751A23u;
            while(running.load()) {
                auto target=(long long)((juce::Time::getMillisecondCounterHiRes()-start)*(benchRate*.001));
                while(sample+512<=target) {
                    for(int i=0;i<512;++i) {
                        double t=double(sample+i)/benchRate;
                        if(mode.startsWith("fft")||mode.startsWith("depth")||mode.startsWith("quad")) {
                            constexpr double tau=6.283185307179586;double beat=std::fmod(t,.5),backbeat=std::fmod(t+.25,.5);
                            double kick=.5*std::exp(-beat*22)*std::sin(tau*(52*beat+1.3*(1-std::exp(-beat*45))));
                            noiseState^=noiseState<<13;noiseState^=noiseState>>17;noiseState^=noiseState<<5;
                            double snare=.28*std::exp(-backbeat*35)*(double(noiseState&0xffff)/32768.-1);
                            double synth=.22*std::sin(tau*220*t+.6*std::sin(tau*3*t))+.08*std::sin(tau*660*t)*(1+.7*std::sin(tau*t));
                            audio.setSample(0,i,float(kick+snare+synth));audio.setSample(1,i,float(kick+snare+synth*.8+.06*std::sin(tau*880*t)));
                        } else {double modulation=meterMotion?.08+.92*std::pow(.5+.5*std::sin(6.283185307179586*2*t),3):1;
                            audio.setSample(0,i,float(modulation*(xy3d?(.12+.8*std::pow(.5+.5*std::sin(6.283185307179586*1.5*t),2)):.72)*std::sin(2*juce::MathConstants<double>::pi*220*t+.6*std::sin(2*juce::MathConstants<double>::pi*7*t))));audio.setSample(1,i,float(modulation*.65*std::sin(2*juce::MathConstants<double>::pi*220*t+.8)));}
                    }
                    p->processBlock(audio,midi);sample+=512;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        juce::Timer::callAfterDelay(durationMs,[]{juce::MessageManager::getInstance()->stopDispatchLoop();});
        juce::MessageManager::getInstance()->runDispatchLoop();
        running.store(false);producer.join();
        std::cout<<"LIVE "<<mode<<(full?" FULLSCREEN / ":" / ")<<(argc>3?argv[3]:"40")<<"-ms window, synthetic audio, "<<(argc>2?argv[2]:"120")<<"-ms phosphor: "
                 <<editor->getProperties()["paintFps"].toString()<<" paint fps / "
                 <<editor->getProperties()["updatesPerSecond"].toString()<<" new waveform frames/s / "
                 <<editor->getProperties()["sweepsPerSecond"].toString()<<" acquired sweeps/s; update "<<editor->getProperties()["updateAverageMs"].toString()<<" ms; paint "<<editor->getProperties()["paintAverageMs"].toString()<<" ms; process CPU "<<100.*double(std::clock()-cpuStart)/CLOCKS_PER_SEC/(durationMs*.001)<<"% of one core\n";
        juce::Component* shown=editor.get();
        if(full){check(bool(editor->getProperties()["fullScreenOpen"]),"detached fullscreen opens");for(int i=0;i<juce::Desktop::getInstance().getNumComponents();++i)if(auto* w=dynamic_cast<fluid::DisplayWindow*>(juce::Desktop::getInstance().getComponent(i)))shown=&w->display;}
        auto live=shown->createComponentSnapshot(shown->getLocalBounds(),true,1.f);
        int gpuImages=0;
        std::function<void(juce::Component&)> composite;
        composite=[&](juce::Component& component){if(auto* panel=dynamic_cast<fluid::SpectrumPanel*>(&component))if(auto image=panel->capturedImage()) {
            auto bounds=shown->getLocalArea(panel,panel->getLocalBounds());juce::Graphics g(live);g.drawImage(*image,bounds.toFloat());++gpuImages;
        }for(int i=0;i<component.getNumChildComponents();++i)composite(*component.getChildComponent(i));};
        composite(*shown);if(mode.startsWith("depth")||mode.startsWith("quad")||xy3d)check(gpuImages>0,"3D context renders and exports real GPU data");
        if(meterMotion)for(int i=0;i<shown->getNumChildComponents();++i)if(auto* panel=dynamic_cast<fluid::MeterPanel*>(shown->getChildComponent(i))) {
            double fps=double(panel->getProperties()["paintFps"]);std::cout<<"Meter actual paints/s: "<<fps<<'\n';check(fps>50,"Native meter animation is display-paced rather than 10/30 fps");
        }
        juce::FileOutputStream preview(juce::File::getCurrentWorkingDirectory().getChildFile("FluidScope-live-"+mode+(full?"-full":"")+".png"));preview.setPosition(0);preview.truncate();juce::PNGImageFormat().writeImageToStream(live,preview);
        if(full){editor->keyPressed(juce::KeyPress(juce::KeyPress::F11Key));check(!bool(editor->getProperties()["fullScreenOpen"]),"fullscreen returns to host");check(window.isShowing()&&window.isAlwaysOnTop(),"host editor remains visible with its original window setting");
            editor->keyPressed(juce::KeyPress(juce::KeyPress::F11Key));auto* reopened=presentation();check(reopened&&fluid::presentationAboveOwner(*reopened,window),"fullscreen can reopen with correct ordering");
            editor->keyPressed(juce::KeyPress(juce::KeyPress::F11Key));check(!bool(editor->getProperties()["fullScreenOpen"]),"F11 closes reopened fullscreen");}
        window.setVisible(false);window.clearContentComponent();return 0;
    }

    for(int frame=0;frame<160;++frame) {
        for(int i=0;i<512;++i) {double t=double(frame*512+i)/48000;audio.setSample(0,i,float(.72*std::sin(2*juce::MathConstants<double>::pi*220*t+.28*std::sin(2*juce::MathConstants<double>::pi*3*t))));audio.setSample(1,i,float(.65*std::sin(2*juce::MathConstants<double>::pi*220*t+.8)));}
        p->processBlock(audio,midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(9);
    }
    auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.f);
    juce::File f=juce::File::getCurrentWorkingDirectory().getChildFile("FluidScope-preview.png");juce::FileOutputStream out(f);out.setPosition(0);out.truncate();juce::PNGImageFormat().writeImageToStream(image,out);out.flush();
    // A knob-only timebase drag must retain a real trace even if the host has
    // delivered no fresh audio since the last GUI frame.
    for(float window:{500.f,40.f,200.f,5.f,80.f,12.f}) {
        set("time",window);juce::MessageManager::getInstance()->runDispatchLoopUntil(24);
        auto frame=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f);
        juce::Image::BitmapData pixels(frame,juce::Image::BitmapData::readOnly);
        int count=0,minY=frame.getHeight(),maxY=0;
        for(int y=214;y<editor->getHeight()-126;++y)for(int x=32;x<editor->getWidth()-32;++x) {
            const auto c=pixels.getPixelColour(x,y);
            if(c.getGreen()>160 && c.getGreen()>c.getRed()*1.3 && c.getBlue()>80) {++count;minY=std::min(minY,y);maxY=std::max(maxY,y);}
        }
        check(count>40 && maxY-minY>40,"timebase drag must never blank or replace a real waveform with zeros");
    }
    set("view",1);set("xyDelay",.25f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    image=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.f);
    juce::FileOutputStream xy(juce::File::getCurrentWorkingDirectory().getChildFile("FluidScope-preview-xy.png"));xy.setPosition(0);xy.truncate();juce::PNGImageFormat().writeImageToStream(image,xy);
    set("xySource",2);set("xyFreq",330);set("xyPhase",45);set("xyAmp",.65f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    image=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.f);
    juce::FileOutputStream osc(juce::File::getCurrentWorkingDirectory().getChildFile("FluidScope-preview-osc.png"));osc.setPosition(0);osc.truncate();juce::PNGImageFormat().writeImageToStream(image,osc);
    p->getStateInformation(state);set("xyDelay",5);set("xySource",0);set("xyFreq",100);
    p->setStateInformation(state.getData(),int(state.getSize()));
    check(std::abs(parameter("xyDelay")->convertFrom0to1(parameter("xyDelay")->getValue())-.25f)<1e-4,"XY delay state restore");
    check(std::abs(parameter("xyFreq")->convertFrom0to1(parameter("xyFreq")->getValue())-330.f)<.01f,"oscillator frequency restore");
    check(parameter("xySource")->convertFrom0to1(parameter("xySource")->getValue())==2,"XY source restore");
    juce::AudioBuffer<float> passthrough(2,64);for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)passthrough.setSample(ch,i,float(.5*std::sin(i*.2+ch)));
    juce::AudioBuffer<float> original;original.makeCopyOf(passthrough);p->processBlock(passthrough,midi);
    for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)check(passthrough.getSample(ch,i)==original.getSample(ch,i),"internal oscillator must not alter audio");
    editor->setSize(940,740);juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    image=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.f);
    juce::FileOutputStream small(juce::File::getCurrentWorkingDirectory().getChildFile("FluidScope-preview-small.png"));small.setPosition(0);small.truncate();juce::PNGImageFormat().writeImageToStream(image,small);
    set("view",2);set("fftWeight",1);set("fftAverage",1);
    for(int block=0;block<180;++block) {
        for(int i=0;i<512;++i){double t=double(block*512+i)/48000;float a=float(.5*std::sin(6.283185307179586*80*t)+.25*std::sin(6.283185307179586*440*t)+.12*std::sin(6.283185307179586*1800*t));audio.setSample(0,i,a);audio.setSample(1,i,-a);}
        p->processBlock(audio,midi);juce::MessageManager::getInstance()->runDispatchLoopUntil(4);
    }
    image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f);
    juce::FileOutputStream fft(juce::File::getCurrentWorkingDirectory().getChildFile("FluidScope-preview-fft.png"));fft.setPosition(0);fft.truncate();juce::PNGImageFormat().writeImageToStream(image,fft);
    editor->setVisible(true);
    check(parameter("view")->getText(0,40)=="Oscilloscope","Waveform view has a consistent descriptive name");
    for(int width:{940,1180})for(int view=0;view<5;++view) {
        editor->setSize(width,width==940?740:900);set("view",float(view));set("meters",1);set("clean",1);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
        fluid::MeterPanel* panel=nullptr;juce::ToggleButton* toggle=nullptr;
        for(int i=0;i<editor->getNumChildComponents();++i) {
            auto* child=editor->getChildComponent(i);
            if(auto* meter=dynamic_cast<fluid::MeterPanel*>(child))panel=meter;
            if(auto* button=dynamic_cast<juce::ToggleButton*>(child))if(button->getButtonText()=="Meters")toggle=button;
        }
        check(panel&&panel->isVisible()&&panel->getHeight()>=500,"Meter sidebar fits the minimum editor and stays independent of Clean");
        check(toggle&&editor->getComponentAt(toggle->getBounds().getCentre())==toggle,"Meter toggle has an unobstructed hit target");
        for(int i=0;i<panel->getNumChildComponents();++i) {
            auto* button=panel->getChildComponent(i);
            check(panel->getLocalBounds().contains(button->getBounds()),"Meter Reset and Settings remain inside the sidebar");
            check(panel->getComponentAt(button->getBounds().getCentre())==button,"Meter controls are clickable at minimum size");
        }
        for(int i=0;i<editor->getNumChildComponents();++i) {
            auto* child=editor->getChildComponent(i);
            if(child->isVisible()&&(dynamic_cast<fluid::SpectrumPanel*>(child)||dynamic_cast<fluid::SignalPanel*>(child)))
                check(!child->getBounds().intersects(panel->getBounds()),"Meter sidebar never covers a plot in any display mode");
        }
        toggle->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        check(parameter("meters")->getValue()==0&&!panel->isVisible(),"Meter toggle hides the sidebar in the normal editor");
        toggle->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        check(parameter("meters")->getValue()==1&&panel->isVisible(),"Meter toggle restores the sidebar");
    }
    for(int width:{940,1180})for(int view=0;view<5;++view) {
        editor->setSize(width,900);set("view",float(view));set("clean",0);juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
        for(const auto text:{juce::String("Clean"),juce::String("Wireframe")}) {
            if(text=="Wireframe"&&view!=3&&view!=4)continue;
            juce::ToggleButton* button=nullptr;
            for(int i=0;i<editor->getNumChildComponents();++i)if(auto* b=dynamic_cast<juce::ToggleButton*>(editor->getChildComponent(i)))if(b->getButtonText()==text)button=b;
            check(button&&button->isVisible(),"Normal editor toggle exists and is visible");
            for(float fraction:{.15f,.5f,.85f}) {
                const auto bounds=button->getBounds();auto* hit=editor->getComponentAt(bounds.getX()+int(bounds.getWidth()*fraction),bounds.getCentreY());
                if(!(hit==button||button->isParentOf(hit)))std::cerr<<"Hit test: "<<text<<" view "<<view<<" width "<<width<<" fraction "<<fraction<<" hit "<<(hit?typeid(*hit).name():"null")<<std::endl;
                check(hit==button||button->isParentOf(hit),"Normal editor toggle hit area is not covered by a label");
            }
            const auto id=text=="Clean"?"clean":"fftWireframe";set(id,0);button->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
            check(parameter(id)->getValue()==1,"Clicking the normal editor toggle changes the actual parameter");
            button->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(100);check(parameter(id)->getValue()==0,"Normal editor toggle switches back off");
        }
    }
    for(int mode:{3,1}) {
        set("view",float(mode));set("xy3D",1);juce::MessageManager::getInstance()->runDispatchLoopUntil(35);
        fluid::SpectrumPanel* target=nullptr;
        for(int i=0;i<editor->getNumChildComponents();++i)if(auto* panel=dynamic_cast<fluid::SpectrumPanel*>(editor->getChildComponent(i)))if(panel->isVisible())target=panel;
        check(target!=nullptr,"3D zoom target is visible");
        const auto now=juce::Time::getCurrentTime();juce::MouseEvent e(juce::Desktop::getInstance().getMainMouseSource(),{120,120},{},0,0,0,0,0,target,target,now,{120,120},now,1,false);
        const char* id=mode==3?"historyZoom":"xyZoom";juce::MouseWheelDetails wheel{};wheel.deltaY=.4f;
        target->mouseWheelMove(e,wheel);check(parameter(id)->convertFrom0to1(parameter(id)->getValue())>1.5f,"Mouse wheel zooms the actual 3D view");
        wheel.deltaY=-100;target->mouseWheelMove(e,wheel);check(std::abs(parameter(id)->convertFrom0to1(parameter(id)->getValue())-.5f)<1e-5,"3D zoom lower bound");
        wheel.deltaY=100;target->mouseWheelMove(e,wheel);check(std::abs(parameter(id)->convertFrom0to1(parameter(id)->getValue())-3.f)<1e-5,"3D zoom upper bound");
        target->mouseDoubleClick(e);check(std::abs(parameter(id)->convertFrom0to1(parameter(id)->getValue())-1.f)<1e-5,"Double click restores the 3D camera zoom");
        const char* orbitYaw=mode==3?"historyOrbitYaw":"xyOrbitYaw",*orbitPitch=mode==3?"historyOrbitPitch":"xyOrbitPitch";
        target->mouseDown(e);target->mouseDrag(e.withNewPosition(juce::Point<float>(2120,-1080)));target->mouseUp(e);
        check(std::abs(parameter(orbitYaw)->convertFrom0to1(parameter(orbitYaw)->getValue()))>90,"3D yaw rotates beyond the former narrow bounds and wraps after a revolution");
        check(parameter(orbitPitch)->convertFrom0to1(parameter(orbitPitch)->getValue())<-40,"3D elevation reaches below the original coordinate plane");
        target->mouseDoubleClick(e);check(std::abs(parameter(orbitYaw)->convertFrom0to1(parameter(orbitYaw)->getValue()))<1e-5 && std::abs(parameter(orbitPitch)->convertFrom0to1(parameter(orbitPitch)->getValue()))<1e-5,"Double click resets both orbit offsets");
    }
    check(parameter("cameraYaw")->getNormalisableRange().start==-32 && parameter("cameraPitch")->getNormalisableRange().start==18,"Legacy camera automation ranges remain unchanged");
    for(int display:{2,4})for(int scale:{0,1})for(int listen:{0,1}) {
        set("view",float(display));set("fftScale",float(scale));set("clean",0);set("listen",float(listen));set("bandLow",1000);set("bandHigh",3000);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
        fluid::SpectrumPanel* target=nullptr;
        for(int i=0;i<editor->getNumChildComponents();++i)if(auto* panel=dynamic_cast<fluid::SpectrumPanel*>(editor->getChildComponent(i)))if(panel->isVisible()){target=panel;break;}
        check(target!=nullptr,"2D FFT band target is visible in spectrum and Quad");
        auto val=[&](const char* id){return parameter(id)->convertFrom0to1(parameter(id)->getValue());};
        const float width=float(target->getWidth()-68),height=float(target->getHeight()-66);
        auto x=[&](float hz){return 48+width*fluid::spectrumX(hz,scale==0,20000);};
        const float handleY=25+height-std::min(46.f,height*.22f);
        auto event=[&](juce::Point<float> point,juce::ModifierKeys mods=juce::ModifierKeys{}){auto now=juce::Time::getCurrentTime();return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,mods,0,0,0,0,0,target,target,now,point,now,1,false);};
        auto drag=[&](juce::Point<float> from,juce::Point<float> to){auto e=event(from);target->mouseDown(e);target->mouseDrag(e.withNewPosition(to));target->mouseUp(e.withNewPosition(to));};
        juce::Point<float> grip{(x(1000)+x(3000))*.5f,handleY};auto press=event(grip);
        target->mouseDown(press);check(std::abs(val("bandLow")-1000)<.01&&std::abs(val("bandHigh")-3000)<.01,"Grabbing an existing band does not recreate or jump its endpoints");
        target->mouseDrag(press.withNewPosition(grip+juce::Point<float>{width*.08f,0}));target->mouseUp(press);
        check(val("bandLow")>1000&&val("bandHigh")>3000,"Centre grip sweeps the selected band upward");
        check(scale==0?std::abs(val("bandHigh")/val("bandLow")-3)<.001:std::abs(val("bandHigh")-val("bandLow")-2000)<.1,"GUI band sweep retains its width in the active scale");
        check(val("listen")==float(listen),"Moving a band preserves the explicit audition state");
        for(float beyond:{-100000.f,100000.f}) {
            set("bandLow",1000);set("bandHigh",3000);drag(grip,{beyond,handleY});
            check(val("bandLow")>=19.99&&val("bandHigh")<=20000.01,"Dragging beyond the canvas clamps the whole band");
            check(scale==0?std::abs(val("bandHigh")/val("bandLow")-3)<.001:std::abs(val("bandHigh")-val("bandLow")-2000)<.1,"Band width survives a GUI drag to either frequency limit");
        }
        set("bandLow",1000);set("bandHigh",3000);
        drag({x(1000),25+height*.5f},{x(500),25+height*.5f});
        check(std::abs(val("bandLow")-500)<1&&std::abs(val("bandHigh")-3000)<.1,"Left edge handle adjusts only the lower frequency");
        drag({x(3000),25+height*.5f},{x(4000),25+height*.5f});
        check(std::abs(val("bandLow")-500)<1&&std::abs(val("bandHigh")-4000)<1,"Right edge handle adjusts only the upper frequency");
        if(display==2&&scale==0&&listen==0) {
            set("bandLow",1000);set("bandHigh",3000);set("listen",0);
            auto alt=event(grip,juce::ModifierKeys(juce::ModifierKeys::altModifier|juce::ModifierKeys::leftButtonModifier));
            int phase=0;
            auto outsideBandRms=[&]{
                double power=0;juce::AudioBuffer<float> b(2,256);
                for(int block=0;block<80;++block) {
                    for(int i=0;i<256;++i){float sample=float(.2*std::sin(6.283185307179586*200*(phase++)/48000));b.setSample(0,i,sample);b.setSample(1,i,sample);}
                    p->processBlock(b,midi);
                    if(block>=64)for(int i=0;i<256;++i)power+=b.getSample(0,i)*b.getSample(0,i);
                }
                return std::sqrt(power/(16*256));
            };
            target->mouseDown(alt);
            check(val("listen")==0,"Alt preview never writes the persistent Listen parameter");
            check(outsideBandRms()<.005,"Alt plus centre grip actually auditions the selected band");
            juce::MemoryBlock previewState;p->getStateInformation(previewState);
            target->mouseUp(alt);
            check(outsideBandRms()>.13,"Mouse release restores original audio after momentary preview");
            p->setStateInformation(previewState.getData(),int(previewState.getSize()));
            check(val("listen")==0&&outsideBandRms()>.13,"Saving during Alt preview never restores a latched audition");
            target->mouseDown(alt);target->modifierKeysChanged(juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier));
            check(outsideBandRms()>.13,"Releasing Alt ends preview even while the mouse remains held");target->mouseUp(alt);
            target->mouseDown(alt);target->focusLost(juce::Component::focusChangedDirectly);
            check(outsideBandRms()>.13,"Focus loss cannot leave momentary band audition latched");target->mouseUp(alt);
            target->mouseDown(alt);target->setVisible(false);target->setVisible(true);
            check(outsideBandRms()>.13,"Changing or hiding the view cancels momentary audition");target->mouseUp(alt);
        }
        if(listen==1) {
            set("bandLow",1000);set("bandHigh",3000);
            auto alt=event(grip,juce::ModifierKeys(juce::ModifierKeys::altModifier|juce::ModifierKeys::leftButtonModifier));
            target->mouseDown(alt);target->mouseUp(alt);check(val("listen")==1,"Alt preview preserves an already enabled manual audition");
        }
    }
    set("listen",0);set("fftScale",0);
    set("view",1);set("xy3D",0);set("clean",0);juce::MessageManager::getInstance()->runDispatchLoopUntil(35);
    auto gridCount=[&]{auto picture=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f);int n=0;auto grid=fluid::appearance(int(parameter("style")->convertFrom0to1(parameter("style")->getValue())),0x56e2c2,0xf3ac71).grid;
        for(int y=370;y<picture.getHeight()-135;++y)for(int x=40;x<picture.getWidth()-254;++x)if(picture.getPixelAt(x,y)==grid)++n;return n;};
    const auto normalGrid=gridCount();set("clean",1);juce::MessageManager::getInstance()->runDispatchLoopUntil(25);const auto cleanGrid=gridCount();
    check(normalGrid>500 && cleanGrid<normalGrid/10,"Clean display removes the actual XY grid pixels");
    set("clean",0);
    set("view",2);set("xy3D",0);juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f);
    set("xyFollow",1);
    set("fftGain",24);juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto gained=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f);
    int changedPixels=0;
    for(int y=380;y<gained.getHeight()-60;++y)for(int x=70;x<gained.getWidth()-60;++x)if(gained.getPixelAt(x,y)!=image.getPixelAt(x,y))++changedPixels;
    check(changedPixels>1000,"FFT visual gain shifts the rendered spectrum");
    juce::AudioBuffer<float> gainOriginal;gainOriginal.makeCopyOf(audio);p->processBlock(audio,midi);
    for(int ch=0;ch<2;++ch)for(int i=0;i<audio.getNumSamples();++i)check(audio.getSample(ch,i)==gainOriginal.getSample(ch,i),"FFT visual gain leaves audio bit-exact");
    set("rearFade",25);set("xy3DTime",.35f);set("historyDecay",65);set("xyZoom",1.8f);set("historyZoom",1.4f);set("clean",1);set("historyOrbitYaw",150);set("fftWireframe",1);set("xyOrbitPitch",-120);
    set("tpLimit",-2.5f);set("lufsGreen",-18);set("lufsRed",-6);set("meterAutoReset",1);set("meters",0);
    set("style",1);set("lineWidth",2.2f);set("bandLow",55);set("bandHigh",140);set("listen",1);
    p->getStateInformation(state);set("tpLimit",0);set("lufsGreen",-14);set("lufsRed",-8);set("meterAutoReset",0);set("meters",1);set("style",0);set("listen",0);set("fftGain",0);set("xyFollow",0);set("xyZoom",1);set("historyZoom",1);set("clean",0);set("historyOrbitYaw",0);set("xyOrbitPitch",0);set("fftWireframe",0);p->setStateInformation(state.getData(),int(state.getSize()));
    check(parameter("style")->convertFrom0to1(parameter("style")->getValue())==1 && parameter("listen")->getValue()==1,"appearance and audition state restore");check(std::abs(parameter("rearFade")->convertFrom0to1(parameter("rearFade")->getValue())-25)<.01f,"rear fade persists");check(std::abs(parameter("xy3DTime")->convertFrom0to1(parameter("xy3DTime")->getValue())-.35f)<.001f,"3D XY time persists");check(std::abs(parameter("historyDecay")->convertFrom0to1(parameter("historyDecay")->getValue())-65)<.01f,"3D visual decay persists");check(std::abs(parameter("fftGain")->convertFrom0to1(parameter("fftGain")->getValue())-24)<.01f,"FFT visual gain persists");check(parameter("xyFollow")->getValue()==1,"XY envelope-follow toggle persists");check(std::abs(parameter("xyZoom")->convertFrom0to1(parameter("xyZoom")->getValue())-1.8f)<.001f && std::abs(parameter("historyZoom")->convertFrom0to1(parameter("historyZoom")->getValue())-1.4f)<.001f,"Independent 3D zoom levels persist");check(parameter("clean")->getValue()==1 && std::abs(parameter("historyOrbitYaw")->convertFrom0to1(parameter("historyOrbitYaw")->getValue())-150)<.001f && std::abs(parameter("xyOrbitPitch")->convertFrom0to1(parameter("xyOrbitPitch")->getValue())+120)<.001f,"Clean mode and full orbit settings persist");check(parameter("fftWireframe")->getValue()==1,"Wireframe state persists");set("fftWireframe",0);set("clean",0);set("listen",0);set("fftGain",0);
    check(std::abs(parameter("tpLimit")->convertFrom0to1(parameter("tpLimit")->getValue())+2.5f)<.001f&&parameter("meterAutoReset")->getValue()==1&&parameter("meters")->getValue()==0,"Meter threshold, transport reset and visibility survive a saved project");
    check(std::abs(parameter("lufsGreen")->convertFrom0to1(parameter("lufsGreen")->getValue())+18)<.001f&&std::abs(parameter("lufsRed")->convertFrom0to1(parameter("lufsRed")->getValue())+6)<.001f,"LUFS colour endpoints survive a saved project");
    set("meters",1);set("meterAutoReset",0);set("tpLimit",-1);set("lufsGreen",-14);set("lufsRed",-8);
    set("bandLow",800);set("bandHigh",1200);set("listen",1);
    std::array<double,3> sine{},cosine{};constexpr double frequencies[]={100,1000,8000};int measured=0;
    for(int block=0;block<160;++block) {
        for(int i=0;i<512;++i){double t=double(block*512+i)/48000;float a=0;for(double f:frequencies)a+=float(.1*std::sin(6.283185307179586*f*t));audio.setSample(0,i,a);audio.setSample(1,i,-a);}
        p->processBlock(audio,midi);
        if(block>=100)for(int i=0;i<512;++i){double t=double(block*512+i)/48000;for(int k=0;k<3;++k){sine[size_t(k)]+=audio.getSample(0,i)*std::sin(6.283185307179586*frequencies[k]*t);cosine[size_t(k)]+=audio.getSample(0,i)*std::cos(6.283185307179586*frequencies[k]*t);}++measured;}
    }
    std::array<double,3> magnitude{};for(int k=0;k<3;++k)magnitude[size_t(k)]=2*std::hypot(sine[size_t(k)],cosine[size_t(k)])/measured;
    check(magnitude[1]>.085&&magnitude[1]<.105,"processor band audition passes the chosen tone");
    check(magnitude[0]<.001&&magnitude[2]<.001,"processor band audition attenuates other tones");
    set("listen",0);
    for(int block=0;block<10;++block){for(int ch=0;ch<2;++ch)for(int i=0;i<512;++i)audio.setSample(ch,i,float(std::sin(i*.13+ch)));p->processBlock(audio,midi);}
    for(int ch=0;ch<2;++ch)for(int i=0;i<512;++i)check(audio.getSample(ch,i)==float(std::sin(i*.13+ch)),"audition fade returns to bit-exact original audio");
    set("bandLow",55);set("bandHigh",140);
    for(int block=0;block<120;++block){for(int i=0;i<512;++i){double t=double(block*512+i)/48000;float a=float(.5*std::sin(6.283185307179586*80*t)+.25*std::sin(6.283185307179586*440*t)+.12*std::sin(6.283185307179586*1800*t));audio.setSample(0,i,a);audio.setSample(1,i,a);}p->processBlock(audio,midi);juce::MessageManager::getInstance()->runDispatchLoopUntil(3);}
    set("style",3);juce::MessageManager::getInstance()->runDispatchLoopUntil(24);
    image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f);juce::FileOutputStream monoImage(juce::File::getCurrentWorkingDirectory().getChildFile("FluidScope-preview-mono.png"));monoImage.setPosition(0);monoImage.truncate();juce::PNGImageFormat().writeImageToStream(image,monoImage);
    set("style",1);juce::MessageManager::getInstance()->runDispatchLoopUntil(24);
    image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f);juce::FileOutputStream greenImage(juce::File::getCurrentWorkingDirectory().getChildFile("FluidScope-preview-green.png"));greenImage.setPosition(0);greenImage.truncate();juce::PNGImageFormat().writeImageToStream(image,greenImage);
    set("fftScale",1);juce::MessageManager::getInstance()->runDispatchLoopUntil(24);image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f);juce::FileOutputStream linear(juce::File::getCurrentWorkingDirectory().getChildFile("FluidScope-preview-linear.png"));linear.setPosition(0);linear.truncate();juce::PNGImageFormat().writeImageToStream(image,linear);
    std::cout<<"PASS: XY source, oscillator/delay state, unchanged audio with oscillator enabled, minimum editor size; mono/stereo, four sample rates, variable blocks, bit-exact passthrough, state restore and editor render\n";
}
