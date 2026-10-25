#pragma once
#include "Appearance.h"
#include "XYSource.h"
#include "TraceReduction.h"
namespace fluid {
// Lightweight companion views for Quad and detached full-screen presentation.
// These readers share the editor's capture; they never consume an audio FIFO.
class SignalPanel : public juce::Component {
    Capture& capture;
    juce::AudioProcessorValueTreeState& state;
    bool xy;
    juce::Path left,right,envelopeL,envelopeR;
    unsigned generation=~0u,painted=~0u,paints=0,updates=0;
    std::array<float,8> last{};
    float value(const char* id)const{return state.getRawParameterValue(id)->load();}
public:
    std::function<void()> expand;
    SignalPanel(Capture& c,juce::AudioProcessorValueTreeState& s,bool isXY):capture(c),state(s),xy(isXY){setOpaque(true);}
    void refresh() {
        const std::array<float,8> settings{{value("gain"),value("xySource"),value("xyDelay"),value("xyFreq"),value("xyPhase"),value("xyAmp"),value("time"),value("xyFollow")}};
        if(capture.frameAvailable() && (generation!=capture.generation || settings!=last)) {
            generation=capture.generation;last=settings;left.clear();right.clear();envelopeL.clear();envelopeR.clear();
            const float w=float(getWidth()),h=float(getHeight()),gain=value("gain");const int n=capture.span();
            const int count=xy?std::min(n,8000):std::max(64,std::min(2400,int(w*1.5f)));
            XYSettings ys{int(value("xySource")),value("xyDelay"),value("xyFreq"),value("xyPhase"),value("xyAmp"),value("xyFollow")>.5f};
            auto y=[&](float v){return std::clamp(h*.52f-v*gain*h*.4f,-h,2*h);};
            std::vector<Stereo> xyPoints;if(xy)xyPoints.reserve(size_t(count));
            for(int i=0;i<count;++i) {
                auto s=xy?xyPoint(capture,capture.origin+double(i)*(n-1)/std::max(1,count-1),ys):capture.at(capture.origin+double(i)*(n-1)/std::max(1,count-1));
                float x=xy?w*.5f+s.l*std::min(w,h)*.4f*gain:float(i)*w/(count-1),yy=xy?h*.52f-s.r*std::min(w,h)*.4f*gain:y(s.l);
                if(xy){xyPoints.push_back({x,yy});continue;}
                if(i==0){left.startNewSubPath(x,yy);if(!xy)right.startNewSubPath(x,y(s.r));}else{left.lineTo(x,yy);if(!xy)right.lineTo(x,y(s.r));}
            }
            if(xy){auto reduced=reduceTrace(xyPoints);for(size_t i=0;i<reduced.size();++i){if(i==0)left.startNewSubPath(reduced[i].l,reduced[i].r);else left.lineTo(reduced[i].l,reduced[i].r);}}
            if(!xy && n>w)for(int x=0;x<int(w);++x) {
                float ll=1e20f,hl=-1e20f,lr=1e20f,hr=-1e20f;
                auto start=(long long)std::ceil(capture.origin+double(x)*n/w),end=(long long)std::ceil(capture.origin+double(x+1)*n/w);
                for(auto j=start;j<end;++j){auto s=capture.at(double(j));ll=std::min(ll,s.l);hl=std::max(hl,s.l);lr=std::min(lr,s.r);hr=std::max(hr,s.r);}
                envelopeL.startNewSubPath(float(x),y(ll));envelopeL.lineTo(float(x),y(hl));envelopeR.startNewSubPath(float(x),y(lr));envelopeR.lineTo(float(x),y(hr));
            }
        }
        repaint();
    }
    void resized()override{generation=~0u;refresh();}
    unsigned takePaints(){auto n=paints;paints=0;return n;}
    unsigned takeUpdates(){auto n=updates;updates=0;return n;}
    void paint(juce::Graphics& g)override {
        ++paints;if(painted!=generation){painted=generation;++updates;}
        auto a=appearance(int(value("style")),int(value("colourL")),int(value("colourR")));g.fillAll(a.panel);
        for(int i=1;i<8;++i){g.setColour(a.grid);g.drawVerticalLine(getWidth()*i/8,0,float(getHeight()));g.drawHorizontalLine(getHeight()*i/8,0,float(getWidth()));}
        auto draw=[&](const juce::Path& p,const juce::Path& e,juce::Colour c){if(value("glow")>0){g.setColour(c.withAlpha(value("glow")*.01f));g.strokePath(p,juce::PathStrokeType(value("lineWidth")+2.7f));}g.setColour(c);g.strokePath(p,juce::PathStrokeType(value("lineWidth"),juce::PathStrokeType::curved));g.strokePath(e,juce::PathStrokeType(.7f));};
        draw(left,envelopeL,a.left);if(!xy)draw(right,envelopeR,a.right);
        g.setFont(11.f);g.setColour(a.muted);g.drawText(xy?"XY / Lissajous":"Waveform / "+juce::String(capture.settings.seconds*1000,1)+" ms",10,6,getWidth()-20,16,juce::Justification::centredLeft);
        if(expand)g.drawText("double-click to expand",10,getHeight()-21,getWidth()-20,16,juce::Justification::centredRight);
    }
    void mouseDoubleClick(const juce::MouseEvent&)override{if(expand)expand();}
};
}
