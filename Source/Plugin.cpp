// FluidScope: a combined oscilloscope/XY evolution of the SocaLabs slPlugins concept.
// Original slPlugins copyright 2020 Roland Rabien; see ../../../LICENSE.md.
#include <JuceHeader.h>
#include "Capture.h"
#include "XYSource.h"
#include "TraceReduction.h"
#include "BandListen.h"
#include "SpectrumPanel.h"
#include "DisplayWindow.h"

class FluidProcessor : public juce::AudioProcessor {
public:
    fluid::Queue queue;
    std::atomic<double> rate{48000};
    juce::AudioProcessorValueTreeState state;
    fluid::SpectrumAnalyser analyser;
    fluid::BandListen audition;
    static juce::AudioProcessorValueTreeState::ParameterLayout parameters() {
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        auto number = [&](const char* id, const char* name, float a, float b, float initial, float skew = 1.f) {
            p.add(std::make_unique<juce::AudioParameterFloat>(id,name,juce::NormalisableRange<float>(a,b,0,skew),initial));
        };
        number("time","Window ms",.2f,500.f,20.f,.3f);
        number("gain","Display gain",.1f,16.f,1.f,.4f);
        number("level","Trigger level",-1.f,1.f,0.f);
        number("hyst","Hysteresis",.0001f,.25f,.015f,.4f);
        number("hold","Holdoff ms",0.f,500.f,5.f,.35f);
        number("pre","Pre-trigger %",0.f,80.f,20.f);
        number("persistence","Persistence ms",0.f,1000.f,120.f,.5f);
        p.add(std::make_unique<juce::AudioParameterChoice>("mode","Acquisition",juce::StringArray{"Auto","Normal","Free"},0));
        p.add(std::make_unique<juce::AudioParameterChoice>("source","Trigger source",juce::StringArray{"Left","Right","Mid"},0));
        p.add(std::make_unique<juce::AudioParameterChoice>("slope","Trigger slope",juce::StringArray{"Rising","Falling"},0));
        p.add(std::make_unique<juce::AudioParameterChoice>("view","View",juce::StringArray{"Stereo","XY / Lissajous","FFT / Spectrum","3D / History","Quad"},0));
        number("xyDelay","XY Y delay ms",0.f,100.f,0.f,.35f);
        number("xyFreq","XY oscillator Hz",.1f,4000.f,220.f,.3f);
        number("xyPhase","XY oscillator phase",0.f,360.f,0.f);
        number("xyAmp","XY oscillator amplitude",0.f,1.f,.75f);
        p.add(std::make_unique<juce::AudioParameterChoice>("xySource","XY Y source",juce::StringArray{"Right input","Left input","Internal sine"},0));
        p.add(std::make_unique<juce::AudioParameterChoice>("style","Appearance",juce::StringArray{"Modern mint","Classic green","Amber CRT","Monochrome","Paper","Ice","Neon","Custom"},0));
        number("lineWidth","Line width px",.6f,4.f,1.3f);
        number("glow","Glow percent",0.f,30.f,10.f);
        number("colourL","Custom left RGB",0.f,16777215.f,float(0x56e2c2));
        number("colourR","Custom right RGB",0.f,16777215.f,float(0xf3ac71));
        p.add(std::make_unique<juce::AudioParameterChoice>("fftScale","Frequency scale",juce::StringArray{"Log frequency","Linear frequency"},0));
        p.add(std::make_unique<juce::AudioParameterChoice>("fftSource","Spectrum source",juce::StringArray{"Stereo power","Left","Right"},0));
        p.add(std::make_unique<juce::AudioParameterChoice>("fftWeight","Spectrum weighting",juce::StringArray{"Flat","Music +3 dB/oct","Bright +4.5 dB/oct","Custom tilt"},1));
        p.add(std::make_unique<juce::AudioParameterChoice>("fftAverage","Frequency smoothing",juce::StringArray{"None","1/24 octave","1/12 octave","1/6 octave"},1));
        number("fftSmooth","Spectrum release ms",20.f,1500.f,180.f,.4f);
        number("fftTilt","Custom tilt dB/oct",0.f,6.f,3.f);
        number("fftFloor","Spectrum floor dB",-120.f,-36.f,-96.f);
        number("history","History seconds",2.f,12.f,6.f);
        number("cameraYaw","History rotation",-32.f,32.f,-14.f);
        number("cameraPitch","History elevation",18.f,60.f,35.f);
        p.add(std::make_unique<juce::AudioParameterBool>("listen","Listen to selected band",false));
        number("bandLow","Selected band low Hz",20.f,20000.f,20.f,.25f);
        number("bandHigh","Selected band high Hz",20.f,20000.f,20000.f,.25f);
        number("rearFade","3D rear fade percent",0.f,50.f,25.f);
        p.add(std::make_unique<juce::AudioParameterBool>("xy3D","XY time depth",false));
        number("xy3DTime","XY time depth seconds",.02f,2.f,.2f,.4f);
        number("xyYaw","XY rotation",-32.f,32.f,-14.f);number("xyPitch","XY elevation",18.f,60.f,35.f);
        number("historyDecay","3D visual decay ms",0.f,500.f,40.f,.4f);
        number("fftGain","FFT visual gain dB",-36.f,36.f,0.f);
        p.add(std::make_unique<juce::AudioParameterBool>("xyFollow","Internal sine follows left input",false));
        number("historyZoom","3D spectrum zoom",.5f,3.f,1.f);
        number("xyZoom","3D XY zoom",.5f,3.f,1.f);
        number("historyOrbitYaw","History orbit yaw",-180.f,180.f,0.f);
        number("historyOrbitPitch","History orbit pitch",-180.f,180.f,0.f);
        number("xyOrbitYaw","XY orbit yaw",-180.f,180.f,0.f);
        number("xyOrbitPitch","XY orbit pitch",-180.f,180.f,0.f);
        p.add(std::make_unique<juce::AudioParameterBool>("clean","Clean display",false));
        p.add(std::make_unique<juce::AudioParameterBool>("fftWireframe","3D FFT wireframe",false));
        return p;
    }
    FluidProcessor() : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)), state(*this,nullptr,"FluidScope",parameters()) {}
    const juce::String getName() const override { return "FluidScope"; }
    void prepareToPlay(double sr,int) override { rate.store(sr);analyser.rate.store(sr);audition.prepare(sr); }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& b) const override {
        return b.getMainInputChannelSet() == b.getMainOutputChannelSet() && (b.getMainInputChannelSet() == juce::AudioChannelSet::stereo() || b.getMainInputChannelSet() == juce::AudioChannelSet::mono());
    }
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals;
        if (b.getNumChannels()) {
            const auto* l=b.getReadPointer(0);const auto* r=b.getReadPointer(b.getNumChannels()>1?1:0);
            queue.push(l,r,b.getNumSamples());analyser.push(l,r,b.getNumSamples());
            audition.process(b.getArrayOfWritePointers(),b.getNumChannels(),b.getNumSamples(),state.getRawParameterValue("listen")->load()>.5f,state.getRawParameterValue("bandLow")->load(),state.getRawParameterValue("bandHigh")->load());
        }
    }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& b) override { auto xml=state.copyState().createXml(); copyXmlToBinary(*xml,b); }
    void setStateInformation(const void* d,int n) override { auto xml=getXmlFromBinary(d,n); if(xml && xml->hasTagName("FluidScope")) state.replaceState(juce::ValueTree::fromXml(*xml)); }
};

