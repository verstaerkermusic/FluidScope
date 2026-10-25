#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

namespace fluid {
// Single producer / single consumer. The audio thread never waits or allocates.
struct Stereo { float l = 0, r = 0; };
class Queue {
public:
    static constexpr unsigned capacity = 1u << 19;
    std::vector<Stereo> data = std::vector<Stereo>(capacity);
    std::atomic<unsigned> read{0}, write{0};
    std::atomic<unsigned> dropped{0};
    void push(const float* l, const float* r, int n) noexcept {
        auto w = write.load(std::memory_order_relaxed);
        const auto available = capacity - (w - read.load(std::memory_order_acquire));
        if (unsigned(n) > available) { dropped.fetch_add(1, std::memory_order_relaxed); return; }
        for (int i = 0; i < n; ++i) data[(w++) & (capacity-1)] = {std::isfinite(l[i]) ? l[i] : 0.f, std::isfinite(r[i]) ? r[i] : 0.f};
        write.store(w, std::memory_order_release);
    }
    template<class F> void drain(F&& f) {
        auto r = read.load(std::memory_order_relaxed);
        const auto w = write.load(std::memory_order_acquire);
        while (r != w) f(data[(r++) & (capacity-1)]);
        read.store(r, std::memory_order_release);
    }
};
struct Settings {
    double rate = 48000, seconds = .02, holdoff = .005;
    float level = 0, hysteresis = .015f, pre = .2f;
    int source = 0, slope = 0, mode = 0; // Auto, Normal, Free
};
class Capture {
public:
    static constexpr int capacity = 1 << 20;
    std::vector<Stereo> ring = std::vector<Stereo>(capacity);
    std::vector<float> envelope=std::vector<float>(capacity);
    float envelopePeak=0,envelopeRelease=0;
    double envelopeRate=0;
    Settings settings;
    long long position = 0, lastTrigger = -capacity;
    double origin = 0;
    // One slot per sample is unnecessary: an edge can occur at most every other
    // sample. This preallocated queue also covers the longest supported window.
    std::vector<double> crossings = std::vector<double>(capacity/2);
    unsigned crossingRead = 0, crossingWrite = 0, storedCrossings = 0;
    float previous = 0;
    bool armed = false, ready = false, triggered = false;
    unsigned generation = 0;
    int span() const { return std::clamp(int(settings.rate * settings.seconds), 16, capacity/4); }
    void resetTrigger() { crossingRead = crossingWrite = storedCrossings = 0; armed = false; ready = false; previous = 0; lastTrigger = position; }
    bool frameAvailable() const {
        return ready && origin >= double(std::max(0LL,position-capacity))
            && std::floor(origin+span()-1)+1 < double(position);
    }
    // Retarget the timebase using retained trigger timestamps and real samples.
    // No audio or trigger state is discarded when a knob changes the window.
    bool reframeFromHistory() {
        const int pre=int(span()*settings.pre);
        const double oldest=double(std::max(0LL,position-capacity));
        if(settings.mode!=2 && storedCrossings>0) {
            const unsigned first=crossingWrite-storedCrossings;
            auto bound=[&](double value,bool inclusive) {
                unsigned lo=0,hi=storedCrossings;
                while(lo<hi) {
                    const auto mid=lo+(hi-lo)/2;
                    const auto time=crossings[(first+mid)&(capacity/2-1)];
                    if(time<value || (inclusive && time==value))lo=mid+1;else hi=mid;
                }
                return lo;
            };
            const auto begin=bound(oldest+pre,false);
            const auto end=bound(double(position)-span()+pre-3,true);
            if(end>begin) {
                origin=crossings[(first+end-1)&(capacity/2-1)]-pre;
                crossingRead=first+end;ready=true;triggered=true;++generation;
                return true;
            }
            crossingRead=first+begin;
        }
        if(position>span()+2 && (settings.mode==2 || (settings.mode==0
            && position-lastTrigger>std::max(settings.rate*.15,double(span()*2))))) {
            origin=double(position-span()-2);ready=true;triggered=false;++generation;
            return true;
        }
        ready=false;
        return false;
    }
    void ingest(Stereo s) {
        ring[size_t(position) & (capacity-1)] = s;
        // Visual-only left/X peak follower: immediate attack, 120-ms decay.
        // Store each sample so old XY trajectories never inherit today's level.
        if(envelopeRate!=settings.rate){envelopeRate=settings.rate;envelopeRelease=float(std::exp(-1./(.120*envelopeRate)));envelopePeak=0;}
        envelopePeak=std::max(std::abs(s.l),envelopePeak*envelopeRelease);
        if(envelopePeak<1e-15f)envelopePeak=0;
        envelope[size_t(position)&(capacity-1)]=envelopePeak;
        const float value = settings.source == 0 ? s.l : settings.source == 1 ? s.r : (s.l+s.r)*.5f;
        const float sign = settings.slope == 0 ? 1.f : -1.f;
        const float v = value * sign, p = previous * sign, level = settings.level * sign;
        if (v < level - settings.hysteresis) armed = true;
        const int pre = int(span() * settings.pre);
        if (armed && p <= level && v > level && position > pre+1
            && position-lastTrigger >= settings.rate*settings.holdoff && settings.mode != 2) {
            const double fraction = (level-p)/double(v-p);
            crossings[crossingWrite++ & (capacity/2-1)] = double(position-1) + fraction;
            storedCrossings=std::min(storedCrossings+1,unsigned(capacity/2));
            lastTrigger = position;
            armed = false;
        }
        if (crossingRead != crossingWrite && position >= crossings[crossingRead & (capacity/2-1)] + span()-pre+2) {
            origin = crossings[crossingRead++ & (capacity/2-1)]-pre;
            ready = true; triggered = true; ++generation;
        } else if (crossingRead == crossingWrite && settings.mode != 1 && position > span()+2
                   && (settings.mode == 2 || position-lastTrigger > std::max(settings.rate*.15, double(span()*2)))) {
            // Auto fallback refreshes at display cadence, independent of input block size.
            if (position % std::max(1, int(settings.rate/120)) == 0) {
                origin = double(position-span()-2); ready = true; triggered = false; ++generation;
            }
        }
        previous = value; ++position;
    }
    float envelopeAt(double index) const {
        const auto i=(long long)std::floor(index);
        if(i<std::max(0LL,position-capacity) || i+1>=position)return 0;
        const float a=envelope[size_t(i)&(capacity-1)],b=envelope[size_t(i+1)&(capacity-1)];
        return a+(b-a)*float(index-i);
    }
    Stereo at(double index) const {
        auto i = (long long)std::floor(index);
        if (i < std::max(0LL, position-capacity) || i+1 >= position) return {};
        auto a = ring[size_t(i)&(capacity-1)], b = ring[size_t(i+1)&(capacity-1)];
        const auto f = float(index-i);
        return {a.l+(b.l-a.l)*f, a.r+(b.r-a.r)*f};
    }
};
}
