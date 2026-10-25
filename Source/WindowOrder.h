#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace fluid {
// Owns only the presentation's native ordering relationship, never the host UI.
class WindowOrder {
    void* ownerWindow=nullptr;
    void* displayWindow=nullptr;
public:
    WindowOrder(juce::Component& presentation,juce::Component& owner);
    ~WindowOrder();
    WindowOrder(const WindowOrder&)=delete;
    WindowOrder& operator=(const WindowOrder&)=delete;
};
bool presentationAboveOwner(const juce::Component& presentation,const juce::Component& owner);
}
