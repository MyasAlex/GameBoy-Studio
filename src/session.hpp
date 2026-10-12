#pragma once
#include "gameboy.hpp"
#include <deque>

// Rewind snapshots are independent RLE packets. A damaged packet can never
// overwrite live emulation state; the core validates the unpacked state first.
class RewindBuffer {
    struct Snapshot { u64 cycles; std::size_t rawSize; std::vector<u8> packet; };
    std::deque<Snapshot> snapshots_;
    unsigned seconds_=30, interval_=30, sinceCapture_=0;
    std::size_t capacity_=61, bytes_=0;
    static constexpr std::size_t memoryLimit_=64*1024*1024;
public:
    static std::vector<u8> pack(std::span<const u8> input) {
        std::vector<u8> out;
        out.reserve(input.size()/4);
        std::size_t i=0;
        while(i<input.size()) {
            std::size_t run=1;
            while(i+run<input.size() && input[i+run]==input[i] && run<130)++run;
            if(run>=3) { out.push_back(static_cast<u8>(128+run-3));out.push_back(input[i]);i+=run;continue; }
            const auto first=i;i+=run;
            while(i<input.size() && i-first<128) {
                run=1;
                while(i+run<input.size() && input[i+run]==input[i] && run<3)++run;
                if(run>=3)break;
                const auto take=std::min(run,128-(i-first));i+=take;
            }
            out.push_back(static_cast<u8>(i-first-1));
            out.insert(out.end(),input.begin()+first,input.begin()+i);
        }
        return out;
    }
    static bool unpack(std::span<const u8> packet,std::size_t expected,std::vector<u8>& output) {
        if(expected>500000)return false;
        std::vector<u8> value;value.reserve(expected);
        std::size_t i=0;
        while(i<packet.size()) {
            const unsigned control=packet[i++];
            const std::size_t length=control<128?control+1:control-128+3;
            if(length>expected-value.size())return false;
            if(control<128) {
                if(length>packet.size()-i)return false;
                value.insert(value.end(),packet.begin()+i,packet.begin()+i+length);i+=length;
            } else {
                if(i>=packet.size())return false;
                value.insert(value.end(),length,packet[i++]);
            }
        }
        if(value.size()!=expected)return false;
        output=std::move(value);return true;
    }
    void configure(unsigned seconds,unsigned intervalFrames=30) {
        seconds_=std::min(seconds,600u);interval_=std::clamp(intervalFrames,1u,600u);
        capacity_=seconds_?std::max<std::size_t>(2,1+static_cast<std::size_t>(seconds_)*GB_CLOCK/GB_FRAME/interval_):0;
        clear();
    }
    void clear() {snapshots_.clear();bytes_=0;sinceCapture_=0;}
    void capture(GameBoy& gb) {
        if(!seconds_ || gb.mem.rom.empty())return;
        if(!snapshots_.empty() && ++sinceCapture_<interval_)return;
        sinceCapture_=0;
        const auto raw=gb.serializeState();if(raw.empty())return;
        auto packet=pack(raw);
        if(packet.size()>memoryLimit_)return;
        while(!snapshots_.empty() && (snapshots_.size()>=capacity_ || bytes_+packet.size()>memoryLimit_)) {
            bytes_-=snapshots_.front().packet.size();snapshots_.pop_front();
        }
        bytes_+=packet.size();snapshots_.push_back({gb.totalCycles,raw.size(),std::move(packet)});
    }
    bool rewind(GameBoy& gb) {
        while(!snapshots_.empty() && snapshots_.back().cycles>=gb.totalCycles) {
            bytes_-=snapshots_.back().packet.size();snapshots_.pop_back();
        }
        if(snapshots_.empty())return false;
        std::vector<u8> raw;
        const auto& point=snapshots_.back();
        if(!unpack(point.packet,point.rawSize,raw) || !gb.deserializeState(raw))return false;
        bytes_-=point.packet.size();snapshots_.pop_back();sinceCapture_=interval_;
        return true;
    }
    std::size_t size()const{return snapshots_.size();}
    std::size_t bytes()const{return bytes_;}
    unsigned seconds()const{return seconds_;}
    double availableSeconds()const {
        return snapshots_.size()<2?0.0:static_cast<double>(snapshots_.back().cycles-snapshots_.front().cycles)/GB_CLOCK;
    }
};