class ColourPopup : public juce::ColourSelector,private juce::ChangeListener {
    std::function<void(juce::Colour)> changed;
    void changeListenerCallback(juce::ChangeBroadcaster*) override {changed(getCurrentColour());}
public:
    ColourPopup(juce::Colour c,std::function<void(juce::Colour)> f):ColourSelector(showColourAtTop|showSliders|showColourspace),changed(std::move(f)) {setSize(280,340);setCurrentColour(c,juce::dontSendNotification);addChangeListener(this);}
    ~ColourPopup() override {removeChangeListener(this);}
};

class FluidEditor : public juce::AudioProcessorEditor, private juce::Timer {
    FluidProcessor& proc;
    fluid::Capture capture;
    fluid::SpectrumPanel spectrum,depth,xyDepth;
    fluid::SignalPanel waveTile,xyTile;
    std::unique_ptr<fluid::DisplayWindow> fullScreen;
    juce::TextButton fullButton{"Full screen / F11"};
    juce::ComboBox style,fftScale,fftSource,fftWeight,fftAverage;
    juce::Slider lineWidth,glow,historyDecaySlider;
    std::array<juce::Label,4> styleLabels;
    juce::TextButton colourLeft{"L / Colour"},colourRight{"R / Colour"};
    std::array<juce::Slider,6> fftSliders;
    std::array<juce::Label,7> fftLabels;
    juce::ToggleButton clean{"Clean"},wireframe{"Wireframe"},listen{"Listen band"},xy3D{"3D / Time"},xyFollow{"Follow input"};
    juce::Slider xyTime,xyFade;juce::Label xyTimeLabel,xyFadeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> xy3DAttachment,xyFollowAttachment,cleanAttachment,wireframeAttachment;
    juce::TextButton clearBand{"Clear band"};
    std::array<juce::Slider,2> bandSliders;
    std::array<juce::Label,2> bandLabels;
    std::array<juce::Label,3> fftHeaderLabels;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> listenAttachment;
    int lastStyle=-1;float lastColourL=-1,lastColourR=-1;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::array<juce::Slider,7> sliders;
    std::array<juce::Label,11> labels;
    std::array<juce::ComboBox,4> combos;
    juce::ComboBox xySource;
    std::array<juce::Slider,4> xySliders;
    std::array<juce::Label,6> xyLabels;
    std::vector<std::unique_ptr<SliderAttachment>> sa;
    std::vector<std::unique_ptr<ComboAttachment>> ca;
    juce::TextButton freeze{"Freeze"}, single{"Single"}, resume{"Run"};
    bool frozen=false, singleArmed=false, wasVisible=false;
    juce::Rectangle<int> plot;
    struct Trace { juce::Path left,right,envelopeL,envelopeR; double born=0; };
    Trace trace;
    fluid::Settings displayedSettings;
    bool replaceHistory=false;
    juce::Image phosphor;
    juce::VBlankAttachment displaySync;
    double lastVBlank=0, lastDecay=0, statsStart=0;
    unsigned paintCount=0, sweepCount=0, updateCount=0, vblankCount=0;
    double paintFps=0, sweepRate=0, updateRate=0;
    double updateTotal=0, paintTotal=0;unsigned updateCalls=0,paintCalls=0;
    static constexpr int historySize=64;
    std::array<double,historySize> completed{};
    unsigned completedCount=0;
    unsigned seen=0, drops=0;
    double lastData=0;
    float lastGain=-1, lastView=-1,lastXY3D=-1;
    std::array<float,6> lastXY{{-1,-1,-1,-1,-1,-1}};
    fluid::XYSettings xySettings() const {
        return {int(value("xySource")),value("xyDelay"),value("xyFreq"),value("xyPhase"),value("xyAmp"),value("xyFollow")>.5f};
    }
    float value(const char* id) const { return proc.state.getRawParameterValue(id)->load(); }
    void set(const char* id,float v) {auto* p=proc.state.getParameter(id);p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(v));p->endChangeGesture();}
    bool spectral() const {return value("view")>=1.5f;}
    bool quad() const {return int(value("view"))==4;}
    void closeFullScreen() {
        fullScreen.reset();getProperties().set("fullScreenOpen",false);replaceHistory=true;
        fullButton.setButtonText("Full screen / F11");resized();repaint();
        getTopLevelComponent()->toFront(true);grabKeyboardFocus();
    }
    void toggleFullScreen() {
        if(fullScreen){closeFullScreen();return;}
        fullScreen=std::make_unique<fluid::DisplayWindow>(capture,proc.analyser,proc.state,*this);getProperties().set("fullScreenOpen",true);
        auto safe=juce::Component::SafePointer<FluidEditor>(this);
        fullScreen->display.frame=[safe]{if(safe){safe->lastVBlank=juce::Time::getMillisecondCounterHiRes();++safe->vblankCount;safe->updateDisplay();}};
        fullScreen->display.exit=[safe]{juce::MessageManager::callAsync([safe]{if(safe&&safe->fullScreen)safe->closeFullScreen();});};
        fullButton.setButtonText("Close full screen");resized();
    }
    fluid::Appearance theme() const {return fluid::appearance(int(value("style")),int(value("colourL")),int(value("colourR")));}
    void applyTheme() {
        auto a=theme();
        auto slider=[&](juce::Slider& s){s.setColour(juce::Slider::thumbColourId,a.left);s.setColour(juce::Slider::trackColourId,a.left.darker(.4f));s.setColour(juce::Slider::backgroundColourId,a.grid);s.setColour(juce::Slider::textBoxTextColourId,a.text);s.setColour(juce::Slider::textBoxBackgroundColourId,a.panel);s.setColour(juce::Slider::textBoxOutlineColourId,a.grid);};
        for(auto& s:sliders)slider(s);for(auto& s:xySliders)slider(s);for(auto& s:fftSliders)slider(s);for(auto& s:bandSliders)slider(s);slider(historyDecaySlider);slider(lineWidth);slider(glow);slider(xyTime);slider(xyFade);
        for(auto* c:{&combos[0],&combos[1],&combos[2],&combos[3],&xySource,&style,&fftScale,&fftSource,&fftWeight,&fftAverage}) {c->setColour(juce::ComboBox::backgroundColourId,a.panel);c->setColour(juce::ComboBox::textColourId,a.text);c->setColour(juce::ComboBox::outlineColourId,a.grid);}
        auto label=[&](auto& list){for(auto& l:list)l.setColour(juce::Label::textColourId,a.muted);};label(labels);label(xyLabels);label(styleLabels);label(fftLabels);label(bandLabels);label(fftHeaderLabels);
        for(auto* b:{&freeze,&single,&resume,&clearBand,&fullButton}) {b->setColour(juce::TextButton::buttonColourId,a.grid);b->setColour(juce::TextButton::textColourOffId,a.text);}
        wireframe.setColour(juce::ToggleButton::textColourId,a.text);wireframe.setColour(juce::ToggleButton::tickColourId,a.left);clean.setColour(juce::ToggleButton::textColourId,a.text);clean.setColour(juce::ToggleButton::tickColourId,a.left);xyFollow.setColour(juce::ToggleButton::textColourId,a.text);xyFollow.setColour(juce::ToggleButton::tickColourId,a.left);xy3D.setColour(juce::ToggleButton::textColourId,a.text);xy3D.setColour(juce::ToggleButton::tickColourId,a.left);xyTimeLabel.setColour(juce::Label::textColourId,a.muted);xyFadeLabel.setColour(juce::Label::textColourId,a.muted);listen.setColour(juce::ToggleButton::textColourId,a.text);listen.setColour(juce::ToggleButton::tickColourId,a.right);
        colourLeft.setColour(juce::TextButton::buttonColourId,a.left);colourLeft.setColour(juce::TextButton::textColourOffId,a.left.contrasting());colourRight.setColour(juce::TextButton::buttonColourId,a.right);colourRight.setColour(juce::TextButton::textColourOffId,a.right.contrasting());
    }
    void clearDisplay() { trace=Trace{}; phosphor=juce::Image(); lastDecay=0; replaceHistory=false; }
    void clear() { clearDisplay(); capture.resetTrigger(); seen=capture.generation; }
