#pragma once
#include "gameboy.hpp"
#include "audio_buffer.hpp"
#include "portable_state.hpp"
#include <numbers>
#include "hq2x_cpu.hpp"

namespace media {
using Frame=host_audio::StereoFrame;
class Equalizer {
    struct Band {double b0=1,b1=0,b2=0,a1=0,a2=0;std::array<double,2> z1{},z2{};};
    std::array<Band,3> bands{};std::array<int,3> gains{};
public:
    void configure(int rate,int low,int middle,int high){gains={std::clamp(low,-12,12),std::clamp(middle,-12,12),std::clamp(high,-12,12)};
        const std::array<double,3> frequencies={120,1000,6000};
        for(int i=0;i<3;++i){auto& b=bands[i];b={};const double a=std::pow(10.0,gains[i]/40.0),w=2*std::numbers::pi*frequencies[i]/rate,alpha=std::sin(w)/2,c=std::cos(w),a0=1+alpha/a;
            b.b0=(1+alpha*a)/a0;b.b1=-2*c/a0;b.b2=(1-alpha*a)/a0;b.a1=-2*c/a0;b.a2=(1-alpha/a)/a0;}
    }
    void reset(){for(auto& b:bands){b.z1={};b.z2={};}}
    void apply(std::span<Frame> frames){if(gains==std::array<int,3>{})return;
        for(auto& frame:frames){std::array<double,2> value={(double)frame.left,(double)frame.right};
            for(auto& b:bands)for(int c=0;c<2;++c){const double output=b.b0*value[c]+b.z1[c];b.z1[c]=b.b1*value[c]-b.a1*output+b.z2[c];b.z2[c]=b.b2*value[c]-b.a2*output;value[c]=output;}
            frame.left=(std::int16_t)std::clamp(value[0],-32768.0,32767.0);frame.right=(std::int16_t)std::clamp(value[1],-32768.0,32767.0);}
    }
};

// Standards-compliant FLAC, independent stereo verbatim subframes. This is
// lossless, with frame CRCs; predictive compression is deliberately omitted.
class FlacWriter {
    std::ofstream file;std::vector<Frame> pending;u64 samples=0,number=0;unsigned rate=44100;bool failed=false;
    unsigned minBlock=65535,maxBlock=0;std::string error;
    static u8 crc8(std::span<const u8> bytes){u8 c=0;for(u8 b:bytes){c^=b;for(int i=0;i<8;++i)c=(u8)((c<<1)^((c&0x80)?7:0));}return c;}
    static u16 crc16(std::span<const u8> bytes){u16 c=0;for(u8 b:bytes){c^=(u16)(b<<8);for(int i=0;i<8;++i)c=(u16)((c<<1)^((c&0x8000)?0x8005:0));}return c;}
    static void utf8(std::vector<u8>& out,u64 value){if(value<128){out.push_back((u8)value);return;}unsigned count=2;
        while(count<7&&value>=(u64(1)<<(5*count+1)))++count;
        out.push_back((u8)(((0xFFu<<(8-count))&255)|(value>>(6*(count-1)))));
        for(int i=(int)count-2;i>=0;--i)out.push_back((u8)(0x80|((value>>(i*6))&63)));}
    bool emit(){if(pending.empty())return true;const auto size=(unsigned)pending.size();std::vector<u8> bytes={0xFF,0xF8,0x70,0x18};
        // Fixed-block strategy, block size is explicitly encoded in each frame.
        utf8(bytes,number++);bytes.push_back((u8)((size-1)>>8));bytes.push_back((u8)(size-1));bytes.push_back(crc8(bytes));
        for(int channel=0;channel<2;++channel){bytes.push_back(2);for(const auto& frame:pending){const u16 v=(u16)(channel?frame.right:frame.left);bytes.push_back((u8)(v>>8));bytes.push_back((u8)v);}}
        const u16 crc=crc16(bytes);bytes.push_back((u8)(crc>>8));bytes.push_back((u8)crc);
        file.write((const char*)bytes.data(),(std::streamsize)bytes.size());if(!file){failed=true;error="FLAC write failed.";return false;}
        samples+=size;minBlock=std::min(minBlock,size);maxBlock=std::max(maxBlock,size);pending.clear();return true;}
    bool streamInfo(){std::array<u8,34> info{};const unsigned min=std::max(16u,minBlock==65535?4096:minBlock),max=std::max(16u,maxBlock?maxBlock:4096);
        info[0]=(u8)(min>>8);info[1]=(u8)min;info[2]=(u8)(max>>8);info[3]=(u8)max;
        const u64 combined=((u64)rate<<44)|(u64(1)<<41)|(u64(15)<<36)|(samples&0xFFFFFFFFFull);
        for(int i=0;i<8;++i)info[10+i]=(u8)(combined>>(56-i*8));
        file.seekp(8);file.write((const char*)info.data(),info.size());return (bool)file;}
public:
    ~FlacWriter(){close();}
    bool open(const std::filesystem::path& path,unsigned sampleRate=44100){if(file.is_open())return false;
        if(sampleRate!=44100&&sampleRate!=48000&&sampleRate!=96000)return false;
        std::error_code ec;if(std::filesystem::exists(path,ec)){error="Choose a new FLAC filename.";return false;}
        file.open(path,std::ios::binary|std::ios::out);if(!file){error="Could not create FLAC file.";return false;}
        rate=sampleRate;samples=number=0;failed=false;minBlock=65535;maxBlock=0;pending.clear();error.clear();
        const u8 header[]={'f','L','a','C',0x80,0,0,34};file.write((const char*)header,8);file.write(std::string(34,'\0').data(),34);return streamInfo()&&(bool)file.seekp(0,std::ios::end);}
    bool append(std::span<const Frame> frames){if(!file.is_open()||failed)return false;for(auto frame:frames){pending.push_back(frame);if(pending.size()==4096&&!emit())return false;}return true;}
    bool close(){if(!file.is_open())return !failed;const bool emitted=emit();const bool header=streamInfo();file.flush();const bool written=(bool)file;file.close();
        if(!emitted||!header||!written){failed=true;error="Could not finalize FLAC file.";}return !failed;}
    bool recording()const{return file.is_open();}
    const std::string& lastError()const{return error;}
};

inline std::vector<u32> scale(std::span<const u32> input,int width,int height,int factor){
    if(width<=0||height<=0||width>1024||height>1024||input.size()!=(std::size_t)width*height||(factor!=2&&factor!=3))return {};
    std::vector<u32> out((std::size_t)width*height*factor*factor);
    auto pixel=[&](int x,int y){return input[(std::size_t)std::clamp(y,0,height-1)*width+std::clamp(x,0,width-1)];};
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){const u32 a=pixel(x-1,y-1),b=pixel(x,y-1),c=pixel(x+1,y-1),d=pixel(x-1,y),e=pixel(x,y),f=pixel(x+1,y),g=pixel(x-1,y+1),h=pixel(x,y+1),i=pixel(x+1,y+1);
        std::array<u32,9> p{};p.fill(e);
        if(b!=h&&d!=f){if(factor==2){p[0]=d==b?d:e;p[1]=b==f?f:e;p[2]=d==h?d:e;p[3]=h==f?f:e;}
            else {p[0]=d==b?d:e;p[1]=(d==b&&e!=c)||(b==f&&e!=a)?b:e;p[2]=b==f?f:e;
                p[3]=(d==b&&e!=g)||(d==h&&e!=a)?d:e;p[5]=(b==f&&e!=i)||(h==f&&e!=c)?f:e;
                p[6]=d==h?d:e;p[7]=(d==h&&e!=i)||(h==f&&e!=g)?h:e;p[8]=h==f?f:e;}}
        for(int py=0;py<factor;++py)for(int px=0;px<factor;++px)out[(std::size_t)(y*factor+py)*(width*factor)+x*factor+px]=p[py*factor+px];}
    return out;
}
inline u32 correctColor(u32 rgb,int preset){
    if(!preset)return rgb;const double r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;
    const auto byte=[](double x){return (u32)std::clamp(std::lround(x),0l,255l);};
    if(preset==1)return 0xFF000000|(byte((26*r+4*g+2*b)/32)<<16)|(byte((2*r+24*g+6*b)/32)<<8)|byte((4*r+4*g+24*b)/32);
    return 0xFF000000|(byte(std::pow(r/255,1.15)*255)<<16)|(byte(std::pow(g/255,1.15)*255)<<8)|byte(std::pow(b/255,1.15)*255);
}
class AviWriter {
    struct Index {u32 offset,size;bool audio;};
    std::ofstream file;u32 frames=0,audioFrames=0,sampleRate=0,clock=GB_CLOCK;std::vector<Index> index;std::streampos movi=0;bool failed=false;
    std::function<std::vector<u8>(std::span<const u32>)> encoder;
    void four(const char* s){file.write(s,4);}void number(u32 v){u8 b[4];for(int i=0;i<4;++i)b[i]=(u8)(v>>(i*8));file.write((char*)b,4);}
    const char* videoTag()const{return encoder?"00dc":"00db";}
    void header(u32 length){file.seekp(0);four("RIFF");number(length);four("AVI ");four("LIST");number(sampleRate?292:192);four("hdrl");four("avih");number(56);
        number((u32)((u64)GB_FRAME*1000000/clock));number(GB_W*GB_H*3*60+sampleRate*4);number(0);number(0x110);number(frames);number(0);number(sampleRate?2:1);number(GB_W*GB_H*3);number(GB_W);number(GB_H);for(int i=0;i<4;++i)number(0);
        four("LIST");number(116);four("strl");four("strh");number(56);four("vids");four(encoder?"MJPG":"DIB ");number(0);number(0);number(0);number(GB_FRAME);number(clock);number(0);number(frames);number(GB_W*GB_H*3);number(0xFFFFFFFF);number(0);
        number(0);number((GB_H<<16)|GB_W);four("strf");number(40);number(40);number(GB_W);number(GB_H);number((24u<<16)|1);number(encoder?0x47504A4D:0);number(GB_W*GB_H*3);for(int i=0;i<4;++i)number(0);
        if(sampleRate){four("LIST");number(92);four("strl");four("strh");number(56);four("auds");number(0);number(0);number(0);number(0);number(4);number(sampleRate*4);number(0);number(audioFrames);number(4096);number(0xFFFFFFFF);number(4);number(0);number(0);
            four("strf");number(16);number(0x00020001);number(sampleRate);number(sampleRate*4);number(0x00100004);}
        four("LIST");number(4);four("movi");movi=file.tellp();
    }
    bool chunk(const char* tag,std::span<const u8> bytes,bool audio){const auto pos=file.tellp();if(pos<0||(u64)pos+bytes.size()+8>1900000000ull||index.size()>2000000){failed=true;return false;}
        index.push_back({(u32)(pos-movi)+4,(u32)bytes.size(),audio});four(tag);number((u32)bytes.size());file.write((const char*)bytes.data(),(std::streamsize)bytes.size());if(bytes.size()&1)file.put(0);if(!file)failed=true;return !failed;}
public:
    ~AviWriter(){close();}
    bool open(const std::filesystem::path& path,unsigned audioRate=0,unsigned clockRate=GB_CLOCK,std::function<std::vector<u8>(std::span<const u32>)> videoEncoder={}){
        if(file.is_open()||std::filesystem::exists(path)||(audioRate&&audioRate!=44100&&audioRate!=48000&&audioRate!=96000)||clockRate<4000000||clockRate>4400000)return false;
        file.open(path,std::ios::binary|std::ios::out);if(!file)return false;frames=audioFrames=0;sampleRate=audioRate;clock=clockRate;encoder=std::move(videoEncoder);failed=false;index.clear();header(0);return (bool)file;}
    bool append(std::span<const u32> pixels){if(!file.is_open()||failed||pixels.size()!=GB_W*GB_H)return false;std::vector<u8> bytes;
        if(encoder){bytes=encoder(pixels);if(bytes.empty()||bytes.size()>1024*1024){failed=true;return false;}}
        else {bytes.resize(GB_W*GB_H*3);for(int y=0;y<GB_H;++y)for(int x=0;x<GB_W;++x){const u32 c=pixels[(GB_H-1-y)*GB_W+x];const std::size_t at=(y*GB_W+x)*3;bytes[at]=(u8)c;bytes[at+1]=(u8)(c>>8);bytes[at+2]=(u8)(c>>16);}}
        if(!chunk(videoTag(),bytes,false))return false;++frames;return true;}
    bool appendAudio(std::span<const Frame> samples){if(!file.is_open()||!sampleRate||failed||samples.size()>65536)return false;std::vector<u8> bytes(samples.size()*4);
        for(std::size_t n=0;n<samples.size();++n){const u16 l=(u16)samples[n].left,r=(u16)samples[n].right;bytes[n*4]=(u8)l;bytes[n*4+1]=(u8)(l>>8);bytes[n*4+2]=(u8)r;bytes[n*4+3]=(u8)(r>>8);}if(!chunk("01wb",bytes,true))return false;audioFrames+=(u32)samples.size();return true;}
    bool close(){if(!file.is_open())return !failed;const auto end=file.tellp();four("idx1");number((u32)index.size()*16);
        for(auto item:index){four(item.audio?"01wb":videoTag());number(item.audio?0:0x10);number(item.offset);number(item.size);}const auto total=file.tellp();header((u32)total-8);
        file.seekp(movi-std::streamoff(8));number((u32)(end-movi)+4);file.flush();const bool result=(bool)file;file.close();return result&&!failed;}
    bool recording()const{return file.is_open();}
};
} // namespace media