// Frame input movies contain their validated initial state and one joypad byte
// per frame. They are local files tied to a ROM hash and the core state format.
class InputMovie {
public:
    enum class Mode { Idle, Recording, Playback };
private:
    Mode mode_=Mode::Idle;
    std::vector<u8> initial_,inputs_;
    std::size_t position_=0;
    u32 romHash_=0;
    u8 mutedMask_=0;
    const u8* romData_=nullptr;
    std::size_t romBytes_=0;
    std::filesystem::path romPath_;
    static constexpr std::size_t maxFrames_=1000000;
    static u32 checksum(std::span<const u8> bytes) {
        u32 value=2166136261u;for(u8 b:bytes){value^=b;value*=16777619u;}return value;
    }
    static u8 channelMask(const APU& apu){u8 mask=0;for(int i=0;i<4;++i)if(apu.mutedChannels[i])mask|=static_cast<u8>(1u<<i);return mask;}
    static void put32(std::vector<u8>& out,u32 v){for(int i=0;i<4;++i)out.push_back(static_cast<u8>(v>>(i*8)));}
    static u32 get32(std::span<const u8> in,std::size_t offset){u32 v=0;for(int i=0;i<4;++i)v|=static_cast<u32>(in[offset+i])<<(8*i);return v;}
public:
    static u8 joypad(const Memory& m) {
        return static_cast<u8>((m.right?1:0)|(m.left?2:0)|(m.up?4:0)|(m.down?8:0)|
               (m.a?16:0)|(m.b?32:0)|(m.select?64:0)|(m.start?128:0));
    }
    static void setJoypad(Memory& m,u8 mask) {
        const auto before=m.joypadRead();
        m.right=(mask&1)!=0;m.left=(mask&2)!=0;m.up=(mask&4)!=0;m.down=(mask&8)!=0;
        m.a=(mask&16)!=0;m.b=(mask&32)!=0;m.select=(mask&64)!=0;m.start=(mask&128)!=0;m.joyChanged(before);
    }
    bool startRecording(GameBoy& gb) {
        auto initial=gb.serializeState();if(initial.empty())return false;
        initial_=std::move(initial);inputs_.clear();position_=0;romHash_=gb.romHash();
        romData_=gb.mem.rom.data();romBytes_=gb.mem.rom.size();romPath_=gb.mem.romPath;
        mutedMask_=channelMask(gb.apu);
        mode_=Mode::Recording;return true;
    }
    void stop(){mode_=Mode::Idle;}
    void clear(){stop();initial_.clear();inputs_.clear();position_=0;romHash_=0;romData_=nullptr;romBytes_=0;romPath_.clear();}
    // Call exactly once immediately before each emulated frame. Input polling
    // is disabled during playback so host input cannot disturb the recording.
    bool beforeFrame(GameBoy& gb) {
        if(mode_==Mode::Idle)return true;
        // Loaded ROM storage is immutable during a session. Detect cartridge
        // replacement without hashing up to 8 MiB at every video frame.
        if(gb.mem.rom.data()!=romData_||gb.mem.rom.size()!=romBytes_||gb.mem.romPath!=romPath_){stop();return false;}
        if(channelMask(gb.apu)!=mutedMask_){stop();return false;}
        if(mode_==Mode::Recording) {
            if(inputs_.size()>=maxFrames_){stop();return false;}
            inputs_.push_back(joypad(gb.mem));position_=inputs_.size();
        } else if(mode_==Mode::Playback) {
            if(position_>=inputs_.size()){stop();setJoypad(gb.mem,0);return false;}
            setJoypad(gb.mem,inputs_[position_++]);
        }
        return true;
    }
    bool save(const std::filesystem::path& path)const {
        if(initial_.empty() || inputs_.empty())return false;
        std::vector<u8> out={'G','B','M','O','V','I','E','2'};
        put32(out,romHash_);put32(out,static_cast<u32>(initial_.size()));put32(out,static_cast<u32>(inputs_.size()));
        put32(out,mutedMask_);
        out.insert(out.end(),initial_.begin(),initial_.end());out.insert(out.end(),inputs_.begin(),inputs_.end());
        put32(out,checksum(out));
        return Memory::writeAtomic(path,std::string_view(reinterpret_cast<const char*>(out.data()),out.size()));
    }
    bool startPlayback(GameBoy& gb,const std::filesystem::path& path) {
        std::error_code error;const auto fileSize=std::filesystem::file_size(path,error);
        if(error || fileSize<28 || fileSize>1500028)return false;
        const auto data=Memory::loadBytes(path);
        if(data.size()!=fileSize || std::memcmp(data.data(),"GBMOVIE2",8)!=0)return false;
        const auto hash=get32(data,8),stateSize=get32(data,12),frames=get32(data,16);
        const auto muted=get32(data,20);
        if(hash!=gb.romHash() || stateSize>500000 || !stateSize || !frames || frames>maxFrames_ ||
           muted>15 || static_cast<u64>(stateSize)+frames+28!=data.size() || get32(data,data.size()-4)!=checksum(std::span(data).first(data.size()-4)))return false;
        std::vector<u8> state(data.begin()+24,data.begin()+24+stateSize);
        if(!gb.deserializeState(state))return false;
        mutedMask_=static_cast<u8>(muted);for(int i=0;i<4;++i)gb.apu.setChannelMuted(i,(mutedMask_&(1u<<i))!=0);
        initial_=std::move(state);inputs_.assign(data.begin()+24+stateSize,data.end()-4);
        romHash_=hash;romData_=gb.mem.rom.data();romBytes_=gb.mem.rom.size();romPath_=gb.mem.romPath;
        position_=0;mode_=Mode::Playback;return true;
    }
    Mode mode()const{return mode_;}
    std::size_t frames()const{return inputs_.size();}
    std::size_t position()const{return position_;}
};