public:
    FluidEditor(FluidProcessor& p) : AudioProcessorEditor(p),proc(p),spectrum(p.analyser,p.state),depth(p.analyser,p.state),xyDepth(p.analyser,p.state,&capture),waveTile(capture,p.state,false),xyTile(capture,p.state,true) {
        addAndMakeVisible(wireframe);wireframeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.state,"fftWireframe",wireframe);
        addAndMakeVisible(clean);cleanAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.state,"clean",clean);
        const char* ids[] = {"time","gain","level","hyst","hold","pre","persistence"};
        const char* names[] = {"WINDOW / ms","GAIN","TRIGGER LEVEL","HYSTERESIS","HOLDOFF / ms","PRE-TRIGGER / %","PERSIST / ms"};
        for(int i=0;i<7;++i) {
            auto& s=sliders[size_t(i)]; addAndMakeVisible(s);
            s.setNumDecimalPlacesToDisplay(i==3 ? 4 : i==2 ? 3 : 2);
            s.setSliderStyle(juce::Slider::LinearHorizontal); s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,20);
            s.setColour(juce::Slider::thumbColourId,juce::Colour(0xff56e2c2));
            s.setColour(juce::Slider::trackColourId,juce::Colour(0xff248f83));
            sa.push_back(std::make_unique<SliderAttachment>(proc.state,ids[i],s));
            s.textFromValueFunction=[i](double v){return juce::String(v,i==3?4:i==2?3:2);};
            s.updateText();
            addAndMakeVisible(labels[size_t(i)]); labels[size_t(i)].setText(names[i],juce::dontSendNotification);
            labels[size_t(i)].setFont(11.f);
        }
        const char* cids[]={"view","mode","source","slope"};
        const char* cnames[]={"DISPLAY","ACQUISITION","TRIGGER SOURCE","EDGE"};
        const juce::StringArray options[]={ {"Stereo","XY / Lissajous","FFT / Spectrum","3D / History","Quad"},{"Auto","Normal","Free"},{"Left","Right","Mid"},{"Rising","Falling"} };
        for(int i=0;i<4;++i) {
            addAndMakeVisible(combos[size_t(i)]); combos[size_t(i)].addItemList(options[i],1);
            ca.push_back(std::make_unique<ComboAttachment>(proc.state,cids[i],combos[size_t(i)]));
            addAndMakeVisible(labels[size_t(i+7)]); labels[size_t(i+7)].setText(cnames[i],juce::dontSendNotification); labels[size_t(i+7)].setFont(11.f);
        }
        addAndMakeVisible(xySource);xySource.addItemList({"Right input","Left input","Internal sine"},1);
        ca.push_back(std::make_unique<ComboAttachment>(proc.state,"xySource",xySource));
        addAndMakeVisible(xyFollow);xyFollowAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.state,"xyFollow",xyFollow);
        xyFollow.setTooltip("Internal sine follows the left/X input envelope. OSC LEVEL becomes a relative multiplier; 1.00 matches the input peak. Visual only.");
        const char* xyIds[]={"xyDelay","xyFreq","xyPhase","xyAmp"};
        const char* xyNames[]={"Y SOURCE","Y DELAY / ms","OSC / Hz","OSC PHASE / deg","OSC LEVEL","ENVELOPE"};
        for(int i=0;i<6;++i) {addAndMakeVisible(xyLabels[size_t(i)]);xyLabels[size_t(i)].setText(xyNames[i],juce::dontSendNotification);xyLabels[size_t(i)].setFont(11.f);}
        for(int i=0;i<4;++i) {
            auto& slider=xySliders[size_t(i)];addAndMakeVisible(slider);
            slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,100,20);
            slider.setColour(juce::Slider::thumbColourId,juce::Colour(0xff56e2c2));slider.setColour(juce::Slider::trackColourId,juce::Colour(0xff248f83));
            sa.push_back(std::make_unique<SliderAttachment>(proc.state,xyIds[i],slider));
            slider.textFromValueFunction=[i](double v){return juce::String(v,i==0?3:i==3?2:1);};slider.updateText();
        }
        addAndMakeVisible(spectrum);addAndMakeVisible(depth);addAndMakeVisible(xyDepth);addAndMakeVisible(waveTile);addAndMakeVisible(xyTile);addAndMakeVisible(fullButton);fullButton.onClick=[this]{toggleFullScreen();};
        setWantsKeyboardFocus(true);
        auto combo=[&](juce::ComboBox& c,const char* id,const juce::StringArray& choices){addAndMakeVisible(c);c.addItemList(choices,1);ca.push_back(std::make_unique<ComboAttachment>(proc.state,id,c));};
        combo(style,"style",{"Modern mint","Classic green","Amber CRT","Monochrome","Paper","Ice","Neon","Custom"});
        combo(fftScale,"fftScale",{"Log frequency","Linear frequency"});combo(fftSource,"fftSource",{"Stereo power","Left","Right"});combo(fftWeight,"fftWeight",{"Flat","Music +3 dB/oct","Bright +4.5 dB/oct","Custom tilt"});combo(fftAverage,"fftAverage",{"None","1/24 octave","1/12 octave","1/6 octave"});
        auto makeSlider=[&](juce::Slider& slider,const char* id,int places){addAndMakeVisible(slider);slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,20);sa.push_back(std::make_unique<SliderAttachment>(proc.state,id,slider));slider.textFromValueFunction=[places](double v){return juce::String(v,places);};slider.updateText();};
        makeSlider(lineWidth,"lineWidth",2);makeSlider(glow,"glow",0);
        const char* styleNames[]={"APPEARANCE","LINE / px","TRACE COLOURS","GLOW / %"};
        for(int i=0;i<4;++i){addAndMakeVisible(styleLabels[size_t(i)]);styleLabels[size_t(i)].setText(styleNames[i],juce::dontSendNotification);styleLabels[size_t(i)].setFont(11.f);}
        addAndMakeVisible(colourLeft);addAndMakeVisible(colourRight);
        auto picker=[this](bool right){auto safe=juce::Component::SafePointer<FluidEditor>(this);auto a=theme();auto popup=std::make_unique<ColourPopup>(right?a.right:a.left,[safe,right](juce::Colour c){if(safe){safe->set(right?"colourR":"colourL",float(c.getARGB()&0xffffff));safe->set("style",7);}});juce::CallOutBox::launchAsynchronously(std::move(popup),(right?colourRight:colourLeft).getScreenBounds(),nullptr);};
        colourLeft.onClick=[picker]{picker(false);};colourRight.onClick=[picker]{picker(true);};
        const char* fftNames[]={"RELEASE / ms","FREQUENCY SMOOTH","CUSTOM TILT / dB/oct","FLOOR / dB","HISTORY / s","REAR FADE / %","FFT GAIN / dB"};
        for(int i=0;i<7;++i){addAndMakeVisible(fftLabels[size_t(i)]);fftLabels[size_t(i)].setText(fftNames[i],juce::dontSendNotification);fftLabels[size_t(i)].setFont(11.f);}
        const char* fftIds[]={"fftSmooth","fftTilt","fftFloor","history","rearFade","fftGain"};for(int i=0;i<6;++i)makeSlider(fftSliders[size_t(i)],fftIds[i],i==1||i==3?1:0);
        makeSlider(historyDecaySlider,"historyDecay",0);
        fftSliders[5].setDoubleClickReturnValue(true,0);
        addAndMakeVisible(xy3D);xy3DAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.state,"xy3D",xy3D);
        makeSlider(xyTime,"xy3DTime",3);makeSlider(xyFade,"rearFade",0);
        addAndMakeVisible(xyTimeLabel);xyTimeLabel.setText("3D TIME / s",juce::dontSendNotification);xyTimeLabel.setFont(11.f);
        addAndMakeVisible(xyFadeLabel);xyFadeLabel.setText("REAR FADE / %",juce::dontSendNotification);xyFadeLabel.setFont(11.f);
        const char* headNames[]={"FREQUENCY SCALE","ANALYSIS INPUT","WEIGHTING"};for(int i=0;i<3;++i){addAndMakeVisible(fftHeaderLabels[size_t(i)]);fftHeaderLabels[size_t(i)].setText(headNames[i],juce::dontSendNotification);fftHeaderLabels[size_t(i)].setFont(11.f);}
        addAndMakeVisible(listen);listenAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.state,"listen",listen);addAndMakeVisible(clearBand);clearBand.onClick=[this]{set("listen",0);set("bandLow",20);set("bandHigh",20000);};
        const char* bandIds[]={"bandLow","bandHigh"};for(int i=0;i<2;++i){makeSlider(bandSliders[size_t(i)],bandIds[i],1);addAndMakeVisible(bandLabels[size_t(i)]);bandLabels[size_t(i)].setText(i?"HIGH / Hz":"LOW / Hz",juce::dontSendNotification);bandLabels[size_t(i)].setFont(11.f);}
        for(auto* b:{&freeze,&single,&resume}) addAndMakeVisible(b);
        freeze.onClick=[this]{ frozen=!frozen; singleArmed=false; if(!frozen) clear(); freeze.setButtonText(frozen?"Unfreeze":"Freeze"); };
        single.onClick=[this]{ clear(); frozen=false; singleArmed=true; freeze.setButtonText("Freeze"); };
        resume.onClick=[this]{ frozen=false; singleArmed=false; clear(); freeze.setButtonText("Freeze"); };
        setResizable(true,true); setResizeLimits(940,740,2400,1700); setSize(1180,900);
        setOpaque(true);
        displaySync=juce::VBlankAttachment(this,[this]{if(!fullScreen){lastVBlank=juce::Time::getMillisecondCounterHiRes();++vblankCount;updateDisplay();}});
        startTimerHz(120);
    }
    ~FluidEditor() override { stopTimer();fullScreen.reset();proc.analyser.requested.store(false); }
    bool keyPressed(const juce::KeyPress& key)override {
        if(key==juce::KeyPress::F11Key){toggleFullScreen();return true;}
        if(key==juce::KeyPress::escapeKey){set("listen",0);if(fullScreen)toggleFullScreen();return true;}return false;
    }
    void resized() override {
        const int w=getWidth();
        fullButton.setBounds(w-410,20,165,26);
        for(int i=0;i<4;++i) { int x=22+i*int((w-320)/4); int width=int((w-340)/4); labels[size_t(i+7)].setBounds(x,62,width,18); combos[size_t(i)].setBounds(x,82,width,28); }
        freeze.setBounds(w-290,82,82,28); single.setBounds(w-200,82,82,28); resume.setBounds(w-110,82,82,28);
        const int cell=(w-44)/7;
        for(int i=0;i<7;++i) { labels[size_t(i)].setBounds(22+i*cell,getHeight()-92,cell,20); sliders[size_t(i)].setBounds(22+i*cell,getHeight()-72,cell-10,54); }
        const bool fft=spectral(),xy=int(value("view"))==1;
        for(int i=1;i<4;++i){combos[size_t(i)].setVisible(!fft);labels[size_t(i+7)].setVisible(!fft);}
        for(int i=0;i<3;++i) {int x=22+(i+1)*int((w-320)/4),width=int((w-340)/4);fftHeaderLabels[size_t(i)].setVisible(fft);fftHeaderLabels[size_t(i)].setBounds(x,62,width,18);}
        const std::array<juce::ComboBox*,3> fftHeader{{&fftScale,&fftSource,&fftWeight}};
        for(int i=0;i<3;++i){auto* c=fftHeader[size_t(i)];c->setVisible(fft);c->setBounds(22+(i+1)*int((w-320)/4),82,int((w-340)/4),28);}
        const int styleCell=(w-48)/4;
        for(int i=0;i<4;++i)styleLabels[size_t(i)].setBounds(24+i*styleCell,126,styleCell-12,18);
        // Give the toggles real, non-overlapping hit areas beside their labels.
        styleLabels[0].setBounds(24,126,styleCell-116,18);clean.setBounds(24+styleCell-108,123,96,22);clean.toFront(false);
        const bool fftDepth=int(value("view"))==3||quad();wireframe.setVisible(fftDepth);
        if(fftDepth)styleLabels[2].setSize(styleCell-122,18);
        wireframe.setBounds(24+3*styleCell-112,123,102,22);wireframe.toFront(false);
        style.setBounds(24,148,styleCell-14,28);lineWidth.setBounds(24+styleCell,142,styleCell-14,46);
        colourLeft.setBounds(24+2*styleCell,148,(styleCell-22)/2,28);colourRight.setBounds(32+2*styleCell+(styleCell-22)/2,148,(styleCell-22)/2,28);glow.setBounds(24+3*styleCell,142,styleCell-14,46);
        const int xyCell=(w-48)/6;
        xySource.setVisible(xy);xySource.setBounds(24,221,xyCell-12,28);
        for(int i=0;i<6;++i) {xyLabels[size_t(i)].setVisible(xy);xyLabels[size_t(i)].setBounds(24+i*xyCell,196,xyCell-12,20);}
        for(int i=0;i<4;++i) {xySliders[size_t(i)].setVisible(xy);xySliders[size_t(i)].setBounds(24+(i+1)*xyCell,216,xyCell-12,48);}
        xyFollow.setVisible(xy);xyFollow.setBounds(24+5*xyCell,220,xyCell-12,30);
        xy3D.setVisible(xy);xy3D.setBounds(24,282,155,28);
        const bool timeDepth=value("xy3D")>.5f;
        sliders[0].setEnabled(!(xy&&timeDepth));
        xyTimeLabel.setVisible(xy);xyFadeLabel.setVisible(xy);xyTime.setVisible(xy);xyFade.setVisible(xy);xyTime.setEnabled(timeDepth);xyFade.setEnabled(timeDepth);
        xyTimeLabel.setBounds(200,266,250,18);xyTime.setBounds(200,284,(w-240)/2,44);xyFadeLabel.setBounds(220+(w-240)/2,266,250,18);xyFade.setBounds(220+(w-240)/2,284,(w-260)/2,44);
        const int fftCell=(w-48)/7;
        for(int i=0;i<7;++i){fftLabels[size_t(i)].setVisible(fft);fftLabels[size_t(i)].setBounds(24+i*fftCell,196,fftCell-12,18);}
        fftAverage.setVisible(fft);fftAverage.setBounds(24+fftCell,220,fftCell-12,28);
        for(int i=0;i<6;++i){int cell=i==0?0:i+1;fftSliders[size_t(i)].setVisible(fft);fftSliders[size_t(i)].setBounds(24+cell*fftCell,214,fftCell-12,48);}
        const bool depthView=int(value("view"))==3||quad();
        fftSliders[0].setVisible(fft&&!depthView);historyDecaySlider.setVisible(fft&&depthView);historyDecaySlider.setBounds(fftSliders[0].getBounds());
        fftLabels[0].setText(depthView?"3D DECAY / ms":"2D RELEASE / ms",juce::dontSendNotification);
        listen.setVisible(fft);listen.setBounds(24,280,140,28);clearBand.setVisible(fft);clearBand.setBounds(170,280,110,28);
        for(int i=0;i<2;++i){bandSliders[size_t(i)].setVisible(fft);bandLabels[size_t(i)].setVisible(fft);bandLabels[size_t(i)].setBounds(300+i*(w-330)/2,266,(w-350)/2,16);bandSliders[size_t(i)].setBounds(300+i*(w-330)/2,282,(w-350)/2,44);}
        for(int i=0;i<7;++i){sliders[size_t(i)].setVisible(!fft||quad());labels[size_t(i)].setVisible(!fft||quad());}
        single.setEnabled(!fft);
        const int top=fft?340:xy?342:204;
        const auto previousPlot=plot;
        plot={24,top,w-48,getHeight()-top-(fft&&!quad()?48:118)};
        const bool show=!fullScreen;
        spectrum.setVisible(show&&(int(value("view"))==2||quad()));depth.setVisible(show&&(int(value("view"))==3||quad()));waveTile.setVisible(show&&quad());xyTile.setVisible(show&&quad()&&!timeDepth);xyDepth.setVisible(show&&timeDepth&&(xy||quad()));xyDepth.set3D(show&&timeDepth&&(xy||quad()));
        spectrum.set3D(false);depth.set3D(show&&(int(value("view"))==3||quad()));
        auto expand=[this](int mode){return [this,mode]{set("view",float(mode));};};
        waveTile.expand=expand(0);xyTile.expand=expand(1);xyDepth.expand=quad()?std::function<void()>(expand(1)):std::function<void()>{};spectrum.expand=quad()?std::function<void()>(expand(2)):std::function<void()>{};depth.expand=quad()?std::function<void()>(expand(3)):std::function<void()>{};
        if(quad()){int pw=(plot.getWidth()-10)/2,ph=(plot.getHeight()-10)/2;waveTile.setBounds(plot.getX(),plot.getY(),pw,ph);xyTile.setBounds(plot.getX()+pw+10,plot.getY(),pw,ph);xyDepth.setBounds(xyTile.getBounds());spectrum.setBounds(plot.getX(),plot.getY()+ph+10,pw,ph);depth.setBounds(plot.getX()+pw+10,plot.getY()+ph+10,pw,ph);}
        else {spectrum.setBounds(plot);depth.setBounds(plot);xyDepth.setBounds(plot);}
        if(previousPlot.getWidth()>0 && previousPlot.getHeight()>0) {
            const float sx=float(plot.getWidth())/previousPlot.getWidth(),sy=float(plot.getHeight())/previousPlot.getHeight();
            const auto transform=juce::AffineTransform::scale(sx,sy);
            for(auto* path:{&trace.left,&trace.right,&trace.envelopeL,&trace.envelopeR})path->applyTransform(transform);
            if(phosphor.isValid()) phosphor=phosphor.rescaled(std::max(1,juce::roundToInt(phosphor.getWidth()*sx)),std::max(1,juce::roundToInt(phosphor.getHeight()*sy)),juce::Graphics::mediumResamplingQuality);
        }
        if(!fft && capture.frameAvailable()) buildTrace();
    }
