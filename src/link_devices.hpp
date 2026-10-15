#pragma once
#include "portable_state.hpp"
#include "session.hpp"
#include <deque>

namespace devices {
class LocalLink {
    GameBoy* first=nullptr,*second=nullptr;u64 target=0;
    static void clock(Memory& sender,Memory& receiver,bool secondary){
        // Both internal clocks is undefined software usage; consistently let
        // the first console drive the cable while both transfers are active.
        if(secondary&&(receiver.io[2]&0x81)==0x81)return;
        const bool outA=sender.externalSerialOut?sender.externalSerialOut():(sender.io[1]&0x80)!=0;
        const bool outB=(receiver.io[2]&0x80)?(receiver.externalSerialOut?receiver.externalSerialOut():(receiver.io[1]&0x80)!=0):true;
        sender.serialPulse(outB);receiver.serialPulse(outA);
    }
public:
    LocalLink()=default;LocalLink(const LocalLink&)=delete;LocalLink& operator=(const LocalLink&)=delete;
    ~LocalLink(){disconnect();}
    void connect(GameBoy& a,GameBoy& b){disconnect();first=&a;second=&b;target=std::max(a.totalCycles-(u64)a.frameDebt,b.totalCycles-(u64)b.frameDebt);
        a.mem.serialClock=[this](){clock(first->mem,second->mem,false);};b.mem.serialClock=[this](){clock(second->mem,first->mem,true);};
        a.mem.infraredInput=[this](){return (second->mem.io[0x56]&1)||second->mem.hucIRLight;};
        b.mem.infraredInput=[this](){return (first->mem.io[0x56]&1)||first->mem.hucIRLight;};}
    void disconnect(){if(first){first->mem.serialClock={};first->mem.infraredInput={};}if(second){second->mem.serialClock={};second->mem.infraredInput={};}first=second=nullptr;target=0;}
    bool connected()const{return first&&second;}
    void runFrame(){if(!connected())return;target+=GB_FRAME;
        while(first->totalCycles<target||second->totalCycles<target){
            if(second->totalCycles>=target||(first->totalCycles<target&&first->totalCycles<=second->totalCycles))first->cpu.step();else second->cpu.step();}
        first->frameDebt=(int)(first->totalCycles-target);second->frameDebt=(int)(second->totalCycles-target);
        if(first->backendRefresh)first->backendRefresh();if(second->backendRefresh)second->backendRefresh();}
    u64 frameTarget()const{return target;}
    void setFrameTarget(u64 value){if(connected())target=value;}
    void synchronize(){if(connected())target=std::max(first->totalCycles-(u64)first->frameDebt,second->totalCycles-(u64)second->frameDebt);}
};

// Two-machine snapshots let a caller replace predicted inputs and replay a
// bounded window. Network transport is separate; no ROMs are sent by this API.
class Rollback {
    struct Frame {u64 number;u8 a,b;std::vector<u8> first,second;};
    GameBoy& first;GameBoy& second;LocalLink& link;std::deque<Frame> frames;u64 next=0;std::size_t capacity;
public:
    explicit Rollback(GameBoy& a,GameBoy& b,LocalLink& cable,std::size_t window=12):first(a),second(b),link(cable),capacity(std::clamp<std::size_t>(window,1,120)){}
    void advance(u8 a,u8 b){if(frames.size()==capacity)frames.pop_front();frames.push_back({next++,a,b,first.serializeState(),second.serializeState()});
        InputMovie::setJoypad(first.mem,a);InputMovie::setJoypad(second.mem,b);link.runFrame();second.apu.clearQueue();}
    bool correct(u64 number,u8 input,bool playerTwo){auto point=std::find_if(frames.begin(),frames.end(),[&](const Frame& f){return f.number==number;});
        if(point==frames.end())return false;if((playerTwo?point->b:point->a)==input)return true;
        const auto oldA=first.serializeState(),oldB=second.serializeState();
        if(!first.deserializeState(point->first)||!second.deserializeState(point->second)){first.deserializeState(oldA);second.deserializeState(oldB);return false;}
        (playerTwo?point->b:point->a)=input;link.synchronize();
        for(auto it=point;it!=frames.end();++it){it->first=first.serializeState();it->second=second.serializeState();
            InputMovie::setJoypad(first.mem,it->a);InputMovie::setJoypad(second.mem,it->b);link.runFrame();first.apu.clearQueue();second.apu.clearQueue();}return true;}
    u64 frame()const{return next;}
};
inline std::vector<u8> savePair(GameBoy& first,GameBoy& second,const LocalLink& cable){const auto a=first.serializeState(),b=second.serializeState();if(a.empty()||b.empty()||!cable.connected())return {};
    std::vector<u8> data={'G','B','P','A','I','R','2',0};saves::put32(data,first.romHash());saves::put32(data,second.romHash());saves::put32(data,(u32)a.size());saves::put32(data,(u32)b.size());const auto target=cable.frameTarget();for(unsigned i=0;i<8;++i)data.push_back((u8)(target>>(i*8)));data.insert(data.end(),a.begin(),a.end());data.insert(data.end(),b.begin(),b.end());saves::put32(data,saves::hash(data));return data;}
inline bool loadPair(GameBoy& first,GameBoy& second,LocalLink& cable,std::span<const u8> data){if(data.size()<36||data.size()>12*1024*1024||std::memcmp(data.data(),"GBPAIR2",8)||saves::le32(data,8)!=first.romHash()||saves::le32(data,12)!=second.romHash()||saves::hash(data.first(data.size()-4))!=saves::le32(data,data.size()-4))return false;
    const auto a=saves::le32(data,16),b=saves::le32(data,20);if(!a||!b||a>6*1024*1024||b>6*1024*1024||(u64)a+b+36!=data.size())return false;u64 target=0;for(unsigned i=0;i<8;++i)target|=(u64)data[24+i]<<(i*8);
    const auto oldA=first.serializeState(),oldB=second.serializeState();if(!first.deserializeState(data.subspan(32,a))||!second.deserializeState(data.subspan(32+a,b))||target>std::max(first.totalCycles,second.totalCycles)||std::max(first.totalCycles,second.totalCycles)-target>GB_FRAME*2){first.deserializeState(oldA);second.deserializeState(oldB);return false;}
    cable.connect(first,second);cable.setFrameTarget(target);first.apu.clearQueue();second.apu.clearQueue();return true;}

inline bool bitmap(const std::filesystem::path& path,std::span<const u32> pixels,unsigned width,unsigned height){
    if(!width||!height||width>2048||height>2048||pixels.size()!=(std::size_t)width*height)return false;
    std::vector<u8> file(54+(std::size_t)width*height*4);file[0]='B';file[1]='M';
    saves::set32(file,2,(u32)file.size());saves::set32(file,10,54);saves::set32(file,14,40);saves::set32(file,18,width);saves::set32(file,22,height);
    saves::set16(file,26,1);saves::set16(file,28,32);saves::set32(file,34,width*height*4);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)saves::set32(file,54+((std::size_t)(height-1-y)*width+x)*4,pixels[(std::size_t)y*width+x]);
    return Memory::writeAtomic(path,{(const char*)file.data(),file.size()});
}
class Printer {
    Memory* attached=nullptr;
    GameBoy* native=nullptr;
    int stage=0,bits=0,busyPolls=0;u8 command=0,compression=0,status=0,incoming=0,reply=0;
    u16 length=0,sum=0,checksum=0;bool terminated=false;
    std::vector<u8> payload,tiles;
    struct Page {unsigned height;std::vector<u32> pixels;};
    std::deque<Page> pages;
    void execute(){
        if(sum!=checksum){status|=1;return;}status&=(u8)~1;
        if(command==1){tiles.clear();status=0;terminated=false;busyPolls=0;}
        else if(command==4){
            if(payload.empty()){terminated=true;return;}terminated=false;std::vector<u8> decoded;
            if(compression){std::size_t at=0;while(at<payload.size()){
                const u8 control=payload[at++];const std::size_t count=(control&127)+(control&128?2:1);
                if(count>8192-decoded.size()){status|=0x10;return;}
                if(control&128){if(at==payload.size()){status|=0x10;return;}decoded.insert(decoded.end(),count,payload[at++]);}
                else {if(count>payload.size()-at){status|=0x10;return;}decoded.insert(decoded.end(),payload.begin()+at,payload.begin()+at+count);at+=count;}
            }}else decoded=payload;
            if(decoded.size()>8192-tiles.size()){status|=4;return;}tiles.insert(tiles.end(),decoded.begin(),decoded.end());status=tiles.empty()?0:8;
        }else if(command==2){
            if(payload.size()!=4||!terminated||tiles.empty()||tiles.size()%320){status|=0x10;return;}
            const unsigned top=(payload[1]>>4)*8,bottom=(payload[1]&15)*8,imageHeight=(unsigned)(tiles.size()/320)*8;
            Page page{top+imageHeight+bottom,std::vector<u32>((std::size_t)160*(top+imageHeight+bottom),0xFFFFFFFF)};
            for(unsigned y=0;y<imageHeight;++y)for(unsigned x=0;x<160;++x){const std::size_t offset=((y/8)*20+x/8)*16+(y&7)*2;
                const unsigned bit=7-(x&7),raw=((tiles[offset]>>bit)&1)|(((tiles[offset+1]>>bit)&1)<<1),shade=(payload[2]>>(2*raw))&3;
                const u32 gray=255-shade*85;page.pixels[(y+top)*160+x]=0xFF000000|(gray<<16)|(gray<<8)|gray;}
            if(payload[0]){if(pages.size()==32)pages.pop_front();pages.push_back(std::move(page));}busyPolls=2;status=6;terminated=false;
        }else if(command==0x0F){if(busyPolls>0){if(--busyPolls==0){status=4;tiles.clear();}}}
        else status|=0x10;
    }
public:
    Printer()=default;Printer(const Printer&)=delete;Printer& operator=(const Printer&)=delete;
    ~Printer(){detach();}
    u8 transfer(u8 byte){const u8 result=reply;reply=0;
        switch(stage){case 0:if(byte==0x88)stage=1;break;case 1:stage=byte==0x33?2:byte==0x88?1:0;break;
            case 2:command=byte;sum=byte;payload.clear();length=0;stage=3;break;
            case 3:compression=byte;sum+=byte;stage=4;break;case 4:length=byte;sum+=byte;stage=5;break;
            case 5:length|=(u16)(byte<<8);sum+=byte;if(length>8192){status|=0x10;stage=0;}else stage=length?6:7;break;
            case 6:payload.push_back(byte);sum+=byte;if(payload.size()==length)stage=7;break;
            case 7:checksum=byte;stage=8;break;case 8:checksum|=(u16)(byte<<8);execute();reply=0x81;stage=9;break;
            case 9:reply=status;stage=10;break;case 10:stage=0;break;default:stage=0;}
        return result;
    }
    void attach(Memory& m){detach();attached=&m;bits=0;incoming=0;reply=0;stage=0;
        m.serialClock=[this](){const bool bit=(attached->io[1]&0x80)!=0,input=(reply&(0x80>>bits))!=0;
            incoming=(u8)((incoming<<1)|(bit?1:0));attached->serialPulse(input);
            if(++bits==8){transfer(incoming);bits=0;incoming=0;}};}
    void attach(GameBoy& g){if(!g.backendPrinter){attach(g.mem);return;}detach();native=&g;
        g.backendPrint=[this](std::span<const u32> image,unsigned height,unsigned top,unsigned bottom){
            if(image.size()!=(std::size_t)160*height||height+top+bottom>512)return;
            Page page{height+top+bottom,std::vector<u32>((std::size_t)160*(height+top+bottom),0xFFFFFFFF)};
            std::copy(image.begin(),image.end(),page.pixels.begin()+(std::size_t)top*160);if(pages.size()==32)pages.pop_front();pages.push_back(std::move(page));};g.backendPrinter(true);}
    void detach(){if(attached)attached->serialClock={};attached=nullptr;if(native){native->backendPrinter(false);native->backendPrint={};native=nullptr;}}
    bool connected()const{return attached!=nullptr||native!=nullptr;}
    std::size_t pageCount()const{return pages.size();}
    bool saveLast(const std::filesystem::path& path)const{return !pages.empty()&&bitmap(path,pages.back().pixels,160,pages.back().height);}
};
} // namespace devices
