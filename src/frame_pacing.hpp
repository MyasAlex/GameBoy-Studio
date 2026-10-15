#pragma once
#include <algorithm>
#include <cmath>

namespace host_timing {
// Host scheduling only: emulated hardware clocks remain unchanged.
class Pacer {
    double pendingFrames_=0,rate_=0,frameSeconds_=0;
    bool configured_=false;
    static constexpr double tolerance=1e-10;
public:
    void reset(){pendingFrames_=0;rate_=0;frameSeconds_=0;configured_=false;}

    // The first sample after reset or a rate/model change belongs to the old
    // timing regime. Establish the new regime without charging that interval.
    void addElapsed(double seconds,double rate,double frameSeconds){
        if(!std::isfinite(seconds)||seconds<0||!std::isfinite(rate)||rate<=0||
           !std::isfinite(frameSeconds)||frameSeconds<=0){reset();return;}
        if(!configured_||rate!=rate_||frameSeconds!=frameSeconds_){
            pendingFrames_=0;rate_=rate;frameSeconds_=frameSeconds;configured_=true;return;
        }
        const double next=pendingFrames_+seconds*rate/frameSeconds;
        const double limit=std::max(2.0,std::ceil(2.0*rate));
        if(!std::isfinite(next)||!std::isfinite(limit)){reset();return;}
        const double whole=std::floor(next+tolerance);
        // Discard excess complete frames after a stall, retaining subframe
        // phase. Ordinary refresh jitter must not lose fractional time.
        pendingFrames_=whole>limit?limit+(next-whole):next;
    }
    bool due()const{return configured_&&pendingFrames_+tolerance>=1.0;}
    void consume(){if(due())pendingFrames_=std::max(0.0,pendingFrames_-1.0);}
    double pendingSeconds()const{return pendingFrames_*frameSeconds_;}
};
} // namespace host_timing
