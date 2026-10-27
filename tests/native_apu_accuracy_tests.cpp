#include "../src/gameboy.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';std::abort();}}while(false)

struct Sound {
    Memory memory;
    APU apu{memory};
    explicit Sound(HardwareModel model=HardwareModel::CGBE){
        memory.hardwareModel=model;memory.cgbHardware=GameBoy::colorModel(model);
        memory.cgbMode=memory.cgbHardware;
        apu.write(0xFF26,0);apu.write(0xFF26,0x80);
    }
    void tick(unsigned dots){while(dots--)apu.tick();}
    void pulse(){apu.write(0xFF11,0x80);apu.write(0xFF12,0xF8);apu.write(0xFF13,0xFF);apu.write(0xFF14,0x87);}
};

// Pulse duty advances on the APU clock, survives retriggers and stops when
// the DAC is disabled. Register writes do not replace the pending duty bit.
static void pulsePhaseAndDuty(){
    Sound s;s.pulse();s.tick(11);CHECK(s.apu.state.channels[0].phase==0);
    s.tick(1);CHECK(s.apu.state.channels[0].phase==1);
    CHECK(s.apu.accuracy.pulseDuty[0]==2);
    s.apu.write(0xFF11,0xC0);CHECK(s.apu.accuracy.pulseDuty[0]==2);
    s.tick(4);CHECK(s.apu.accuracy.pulseDuty[0]==3);
    const auto phase=s.apu.state.channels[0].phase;
    s.apu.write(0xFF14,0x87);CHECK(s.apu.state.channels[0].phase==phase);
    s.tick(7);CHECK(s.apu.state.channels[0].phase==phase);
    s.tick(1);CHECK(s.apu.state.channels[0].phase==((phase+1)&7));
    s.apu.write(0xFF12,0);const auto stopped=s.apu.state.channels[0].phase;
    s.tick(100);CHECK(s.apu.state.channels[0].phase==stopped);
}

// CGB revisions use different manual envelope arithmetic when direction is
// changed with period zero. These are externally visible PCM volume values.
static void revisionEnvelopeAndClock(){
    for(auto model:{HardwareModel::CGBB,HardwareModel::CGBE}){
        Sound s(model);s.pulse();s.apu.write(0xFF12,0xF0);
        CHECK(s.apu.state.channels[0].volume==(model==HardwareModel::CGBB?0:1));
    }
    Sound s;s.apu.write(0xFF12,0xF0);s.apu.write(0xFF14,0x80);
    s.apu.write(0xFF12,0xF1);CHECK(s.apu.state.channels[0].volume==14);
    s.apu.state.frameStep=0;s.apu.sequencer();CHECK(s.apu.state.channels[0].volume==14);
    s.apu.sequencer();CHECK(s.apu.state.channels[0].volume==13);
}

static void dividerPowerupAndLength(){
    Sound s;s.apu.write(0xFF26,0);s.memory.divCounter=s.memory.apuDividerMask();
    s.apu.write(0xFF26,0x80);s.apu.write(0xFF11,0x3E);
    s.apu.write(0xFF12,0xF0);s.apu.write(0xFF14,0xC0);
    s.apu.sequencer();CHECK(s.apu.read(0xFF26)&1);
    s.apu.sequencer();CHECK(!(s.apu.read(0xFF26)&1));
    for(auto model:{HardwareModel::CGBB,HardwareModel::CGBE}){
        Sound r(model);r.apu.write(0xFF11,0x3F);r.apu.write(0xFF12,0xF0);
        r.apu.write(0xFF14,0x80);r.apu.state.frameStep=1;
        r.apu.write(0xFF14,0); // Older CGB also clocks a disabled length control.
        CHECK((r.apu.read(0xFF26)&1)==(model==HardwareModel::CGBB?0:1));
    }
}

static void suppressedDividerLengthWrites(){
    // NRx4 consults whether the *next event* clocks length. A suppressed
    // first event after high-DIV power-up follows the same extra-clock rule
    // as an ordinary odd sequencer step, including the zero-counter reload.
    constexpr std::array<u16,4> lengthRegs={0xFF11,0xFF16,0xFF1B,0xFF20};
    constexpr std::array<u16,4> controlRegs={0xFF14,0xFF19,0xFF1E,0xFF23};
    constexpr std::array<u16,4> dacRegs={0xFF12,0xFF17,0xFF1A,0xFF21};
    for(bool fast:{false,true})for(unsigned i=0;i<4;++i){
        Sound s;s.apu.write(0xFF26,0);s.memory.doubleSpeed=fast;
        s.memory.divCounter=s.memory.apuDividerMask();s.apu.write(0xFF26,0x80);
        s.apu.write(dacRegs[i],i==2?0x80:0xF0);
        s.apu.write(lengthRegs[i],i==2?0xFE:0x3E);
        s.apu.write(controlRegs[i],0xC0);
        CHECK(s.apu.state.channels[i].length==1&&s.apu.state.channels[i].enabled);
        s.apu.sequencer();CHECK(s.apu.state.channels[i].length==1&&s.apu.state.channels[i].enabled);
        s.apu.sequencer();CHECK(!s.apu.state.channels[i].enabled);

        s.apu.write(0xFF26,0);s.apu.write(0xFF26,0x80);
        s.apu.write(dacRegs[i],i==2?0x80:0xF0);
        s.apu.write(lengthRegs[i],i==2?0xFF:0x3F);
        s.apu.write(controlRegs[i],0xC0);
        CHECK(s.apu.state.channels[i].length==(i==2?255:63));
        CHECK(s.apu.state.channels[i].enabled);
    }
}

