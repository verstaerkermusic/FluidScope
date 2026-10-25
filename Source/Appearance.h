#pragma once
#include <JuceHeader.h>
namespace fluid {
struct Appearance {
    juce::Colour background,panel,grid,text,muted,left,right;
};
inline Appearance appearance(int preset,int customL=0x56e2c2,int customR=0xf3ac71) {
    Appearance a{juce::Colour(0xff10161c),juce::Colour(0xff080e13),juce::Colour(0xff25333c),juce::Colour(0xffedf6f5),juce::Colour(0xff93adb6),juce::Colour(0xff56e2c2),juce::Colour(0xfff3ac71)};
    if(preset==1) a={juce::Colour(0xff0a100b),juce::Colour(0xff020703),juce::Colour(0xff173421),juce::Colour(0xffd3f7d3),juce::Colour(0xff76a982),juce::Colour(0xff64ff85),juce::Colour(0xffb8ff52)};
    if(preset==2) a={juce::Colour(0xff17110a),juce::Colour(0xff0a0602),juce::Colour(0xff3a2c16),juce::Colour(0xffffedcc),juce::Colour(0xffbc9b6a),juce::Colour(0xffffc15a),juce::Colour(0xffff7750)};
    if(preset==3) a={juce::Colour(0xff141414),juce::Colour(0xff030303),juce::Colour(0xff303030),juce::Colours::white,juce::Colour(0xffaaaaaa),juce::Colours::white,juce::Colour(0xff9f9f9f)};
    if(preset==4) a={juce::Colour(0xffeaecec),juce::Colour(0xfffafafa),juce::Colour(0xffd4d8da),juce::Colour(0xff172129),juce::Colour(0xff56626b),juce::Colour(0xff123e58),juce::Colour(0xffa44a30)};
    if(preset==5) {a.left=juce::Colour(0xff76cfff);a.right=juce::Colour(0xffbdb9ff);}
    if(preset==6) {a.left=juce::Colour(0xfffb65da);a.right=juce::Colour(0xff58eeff);}
    if(preset==7) {a.left=juce::Colour(0xff000000u|unsigned(customL));a.right=juce::Colour(0xff000000u|unsigned(customR));}
    return a;
}
}
