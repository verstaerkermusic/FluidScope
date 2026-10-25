#pragma once
#include "Capture.h"

namespace fluid {
struct XYSettings {
    int source = 0; // right input, left input, internal sine
    double delayMs = 0, frequency = 220, phaseDegrees = 0;
    float amplitude = .75f;
    bool follow = false;
};
inline Stereo xyPoint(const Capture& capture, double index, const XYSettings& s) {
    const double delayed = index - std::clamp(s.delayMs, 0., 100.) * capture.settings.rate * .001;
    const float x = capture.at(index).l;
    if(s.source == 2) {
        constexpr double tau = 6.2831853071795864769;
        const double frequency = std::clamp(s.frequency, .1, capture.settings.rate*.45);
        const double phase = std::remainder(delayed*frequency/capture.settings.rate+s.phaseDegrees/360.,1.);
        return {x, s.amplitude*(s.follow?capture.envelopeAt(delayed):1.f)*float(std::sin(tau*phase))};
    }
    const auto y = capture.at(delayed);
    return {x, s.source == 1 ? y.l : y.r};
}
}
