#pragma once
#include "gameboy.hpp"
#include <bit>
#include <type_traits>
#include <memory>

// BESS 1.1 plus a portable, field-encoded GBSP extension. Native GBSTATE3-6
// remain readable. No C++ object layout or padding is stored in GBSP.
namespace saves {
inline u32 le32(std::span<const u8> b,std::size_t at){return (u32)b[at]|((u32)b[at+1]<<8)|((u32)b[at+2]<<16)|((u32)b[at+3]<<24);}
inline u16 le16(std::span<const u8> b,std::size_t at){return (u16)(b[at]|(b[at+1]<<8));}
inline void put32(std::vector<u8>& b,u32 v){for(int i=0;i<4;++i)b.push_back((u8)(v>>(i*8)));}
inline void set32(std::vector<u8>& b,std::size_t at,u32 v){for(int i=0;i<4;++i)b[at+i]=(u8)(v>>(i*8));}
inline void set16(std::vector<u8>& b,std::size_t at,u16 v){b[at]=(u8)v;b[at+1]=(u8)(v>>8);}
inline u32 hash(std::span<const u8> b){u32 v=2166136261u;for(u8 n:b){v^=n;v*=16777619u;}return v;}
struct Codec {
    std::vector<u8> output;std::span<const u8> input;std::size_t at=0;bool reading=false,ok=true;
    explicit Codec(std::span<const u8> bytes):input(bytes),reading(true){}
    Codec()=default;
    template<class T> void field(T& value){
        constexpr unsigned size=std::is_same_v<T,bool>?1:std::is_same_v<T,int>?4:sizeof(T);
        static_assert(size<=8);
        u64 v=0;
        if(reading){if(at>input.size()||size>input.size()-at){ok=false;return;}
            for(unsigned i=0;i<size;++i)v|=(u64)input[at++]<<(8*i);
            if constexpr(std::is_same_v<T,bool>){if(v>1){ok=false;return;}value=v!=0;}
            else if constexpr(std::is_same_v<T,double>){value=std::bit_cast<double>(v);if(!std::isfinite(value))ok=false;}
            else if constexpr(std::is_same_v<T,int>)value=(int)(std::int32_t)(u32)v;
            else value=(T)v;
        }else{
            if constexpr(std::is_same_v<T,double>)v=std::bit_cast<u64>(value);
            else v=(u64)value;
            for(unsigned i=0;i<size;++i)output.push_back((u8)(v>>(8*i)));
        }
    }
    template<class... T> void fields(T&... v){(field(v),...);}
    template<class T> void array(T& data){for(auto& v:data)field(v);}
};
inline void visit(GameBoy& g,Codec& c){
    auto& m=g.mem;auto& cpu=g.cpu;auto& p=g.ppu;auto& a=g.apu;auto& s=p.state;auto& t=a.state;
    c.fields(cpu.A,cpu.F,cpu.B,cpu.C,cpu.D,cpu.E,cpu.H,cpu.L,cpu.SP,cpu.PC,cpu.IME,cpu.halted,cpu.stopped,cpu.haltBug,cpu.locked,cpu.imeDelay);
    c.fields(m.ie,m.divCounter,m.timerReload,m.timerJustReloaded,m.serialTicks,m.serialBits,m.serialSent,m.dmaTicks,m.dmaPos,m.dmaDelay,m.dmaSource,m.pendingDmaSource);
    c.fields(m.ppuMode,m.ppuStatMode,m.ppuDot,m.ppuCompareLY,m.statLine,m.ramEnabled,m.romBank,m.bankHigh,m.ramBank,m.mbcMode,m.latchWrite,m.rtcLatched,m.rtcCycles);
    c.fields(m.cgbMode,m.cgbHardware,m.dmgCompatibility,m.doubleSpeed,m.cpuHalted,m.hdmaActive,m.hdmaHblank,m.hdmaSource,m.hdmaDest,m.hdmaBlocks,m.hdmaBytes,m.hdmaDot,m.dotPhase,m.speedSwitchTicks);
    c.fields(m.bootROMMapped,m.bootROMCGB,m.right,m.left,m.up,m.down,m.a,m.b,m.select,m.start,m.rumbleActive,m.hucIRMode,m.hucIRLight);
    c.array(m.vram);c.array(m.wram);c.array(m.oam);c.array(m.io);c.array(m.hram);c.array(m.bgPalette);c.array(m.objPalette);c.array(m.rtc);c.array(m.latchedRTC);c.array(m.soundTrigger);
    u32 size=(u32)m.eram.size();c.field(size);if(size!=m.eram.size()){c.ok=false;return;}c.array(m.eram);
    auto& e=m.motion;c.fields(e.enabled,e.latchReady,e.writeEnabled,e.pins,e.phase,e.bits,e.command,e.readWord,e.x,e.y,e.shift,e.tiltX,e.tiltY);
    c.array(m.cameraRegisters);c.field(m.cameraTicks);c.array(m.cameraPending);
    c.fields(g.currentPalette,g.frameDebt,g.totalCycles,p.dot,p.palette,p.lcdWasOn,p.frameNumber);
    c.fields(s.line,s.windowLine,s.selectedCount,s.x,s.stall,s.scx,s.lastSpriteTile,s.windowX,s.windowYTriggered,s.windowActive,s.windowDrawn,s.firstLine);
    for(auto& sprite:s.sprites)c.fields(sprite.id,sprite.x,sprite.y,sprite.tile,sprite.attr);c.array(s.fetched);
    c.array(p.pixels);c.array(p.drawing);
    for(auto& channel:t.channels)c.fields(channel.timer,channel.length,channel.phase,channel.volume,channel.envelopeTimer,channel.enabled,channel.dac,channel.lengthEnabled,channel.envelopeRunning,channel.started);
    c.fields(t.frameStep,t.sweepShadow,t.sweepTimer,t.waveAccess,t.waveByte,t.sweepEnabled,t.sweepNegated,t.lfsr,t.waveBuffer,t.sampleClock,t.mixDivider,t.mixCount,t.sumLeft,t.sumRight,t.capLeft,t.capRight,a.sampleRate);
}
inline void configureClone(GameBoy& target,const GameBoy& source){
    target.mem.rom=source.mem.rom;target.mem.eram=source.mem.eram;target.mem.romPath=source.mem.romPath;
    target.mem.mbc=source.mem.mbc;target.mem.hasRTC=source.mem.hasRTC;target.mem.hasBattery=source.mem.hasBattery;
    target.mem.cgbROM=source.mem.cgbROM;target.mem.mbc1Multicart=source.mem.mbc1Multicart;target.mem.rumble=source.mem.rumble;
    target.mem.dmgBootROM=source.mem.dmgBootROM;target.mem.cgbBootROM=source.mem.cgbBootROM;
    target.model=source.model;target.reset();target.mem.bootROM=source.mem.bootROM;target.mem.cgbMode=source.mem.cgbMode;target.mem.cgbHardware=source.mem.cgbHardware;target.mem.dmgCompatibility=source.mem.dmgCompatibility;
}
inline std::vector<u8> portable(const GameBoy& source){
    auto cloneStorage=std::make_unique<GameBoy>();auto& clone=*cloneStorage;configureClone(clone,source);
    if(!clone.deserializeState(source.serializeState()))return {};
    Codec c;u32 signature=0x32505347,rom=source.romHash(),boot=source.mem.bootROMMapped?hash(source.mem.bootROM):0;
    c.fields(signature,rom,boot);visit(clone,c);const u32 sum=hash(c.output);put32(c.output,sum);return c.output;
}
inline void block(std::vector<u8>& file,std::string_view name,std::span<const u8> data){file.insert(file.end(),name.begin(),name.end());put32(file,(u32)data.size());file.insert(file.end(),data.begin(),data.end());}
inline std::vector<u8> exportBess(const GameBoy& g){
    if(g.backendBess)return g.backendBess();
    if(g.mem.rom.empty())return {};
    std::vector<u8> file,core(0xD0);set16(core,0,1);set16(core,2,1);
    const std::string model=g.mem.isCGBHardware()?"CC  ":"GD  ";std::copy(model.begin(),model.end(),core.begin()+4);
    set16(core,8,g.cpu.PC);set16(core,10,g.cpu.AF());set16(core,12,g.cpu.BC());set16(core,14,g.cpu.DE());set16(core,16,g.cpu.HL());set16(core,18,g.cpu.SP);
    core[20]=g.cpu.IME?1:0;core[21]=g.mem.ie;core[22]=g.cpu.stopped?2:g.cpu.halted?1:0;
    std::copy(g.mem.io.begin(),g.mem.io.end(),core.begin()+24);core[24+4]=(u8)(g.mem.divCounter>>8);
    core[24+0x50]=g.mem.bootROMMapped?0:1;core[24+0x4C]=g.mem.cgbMode?0x80:g.mem.dmgCompatibility?4:0;
    auto buffer=[&](std::size_t at,std::span<const u8> bytes){set32(core,at,(u32)bytes.size());set32(core,at+4,(u32)file.size());file.insert(file.end(),bytes.begin(),bytes.end());};
    buffer(0x98,std::span<const u8>(g.mem.wram).first(g.mem.isCGBHardware()?32768:8192));
    buffer(0xA0,std::span<const u8>(g.mem.vram).first(g.mem.isCGBHardware()?16384:8192));
    buffer(0xA8,g.mem.eram);buffer(0xB0,g.mem.oam);buffer(0xB8,g.mem.hram);
    buffer(0xC0,g.mem.isCGBHardware()?std::span<const u8>(g.mem.bgPalette):std::span<const u8>());
    buffer(0xC8,g.mem.isCGBHardware()?std::span<const u8>(g.mem.objPalette):std::span<const u8>());
    const u32 start=(u32)file.size();const std::string name="GameBoy Studio 0.9";block(file,"NAME",{(const u8*)name.data(),name.size()});
    std::vector<u8> info(g.mem.rom.begin()+0x134,g.mem.rom.begin()+0x144);info.push_back(g.mem.rom[0x14E]);info.push_back(g.mem.rom[0x14F]);block(file,"INFO",info);block(file,"CORE",core);
    std::vector<u8> mbc;auto write=[&](u16 addr,u8 value){mbc.push_back((u8)addr);mbc.push_back((u8)(addr>>8));mbc.push_back(value);};
    if(g.mem.mbc){write(0,g.mem.ramEnabled?0x0A:0);write(g.mem.mbc==2?0x2100:0x2000,(u8)g.mem.romBank);
        if(g.mem.mbc==5)write(0x3000,(u8)(g.mem.romBank>>8));write(0x4000,g.mem.mbc==1?g.mem.bankHigh:g.mem.ramBank);
        if(g.mem.mbc==1)write(0x6000,g.mem.mbcMode);if(g.mem.mbc==7)write(0x4000,g.mem.motion.enabled?0x40:0);
        if(g.mem.mbc==101)write(0,g.mem.hucIRMode?0x0E:0);block(file,"MBC ",mbc);}
    if(g.mem.hasRTC){std::vector<u8> rtc(48);for(int i=0;i<5;++i){rtc[i*4]=g.mem.rtc[i];rtc[20+i*4]=g.mem.latchedRTC[i];}
        const auto now=(u64)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        for(int i=0;i<8;++i)rtc[40+i]=(u8)(now>>(i*8));block(file,"RTC ",rtc);}
    const auto exact=portable(g);if(exact.empty())return {};block(file,"GBSP",exact);block(file,"END ",{});put32(file,start);file.insert(file.end(),{'B','E','S','S'});return file;
}
inline bool importBess(GameBoy& live,std::span<const u8> file,std::string* error=nullptr){
    if(live.backendRestore){const bool ok=live.backendRestore(file);if(error)*error=ok?"":"Hardware backend rejected the BESS state.";return ok;}
    auto fail=[&](const char* text){if(error)*error=text;return false;};
    if(live.mem.rom.size()<0x150)return fail("Load a ROM before importing a state.");
    if(file.size()<16||file.size()>2*1024*1024||std::memcmp(file.data()+file.size()-4,"BESS",4))return fail("Missing BESS footer.");
    std::size_t at=le32(file,file.size()-8);if(at>file.size()-8)return fail("Invalid BESS block offset.");
    std::span<const u8> core,exact,info,mbc,rtc;bool end=false;unsigned blocks=0;
    while(at<file.size()-8&&++blocks<=128){if(file.size()-8-at<8)return fail("Truncated BESS block.");
        const std::string tag((const char*)file.data()+at,4);const auto size=le32(file,at+4);at+=8;
        if(size>file.size()-8-at)return fail("BESS block exceeds file.");const auto data=file.subspan(at,size);at+=size;
        if(tag=="CORE"){if(!core.empty())return fail("Duplicate CORE block.");core=data;}
        else if(tag=="GBSP"){if(!exact.empty())return fail("Duplicate GBSP block.");exact=data;}
        else if(tag=="INFO")info=data;else if(tag=="MBC ")mbc=data;else if(tag=="RTC ")rtc=data;
        else if(tag=="END "){if(size||at!=file.size()-8)return fail("Invalid END block.");end=true;break;}
    }
    if(!end||core.size()<0xD0||le16(core,0)!=1)return fail("Missing CORE/END or unsupported BESS version.");
    if(core[4]!='G'&&core[4]!='C')return fail("This state requires an unsupported SGB model.");
    if(!info.empty()&&(info.size()!=18||!std::equal(info.begin(),info.begin()+16,live.mem.rom.begin()+0x134)||info[16]!=live.mem.rom[0x14E]||info[17]!=live.mem.rom[0x14F]))return fail("BESS cartridge does not match the loaded ROM.");
    for(std::size_t offset=0x98;offset<=0xC8;offset+=8){const u32 size=le32(core,offset),pos=le32(core,offset+4);if(pos>file.size()-8||size>file.size()-8-pos)return fail("Invalid BESS memory buffer.");}
    const bool color=core[4]=='C',compat=color&&(core[24+0x4C]&4);
    if(color!=live.mem.isCGBHardware()||(core[24+0x50]!=0&&compat!=live.mem.dmgCompatibility))return fail("Select the same hardware model before importing this state.");
    auto storage=std::make_unique<GameBoy>();auto& tmp=*storage;configureClone(tmp,live);
    if(!exact.empty()){
        if(exact.size()<16||hash(exact.first(exact.size()-4))!=le32(exact,exact.size()-4))return fail("GBSP checksum mismatch.");
        Codec codec(exact.first(exact.size()-4));u32 version=0,rom=0,boot=0;codec.fields(version,rom,boot);
        if(version!=0x32505347||rom!=live.romHash())return fail("GBSP version/ROM mismatch.");visit(tmp,codec);
        if(!codec.ok||codec.at!=codec.input.size())return fail("Invalid portable state payload.");
        if(tmp.mem.bootROMMapped){const auto& image=tmp.mem.bootROMCGB?live.mem.cgbBootROM:live.mem.dmgBootROM;
            if(hash(live.mem.bootROM)==boot&&!live.mem.bootROM.empty())tmp.mem.bootROM=live.mem.bootROM;
            else if(hash(image)==boot&&!image.empty())tmp.mem.bootROM=image;else return fail("This state needs its original boot ROM.");}
    }else{
        tmp.cpu.PC=le16(core,8);tmp.cpu.setAF(le16(core,10));tmp.cpu.setBC(le16(core,12));tmp.cpu.setDE(le16(core,14));tmp.cpu.setHL(le16(core,16));tmp.cpu.SP=le16(core,18);
        if(core[20]>1||core[22]>2)return fail("Invalid BESS CPU execution state.");tmp.cpu.IME=core[20]!=0;tmp.cpu.halted=core[22]==1;tmp.cpu.stopped=core[22]==2;
        std::copy(core.begin()+24,core.begin()+152,tmp.mem.io.begin());tmp.mem.ie=core[21];tmp.mem.divCounter=(u16)(tmp.mem.io[4]<<8);
        tmp.mem.cgbMode=color&&!compat;tmp.mem.bootROMMapped=core[24+0x50]==0;tmp.mem.bootROMCGB=color;
        if(tmp.mem.bootROMMapped){tmp.mem.bootROM=color?live.mem.cgbBootROM:live.mem.dmgBootROM;if(tmp.mem.bootROM.empty())return fail("Boot ROM is required.");}
        auto buffer=[&](std::size_t offset,auto& dest){const u32 size=le32(core,offset),pos=le32(core,offset+4);
            if(pos>file.size()-8||size>file.size()-8-pos)return false;std::fill(dest.begin(),dest.end(),0);
            std::copy_n(file.begin()+pos,std::min<std::size_t>(size,dest.size()),dest.begin());return true;};
        if(!buffer(0x98,tmp.mem.wram)||!buffer(0xA0,tmp.mem.vram)||!buffer(0xA8,tmp.mem.eram)||!buffer(0xB0,tmp.mem.oam)||!buffer(0xB8,tmp.mem.hram)||!buffer(0xC0,tmp.mem.bgPalette)||!buffer(0xC8,tmp.mem.objPalette))return fail("Invalid BESS memory buffer.");
        if(mbc.size()%3)return fail("Invalid MBC block.");for(std::size_t n=0;n<mbc.size();n+=3){const u16 addr=le16(mbc,n);if(addr>=0x8000&&(addr<0xA000||addr>=0xC000))return fail("Invalid MBC register address.");tmp.mem.write(addr,mbc[n+2]);}
        if(!rtc.empty()){if(rtc.size()!=48)return fail("Invalid RTC block.");for(int i=0;i<5;++i){tmp.mem.rtc[i]=rtc[i*4];tmp.mem.latchedRTC[i]=rtc[20+i*4];}tmp.mem.rtcLatched=true;}
        tmp.mem.doubleSpeed=color&&!compat&&(tmp.mem.io[0x4D]&0x80);tmp.mem.io[0x4D]&=1;tmp.mem.io[0x4F]&=1;tmp.mem.io[0x70]&=7;tmp.mem.io[0x68]&=0xBF;tmp.mem.io[0x6A]&=0xBF;
        const u8 ly=tmp.mem.io[0x44];if(ly>153)return fail("Invalid BESS scanline.");tmp.ppu.reset();tmp.ppu.state.line=ly;tmp.mem.io[0x44]=tmp.mem.ppuCompareLY=ly;
        if(ly>=144)tmp.mem.ppuMode=tmp.mem.ppuStatMode=1;tmp.apu.reset();tmp.mem.hdmaActive=tmp.mem.hdmaHblank=false;tmp.mem.hdmaBytes=0;tmp.mem.serialTicks=0;tmp.mem.serialBits=0;
        tmp.mem.refreshStat();
    }
    auto validator=std::make_unique<GameBoy>();auto& validate=*validator;configureClone(validate,live);const auto native=tmp.serializeState();if(!validate.deserializeState(native))return fail("State failed core validation.");
    if(!live.deserializeState(native))return fail("State could not be committed.");if(error)error->clear();return true;
}
inline bool save(GameBoy& gb,const std::filesystem::path& path){const auto bytes=exportBess(gb);return !bytes.empty()&&Memory::writeAtomic(path,{(const char*)bytes.data(),bytes.size()},true);}
inline bool load(GameBoy& gb,const std::filesystem::path& path,std::string* error=nullptr){const auto bytes=Memory::loadBytes(path,6*1024*1024);
    if(bytes.size()>=4&&!std::memcmp(bytes.data()+bytes.size()-4,"BESS",4))return importBess(gb,bytes,error);return gb.deserializeState(bytes);}
} // namespace saves
