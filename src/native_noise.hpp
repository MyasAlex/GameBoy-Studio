#pragma once
#include <cstdint>

// The noise oscillator has a small prescaler followed by a binary divider.
// Keeping these stages separate preserves their phase when NR43 changes;
// a single countdown of the final sample period cannot do that.
namespace native_noise {
struct State {
    std::uint8_t phase=0,prescaler=1,triggerDelay=0;
    std::uint16_t counter=0;
};

inline unsigned divisor(std::uint8_t frequency){
    const unsigned value=(frequency&7u)*2u;
    return value?value:1u;
}
inline unsigned period(std::uint8_t frequency){
    return divisor(frequency)*8u<<(frequency>>4);
}
inline void reset(State& state){state={};}
// Legacy states stored only a countdown to the next LFSR clock. Reconstruct a
// divider phase with that same next edge, without changing the saved LFSR.
inline void restoreCountdown(State& state,std::uint8_t frequency,unsigned remaining){
    reset(state);
    const unsigned span=period(frequency);
    if(!remaining||remaining>span)remaining=span;
    state.phase=static_cast<std::uint8_t>((4u-(remaining&3u))&3u);
    const unsigned clocks=(remaining+3u)/4u,divide=divisor(frequency);
    state.prescaler=static_cast<std::uint8_t>((clocks-1u)%divide+1u);
    const unsigned bit=frequency>>4;
    if(bit<14){
        const unsigned increments=(clocks+divide-1u)/divide;
        state.counter=static_cast<std::uint16_t>(((1u<<bit)-increments)&((2u<<bit)-1u));
    }
}
inline void frequencyWrite(State& state,std::uint8_t frequency){
    state.prescaler=static_cast<std::uint8_t>(divisor(frequency));
}
inline void trigger(State& state,std::uint8_t frequency,std::uint16_t& lfsr){
    state.prescaler=static_cast<std::uint8_t>(divisor(frequency));
    state.counter=0;state.triggerDelay=10;
    // This is the complemented 15-bit representation retained by Studio's
    // legacy snapshots. It is equivalent to the hardware XNOR LFSR reset to 0.
    lfsr=0x7FFF;
}
inline void shift(std::uint16_t& lfsr,std::uint8_t frequency){
    const unsigned feedback=(lfsr^(lfsr>>1))&1u;
    lfsr=static_cast<std::uint16_t>((lfsr>>1)|(feedback<<14));
    if(frequency&8u)lfsr=static_cast<std::uint16_t>((lfsr&~0x40u)|(feedback<<6));
}
inline bool tick(State& state,std::uint8_t frequency,bool started,std::uint16_t& lfsr){
    bool shifted=false;
    state.phase=static_cast<std::uint8_t>((state.phase+1)&3);
    if(state.phase==0&&--state.prescaler==0){
        state.prescaler=static_cast<std::uint8_t>(divisor(frequency));
        if(started&&!state.triggerDelay){
            const unsigned bit=frequency>>4;
            const bool before=(state.counter&(1u<<bit))!=0;
            state.counter=static_cast<std::uint16_t>((state.counter+1)&0x7FFF);
            const bool after=(state.counter&(1u<<bit))!=0;
            if(bit<14&&!before&&after){shift(lfsr,frequency);shifted=true;}
        }
    }
    if(state.triggerDelay)--state.triggerDelay;
    return shifted;
}
template<class Codec>inline void visit(State& state,Codec& codec){
    codec.fields(state.phase,state.prescaler,state.triggerDelay,state.counter);
}
inline bool valid(const State& state){
    return state.phase<4&&state.prescaler>=1&&state.prescaler<=14&&
           state.triggerDelay<=10&&state.counter<0x8000;
}
} // namespace native_noise