static std::array<std::int16_t,64> dcStep(HardwareModel model,int rate){
    auto g=std::make_unique<GameBoy>();g->model=model;g->reset();
    CHECK(g->apu.setSampleRate(rate));
    // An enabled DAC on an untriggered channel produces constant DC. Observe
    // its decay through the public PCM queue rather than the filter's fields.
    g->apu.write(0xFF26,0);g->apu.write(0xFF26,0x80);
    g->apu.write(0xFF12,0xF0);g->apu.write(0xFF24,0x77);g->apu.write(0xFF25,0x11);
    for(unsigned dots=0;dots<10000&&g->apu.available()<32;++dots)g->apu.tick();
    CHECK(g->apu.available()>=32);
    std::array<std::int16_t,64> result{};g->apu.samples(result.data(),32);return result;
}

static void monochromeCapacitorProfile(){
    for(int rate:{44100,48000,96000}){
        const auto dmg=dcStep(HardwareModel::DMG,rate),pocket=dcStep(HardwareModel::Pocket,rate);
        CHECK(pocket==dcStep(HardwareModel::Light,rate));
        CHECK(pocket==dcStep(HardwareModel::CGBE,rate));
        CHECK(dmg==dcStep(HardwareModel::SGB,rate));
        CHECK(pocket[0]>0&&pocket.back()>0&&pocket.back()<dmg.back());
    }
}

static void sweepDelayedOverflow(){
    Sound s;s.apu.write(0xFF10,0x11);s.apu.write(0xFF12,0xF0);
    s.apu.write(0xFF13,0);s.apu.write(0xFF14,0x84); // 1024 -> 1536 -> overflow.
    CHECK(s.apu.read(0xFF26)&1);s.apu.state.frameStep=2;s.apu.sequencer();
    CHECK(s.apu.period(0)==1536);s.tick(31);CHECK(s.apu.read(0xFF26)&1);
    s.tick(1);CHECK(!(s.apu.read(0xFF26)&1));
}

static void noiseClockAndWidth(){
    Sound s;s.apu.write(0xFF21,0xF0);s.apu.write(0xFF22,0);s.apu.write(0xFF23,0x80);
    // NR43 divisor zero is the shortest oscillator period. The trigger has
    // its own startup delay, then the divider retains phase across writes.
    s.tick(11);CHECK(s.apu.state.lfsr==0x7FFF);s.tick(1);CHECK(s.apu.state.lfsr==0x3FFF);
    s.tick(8);CHECK(s.apu.state.lfsr==0x1FFF);
    const auto divider=s.apu.accuracy.noise.counter;
    s.apu.write(0xFF22,0x08);CHECK(s.apu.accuracy.noise.counter==divider);
    s.tick(8);CHECK(s.apu.state.lfsr==0x0FBF);
    s.apu.write(0xFF23,0x80);CHECK(s.apu.state.lfsr==0x7FFF);
    s.tick(11);CHECK(s.apu.state.lfsr==0x7FFF);s.tick(1);CHECK(s.apu.state.lfsr==0x3FBF);
    s.apu.write(0xFF22,0xE0);const auto held=s.apu.state.lfsr;
    s.tick(10000);CHECK(s.apu.state.lfsr==held);
}

static void legacyNoiseCountdown(){
    // A legacy save gives only its next oscillator edge. Import must retain
    // that edge and the oscillator period rather than restarting the sound.
    for(unsigned shift=0;shift<6;++shift)for(unsigned divisor=0;divisor<8;++divisor){
        const auto frequency=(u8)((shift<<4)|divisor);
        const unsigned period=native_noise::period(frequency);
        for(unsigned remaining:{1u,2u,period/3,period}){
            native_noise::State state;u16 lfsr=0x7FFF;
            native_noise::restoreCountdown(state,frequency,remaining);
            CHECK(native_noise::valid(state));
            for(unsigned dot=1;dot<=remaining+period;++dot)
                CHECK(native_noise::tick(state,frequency,true,lfsr)==(dot==remaining||dot==remaining+period));
        }
    }
}

int main(){
    pulsePhaseAndDuty();revisionEnvelopeAndClock();dividerPowerupAndLength();suppressedDividerLengthWrites();monochromeCapacitorProfile();
    sweepDelayedOverflow();noiseClockAndWidth();legacyNoiseCountdown();
    std::cout<<"Native APU pulse, envelope, divider, sweep and noise hardware regressions PASS\n";
}
