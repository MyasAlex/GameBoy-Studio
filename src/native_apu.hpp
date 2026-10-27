#pragma once
#include "native_noise.hpp"
// Internal module: included after Memory and CPU by gameboy.hpp.
// APU counters run on the emulated clock, never on the host's audio callbacks.
class APU {
public:
    Memory& m;
    static constexpr int rate=44100;
    int sampleRate=rate;
    std::array<int,4> channelVolume={100,100,100,100};
    std::array<std::array<std::int16_t,512>,4> scope{};
    std::size_t scopePosition=0;
    double capacitorCharge=std::pow(0.999958,(double)GB_CLOCK/rate);
    bool setSampleRate(int value){if(value!=44100&&value!=48000&&value!=96000)return false;
        sampleRate=value;clearQueue();state.sampleClock=0;updateCapacitor();return true;}
    void updateCapacitor(){
        // Pan Docs' analogue high-pass model groups the MGB (Pocket/Light)
        // capacitor with CGB, despite those models remaining monochrome.
        const bool mgb=m.hardwareModel==HardwareModel::Pocket||m.hardwareModel==HardwareModel::Light;
        capacitorCharge=std::pow(m.isCGBHardware()||mgb?0.998943:0.999958,(double)GB_CLOCK/sampleRate);
    }
    void setChannelVolume(int channel,int percent){if(channel>=0&&channel<4)channelVolume[(std::size_t)channel]=std::clamp(percent,0,100);}
    struct Channel {
        int timer=1,length=0,phase=0,volume=0,envelopeTimer=0;
        bool enabled=false,dac=false,lengthEnabled=false,envelopeRunning=false,started=false;
    };
    struct State {
        std::array<Channel,4> channels{};
        int frameStep=0,sweepShadow=0,sweepTimer=0,waveAccess=0,waveByte=0;
        bool sweepEnabled=false,sweepNegated=false;
        u16 lfsr=0x7FFF;
        u8 waveBuffer=0;
        u32 sampleClock=0;
        int mixDivider=0,mixCount=0;
        double sumLeft=0,sumRight=0,capLeft=0,capRight=0;
    } state;
    // Pulse divider phase is shared by both square channels and starts with
    // NR52 power-up. Duty changes are sampled when the waveform advances.
    struct AccuracyState {
        u8 pulseClock=0;
        std::array<u8,2> pulseDuty{};
        native_noise::State noise;
        std::array<bool,4> envelopePending{};
        u8 sweepDelay=0;
        u8 sweepOverflowDelay=0;
        bool skipFrameSequencer=false,waveLengthPending=false;
    } accuracy;
    template<class Codec> static void visitAccuracy(AccuracyState& a,Codec& c){c.fields(a.pulseClock);c.array(a.pulseDuty);c.array(a.envelopePending);c.fields(a.sweepDelay,a.sweepOverflowDelay,a.skipFrameSequencer,a.waveLengthPending);native_noise::visit(a.noise,c);}
    template<class Codec> void visitAccuracy(Codec& c){visitAccuracy(accuracy,c);}
    bool accuracyValid()const{return accuracy.pulseClock<4&&accuracy.pulseDuty[0]<4&&accuracy.pulseDuty[1]<4&&accuracy.sweepDelay<=36&&accuracy.sweepOverflowDelay<=4&&native_noise::valid(accuracy.noise);}
    void restoreLegacyAccuracy(){accuracy={};accuracy.pulseClock=(u8)(state.mixDivider&3);
        accuracy.pulseDuty={(u8)(m.io[0x11]>>6),(u8)(m.io[0x16]>>6)};
        native_noise::restoreCountdown(accuracy.noise,m.io[0x22],(unsigned)state.channels[3].timer);}
    std::array<std::int16_t,32768> queue{};
    std::size_t readPos=0,writePos=0,count=0;
    std::array<bool,4> mutedChannels{};
    void setChannelMuted(int channel,bool muted){if(channel>=0&&channel<4)mutedChannels[(std::size_t)channel]=muted;}
    explicit APU(Memory& memory):m(memory){}
    void clearQueue(){readPos=writePos=count=0;}
    std::size_t available()const{return count/2;}
    int period(int channel)const{
        static constexpr int lo[3]={0x13,0x18,0x1D};
        return m.io[lo[channel]]|((m.io[lo[channel]+1]&7)<<8);
    }
    int reload(int channel)const{
        if(channel<3)return (2048-period(channel))*(channel==2?2:4);
        static constexpr int divs[8]={8,16,32,48,64,80,96,112};
        return divs[m.io[0x22]&7]<<(m.io[0x22]>>4);
    }
    int envelopeRegister(int channel)const{return channel==0?0x12:channel==1?0x17:0x21;}
    void status(){
        m.io[0x26]&=0x80;
        for(int i=0;i<4;++i)if(state.channels[i].enabled)m.io[0x26]|=(u8)(1<<i);
    }
    void reset(){
        state={};accuracy={};clearQueue();updateCapacitor();scopePosition=0;for(auto& channel:scope)channel.fill(0);
        for(int i=0;i<4;++i){
            auto& c=state.channels[i];
            c.dac=i==2?(m.io[0x1A]&0x80)!=0:(m.io[envelopeRegister(i)]&0xF8)!=0;
            c.enabled=c.dac&&(m.io[0x26]&(1<<i));c.timer=reload(i);
            c.started=c.enabled;c.length=i==2?256:64;
        }
        status();
    }
    u8 read(u16 addr)const{
        static constexpr u8 masks[32]={0x80,0x3F,0,0xFF,0xBF,0xFF,0x3F,0,0xFF,0xBF,
            0x7F,0xFF,0x9F,0xFF,0xBF,0xFF,0xFF,0,0,0xBF,0,0,0x70,
            0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        if(addr>=0xFF30){
            if(state.channels[2].enabled)
                return (m.isCGBHardware()||state.waveAccess>0)?m.io[0x30+state.waveByte]:0xFF;
            return m.io[addr-0xFF00];
        }
        return (u8)(m.io[addr-0xFF00]|masks[addr-0xFF10]);
    }
    int sweepCalculation(bool disable=true){
        const int delta=state.sweepShadow>>(m.io[0x10]&7);
        if(m.io[0x10]&8)state.sweepNegated=true;
        const int value=state.sweepShadow+((m.io[0x10]&8)?-delta:delta);
        if(disable&&value>2047)state.channels[0].enabled=false;
        return value;
    }
    bool nextEventSkipsLength()const{
        // Powering NR52 on while DIV-APU is high suppresses the next edge.
        // That interval also uses NRx4's extra length-clock/reload rule.
        return (state.frameStep&1)||accuracy.skipFrameSequencer;
    }
    void trigger(int i){
        auto& c=state.channels[i];
        const bool wasStarted=c.enabled;
        c.enabled=c.dac;c.started=true;
        accuracy.envelopePending[(std::size_t)i]=false;
        if(!c.length){c.length=i==2?256:64;if(c.lengthEnabled&&nextEventSkipsLength())--c.length;}
        c.timer=reload(i)+(i==2?6:0);
        if(i<2){
            // The waveform counter ticks at 1 MHz relative to APU power-up,
            // not to the CPU speed. Initial enable needs two extra ticks;
            // retrigger needs one and preserves the waveform position.
            c.timer=reload(i)+(wasStarted?4:8)+((4-accuracy.pulseClock)&3);
        }
        if(i==2){
            accuracy.waveLengthPending=false;
            c.phase=0;state.waveAccess=0;
            if(m.isCGBHardware())state.waveByte=0;
        }else{
            const int env=m.io[envelopeRegister(i)];
            c.volume=env>>4;c.envelopeTimer=(env&7)?(env&7):8;
            if(state.frameStep==7)++c.envelopeTimer;
            c.envelopeRunning=true;
        }
        if(i==0){
            accuracy.sweepDelay=0;
            accuracy.sweepOverflowDelay=0;
            state.sweepShadow=period(0);state.sweepTimer=(m.io[0x10]>>4)&7;
            if(!state.sweepTimer)state.sweepTimer=8;
            state.sweepEnabled=(m.io[0x10]&0x77)!=0;state.sweepNegated=false;
            if(m.io[0x10]&7){if(wasStarted)accuracy.sweepDelay=(u8)(36-accuracy.pulseClock);else sweepCalculation();}
        }
        if(i==3)native_noise::trigger(accuracy.noise,m.io[0x22],state.lfsr);
        status();
    }
    void write(u16 addr,u8 value){
        const int reg=addr-0xFF00;
        if(reg>=0x30){
            if(state.channels[2].enabled){if(m.isCGBHardware()||state.waveAccess>0)m.io[0x30+state.waveByte]=value;}
            else m.io[reg]=value;
            return;
        }
        if(reg==0x26){
            const bool old=(m.io[reg]&0x80)!=0,on=(value&0x80)!=0;
            if(old&&!on){
                for(int j=0x10;j<=0x25;++j)m.io[j]=0;
                for(auto& c:state.channels){const int len=c.length;c={};c.length=m.isCGBHardware()?0:len;}
                state.frameStep=0;state.sweepEnabled=false;state.waveBuffer=0;accuracy={};
            }else if(!old&&on){
                state.frameStep=0;state.waveBuffer=0;accuracy={};
                accuracy.skipFrameSequencer=(m.divCounter&m.apuDividerMask())!=0;
                for(auto& c:state.channels)c.phase=0;
            }
            m.io[reg]=(u8)(on?0x80:0);status();return;
        }
        if(!(m.io[0x26]&0x80)){
            // On DMG, length registers remain writable while sound is off.
            if(!m.isCGBHardware()&&(reg==0x11||reg==0x16||reg==0x1B||reg==0x20)){
                const int i=reg==0x11?0:reg==0x16?1:reg==0x1B?2:3;
                state.channels[i].length=i==2?256-value:64-(value&63);
            }
            return;
        }
        if(reg==0x15||reg==0x1F||reg>=0x27)return;
        const u8 old=m.io[reg];
        m.io[reg]=value;
        if(reg==0x22)native_noise::frequencyWrite(accuracy.noise,value);
        if(reg==0x10){
            if((old&8)&&!(value&8)&&state.sweepNegated)state.channels[0].enabled=false;
        }
        if(reg==0x11||reg==0x16||reg==0x1B||reg==0x20){
            const int i=reg==0x11?0:reg==0x16?1:reg==0x1B?2:3;
            state.channels[i].length=i==2?256-value:64-(value&63);
        }
        if(reg==0x12||reg==0x17||reg==0x21){
            const int i=reg==0x12?0:reg==0x17?1:3;
            auto& c=state.channels[i];
            if(c.enabled){
                const bool earlyColor=m.hardwareModel==HardwareModel::CGB0||m.hardwareModel==HardwareModel::CGBA||m.hardwareModel==HardwareModel::CGBB||m.hardwareModel==HardwareModel::CGBC;
                if(m.isCGBHardware()&&!earlyColor){
                    // On later CGB envelope hardware, the direction and zero-
                    // period transitions affect the volume arithmetic jointly.
                    const bool up=old&8,nextUp=value&8,zero=(old&7)==0,nextZero=(value&7)==0;
                    if(up){if(!nextUp)c.volume=16-c.volume-(zero&&!nextZero);else if(zero)++c.volume;}
                    else if(nextUp)c.volume=(zero?15:14)-c.volume;
                    else if(zero&&!nextZero)--c.volume;
                }else{
                    // Original DMG/CGB-02/04 manual-volume arithmetic.
                    if(!(old&7)&&c.envelopeRunning)++c.volume;
                    else if(!(old&8))c.volume+=2;
                    if((old^value)&8)c.volume=16-c.volume;
                }
                c.volume&=15;
            }
            if(!(old&7)&&(value&7)){
                c.envelopeTimer=1;c.envelopeRunning=true;
                accuracy.envelopePending[(std::size_t)i]=true;
            }else if(!(value&7))accuracy.envelopePending[(std::size_t)i]=false;
            c.dac=(value&0xF8)!=0;if(!c.dac)c.enabled=false;
        }
        if(reg==0x1A){state.channels[2].dac=(value&0x80)!=0;if(!state.channels[2].dac)state.channels[2].enabled=false;}
        if(reg==0x14||reg==0x19||reg==0x1E||reg==0x23){
            const int i=reg==0x14?0:reg==0x19?1:reg==0x1E?2:3;
            auto& c=state.channels[i];const bool enable=(value&0x40)!=0;
            if(i==2&&accuracy.waveLengthPending){c.enabled=false;accuracy.waveLengthPending=false;}
            const bool oldLengthQuirk=m.hardwareModel==HardwareModel::CGB0||m.hardwareModel==HardwareModel::CGBA||m.hardwareModel==HardwareModel::CGBB;
            if(!c.lengthEnabled&&(enable||oldLengthQuirk)&&nextEventSkipsLength()&&c.length>0)
                if(--c.length==0){
                    if(i==2&&m.hardwareModel==HardwareModel::CGBB)accuracy.waveLengthPending=true;
                    else c.enabled=false;
                }
            c.lengthEnabled=enable;
            if(value&0x80){
                if(!m.isCGBHardware()&&i==2&&c.enabled&&c.timer==2){
                    const int byte=((c.phase+1)&31)/2;
                    if(byte<4)m.io[0x30]=m.io[0x30+byte];
                    else {const int base=byte&~3;for(int j=0;j<4;++j)m.io[0x30+j]=m.io[0x30+base+j];}
                }
                trigger(i);
            }
        }
        status();
    }
    void sequencer(){
        if(!(m.io[0x26]&0x80))return;
        if(accuracy.skipFrameSequencer){accuracy.skipFrameSequencer=false;return;}
        const int step=state.frameStep;
        if(!(step&1))for(auto& c:state.channels)
            if(c.lengthEnabled&&c.length>0&&--c.length==0)c.enabled=false;
        if(step==2||step==6){
            if(--state.sweepTimer<=0){
                const int pace=(m.io[0x10]>>4)&7;state.sweepTimer=pace?pace:8;
                if(state.sweepEnabled&&pace){
                    const int value=sweepCalculation(false);
                    if(value>2047)accuracy.sweepOverflowDelay=4;
                    if(value<=2047&&(m.io[0x10]&7)){
                        state.sweepShadow=value;m.io[0x13]=(u8)(value&255);m.io[0x14]=(u8)((m.io[0x14]&0xF8)|(value>>8));
                        accuracy.sweepDelay=32;
                    }
                }
            }
        }
        if(step&1){
            for(int i:{0,1,3}){
                auto& c=state.channels[i];
                const bool pending=accuracy.envelopePending[(std::size_t)i];
                if(pending)accuracy.envelopePending[(std::size_t)i]=false;
                if((step==7||pending)&&c.envelopeRunning&&--c.envelopeTimer<=0){
                    const int env=m.io[envelopeRegister(i)];c.envelopeTimer=(env&7)?(env&7):8;
                    if(env&7){const int v=c.volume+((env&8)?1:-1);
                        if(v>=0&&v<=15)c.volume=v;else c.envelopeRunning=false;}
                }
            }
        }
        state.frameStep=(step+1)&7;status();
    }
    int output(int i)const{
        const auto& c=state.channels[i];
        if(!c.enabled)return 0;
        if(i<2){
            static constexpr u8 duty[4]={0x80,0x81,0xE1,0x7E};
            return ((duty[accuracy.pulseDuty[(std::size_t)i]]>>c.phase)&1)?c.volume:0;
        }
        if(i==2){const int volume=(m.io[0x1C]>>5)&3;
            const int sample=(c.phase&1)?(state.waveBuffer&15):(state.waveBuffer>>4);
            return volume?sample>>(volume-1):0;}
        return (state.lfsr&1)?0:c.volume;
    }
    void mix(){
        double left=0,right=0;
        if(m.io[0x26]&0x80)for(int i=0;i<4;++i)if(state.channels[i].dac&&!mutedChannels[i]){
            const double sample=(15-output(i)*2)*(channelVolume[(std::size_t)i]/100.0);
            if(m.io[0x25]&(1<<(i+4)))left+=sample;
            if(m.io[0x25]&(1<<i))right+=sample;
        }
        state.sumLeft+=left*(((m.io[0x24]>>4)&7)+1);
        state.sumRight+=right*((m.io[0x24]&7)+1);++state.mixCount;
    }
    u8 pcm(u16 addr)const{
        const int first=addr==0xFF76?0:2;
        return static_cast<u8>(output(first)|(output(first+1)<<4));
    }
    void emit(){
        const double left=state.mixCount?state.sumLeft/state.mixCount:0;
        const double right=state.mixCount?state.sumRight/state.mixCount:0;
        const double filteredL=left-state.capLeft,filteredR=right-state.capRight;
        // Model-specific capacitor, resampled from the base 4.194 MHz clock.
        const double charge=capacitorCharge;
        state.capLeft=left-filteredL*charge;state.capRight=right-filteredR*charge;
        for(int i=0;i<4;++i)scope[(std::size_t)i][scopePosition]=(std::int16_t)(output(i)*2048);
        scopePosition=(scopePosition+1)%512;
        if(count+2>queue.size()){readPos=(readPos+2)%queue.size();count-=2;}
        queue[writePos]=(std::int16_t)std::clamp(filteredL*48.0,-32767.0,32767.0);writePos=(writePos+1)%queue.size();
        queue[writePos]=(std::int16_t)std::clamp(filteredR*48.0,-32767.0,32767.0);writePos=(writePos+1)%queue.size();count+=2;
        state.sumLeft=state.sumRight=0;state.mixCount=0;
    }
    void tick(){
        accuracy.pulseClock=(u8)((accuracy.pulseClock+1)&3);
        if(accuracy.sweepOverflowDelay&&--accuracy.sweepOverflowDelay==0){state.channels[0].enabled=false;status();}
        if(accuracy.sweepDelay&&--accuracy.sweepDelay==0&&state.sweepEnabled&&(m.io[0x10]&7)){
            sweepCalculation();status();
        }
        if(state.waveAccess>0)--state.waveAccess;
        if(m.io[0x26]&0x80){
            // NR43 selects divider taps; writes retain the binary divider phase.
            if(native_noise::tick(accuracy.noise,m.io[0x22],state.channels[3].started,state.lfsr))state.channels[3].timer=reload(3);
            if(state.channels[3].timer>1)--state.channels[3].timer;
            for(int i=0;i<3;++i){
                auto& c=state.channels[i];
                if(!c.started||(i<2&&!c.enabled))continue;
                if(--c.timer<=0){
                    c.timer=reload(i);
                    if(i<2){c.phase=(c.phase+1)&7;accuracy.pulseDuty[(std::size_t)i]=(u8)(m.io[i==0?0x11:0x16]>>6);}
                    else if(i==2){
                        if(c.enabled){c.phase=(c.phase+1)&31;state.waveByte=c.phase/2;
                            state.waveBuffer=m.io[0x30+state.waveByte];state.waveAccess=2;}
                    }
                }
            }
        }
        if(++state.mixDivider==4){state.mixDivider=0;mix();}
        state.sampleClock+=(u32)sampleRate;
        if(state.sampleClock>=GB_CLOCK){state.sampleClock-=GB_CLOCK;emit();}
    }
    void samples(std::int16_t* stereo,int frames){
        const std::size_t take=std::min(count,(std::size_t)frames*2);
        for(std::size_t i=0;i<take;++i){stereo[i]=queue[readPos];readPos=(readPos+1)%queue.size();}
        count-=take;std::fill(stereo+take,stereo+frames*2,std::int16_t{0});
    }
};
