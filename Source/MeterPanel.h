#pragma once
#include "MeterAnalyser.h"
#include "Appearance.h"
namespace fluid {
class MeterPanel : public juce::Component,private juce::Timer {
    MeterAnalyser& analyser;
    juce::AudioProcessorValueTreeState& state;
    MeterFrame frame;
    std::array<float,2> animated{{meterSilence,meterSilence}};
    juce::TextButton resetButton{"Reset"},settingsButton{"Settings"};
    bool warning=false;
    float value(const char* id) const {return state.getRawParameterValue(id)->load();}
    static juce::String text(float db) {return db<=meterSilence?juce::String("-inf"):juce::String(db,1);}
    juce::Colour loudnessColour(float db) const {
        float green=value("lufsGreen"),red=std::max(green+1,value("lufsRed"));
        float t=juce::jlimit(0.f,1.f,(db-green)/(red-green));
        const juce::Colour mint(0xff56dba4),yellow(0xffefcd57),deepRed(0xffdc3444);
        return t<.5f?mint.interpolatedWith(yellow,t*2):yellow.interpolatedWith(deepRed,(t-.5f)*2);
    }
    void timerCallback() override {
        if(!isShowing())return;
        frame=analyser.snapshot();warning=frame.maxTruePeak>value("tpLimit");
        for(int c=0;c<2;++c)animated[size_t(c)]=std::max(frame.peak[size_t(c)],animated[size_t(c)]-1.6f);
        auto a=appearance(int(value("style")),int(value("colourL")),int(value("colourR")));
        for(auto* b:{&resetButton,&settingsButton}) {
            if(b->findColour(juce::TextButton::buttonColourId)!=a.grid)b->setColour(juce::TextButton::buttonColourId,a.grid);
            if(b->findColour(juce::TextButton::textColourOffId)!=a.text)b->setColour(juce::TextButton::textColourOffId,a.text);
        }
        repaint();
    }
    void settings() {
        struct Options : juce::Component {
            std::array<juce::Label,3> labels;
            std::array<juce::Slider,3> sliders;
            juce::ToggleButton transport{"Reset on playback start"};
            std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ta;
            Options(juce::AudioProcessorValueTreeState& s) {
                const char* names[]={"True Peak warning / dBTP","LUFS green","LUFS deep red"};const char* ids[]={"tpLimit","lufsGreen","lufsRed"};
                for(int i=0;i<3;++i){auto n=size_t(i);addAndMakeVisible(labels[n]);labels[n].setText(names[i],juce::dontSendNotification);labels[n].setBounds(12,10+i*64,286,22);addAndMakeVisible(sliders[n]);sliders[n].setSliderStyle(juce::Slider::LinearHorizontal);sliders[n].setTextBoxStyle(juce::Slider::TextBoxRight,false,66,24);sliders[n].setNumDecimalPlacesToDisplay(1);sliders[n].setBounds(12,34+i*64,286,28);attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(s,ids[i],sliders[n]));}
                addAndMakeVisible(transport);transport.setBounds(12,207,286,28);ta=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(s,"meterAutoReset",transport);setSize(310,248);
            }
        };
        juce::CallOutBox::launchAsynchronously(std::make_unique<Options>(state),settingsButton.getScreenBounds(),nullptr);
    }
public:
    MeterPanel(MeterAnalyser& a,juce::AudioProcessorValueTreeState& s):analyser(a),state(s) {
        setName("Input meters");addAndMakeVisible(resetButton);addAndMakeVisible(settingsButton);
        resetButton.onClick=[this]{analyser.dropped.store(0);analyser.reset();frame=MeterFrame{};warning=false;animated.fill(meterSilence);repaint();};settingsButton.onClick=[this]{settings();};
        resetButton.setTooltip("Reset input peak holds, True Peak maximum and integrated loudness.");
        setTooltipForSettings();startTimerHz(30);
    }
    void setTooltipForSettings() {settingsButton.setTooltip("True Peak threshold, LUFS colour range and transport reset.");}
    void resized() override {resetButton.setBounds(12,getHeight()-38,(getWidth()-30)/2,26);settingsButton.setBounds(18+(getWidth()-30)/2,getHeight()-38,(getWidth()-30)/2,26);}
    void paint(juce::Graphics& g) override {
        auto a=appearance(int(value("style")),int(value("colourL")),int(value("colourR")));
        g.fillAll(a.panel);g.setColour(a.grid);g.drawRect(getLocalBounds());
        int w=getWidth(),h=getHeight();
        auto label=[&](juce::String s,int y,int height,float font,juce::Colour colour){g.setFont(font);g.setColour(colour);g.drawText(s,12,y,w-24,height,juce::Justification::centred);};
        label("INPUT / PRE-LISTEN",10,18,10,a.muted);
        // Keep the lower LUFS section usable even at the minimum editor size.
        int meterTop=50,meterBottom=std::max(106,h-320),meterHeight=meterBottom-meterTop;
        auto y=[&](float db){return float(meterBottom)-juce::jlimit(0.f,1.f,(db+60)/66)*meterHeight;};
        for(int c=0;c<frame.channels;++c) {
            int x=frame.channels==1?w/2-18:48+c*(w-104);
            g.setColour(a.grid.withAlpha(.5f));g.fillRect(x,meterTop,16,meterHeight);
            float top=y(animated[size_t(c)]);g.setColour(a.left.withAlpha(.55f));g.fillRect(float(x),top,16.f,float(meterBottom)-top);
            g.setColour(a.right);g.fillRect(float(x),y(frame.rms[size_t(c)]),16.f,2.f);
            g.setColour(a.text);g.fillRect(float(x),y(frame.hold[size_t(c)]),16.f,1.f);
            g.setFont(10.f);g.drawText(c==0?"L":"R",x-3,30,22,18,juce::Justification::centred);
        }
        for(int db:{0,-12,-24,-36,-48,-60}) {g.setColour(db==0?a.right:a.muted);g.setFont(9.f);g.drawText(juce::String(db),4,int(y(float(db)))-7,28,14,juce::Justification::centredRight);}
        auto pair=[&](juce::String title,const std::array<float,2>& values,int yy){g.setFont(10.f);g.setColour(a.muted);g.drawText(title,10,yy,w-20,15,juce::Justification::centred);g.setFont(14.f);g.setColour(a.text);g.drawText(text(values[0])+(frame.channels>1?"   "+text(values[1]):""),10,yy+16,w-20,20,juce::Justification::centred);};
        int yy=meterBottom+7;pair("PEAK / dBFS",frame.peak,yy);pair("RMS / 400 ms",frame.rms,yy+40);
        yy+=83;
        g.setColour(warning?juce::Colour(0xffdc3444):a.grid);g.fillRoundedRectangle(10.f,float(yy),float(w-20),43.f,4.f);
        label("TRUE PEAK   "+text(frame.truePeak)+" dBTP",yy+3,18,11,warning?juce::Colours::white:a.text);
        label("MAX "+text(frame.maxTruePeak)+"   /   LIMIT "+juce::String(value("tpLimit"),1),yy+22,16,10,warning?juce::Colours::white:a.muted);
        yy+=50;label("SHORT-TERM / LUFS",yy,17,10,a.muted);
        label(text(frame.shortTerm),yy+15,35,29,loudnessColour(frame.shortTerm));
        label("M "+text(frame.momentary)+"    I "+text(frame.integrated),yy+53,22,12,a.text);
        label("M 400 ms   /   S 3 s   /   I gated",yy+77,15,9,a.muted);
        if(analyser.dropped.load())label("INPUT GAP / RESET METERS",h-57,15,9,a.right);
        else label(juce::String(frame.seconds,1)+" s",h-57,15,10,a.muted);
    }
};
}
