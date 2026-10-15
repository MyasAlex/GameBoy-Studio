#include "../src/frame_pacing.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

#define CHECK(value) do{if(!(value)){std::cerr<<"FAIL "<<#value<<" line "<<__LINE__<<'\n';std::exit(1);}}while(false)
static constexpr double frameSeconds=70224.0/4194304.0;
static int drain(host_timing::Pacer& pacer){int frames=0;while(pacer.due()){pacer.consume();CHECK(++frames<1000);}return frames;}

static void refreshIndependent(){
    for(int refresh:{60,144,240})for(double rate:{0.25,0.5,1.0,2.0,8.0,16.0}){
        host_timing::Pacer pacer;pacer.addElapsed(0,rate,frameSeconds);
        double elapsed=0;int frames=0,step=0;
        while(elapsed<10){
            // Two incommensurate components exercise nonuniform timer wakes.
            const double interval=(1.0+0.24*std::sin(step*0.73)+0.11*std::cos(step*1.17))/refresh;
            const double dt=std::min(interval,10.0-elapsed);
            pacer.addElapsed(dt,rate,frameSeconds);frames+=drain(pacer);elapsed+=dt;++step;
        }
        const int expected=(int)std::floor(elapsed*rate/frameSeconds+1e-8);
        CHECK(frames==expected);
        CHECK(std::abs(pacer.pendingSeconds()-(elapsed*rate-frames*frameSeconds))<1e-9);
    }
}
static void boundedCatchup(){
    for(double rate:{1.0,2.0,8.0}){
        host_timing::Pacer pacer;pacer.addElapsed(0,rate,frameSeconds);
        pacer.addElapsed(0.15,rate,frameSeconds);
        CHECK(drain(pacer)==(int)(2*rate));
        CHECK(pacer.pendingSeconds()>=0&&pacer.pendingSeconds()<frameSeconds);
        const double remainder=std::fmod(0.15*rate,frameSeconds);
        CHECK(std::abs(pacer.pendingSeconds()-remainder)<1e-12);
    }
    host_timing::Pacer pacer;pacer.addElapsed(0,1,frameSeconds);
    pacer.addElapsed(frameSeconds*0.75,1,frameSeconds);
    pacer.addElapsed(frameSeconds*1.5,1,frameSeconds);
    CHECK(drain(pacer)==2);
    CHECK(std::abs(pacer.pendingSeconds()-frameSeconds*0.25)<1e-12);
    pacer.consume();CHECK(std::abs(pacer.pendingSeconds()-frameSeconds*0.25)<1e-12);
}
static void transitions(){
    host_timing::Pacer pacer;
    pacer.addElapsed(30,1,frameSeconds);CHECK(!pacer.due()&&pacer.pendingSeconds()==0);
    pacer.addElapsed(frameSeconds,1,frameSeconds);CHECK(drain(pacer)==1);
    // Turbo begins after this sample's elapsed interval: do not multiply old
    // elapsed time, or retain the normal-speed fractional debt.
    pacer.addElapsed(frameSeconds*0.5,1,frameSeconds);
    pacer.addElapsed(frameSeconds*4,8,frameSeconds);CHECK(!pacer.due()&&pacer.pendingSeconds()==0);
    pacer.addElapsed(frameSeconds,8,frameSeconds);CHECK(pacer.due());
    // Releasing turbo discards every accelerated frame still pending.
    pacer.addElapsed(frameSeconds,1,frameSeconds);CHECK(!pacer.due()&&pacer.pendingSeconds()==0);
    pacer.addElapsed(frameSeconds,1,frameSeconds);CHECK(drain(pacer)==1);
    // Pausing/resetting must not charge the paused wall-clock interval.
    pacer.addElapsed(frameSeconds*0.5,1,frameSeconds);pacer.reset();
    pacer.addElapsed(300,1,frameSeconds);CHECK(!pacer.due()&&pacer.pendingSeconds()==0);
    pacer.addElapsed(frameSeconds,1,frameSeconds);CHECK(drain(pacer)==1);
    // A hardware clock change also establishes a fresh time budget.
    pacer.addElapsed(frameSeconds,1,frameSeconds/2);CHECK(!pacer.due());
    pacer.addElapsed(frameSeconds/2,1,frameSeconds/2);CHECK(drain(pacer)==1);
}
static void invalidInputs(){
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    for(double dt:{-0.001,nan,inf}){
        host_timing::Pacer pacer;pacer.addElapsed(0,1,frameSeconds);pacer.addElapsed(frameSeconds,1,frameSeconds);
        pacer.addElapsed(dt,1,frameSeconds);CHECK(!pacer.due()&&pacer.pendingSeconds()==0);
        pacer.addElapsed(frameSeconds,1,frameSeconds);CHECK(!pacer.due());
        pacer.addElapsed(frameSeconds,1,frameSeconds);CHECK(drain(pacer)==1);
    }
    for(double value:{0.0,-1.0,nan,inf}){
        host_timing::Pacer pacer;pacer.addElapsed(0,1,frameSeconds);pacer.addElapsed(frameSeconds,1,frameSeconds);
        pacer.addElapsed(1,value,frameSeconds);CHECK(!pacer.due()&&pacer.pendingSeconds()==0);
        pacer.addElapsed(0,1,frameSeconds);pacer.addElapsed(frameSeconds,1,frameSeconds);
        pacer.addElapsed(1,1,value);CHECK(!pacer.due()&&pacer.pendingSeconds()==0);
    }
    host_timing::Pacer pacer;pacer.addElapsed(0,16,frameSeconds);
    pacer.addElapsed(std::numeric_limits<double>::max(),16,frameSeconds);
    CHECK(!pacer.due()&&pacer.pendingSeconds()==0);
}
int main(){
    refreshIndependent();boundedCatchup();transitions();invalidInputs();
    std::cout<<"PASS: 10-second refresh-independent pacing at 60/144/240 Hz, explicit rates, catchup bounds, transitions and invalid clocks\n";
}
