#import <AppKit/AppKit.h>
#include "WindowOrder.h"
namespace fluid {
static NSWindow* nativeWindow(const juce::Component& c) {
    if(auto* peer=c.getPeer())return [(NSView*)peer->getNativeHandle() window];
    return nil;
}
WindowOrder::WindowOrder(juce::Component& presentation,juce::Component& owner) {
    auto* child=nativeWindow(presentation);auto* parent=nativeWindow(owner);
    if(child==nil || child==parent)return;
    displayWindow=[child retain];ownerWindow=[parent retain];
    // Cover this display in the existing Space. Floating DAW editor windows must
    // not cover it; app deactivation still reveals other apps normally.
    [child setHidesOnDeactivate:YES];
    [child setHasShadow:NO];
    if(parent)[parent addChildWindow:child ordered:NSWindowAbove];
    [child setLevel:std::max(NSInteger(NSStatusWindowLevel+1),parent?[parent level]+1:NSInteger(0))];
    [child makeKeyAndOrderFront:nil];
}
WindowOrder::~WindowOrder() {
    auto* child=(NSWindow*)displayWindow;auto* parent=(NSWindow*)ownerWindow;
    if(child && [child parentWindow]==parent)[parent removeChildWindow:child];
    [child release];[parent release];
}
bool presentationAboveOwner(const juce::Component& presentation,const juce::Component& owner) {
    auto* child=nativeWindow(presentation);auto* parent=nativeWindow(owner);
    if(!child || !parent || child==parent || [child parentWindow]!=parent || [child level]<=[parent level])return false;
    auto* windows=[NSApp orderedWindows];const auto a=[windows indexOfObjectIdenticalTo:child],b=[windows indexOfObjectIdenticalTo:parent];
    return a!=NSNotFound && b!=NSNotFound && a<b;
}
}
