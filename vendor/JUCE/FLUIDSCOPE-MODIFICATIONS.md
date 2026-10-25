# FluidScope JUCE snapshot

Upstream: https://github.com/juce-framework/JUCE
Version: 7.0.10
Commit: a8ae6edda6d3be78a139ec5e429dc57ef047e82a
Snapshot prepared: 2026-10-25

This snapshot includes a local modification to `modules/juce_gui_basics/native/juce_Windowing_mac.mm`: `createNSWindowSnapshot` uses NSView bitmap caching instead of CGWindowListCreateImage. This is the JUCE code used by the distributed FluidScope build. The change is recorded in the repository's patches/JUCE-7.0.10-macos-window-snapshot.patch. Other upstream source files are unchanged. JUCE's original licence and third-party notices remain in this directory.