private:
    void timerCallback() override {
        // Detached editors/test renders have no display clock. Visible editors
        // follow vblank; the timer only provides a fallback if that clock stops.
        if(juce::Time::getMillisecondCounterHiRes()-lastVBlank>250) updateDisplay();
    }
    void decayPhosphor(double now) {
        if(phosphor.isValid() && !frozen) {
            const double tau=value("persistence");
            if(tau<=0) phosphor.clear(phosphor.getBounds());
            else if(lastDecay>0) phosphor.multiplyAllAlphas(float(std::exp(-(now-lastDecay)/tau)));
        }
        lastDecay=now;
    }
    void updateDisplay() {
        const auto now=juce::Time::getMillisecondCounterHiRes();
        if(!fullScreen && getPeer()!=nullptr && (!isShowing() || getPeer()->isMinimised())) {
            proc.analyser.requested.store(false);
            proc.queue.drain([](fluid::Stereo){});
            if(wasVisible) {clear();wasVisible=false;}
            return;
        }
        wasVisible=true;
        const bool styleChanged=lastStyle!=int(value("style"))||lastColourL!=value("colourL")||lastColourR!=value("colourR");
        if(styleChanged){lastStyle=int(value("style"));lastColourL=value("colourL");lastColourR=value("colourR");applyTheme();phosphor=juce::Image();replaceHistory=true;repaint();}
        const float previousView=lastView;
        const bool xyModeChanged=lastXY3D!=value("xy3D");lastXY3D=value("xy3D");
        const bool viewChangedEarly=lastView!=value("view");
        if(viewChangedEarly){lastView=value("view");replaceHistory=true;resized();repaint();}
        if(xyModeChanged){resized();repaint();}
        if(!frozen&&spectral()&&!quad())capture.settings.rate=proc.rate.load();
        proc.analyser.requested.store(spectral()&&!frozen);
        if(spectral()) {
            if(!quad())proc.queue.drain([&](fluid::Stereo sample){capture.ingest(sample);});
            proc.analyser.source.store(int(value("fftSource")));proc.analyser.frequencySmooth.store(int(value("fftAverage")));proc.analyser.smoothMs.store(value("fftSmooth"));proc.analyser.historyDecayMs.store(value("historyDecay"));proc.analyser.historySeconds.store(value("history"));
            bool fresh=spectrum.tick();if(int(value("view"))==3||quad())fresh=depth.tick()||fresh;
            if(fullScreen)fullScreen->display.tick();
            if(fresh)lastData=now;
            updateCount+=fullScreen?fullScreen->display.takeUpdates():int(value("view"))==3||quad()?depth.takeUpdates():spectrum.takeUpdates();
            fftSliders[1].setEnabled(int(value("fftWeight"))==3);fftSliders[3].setEnabled(int(value("view"))==3||quad());fftSliders[4].setEnabled(int(value("view"))==3||quad());
            const auto frames=fullScreen?fullScreen->display.takePaints():int(value("view"))==3||quad()?depth.takePaints():spectrum.takePaints();paintCount+=frames;
            if(statsStart==0)statsStart=now;
            if(!quad() && now-statsStart>=1000){const double factor=1000/(now-statsStart);paintFps=std::min(paintCount,vblankCount?vblankCount:paintCount)*factor;updateRate=std::min(updateCount*factor,paintFps);getProperties().set("paintFps",paintFps);getProperties().set("updatesPerSecond",updateRate);getProperties().set("spectrumGraphicsReady",depth.graphicsReady());paintCount=updateCount=vblankCount=0;statsStart=now;repaint(0,12,getWidth(),42);repaint(0,plot.getBottom(),getWidth(),42);}
            listen.setButtonText(value("listen")>.5f?"LISTENING":"Listen band");if(!quad())return;
        }
        if(viewChangedEarly && (previousView==2||previousView==3)){capture.resetTrigger();seen=capture.generation;replaceHistory=true;}
        auto old=capture.settings;
        auto& s=capture.settings;
        s.rate=proc.rate.load(); s.seconds=value("time")*.001; s.level=value("level"); s.hysteresis=value("hyst"); s.holdoff=value("hold")*.001;
        s.pre=value("pre")*.01f; s.mode=int(value("mode")); s.source=int(value("source")); s.slope=int(value("slope"));
        if(singleArmed) s.mode=1;
        if(frozen) s=old;
        const bool triggerChanged=old.rate!=s.rate || old.level!=s.level || old.hysteresis!=s.hysteresis || old.holdoff!=s.holdoff || old.mode!=s.mode || old.source!=s.source || old.slope!=s.slope;
        const bool timebaseChanged=old.seconds!=s.seconds || old.pre!=s.pre;
        if(!frozen && triggerChanged) {
            capture.resetTrigger();seen=capture.generation;replaceHistory=true;
        } else if(!frozen && timebaseChanged) {
            capture.reframeFromHistory();replaceHistory=true;
        }
        const auto d=proc.queue.dropped.load(); if(d!=drops) {
            capture.resetTrigger();seen=capture.generation;replaceHistory=true;drops=d;
        }
        decayPhosphor(now);
        bool arrived=false;
        completedCount=0;
        proc.queue.drain([&](fluid::Stereo sample){ arrived=true; if(!frozen) {
            const auto before=capture.generation;
            capture.ingest(sample);
            if(capture.generation!=before) {
                completed[completedCount++ % historySize]=capture.origin;
                ++sweepCount;
            }
            if(singleArmed && capture.ready && capture.triggered) { buildTrace(); frozen=true; singleArmed=false; freeze.setButtonText("Unfreeze"); }
        }});
        if(arrived) lastData=now;
        if(!frozen && capture.frameAvailable() && capture.generation!=seen) {
            // Deposit several actual sweeps into the phosphor, rather than
            // discarding every acquisition between two screen refreshes.
            const unsigned available=std::min(completedCount,unsigned(historySize));
            // A long XY window already includes the intervening trajectories.
            // Depositing eight overlapping copies wastes work and adds no time detail.
            const unsigned limit=int(value("view"))==1?unsigned(std::clamp(int(std::ceil(1./(60.*s.seconds))),1,8)):8u;
            const unsigned count=std::min(available,value("persistence")>0?limit:1u);
            const double latest=capture.origin;
            for(unsigned i=0;i<count;++i) {
                unsigned index=completedCount-available+(count==1?available-1:i*(available-1)/(count-1));
                capture.origin=completed[index%historySize];buildTrace();
            }
            capture.origin=latest;
            if(count==0)buildTrace(); // Timebase can change even without new audio.
            if(!quad() && !(int(value("view"))==1&&value("xy3D")>.5f))++updateCount;
        }
        if(!fullScreen&&(int(value("view"))==1||quad())&&value("xy3D")>.5f){xyDepth.tick();if(!quad()&&!fullScreen){paintCount+=xyDepth.takePaints();updateCount+=xyDepth.takeUpdates();}}
        if(fullScreen && !spectral())fullScreen->display.tick();
        const std::array<float,6> xyValues{{value("xyDelay"),value("xyFreq"),value("xyPhase"),value("xyAmp"),value("xySource"),value("xyFollow")}};
        const bool viewChanged=lastView!=value("view");
        if(lastGain!=value("gain") || viewChanged || lastXY!=xyValues) {
            lastGain=value("gain");lastView=value("view");lastXY=xyValues;replaceHistory=true;
            if(viewChanged) {resized();repaint();}
            else if(capture.frameAvailable()) buildTrace();
        }
        const bool internal=value("xySource")>1.5f;
        xyFollow.setEnabled(internal);xyLabels[4].setText(value("xyFollow")>.5f?"OSC RATIO":"OSC LEVEL",juce::dontSendNotification);
        for(int i=1;i<4;++i) xySliders[size_t(i)].setEnabled(internal);
        if(fullScreen && !spectral()){paintCount+=fullScreen->display.takePaints();if(int(value("view"))==1&&value("xy3D")>.5f)updateCount+=fullScreen->display.takeUpdates();}
        if(statsStart==0) statsStart=now;
        if(now-statsStart>=1000) {
            const double factor=1000/(now-statsStart);
            paintFps=std::min(paintCount,vblankCount?vblankCount:paintCount)*factor;sweepRate=sweepCount*factor;updateRate=std::min(updateCount*factor,paintFps);
            if(quad())getProperties().set("spectrumGraphicsReady",depth.graphicsReady());
            getProperties().set("updateAverageMs",updateTotal/std::max(1u,updateCalls));
            getProperties().set("paintAverageMs",paintTotal/std::max(1u,paintCalls));
            updateTotal=paintTotal=0;updateCalls=paintCalls=0;
            getProperties().set("paintFps",paintFps);
            getProperties().set("sweepsPerSecond",sweepRate);
            getProperties().set("updatesPerSecond",updateRate);
            paintCount=sweepCount=updateCount=vblankCount=0;statsStart=now;
            repaint(getWidth()-240,12,240,42);
            repaint(0,plot.getBottom()+3,getWidth(),28);
        }
        updateTotal+=juce::Time::getMillisecondCounterHiRes()-now;++updateCalls;
        repaint(plot.expanded(2));

    }
    static void rasterPhosphor(const juce::Path& path,juce::Image::BitmapData& pixels,
                               juce::Colour colour,float alpha,float scale) {
        // The history is already a polyline. Deposit its light directly rather
        // than repeatedly expanding thousands of joins into stroked polygons.
        std::array<juce::PixelARGB,256> palette;
        for(int i=0;i<256;++i) {palette[size_t(i)]=juce::PixelARGB(juce::uint8(i),colour.getRed(),colour.getGreen(),colour.getBlue());palette[size_t(i)].premultiply();}
        auto splat=[&](int x,int y,float a) {
            if(unsigned(x)>=unsigned(pixels.width) || unsigned(y)>=unsigned(pixels.height)) return;
            const int coverage=std::clamp(juce::roundToInt(a*255.f),0,255);
            if(coverage>0) reinterpret_cast<juce::PixelARGB*>(pixels.getLinePointer(y))[x].blend(palette[size_t(coverage)]);
        };
        float px=0,py=0;
        juce::Path::Iterator it(path);
        while(it.next()) {
            const float x=it.x1*scale,y=it.y1*scale;
            if(it.elementType==juce::Path::Iterator::lineTo) {
                const float dx=x-px,dy=y-py;
                const float length=std::max(std::abs(dx),std::abs(dy));
                if(!((px<0 && x<0)||(py<0 && y<0)||(px>=pixels.width && x>=pixels.width)||(py>=pixels.height && y>=pixels.height))) {
                    const int steps=std::max(1,int(std::ceil(length/.75f)));
                    const float energy=alpha*length/steps;
                    for(int i=0;i<steps;++i) {
                        const float f=(i+.5f)/steps,xx=px+dx*f,yy=py+dy*f;
                        const int ix=int(std::floor(xx)),iy=int(std::floor(yy));
                        const float fx=xx-ix,fy=yy-iy;
                        splat(ix,iy,energy*(1-fx)*(1-fy));splat(ix+1,iy,energy*fx*(1-fy));
                        splat(ix,iy+1,energy*(1-fx)*fy);splat(ix+1,iy+1,energy*fx*fy);
                    }
                }
            }
            px=x;py=y;
        }
    }
    void buildTrace() {
        if(!capture.frameAvailable())return;
        if(int(value("view"))==1&&value("xy3D")>.5f){seen=capture.generation;replaceHistory=false;return;}
        if(fullScreen){fullScreen->display.tick();seen=capture.generation;displayedSettings=capture.settings;return;}
        if(quad()){waveTile.refresh();xyTile.refresh();seen=capture.generation;displayedSettings=capture.settings;replaceHistory=false;return;}
        if(replaceHistory) {phosphor=juce::Image();lastDecay=0;replaceHistory=false;}
        displayedSettings=capture.settings;
        seen=capture.generation;
        Trace t; t.born=juce::Time::getMillisecondCounterHiRes();
        const float gain=value("gain"), w=float(plot.getWidth()), h=float(plot.getHeight());
        auto y=[&](float v){return std::clamp(h*.5f-v*gain*h*.44f,-h,h*2);};
        const int n=capture.span();
        if(int(value("view"))==1) {
            // Equal scale on both axes keeps phase and ratio figures geometrically accurate.
            float scale=std::min(w,h)*.44f*gain;
            int count=std::min(n,24000);
            const auto xy=xySettings();
            std::vector<fluid::Stereo> points;points.reserve(size_t(count));
            for(int i=0;i<count;++i) { auto a=fluid::xyPoint(capture,capture.origin+double(i)*(n-1)/std::max(1,count-1),xy);
                float x=std::clamp(w*.5f+a.l*scale,-w,w*2), yy=std::clamp(h*.5f-a.r*scale,-h,h*2);
                points.push_back({x,yy});
            }
            const auto reduced=fluid::reduceTrace(points);
            for(size_t i=0;i<reduced.size();++i){if(i==0)t.left.startNewSubPath(reduced[i].l,reduced[i].r);else t.left.lineTo(reduced[i].l,reduced[i].r);
            }
        } else {
            // Sample interpolation when zoomed in; per-pixel min/max retains narrow transients when zoomed out.
            const int points=std::min(12000,std::max(int(w*2),n< int(w*2)?int(w*2):int(w)));
            for(int i=0;i<points;++i) {
                double pos=capture.origin+double(i)*(n-1)/(points-1);
                auto a=capture.at(pos); float x=w*float(i)/float(points-1);
                if(i==0) {t.left.startNewSubPath(x,y(a.l));t.right.startNewSubPath(x,y(a.r));}
                else {t.left.lineTo(x,y(a.l));t.right.lineTo(x,y(a.r));}
            }
            if(n>w) for(int px=0;px<int(w);++px) {
                float loL=1e20f,hiL=-1e20f,loR=1e20f,hiR=-1e20f;
                const auto first=(long long)std::ceil(capture.origin+double(px)*n/w);
                const auto end=(long long)std::ceil(capture.origin+double(px+1)*n/w);
                for(auto j=first;j<end;++j) {
                    auto a=capture.at(double(j)); loL=std::min(loL,a.l);hiL=std::max(hiL,a.l);loR=std::min(loR,a.r);hiR=std::max(hiR,a.r);
                }
                t.envelopeL.startNewSubPath(float(px),y(loL));t.envelopeL.lineTo(float(px),y(hiL));
                t.envelopeR.startNewSubPath(float(px),y(loR));t.envelopeR.lineTo(float(px),y(hiR));
            }
        }
        const double persistence=value("persistence");
        if(persistence>0 && !frozen && plot.getWidth()>0 && plot.getHeight()>0) {
            const float scale=std::clamp(float(getPeer()!=nullptr?getPeer()->getPlatformScaleFactor():1.),1.f,2.f);
            const int width=juce::roundToInt(w*scale), height=juce::roundToInt(h*scale);
            if(!phosphor.isValid() || phosphor.getWidth()!=width || phosphor.getHeight()!=height)
                phosphor=juce::Image(juce::Image::ARGB,width,height,true,juce::SoftwareImageType{});
            juce::Image::BitmapData pixels(phosphor,juce::Image::BitmapData::readWrite);
            // Account for the age within the audio block, so a delayed callback
            // doesn't give old sweeps the same brightness as fresh ones.
            const double age=std::max(0.,(capture.position-(capture.origin+n))*1000/capture.settings.rate);
            const float alpha=float(.30*std::exp(-age/persistence));
            auto deposit=[&](const juce::Path& p,const juce::Path& e,juce::Colour colour){
                rasterPhosphor(p,pixels,colour,alpha,scale);
                rasterPhosphor(e,pixels,colour,alpha*.5f,scale);
            };
            deposit(t.left,t.envelopeL,theme().left);
            deposit(t.right,t.envelopeR,theme().right);
        }
        trace=std::move(t);
    }
    void paint(juce::Graphics& g) override {
        const auto paintStart=juce::Time::getMillisecondCounterHiRes();
        if(!fullScreen && !spectral() && !(int(value("view"))==1&&value("xy3D")>.5f) && g.getClipBounds().intersects(plot)) ++paintCount;
        const auto a=theme();
        g.fillAll(a.background);
        g.setColour(a.text);g.setFont(27.f);g.drawText("FluidScope",24,15,240,35,juce::Justification::centredLeft);
        g.setFont(12.f);g.setColour(a.muted);g.drawText("verstaerker  /  WAVEFORM + PHASE EXPLORER",260,24,430,20,juce::Justification::centredLeft);
        bool stale=juce::Time::getMillisecondCounterHiRes()-lastData>600;
        juce::String status=value("listen")>.5f?"BAND LISTENING":frozen?"HOLD":singleArmed?"SINGLE / ARMED":spectral()?(proc.analyser.snapshot()?(stale?"NO AUDIO":"FFT LIVE"):"FFT WARMUP"):replaceHistory?"UPDATING":stale?"NO AUDIO":capture.triggered?"TRIGGERED":capture.settings.mode==1?"WAITING":"AUTO / FREE";
        g.setColour(a.left);g.drawText(status,getWidth()-220,24,196,20,juce::Justification::centredRight);
        g.setColour(a.panel);g.fillRoundedRectangle(plot.toFloat().expanded(1),8);
        if(fullScreen){g.setColour(a.muted);g.drawText("Fullscreen display open / F11 to return",plot,juce::Justification::centred);}
        if(!fullScreen && !spectral() && !(int(value("view"))==1&&value("xy3D")>.5f)) { juce::Graphics::ScopedSaveState save(g);g.reduceClipRegion(plot);g.setOrigin(plot.getPosition());
        float w=float(plot.getWidth()),h=float(plot.getHeight());
        if(value("clean")<.5f)for(int i=0;i<=10;++i) {g.setColour(i==5?a.grid.brighter(.25f):a.grid);g.drawVerticalLine(int(w*i/10),0,h);g.drawHorizontalLine(int(h*i/10),0,w);}
        if(value("clean")<.5f&&value("view")<.5f) {
            float x=w*displayedSettings.pre, y=h*.5f-displayedSettings.level*value("gain")*h*.44f;
            g.setColour(a.right.withAlpha(.5f));g.drawVerticalLine(int(x),0,h);g.drawHorizontalLine(int(y),0,w);
        } else if(value("clean")<.5f){g.setColour(juce::Colour(0xff304651));float d=std::min(w,h)*.88f;g.drawEllipse((w-d)*.5f,(h-d)*.5f,d,d,1);}
        if(phosphor.isValid()) g.drawImage(phosphor,juce::Rectangle<float>(0,0,w,h));
        auto draw=[&](const juce::Path& p,const juce::Path& e,juce::Colour c){
            if(value("glow")>0){g.setColour(c.withAlpha(value("glow")*.01f));g.strokePath(p,juce::PathStrokeType(value("lineWidth")+2.7f));}
            g.setColour(c);g.strokePath(p,juce::PathStrokeType(value("lineWidth"),juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            g.strokePath(e,juce::PathStrokeType(.8f));
        };
        draw(trace.left,trace.envelopeL,a.left);
        draw(trace.right,trace.envelopeR,a.right);
        }
        g.setColour(a.muted);g.setFont(12.f);
        if(value("clean")>.5f)return;
        if(spectral()) {
            g.drawText(value("listen")>.5f?"Band audition: steep 48 dB/oct edges / analysis stays on the original input":"Original audio / select a range, then enable Listen band",24,plot.getBottom()+5,670,22,juce::Justification::centredLeft);
        } else g.drawText(int(value("view"))==1?"X LEFT / Y "+juce::String(value("xySource")>1.5f?"SINE":value("xySource")>.5f?"LEFT":"RIGHT")+" / delay "+juce::String(value("xyDelay"),3)+" ms":"L  mint    R  amber    /    "+juce::String(displayedSettings.seconds*100.,2)+" ms / div",24,plot.getBottom()+7,600,22,juce::Justification::centredLeft);
        g.drawText(juce::String(juce::roundToInt(paintFps))+" fps  /  "+juce::String(juce::roundToInt(updateRate))+" new/s  /  "+(spectral()?juce::String("FFT"):juce::String(juce::roundToInt(sweepRate))+" sweeps/s"),
                   getWidth()-420,plot.getBottom()+7,396,22,juce::Justification::centredRight);
        paintTotal+=juce::Time::getMillisecondCounterHiRes()-paintStart;++paintCalls;
    }
};
juce::AudioProcessorEditor* FluidProcessor::createEditor() { return new FluidEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FluidProcessor(); }
