#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace fluid {
// Shift both endpoints by one common axis offset. At the bounds the band
// stops as a whole, preserving its octave width (log) or Hz width (linear).
inline std::array<float,2> shiftBand(float low,float high,float axisDelta,bool logarithmic,float maximum) {
    maximum=std::max(21.f,maximum);
    if(low>high)std::swap(low,high);
    low=std::clamp(low,20.f,std::nextafter(maximum,0.f));
    high=std::clamp(high,std::nextafter(low,maximum),maximum);
    if(logarithmic) {
        double offset=axisDelta*std::log(double(maximum)/20);
        offset=std::clamp(offset,std::log(20./low),std::log(double(maximum)/high));
        const double ratio=std::exp(offset);
        return {float(low*ratio),float(high*ratio)};
    }
    float offset=std::clamp(axisDelta*maximum,20.f-low,maximum-high);
    return {low+offset,high+offset};
}
}
