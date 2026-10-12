#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <share.h>
#else
#include <unistd.h>
#endif

namespace host_audio {
struct StereoFrame { std::int16_t left=0,right=0; };
static_assert(sizeof(StereoFrame)==4);

// One producer and one consumer. Only resetStopped() may modify both indices,
// and it requires that neither thread is using the ring. Full queues reject new
// frames instead of having the producer overwrite a frame being consumed.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4324) // Deliberate cache-line padding separates producer/consumer atomics.
#endif
template<std::size_t Capacity> class StereoRing {
    static_assert(Capacity>=2&&(Capacity&(Capacity-1))==0);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,"Audio ring requires lock-free 64-bit atomics (x64).");
    std::array<StereoFrame,Capacity> data{};
    alignas(64) std::atomic<std::uint64_t> readIndex{0};
    alignas(64) std::atomic<std::uint64_t> writeIndex{0};
public:
    static constexpr std::size_t capacity=Capacity;
    std::size_t push(std::span<const StereoFrame> frames) {
        const auto write=writeIndex.load(std::memory_order_relaxed);
        const auto read=readIndex.load(std::memory_order_acquire);
        const auto take=std::min<std::size_t>(frames.size(),Capacity-(std::size_t)(write-read));
        for(std::size_t n=0;n<take;++n)data[(write+n)&(Capacity-1)]=frames[n];
        writeIndex.store(write+take,std::memory_order_release);return take;
    }
    std::size_t pop(std::span<StereoFrame> frames) {
        const auto read=readIndex.load(std::memory_order_relaxed);
        const auto write=writeIndex.load(std::memory_order_acquire);
        const auto take=std::min<std::size_t>(frames.size(),(std::size_t)(write-read));
        for(std::size_t n=0;n<take;++n)frames[n]=data[(read+n)&(Capacity-1)];
        readIndex.store(read+take,std::memory_order_release);return take;
    }
    bool tryPop(StereoFrame& frame){return pop(std::span<StereoFrame>(&frame,1))==1;}
    std::size_t size()const {
        const auto read=readIndex.load(std::memory_order_acquire),write=writeIndex.load(std::memory_order_acquire);
        return std::min<std::size_t>(Capacity,(std::size_t)(write-read));
    }
    void discardConsumer(){readIndex.store(writeIndex.load(std::memory_order_acquire),std::memory_order_release);}
    void resetStopped(){readIndex.store(0,std::memory_order_relaxed);writeIndex.store(0,std::memory_order_relaxed);}
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

// 64-frame (1.45 ms at 44.1 kHz) transitions around startup, mute, queue gaps.
// This is applied to host playback only; WAV recording preserves source PCM.
class SmoothRenderer {
    StereoFrame last{},transition{};
    unsigned progress=64;
    bool dataSegment=false;
    std::int32_t volume=65536;
    static std::int16_t blend(std::int16_t from,std::int16_t to,unsigned step){
        return (std::int16_t)(((std::int32_t)from*(64-step)+(std::int32_t)to*step)/64);
    }
public:
    void reset(){last={};transition={};progress=64;dataSegment=false;volume=65536;}
    void restart(){transition=last;progress=0;dataSegment=false;}
    template<std::size_t Capacity> std::size_t render(StereoRing<Capacity>& ring,std::span<StereoFrame> output,bool playing,int percent=100) {
        std::size_t missing=0;if(!playing)ring.discardConsumer();
        const auto target=(std::int32_t)(std::clamp(percent,0,100)*65536/100);
        for(auto& frame:output) {
            StereoFrame source{};const bool have=playing&&ring.tryPop(source);
            if(have!=dataSegment){dataSegment=have;transition=last;progress=0;}
            progress=std::min(64u,progress+1);
            if(!have){source={};if(playing)++missing;}
            frame.left=blend(transition.left,source.left,progress);frame.right=blend(transition.right,source.right,progress);last=frame;
            const auto difference=target-volume;
            if(difference)volume+=difference/64?difference/64:(difference>0?1:-1);
            frame.left=(std::int16_t)(((std::int64_t)frame.left*volume)/65536);frame.right=(std::int16_t)(((std::int64_t)frame.right*volume)/65536);
        }
        return missing;
    }
};

class WavWriter {
    std::FILE* file=nullptr;
    std::uint64_t bytes=0,limit;
    bool failed=false;
    std::string error;
    static void put16(std::array<unsigned char,44>& header,std::size_t pos,std::uint16_t value){header[pos]=(unsigned char)value;header[pos+1]=(unsigned char)(value>>8);}
    static void put32(std::array<unsigned char,44>& header,std::size_t pos,std::uint32_t value){for(unsigned n=0;n<4;++n)header[pos+n]=(unsigned char)(value>>(n*8));}
    bool header() {
        std::array<unsigned char,44> value{};
        auto tag=[&](std::size_t pos,const char* text){for(unsigned n=0;n<4;++n)value[pos+n]=(unsigned char)text[n];};
        tag(0,"RIFF");put32(value,4,(std::uint32_t)(36+bytes));tag(8,"WAVE");tag(12,"fmt ");put32(value,16,16);
        put16(value,20,1);put16(value,22,2);put32(value,24,44100);put32(value,28,176400);put16(value,32,4);put16(value,34,16);
        tag(36,"data");put32(value,40,(std::uint32_t)bytes);
        return std::fseek(file,0,SEEK_SET)==0&&std::fwrite(value.data(),1,value.size(),file)==value.size();
    }
    bool fail(std::string message){failed=true;error=std::move(message);return false;}
public:
    static constexpr std::uint64_t maxDataBytes=(std::uint64_t(std::numeric_limits<std::uint32_t>::max())-36)&~std::uint64_t(3);
    explicit WavWriter(std::uint64_t dataLimit=maxDataBytes):limit(std::min(dataLimit,maxDataBytes)&~std::uint64_t(3)) {}
    WavWriter(const WavWriter&)=delete;WavWriter& operator=(const WavWriter&)=delete;
    ~WavWriter(){if(file)close();}
    bool open(const std::filesystem::path& path) {
        if(file){error="A WAV recording is already active.";return false;}
        bytes=0;failed=false;error.clear();
        // O_EXCL/CREATE_NEW semantics are atomic: an existing ROM/save/WAV is never truncated.
#ifdef _WIN32
        int descriptor=-1;
        const auto opened=_wsopen_s(&descriptor,path.c_str(),_O_CREAT|_O_EXCL|_O_RDWR|_O_BINARY,_SH_DENYWR,_S_IREAD|_S_IWRITE);
        if(opened||descriptor<0)return fail("Could not create WAV file; choose a new filename in a writable folder.");
        file=_fdopen(descriptor,"w+b");if(!file)_close(descriptor);
#else
        const int descriptor=::open(path.c_str(),O_CREAT|O_EXCL|O_RDWR,0644);
        if(descriptor<0)return fail("Could not create WAV file; choose a new filename in a writable folder.");
        file=::fdopen(descriptor,"w+b");if(!file)::close(descriptor);
#endif
        if(!file)return fail("Could not open the WAV output stream.");
        if(!header()){fail("Could not write the WAV header.");close();return false;}
        return true;
    }
    bool append(std::span<const StereoFrame> frames) {
        if(!file)return fail("No WAV recording is active.");
        if(failed)return false;
        if(frames.size()>(limit-bytes)/4)return fail("WAV recording reached its RIFF size limit.");
        std::array<unsigned char,2048> encoded{};
        std::size_t offset=0;
        while(offset<frames.size()) {
            const auto take=std::min<std::size_t>(512,frames.size()-offset);
            for(std::size_t n=0;n<take;++n){const auto& frame=frames[offset+n];const auto l=(std::uint16_t)frame.left,r=(std::uint16_t)frame.right;
                encoded[n*4]=(unsigned char)l;encoded[n*4+1]=(unsigned char)(l>>8);encoded[n*4+2]=(unsigned char)r;encoded[n*4+3]=(unsigned char)(r>>8);}
            const auto written=std::fwrite(encoded.data(),4,take,file);bytes+=written*4;
            if(written!=take)return fail("Could not write WAV audio data.");offset+=take;
        }
        return true;
    }
    bool close() {
        if(!file)return !failed;
        // Even a capped/interrupted recording gets a finalized header for the complete frames.
        std::clearerr(file);
        const bool finalized=header();const bool flushed=std::fflush(file)==0;const bool closed=std::fclose(file)==0;file=nullptr;
        if(!finalized||!flushed||!closed)return fail("Could not finalize the WAV file; check disk space and permissions.");
        return !failed;
    }
    bool recording()const{return file!=nullptr;}
    bool good()const{return !failed;}
    const std::string& lastError()const{return error;}
    std::uint64_t dataBytes()const{return bytes;}
};
} // namespace host_audio
