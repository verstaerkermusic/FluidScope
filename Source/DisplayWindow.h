#pragma once
#include "SignalPanel.h"
#include "SpectrumPanel.h"
#include "WindowOrder.h"
namespace fluid {
class Presentation : public juce::Component {
    juce::AudioProcessorValueTreeState& state;
    SignalPanel wave,xy;
    SpectrumPanel spectrum,depth,xyDepth;
    juce::ComboBox view;
    juce::ToggleButton listen{"Listen band"},xy3D{"3D XY"};
    juce::TextButton close{"Return / Esc"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> va;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba,xa;
    int lastMode=-1,lastStyle=-1;float lastXY3D=-1;
    unsigned nativePaints=0;
    juce::VBlankAttachment displayClock;
    float value(const char* id)const{return state.getRawParameterValue(id)->load();}
    void setView(int i){auto* p=state.getParameter("view");p->setValueNotifyingHost(p->convertTo0to1(float(i)));}
public:
    std::function<void()> exit,frame;
    Presentation(Capture& c,SpectrumAnalyser& a,juce::AudioProcessorValueTreeState& s):state(s),wave(c,s,false),xy(c,s,true),spectrum(a,s),depth(a,s),xyDepth(a,s,&c) {
        for(auto* child:std::initializer_list<juce::Component*>{&wave,&xy,&spectrum,&depth,&xyDepth,&view,&listen,&xy3D,&close})addAndMakeVisible(child);
        view.addItemList({"Waveform","XY / Lissajous","FFT / Spectrum","3D / History","Quad"},1);
        va=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state,"view",view);
        ba=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state,"listen",listen);
        xa=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state,"xy3D",xy3D);
        close.onClick=[this]{if(exit)exit();};setOpaque(true);setWantsKeyboardFocus(true);
        displayClock=juce::VBlankAttachment(this,[this]{if(frame)frame();});
    }
    void resized()override {
        view.setBounds(230,9,220,28);xy3D.setBounds(470,9,120,28);listen.setBounds(getWidth()-340,9,170,28);close.setBounds(getWidth()-160,9,145,28);
        int mode=int(value("view"));auto b=getLocalBounds().reduced(16);b.removeFromTop(36);b.removeFromBottom(24);
        const bool time=value("xy3D")>.5f;xy3D.setVisible(mode==1||mode==4);wave.setVisible(mode==0||mode==4);xy.setVisible((mode==1||mode==4)&&!time);xyDepth.setVisible((mode==1||mode==4)&&time);xyDepth.set3D((mode==1||mode==4)&&time);spectrum.setVisible(mode==2||mode==4);depth.setVisible(mode==3||mode==4);
        spectrum.set3D(false);depth.set3D(mode==3||mode==4);
        wave.expand=mode==4?std::function<void()>([this]{setView(0);}):std::function<void()>{};xy.expand=mode==4?std::function<void()>([this]{setView(1);}):std::function<void()>{};
        xyDepth.expand=mode==4?std::function<void()>([this]{setView(1);}):std::function<void()>{};
        spectrum.expand=mode==4?std::function<void()>([this]{setView(2);}):std::function<void()>{};depth.expand=mode==4?std::function<void()>([this]{setView(3);}):std::function<void()>{};
        if(mode==4){int w=(b.getWidth()-12)/2,h=(b.getHeight()-12)/2;wave.setBounds(b.getX(),b.getY(),w,h);xy.setBounds(b.getX()+w+12,b.getY(),w,h);xyDepth.setBounds(xy.getBounds());spectrum.setBounds(b.getX(),b.getY()+h+12,w,h);depth.setBounds(b.getX()+w+12,b.getY()+h+12,w,h);}
        else {wave.setBounds(b);xy.setBounds(b);xyDepth.setBounds(b);spectrum.setBounds(b);depth.setBounds(b);}
    }
    void tick() {
        int style=int(value("style"));if(style!=lastStyle){lastStyle=style;auto a=appearance(style,int(value("colourL")),int(value("colourR")));view.setColour(juce::ComboBox::backgroundColourId,a.panel);view.setColour(juce::ComboBox::textColourId,a.text);xy3D.setColour(juce::ToggleButton::textColourId,a.text);xy3D.setColour(juce::ToggleButton::tickColourId,a.left);listen.setColour(juce::ToggleButton::textColourId,a.text);listen.setColour(juce::ToggleButton::tickColourId,a.right);close.setColour(juce::TextButton::buttonColourId,a.grid);close.setColour(juce::TextButton::textColourOffId,a.text);repaint();}
        int mode=int(value("view"));if(mode!=lastMode||lastXY3D!=value("xy3D")){lastMode=mode;lastXY3D=value("xy3D");resized();repaint();}
        if(mode==0||mode==4)wave.refresh();if(mode==1||mode==4){if(value("xy3D")>.5f)xyDepth.tick();else xy.refresh();}if(mode==2||mode==4)spectrum.tick();if(mode==3||mode==4)depth.tick();
    }
    unsigned takePaints(){int mode=int(value("view"));unsigned n=mode==2?spectrum.takePaints():mode==3||mode==4?depth.takePaints():mode==1?(value("xy3D")>.5f?xyDepth.takePaints():xy.takePaints()):wave.takePaints();nativePaints=0;return n;}
    unsigned takeUpdates(){int mode=int(value("view"));return mode==2?spectrum.takeUpdates():mode==3||mode==4?depth.takeUpdates():mode==1?(value("xy3D")>.5f?xyDepth.takeUpdates():xy.takeUpdates()):wave.takeUpdates();}
    bool keyPressed(const juce::KeyPress& key)override {
        if(key==juce::KeyPress::escapeKey || key==juce::KeyPress::F11Key || key==juce::KeyPress::returnKey){if(key==juce::KeyPress::escapeKey)state.getParameter("listen")->setValueNotifyingHost(0);if(exit)exit();return true;}return false;
    }
    void paint(juce::Graphics& g)override {
        if(int(value("view"))<2)++nativePaints;
        auto a=appearance(int(value("style")),int(value("colourL")),int(value("colourR")));g.fillAll(a.background);g.setColour(a.text);g.setFont(17.f);g.drawText("FluidScope / verstaerker",16,8,208,28,juce::Justification::centredLeft);
        g.setFont(12.f);g.setColour(a.muted);g.drawText("F11 / Esc: return to controls"+juce::String(value("listen")>.5f?" / BAND LISTENING":""),16,getHeight()-28,getWidth()-32,22,juce::Justification::centredLeft);
    }
};
class DisplayWindow : public juce::DocumentWindow {
    std::unique_ptr<WindowOrder> order;
    juce::BorderSize<int> getBorderThickness()override{return {};}
public:
    Presentation display;
    DisplayWindow(Capture& c,SpectrumAnalyser& a,juce::AudioProcessorValueTreeState& s,juce::Component& owner):DocumentWindow("FluidScope",juce::Colours::black,juce::DocumentWindow::closeButton),display(c,a,s) {
        setUsingNativeTitleBar(false);setTitleBarHeight(0);setContentNonOwned(&display,false);setResizable(false,false);
        const auto& displays=juce::Desktop::getInstance().getDisplays();const auto* monitor=displays.getDisplayForRect(owner.getScreenBounds());
        if(!monitor)monitor=displays.getPrimaryDisplay();
        setBounds(monitor?monitor->totalArea:juce::Rectangle<int>(0,0,1280,850));setVisible(true);
        order=std::make_unique<WindowOrder>(*this,owner);toFront(true);display.grabKeyboardFocus();
    }
    ~DisplayWindow()override {order.reset();setVisible(false);clearContentComponent();}
    void closeButtonPressed()override {if(display.exit)display.exit();}
};
}
