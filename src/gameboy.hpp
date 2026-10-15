#pragma once
// Game Boy Studio - original DMG/CGB core, clocked at individual CPU bus accesses.
// No copyrighted ROM or Nintendo boot ROM is included.
#include "accuracy_api.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using u8 = std::uint8_t;
using s8 = std::int8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
static constexpr int GB_W=160, GB_H=144, GB_CLOCK=4194304, GB_FRAME=70224;
enum class HardwareModel { Auto, DMG, CGB, Pocket, Light, CGB0, CGBA, CGBB, CGBC, CGBD, CGBE, AGB, SGB, SGB2, SGBPAL };

class Memory {
public:
    std::vector<u8> rom, eram;
    // Boot images are supplied by the user. Configuration changes take effect
    // on the next reset; the active image stays stable while the CPU runs.
    std::vector<u8> dmgBootROM,cgbBootROM,bootROM;
    std::filesystem::path dmgBootROMPath,cgbBootROMPath;
    bool bootROMMapped=false,bootROMCGB=false;
    std::array<u8,16384> vram{};
    std::array<u8,32768> wram{};
    std::array<u8,64> bgPalette{},objPalette{};
    std::array<u8,160> oam{};
    std::array<u8,128> io{};
    std::array<u8,127> hram{};
    std::array<u32,4> soundTrigger{};
    u8 ie=0;
    u16 divCounter=0;
    int timerReload=-1, serialTicks=0, dmaTicks=0, dmaPos=160, dmaDelay=0;
    bool timerJustReloaded=false, mbc1Multicart=false, rumble=false;
    u16 dmaSource=0,pendingDmaSource=0;
    int ppuMode=2,ppuStatMode=2,ppuDot=0;
    u8 serialSent=0,ppuCompareLY=0;
    bool statLine=false;
    bool right=false,left=false,up=false,down=false,a=false,b=false,select=false,start=false;
    bool ramEnabled=false, hasBattery=false, hasRTC=false, cgbROM=false;
    bool cgbMode=false,doubleSpeed=false,cpuHalted=false;
    bool cgbHardware=false,dmgCompatibility=false,rumbleActive=false,hucIRMode=false,hucIRLight=false;
    int serialBits=0;
    std::function<void()> serialClock;
    std::function<bool()> infraredInput;
    std::function<u8(u16,u8)> romReadFilter;
    std::function<void(u16,u8,bool,bool)> busObserver;
    std::function<void(u16,u8,bool)> ppuBusObserver;
    struct MotionEEPROM {
        bool enabled=false,latchReady=false,writeEnabled=false;
        u8 pins=0; int phase=0,bits=0; u16 command=0,readWord=0xFFFF,x=0x8000,y=0x8000;
        u32 shift=0; std::int16_t tiltX=0,tiltY=0;
    } motion;
    std::array<u8,64> cameraRegisters{};
    std::array<u8,3584> cameraPending{};
    int cameraTicks=0;
    std::function<std::array<u8,128*112>()> cameraImage;
    bool hdmaActive=false,hdmaHblank=false;
    u16 hdmaSource=0,hdmaDest=0x8000;
    int hdmaBlocks=0,hdmaBytes=0,hdmaDot=0,dotPhase=0,speedSwitchTicks=0;
    int mbc=0;
    u16 romBank=1;
    u8 bankHigh=0, ramBank=0, mbcMode=0, latchWrite=0;
    std::array<u8,5> rtc{}, latchedRTC{};
    bool rtcLatched=false;
    u64 rtcCycles=0;
    std::filesystem::path romPath;
    std::string gameTitle;
    std::string serialOutput;
    std::string loadError;
    std::function<void(u16,u8)> soundWrite;
    std::function<u8(u16)> soundRead;
    std::function<u8(u16)> soundPCM;
    std::function<void()> dividerReset;
    std::function<void(bool)> lcdPower;
    // Optional hardware backend. The original core remains usable for legacy states.
    std::function<u8(u16)> externalRead;
    std::function<void(u16,u8)> externalWrite;
    std::function<void(bool)> externalSerialPulse;
    std::function<bool()> externalSerialOut;
    std::function<bool()> externalBatterySave;
    std::function<std::size_t(bool)> externalROMBank;
    std::function<void(unsigned,std::size_t,u8)> externalPoke;
    bool extendedControllers=false;
    void pokeVRAM(unsigned bank,std::size_t offset,u8 value){if(bank>1||offset>=8192)return;vram[bank*8192+offset]=value;if(externalPoke)externalPoke(3,bank*8192+offset,value);}
    void pokeOAM(std::size_t offset,u8 value){if(offset>=oam.size())return;oam[offset]=value;if(externalPoke)externalPoke(7,offset,value);}
    void pokePalette(bool obj,std::size_t offset,u8 value){if(offset>=64)return;(obj?objPalette:bgPalette)[offset]=value;if(externalPoke)externalPoke(obj?9:8,offset,value);}

    Memory() { resetIO(); }
    void resetIO() {
        io.fill(0); hram.fill(0); vram.fill(0); wram.fill(0); oam.fill(0);
        soundTrigger.fill(0); divCounter=0; ie=0; timerReload=-1; timerJustReloaded=false;
        bgPalette.fill(0xFF);objPalette.fill(0xFF);
        doubleSpeed=cpuHalted=hdmaActive=hdmaHblank=false;
        hdmaSource=0;hdmaDest=0x8000;hdmaBlocks=hdmaBytes=hdmaDot=dotPhase=speedSwitchTicks=0;
        serialTicks=0;serialBits=0;rumbleActive=false;hucIRMode=hucIRLight=false;motion={};cameraRegisters.fill(0);cameraPending.fill(0);cameraTicks=0; dmaTicks=0; dmaPos=160; dmaDelay=0;
        dmaSource=pendingDmaSource=0;
        ppuMode=ppuStatMode=2;ppuDot=0; statLine=false;serialSent=ppuCompareLY=0;
        serialOutput.clear();
        right=left=up=down=a=b=select=start=false;
        io[0x00]=0xCF; io[0x0F]=0xE1;
        io[0x10]=0x80; io[0x11]=0xBF; io[0x12]=0xF3; io[0x14]=0xBF;
        io[0x16]=0x3F; io[0x19]=0xBF; io[0x1A]=0x7F; io[0x1B]=0xFF;
        io[0x1C]=0x9F; io[0x1E]=0xBF; io[0x20]=0xFF; io[0x23]=0xBF;
        io[0x24]=0x77; io[0x25]=0xF3; io[0x26]=0xF1;
        io[0x40]=0x91; io[0x41]=0x85; io[0x47]=0xFC;
        io[0x48]=0xFF; io[0x49]=0xFF;
        if(cgbMode){io[0x00]=0xCF;io[0x02]=1;io[0x55]=0xFF;io[0x56]=0;}
    }
    static std::vector<u8> loadBytes(const std::filesystem::path& path,std::size_t maxBytes=32*1024*1024) {
        // Validate file length before allocating. File size, seeking and the
        // exact read must all succeed; a partial read is never a usable image.
        std::ifstream f(path,std::ios::binary|std::ios::ate);
        if(!f)return {};
        const auto length=f.tellg();
        if(length<0||(std::uintmax_t)length>maxBytes||
           (std::uintmax_t)length>(std::uintmax_t)std::numeric_limits<std::streamsize>::max())return {};
        const auto size=(std::size_t)length;
        std::vector<u8> bytes(size);
        if(!f.seekg(0,std::ios::beg))return {};
        if(size&&(!f.read((char*)bytes.data(),(std::streamsize)size)||f.gcount()!=(std::streamsize)size))return {};
        // Also reject a file that grew after its size was checked.
        if(f.peek()!=std::char_traits<char>::eof()||f.bad())return {};
        return bytes;
    }
    static bool writeAtomic(const std::filesystem::path& path,std::string_view bytes,bool keepBackup=false){
        auto temporary=path;temporary+=".tmp";
        {std::ofstream f(temporary,std::ios::binary|std::ios::trunc);
            if(!f)return false;f.write(bytes.data(),(std::streamsize)bytes.size());f.flush();
            if(!f){std::error_code ec;std::filesystem::remove(temporary,ec);return false;}}
        std::error_code ec;
        if(keepBackup&&std::filesystem::exists(path,ec)){
            auto previous=path;previous+=".bak";
            std::filesystem::copy_file(path,previous,std::filesystem::copy_options::overwrite_existing,ec);
            if(ec){std::filesystem::remove(temporary,ec);return false;}
        }
        std::filesystem::rename(temporary,path,ec);
        if(!ec)return true;
        // Windows rename cannot replace an existing file. Keep the old file
        // until the replacement is ready, and restore it if committing fails.
        auto backup=path;backup+=".bak";
        if(!std::filesystem::exists(path,ec)){std::filesystem::remove(temporary,ec);return false;}
        std::filesystem::remove(backup,ec);ec.clear();
        std::filesystem::rename(path,backup,ec);
        if(ec){std::filesystem::remove(temporary,ec);return false;}
        std::filesystem::rename(temporary,path,ec);
        if(ec){std::filesystem::rename(backup,path,ec);return false;}
        if(!keepBackup)std::filesystem::remove(backup,ec);return true;
    }
    bool loadROM(const std::filesystem::path& path) {
        auto data=loadBytes(path,8*1024*1024);
        loadError.clear();
        if(data.size()<32768 || data.size()>8*1024*1024 || data.size()%16384) {
            loadError="ROM must contain complete 16 KiB banks (32 KiB to 8 MiB).";return false;
        }
        const u8 type=data[0x147];
        int controller=-1;
        switch(type) {
            case 0x00: case 0x08: case 0x09: controller=0;break;
            case 0x01: case 0x02: case 0x03: controller=1;break;
            case 0x05: case 0x06: controller=2;break;
            case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: controller=3;break;
            case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: controller=5;break;
            case 0x22:controller=7;break;
            case 0xFF:controller=101;break;
            case 0xFC:controller=252;break;
            default:
                if(extendedControllers&&(type==0x0B||type==0x0C||type==0x0D||type==0x20||type==0xFE))controller=type==0x20?6:type==0xFE?103:100;
                else {loadError="Unsupported cartridge controller: "+std::to_string(type);return false;}break;
        }
        rom=std::move(data); romPath=path; mbc=controller;
        romBank=1; bankHigh=0; ramBank=0; mbcMode=0; ramEnabled=(mbc==0);
        latchWrite=0; rumble=type>=0x1C&&type<=0x1E;mbc1Multicart=false;
        if(mbc==1 && rom.size()>=0x80000)
            mbc1Multicart=std::any_of(rom.begin()+0x104,rom.begin()+0x134,[](u8 b){return b!=0&&b!=0xFF;})&&
                std::equal(rom.begin()+0x104,rom.begin()+0x134,rom.begin()+0x40104);
        hasBattery=(type==0x03 || type==0x06 || type==0x09 || type==0x0F || type==0x10 || type==0x13 || type==0x1B || type==0x1E || type==0x22 || type==0xFF || type==0xFC);
        hasRTC=(type==0x0F || type==0x10); cgbROM=(rom[0x143]&0x80)!=0;
        std::size_t size=0;
        switch(rom[0x149]) { case 1:size=2048;break; case 2:size=8192;break; case 3:size=32768;break;case 4:size=131072;break;case 5:size=65536;break; default:break; }
        if(mbc==2) size=512;
        if(mbc==7)size=256;
        if(mbc==252)size=131072;
        eram.assign(size,0xFF);
        gameTitle.clear(); for(int i=0x134;i<(cgbROM?0x13F:0x144) && rom[i];++i) { if(rom[i]<32||rom[i]>126)break;gameTitle+=(char)rom[i]; }
        rtc.fill(0);latchedRTC.fill(0);rtcCycles=0;rtcLatched=false;
        loadBattery();
        return true;
    }
    std::size_t banks() const {return std::max<std::size_t>(2,(rom.size()+0x3FFF)/0x4000);}
    std::size_t bank0() const {if(externalROMBank)return externalROMBank(false);return mbc==1&&mbcMode ? ((std::size_t)bankHigh<<(mbc1Multicart?4:5))%banks() : 0;}
    std::size_t bankN() const {
        if(externalROMBank)return externalROMBank(true);
        std::size_t n=1;
        if(mbc==0) n=1;
        if(mbc==1) {const int mask=mbc1Multicart?15:31;n=(romBank&mask)|((std::size_t)bankHigh<<(mbc1Multicart?4:5));}
        if(mbc==2) n=romBank&15;
        if(mbc==3) n=romBank&127;
        if(mbc==5) n=romBank&511;
        if(mbc==7)n=romBank&127;
        if(mbc==101)n=romBank&63;
        if(mbc==252)n=romBank&63;
        return n%banks();
    }
    std::size_t extBank() const {
        if(mbc==1) return mbcMode ? (bankHigh&3):0;
        if(mbc==3 || mbc==5) return ramBank & (mbc==5?15:7);
        if(mbc==101)return ramBank&3;
        if(mbc==252)return ramBank&15;
        return 0;
    }
    unsigned vramBank()const{return cgbMode?(io[0x4F]&1):0;}
    unsigned wramBank()const {const unsigned n=cgbMode?(io[0x70]&7):1;return n?n:1;}
    u8 vramByte(unsigned bank,u16 offset)const{return vram[(bank&1)*8192+(offset&0x1FFF)];}
    std::size_t wramOffset(u16 addr)const {
        if(addr>=0xE000)addr-=0x2000;
        return addr<0xD000?addr-0xC000:wramBank()*4096+addr-0xD000;
    }
    unsigned apuDividerMask()const{return doubleSpeed?0x2000:0x1000;}
    bool paletteBlocked()const{return (io[0x40]&0x80)&&ppuMode==3;}
    bool cpuDmaStalled()const{return hdmaBytes>0;}
    bool isCGBHardware()const{return cgbHardware||cgbMode;}
    bool infraredLight()const{return infraredInput&&infraredInput();}
    void cameraWrite(u16 address,u8 value){
        const unsigned reg=address&0x7F;if(reg>=54)return;
        cameraRegisters[reg]=reg?value:value&7;
        if(reg||(value&1)==0||cameraTicks)return;
        auto image=cameraImage?cameraImage():std::array<u8,128*112>{};cameraPending.fill(0);
        for(unsigned y=0;y<112;++y)for(unsigned x=0;x<128;++x){unsigned intensity=image[y*128+x];if(cameraRegisters[4]&8)intensity=255-intensity;
            const unsigned threshold=6+((y&3)*4+(x&3))*3;
            const unsigned level=intensity<cameraRegisters[threshold]?3:intensity<cameraRegisters[threshold+1]?2:intensity<cameraRegisters[threshold+2]?1:0;
            const unsigned tile=(y/8)*16+x/8,offset=tile*16+(y&7)*2,bit=7-(x&7);
            cameraPending[offset]|=(u8)((level&1)<<bit);cameraPending[offset+1]|=(u8)(((level>>1)&1)<<bit);}
        cameraTicks=4*(32446+((cameraRegisters[1]&0x80)?0:512)+16*((cameraRegisters[2]<<8)|cameraRegisters[3]));
    }
    u8 motionRead(u16 address)const {
        if(!ramEnabled||!motion.enabled||address>=0xB000)return 0xFF;
        switch((address>>4)&15){case 2:return (u8)motion.x;case 3:return (u8)(motion.x>>8);
            case 4:return (u8)motion.y;case 5:return (u8)(motion.y>>8);case 6:return 0;
            case 8:return (u8)(motion.pins|((motion.phase==2)?((motion.readWord>>15)&1):1));default:return 0xFF;}
    }
    void motionWrite(u16 address,u8 value){
        if(!ramEnabled||!motion.enabled||address>=0xB000)return;
        const int reg=(address>>4)&15;
        if(reg==0&&value==0x55){motion.latchReady=true;motion.x=motion.y=0x8000;}
        if(reg==1&&value==0xAA&&motion.latchReady){motion.latchReady=false;
            motion.x=(u16)std::clamp(0x81D0+(int)motion.tiltX,0,65535);
            motion.y=(u16)std::clamp(0x81D0+(int)motion.tiltY,0,65535);}
        if(reg!=8)return;
        const u8 old=motion.pins;motion.pins=value&0xC2;
        if(!(value&0x80)){motion.phase=motion.bits=0;motion.command=0;motion.shift=0;motion.readWord=0xFFFF;return;}
        if((old&0x40)||!(value&0x40))return;
        const int bit=(value>>1)&1;
        if(motion.phase==2){motion.readWord=(u16)((motion.readWord<<1)|1);if(++motion.bits==16){
                motion.bits=0;motion.command=(u16)((motion.command&~127)|((motion.command+1)&127));
                const auto at=(motion.command&127)*2;motion.readWord=(u16)((eram[at]<<8)|eram[at+1]);}return;}
        if(motion.phase==3){motion.shift=(motion.shift<<1)|(unsigned)bit;if(++motion.bits==16){
                if(motion.writeEnabled){if((motion.command>>8)==1){const auto at=(motion.command&127)*2;eram[at]=(u8)(motion.shift>>8);eram[at+1]=(u8)motion.shift;}
                    else for(std::size_t i=0;i<eram.size();i+=2){eram[i]=(u8)(motion.shift>>8);eram[i+1]=(u8)motion.shift;}}
                motion.phase=4;}return;}
        if(motion.phase==4)return;
        if(motion.phase==0){if(bit){motion.phase=1;motion.bits=0;motion.command=0;}return;}
        motion.command=(u16)((motion.command<<1)|bit);
        if(++motion.bits<10)return;
        motion.bits=0;motion.phase=4;const int op=motion.command>>8,at=(motion.command&127)*2;
        if(op==2){motion.readWord=(u16)((eram[at]<<8)|eram[at+1]);motion.phase=2;}
        else if(op==1){motion.phase=3;motion.shift=0;}
        else if(op==3){if(motion.writeEnabled)eram[at]=eram[at+1]=0xFF;}
        else switch((motion.command>>6)&3){case 0:motion.writeEnabled=false;break;case 3:motion.writeEnabled=true;break;
            case 2:if(motion.writeEnabled)std::fill(eram.begin(),eram.end(),0xFF);break;case 1:motion.phase=3;motion.shift=0;break;}
    }
    void serialPulse(bool input){
        if(externalSerialPulse){externalSerialPulse(input);return;}
        if(!(io[2]&0x80))return;
        io[1]=(u8)((io[1]<<1)|(input?1:0));++serialBits;
        if(io[2]&1)serialTicks=std::max(0,serialTicks-(cgbMode&&(io[2]&2)?16:512));
        if(serialBits==8){if(serialOutput.size()<65536)serialOutput+=(char)serialSent;io[2]&=(u8)~0x80;io[0x0F]|=8;serialTicks=0;}
    }
    bool cpuStalled()const{return cpuDmaStalled()||speedSwitchTicks>0;}
    bool oamDmaBlocks(u16 addr)const{
        if(dmaPos>=160)return false;
        if(addr>=0xFE00&&addr<0xFEA0)return true;
        if(addr>=0xFE00)return false;
        if(!cgbMode){
            const bool dmaVideo=dmaSource>=0x8000&&dmaSource<0xA000;
            const bool cpuVideo=addr>=0x8000&&addr<0xA000;
            return dmaVideo==cpuVideo;
        }
        // CGB cartridge and internal RAM use independent buses. OAM DMA
        // blocks CPU accesses on its source bus; HRAM and I/O stay available.
        const auto bus=[](u16 address){
            if(address<0x8000||(address>=0xA000&&address<0xC000))return 0;
            return address<0xA000?1:2;
        };
        const u16 source=dmaSource>=0xE000?(u16)(dmaSource-0x2000):dmaSource;
        return bus(addr)==bus(source);
    }
    bool speedSwitch(){
        if(!cgbMode||!(io[0x4D]&1))return false;
        write(0xFF04,0);doubleSpeed=!doubleSpeed;io[0x4D]=0;dotPhase=0;
        speedSwitchTicks=8200;return true;
    }
    void startDmaBlock(){if(hdmaActive&&hdmaBlocks>0&&hdmaBytes==0){hdmaBytes=16;hdmaDot=0;}}
    void hblankDMA(){if(cgbMode&&hdmaActive&&hdmaHblank&&!cpuHalted&&io[0x44]<144)startDmaBlock();}
    void startVramDMA(u8 value){
        if(hdmaActive&&hdmaHblank){
            if(!(value&0x80)){hdmaActive=false;hdmaBytes=0;io[0x55]=(u8)(0x80|(hdmaBlocks-1));}
            return;
        }
        hdmaSource=(u16)((io[0x51]<<8)|(io[0x52]&0xF0));
        hdmaDest=(u16)(0x8000|((io[0x53]&0x1F)<<8)|(io[0x54]&0xF0));
        hdmaBlocks=(value&0x7F)+1;hdmaActive=true;
        hdmaHblank=(value&0x80)&&(io[0x40]&0x80);
        io[0x55]=(u8)(hdmaBlocks-1);
        if(!hdmaHblank)startDmaBlock();
    }
    void tickBaseDot(){
        if(hasRTC&&++rtcCycles>=GB_CLOCK){rtcCycles-=GB_CLOCK;rtcSecond();}
        if(!hdmaBytes||++hdmaDot<2)return;
        hdmaDot=0;
        // VRAM is not a valid CGB DMA source. Read disconnected bus there.
        const u8 value=(hdmaSource>=0x8000&&hdmaSource<0xA000)?0xFF:read(hdmaSource);
        vram[vramBank()*8192+hdmaDest-0x8000]=value;
        ++hdmaSource;++hdmaDest;--hdmaBytes;
        if(hdmaBytes)return;
        --hdmaBlocks;
        io[0x51]=(u8)(hdmaSource>>8);io[0x52]=(u8)(hdmaSource&0xF0);
        io[0x53]=(u8)((hdmaDest>>8)&0x1F);io[0x54]=(u8)(hdmaDest&0xF0);
        if(hdmaBlocks==0||hdmaDest>=0xA000){hdmaActive=false;io[0x55]=0xFF;return;}
        io[0x55]=(u8)(hdmaBlocks-1);
        if(!hdmaHblank)startDmaBlock();
    }
    void refreshStat() {
        u8& stat=io[0x41];
        stat=(u8)((stat&0xFC)|(ppuMode&3));
        if(!(io[0x40]&0x80))return; // LY comparison clock stops with the LCD.
        if(ppuCompareLY!=0xFF&&ppuCompareLY==io[0x45])stat|=4;else stat&=(u8)~4;
        const bool line=((stat&4)&&(stat&0x40)) || (ppuStatMode==0&&(stat&8)) ||
                        (ppuStatMode==1&&(stat&0x10)) || (ppuStatMode==2&&(stat&0x20));
        if(line&&!statLine)io[0x0F]|=2;
        statLine=line;
    }
    u8 joypadRead() const {
        u8 v=(io[0]&0x30)|0xCF;
        if(!(v&0x10)) {if(right)v&=~1;if(left)v&=~2;if(up)v&=~4;if(down)v&=~8;}
        if(!(v&0x20)) {if(a)v&=~1;if(b)v&=~2;if(select)v&=~4;if(start)v&=~8;}
        return v;
    }
    void joyChanged(u8 before) { if((before&~joypadRead()&15)!=0) io[0x0F]|=0x10; }
    u8 read(u16 addr) const {
        if(externalRead)return externalRead(addr);
        if(bootROMMapped&&(addr<0x100||(bootROMCGB&&addr>=0x200&&addr<0x900)))
            return bootROM[addr];
        if(addr==0xFF50)return 0xFF; // BANK is a write-only, one-way latch.
        if(addr<0x8000){const u8 value=rom.empty()?0xFF:rom[((addr<0x4000?bank0():bankN())*16384+(addr&0x3FFF))%rom.size()];
            return romReadFilter?romReadFilter(addr,value):value;}
        if(addr<0xA000) return vram[vramBank()*8192+addr-0x8000];
        if(addr<0xC000) {
            if(mbc==252){if(ramBank&16)return (addr&127)==0?cameraRegisters[0]:0;
                if(cameraRegisters[0]&1)return 0;return eram.empty()?0xFF:eram[(extBank()*8192+addr-0xA000)%eram.size()];}
            if(mbc==7)return motionRead(addr);
            if(mbc==101&&hucIRMode)return (u8)(0xC0|(infraredLight()?1:0));
            if(!ramEnabled)return 0xFF;
            if(mbc==3 && ramBank>=8) return hasRTC && ramBank<=12 ? (rtcLatched?latchedRTC:rtc)[ramBank-8] : 0xFF;
            if(eram.empty())return 0xFF;
            std::size_t offset=(extBank()*8192+addr-0xA000)%eram.size();
            return mbc==2 ? (u8)(0xF0|(eram[(addr-0xA000)&511]&15)) : eram[offset];
        }
        if(addr<0xFE00)return wram[wramOffset(addr)];
        if(addr<0xFEA0)return oam[addr-0xFE00];
        if(addr<0xFF00)return 0xFF;
        if(addr==0xFFFF)return ie;
        if(addr>=0xFF80)return hram[addr-0xFF80];
        if(addr>=0xFF10&&addr<=0xFF3F){
            if(soundRead)return soundRead(addr);
            static constexpr u8 masks[0x30]={0x80,0x3F,0,0xFF,0xBF,0xFF,0x3F,0,0xFF,0xBF,
                0x7F,0xFF,0x9F,0xFF,0xBF,0xFF,0xFF,0,0,0xBF,0,0,0x70,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
            return addr>=0xFF30?io[addr-0xFF00]:(u8)(io[addr-0xFF00]|masks[addr-0xFF10]);
        }
        if(addr>=0xFF4C&&addr<=0xFF7F){
            if(!cgbMode)return 0xFF;
            switch(addr){
                case 0xFF4D:return (u8)(0x7E|(doubleSpeed?0x80:0)|(io[0x4D]&1));
                case 0xFF4F:return io[0x4F]|0xFE;
                case 0xFF55:return io[0x55];
                case 0xFF56:return (u8)(0x3C|(io[0x56]&0xC1)|(((io[0x56]&0xC0)==0xC0&&infraredLight())?0:2));
                case 0xFF68:case 0xFF6A:return io[addr-0xFF00]|0x40;
                case 0xFF69:return paletteBlocked()?0xFF:bgPalette[io[0x68]&0x3F];
                case 0xFF6B:return paletteBlocked()?0xFF:objPalette[io[0x6A]&0x3F];
                case 0xFF6C:return io[0x6C]|0xFE;
                case 0xFF70:return io[0x70]|0xF8;
                case 0xFF72:case 0xFF73:case 0xFF74:return io[addr-0xFF00];
                case 0xFF75:return io[0x75]|0x8F;
                case 0xFF76:case 0xFF77:return soundPCM?soundPCM(addr):0;
                default:return 0xFF;
            }
        }
        if(addr==0xFF03||(addr>=0xFF08&&addr<=0xFF0E))return 0xFF;
        switch(addr) {
            case 0xFF00:return joypadRead();
            case 0xFF02:return io[2]|(cgbMode?0x7C:0x7E);
            case 0xFF04:return (u8)(divCounter>>8);
            case 0xFF07:return io[7]|0xF8;
            case 0xFF0F:return io[0x0F]|0xE0;
            case 0xFF41:return io[0x41]|0x80;
            case 0xFF44:return io[0x44];
            case 0xFF26:return (io[0x26]&0x8F)|0x70;
            default:return io[addr-0xFF00];
        }
    }
    u8 cpuRead(u16 addr) const {
        if(externalRead)return externalRead(addr);
        bool blocked=oamDmaBlocks(addr);
        if(io[0x40]&0x80){
            if(addr>=0x8000&&addr<0xA000&&(ppuMode==3||(ppuMode==2&&ppuDot>=80)))blocked=true;
            if(addr>=0xFE00&&addr<0xFEA0&&(ppuStatMode==2||ppuMode==3||dmaPos<160))blocked=true;
        }
        const u8 value=blocked?0xFF:read(addr);if(busObserver)busObserver(addr,value,false,blocked);return value;
    }
    void cpuWrite(u16 addr,u8 v){
        if(externalWrite){externalWrite(addr,v);return;}
        bool blocked=oamDmaBlocks(addr);
        if(io[0x40]&0x80){
            if(addr>=0x8000&&addr<0xA000&&ppuMode==3)blocked=true;
            if(addr>=0xFE00&&addr<0xFEA0&&((ppuMode==2&&ppuDot<80)||ppuMode==3||dmaPos<160))blocked=true;
        }
        if(busObserver)busObserver(addr,v,true,blocked);if(!blocked)write(addr,v);
    }
    bool timerSignal() const {
        static constexpr int bits[4]={9,3,5,7};
        return (io[7]&4) && (divCounter & (1u<<bits[io[7]&3]));
    }
    void advanceTima() {
        if(timerReload>=0)return;
        if(io[5]==0xFF) {io[5]=0;timerReload=4;}
        else ++io[5];
    }
    void advanceRTC(u64 seconds) {
        if(!hasRTC||(rtc[4]&0x40))return;
        const u64 total=seconds+(rtc[0]%60)+(rtc[1]%60)*60+(rtc[2]%24)*3600;
        const u64 days=(rtc[3]|((rtc[4]&1)<<8))+total/86400;
        rtc[0]=total%60;rtc[1]=(total/60)%60;rtc[2]=(total/3600)%24;
        if(days>=512)rtc[4]|=0x80;
        rtc[3]=days&255;rtc[4]=(rtc[4]&0xFE)|((days>>8)&1);
    }
    void rtcSecond(){advanceRTC(1);}
    void write(u16 addr,u8 v) {
        if(externalWrite){externalWrite(addr,v);return;}
        if(addr==0xFF50){if(v){bootROMMapped=false;if(cgbHardware&&((io[0x4C]&4)||dmgCompatibility)){dmgCompatibility=true;cgbMode=false;doubleSpeed=false;}}return;}
        if(addr<0x8000) {
            if(mbc==0)return;
            if(mbc==101){if(addr<0x2000){hucIRMode=v==0x0E;ramEnabled=true;}else if(addr<0x4000)romBank=v&63;else if(addr<0x6000)ramBank=v&3;return;}
            if(mbc==252){if(addr<0x2000)ramEnabled=(v&15)==10;else if(addr<0x4000){romBank=v&63;if(!romBank)romBank=1;}else if(addr<0x6000)ramBank=v&31;return;}
            if(mbc==7){if(addr<0x2000)ramEnabled=(v&15)==10;else if(addr<0x4000)romBank=v&127;else if(addr<0x6000)motion.enabled=v==0x40;return;}
            if(mbc==2) {
                if(addr<0x4000) { if(addr&0x100) {romBank=v&15;if(!romBank)romBank=1;}else ramEnabled=((v&15)==10); }
                return;
            }
            if(addr<0x2000){ramEnabled=(v&15)==10;return;}
            if(addr<0x4000){
                if(mbc==1){romBank=v&31;if(!romBank)romBank=1;}
                if(mbc==3){romBank=v&127;if(!romBank)romBank=1;}
                if(mbc==5){if(addr<0x3000)romBank=(romBank&0x100)|v;else romBank=(romBank&255)|((v&1)<<8);}
                return;
            }
            if(addr<0x6000){if(mbc==1)bankHigh=v&3;else {ramBank=(mbc==5)?(v&(rumble?7:15)):v;if(mbc==5&&rumble)rumbleActive=(v&8)!=0;}return;}
            if(mbc==1)mbcMode=v&1;
            if(mbc==3){if(latchWrite==0&&v==1){latchedRTC=rtc;rtcLatched=true;} latchWrite=v;}
            return;
        }
        if(addr<0xA000){vram[vramBank()*8192+addr-0x8000]=v;return;}
        if(addr<0xC000){
            if(mbc==252){if(ramBank&16)cameraWrite(addr,v);else if(ramEnabled&&!(cameraRegisters[0]&1))eram[(extBank()*8192+addr-0xA000)%eram.size()]=v;return;}
            if(mbc==7){motionWrite(addr,v);return;}
            if(mbc==101&&hucIRMode){hucIRLight=(v&1)!=0;return;}
            if(!ramEnabled)return;
            if(mbc==3&&ramBank>=8){if(hasRTC&&ramBank<=12){int n=ramBank-8;rtc[n]=v;if(n==0)rtc[n]%=60;if(n==1)rtc[n]%=60;if(n==2)rtc[n]%=24;}return;}
            if(eram.empty())return;
            if(mbc==2)eram[(addr-0xA000)&511]=v&15;
            else eram[(extBank()*8192+addr-0xA000)%eram.size()]=v;
            return;
        }
        if(addr<0xFE00){wram[wramOffset(addr)]=v;return;}
        if(addr<0xFEA0){oam[addr-0xFE00]=v;return;}
        if(addr<0xFF00)return;
        if(addr==0xFFFF){ie=v;return;}
        if(addr>=0xFF80){hram[addr-0xFF80]=v;return;}
        if(addr>=0xFF10&&addr<=0xFF3F&&soundWrite){soundWrite(addr,v);return;}
        if(addr>=0xFF4C&&addr<=0xFF7F){
            if(!cgbMode)return;
            switch(addr){
                case 0xFF4C:if(bootROMMapped)io[0x4C]=v;break;
                case 0xFF6C:if(bootROMMapped)io[0x6C]=v&1;break;
                case 0xFF4D:io[0x4D]=v&1;break;
                case 0xFF4F:io[0x4F]=v&1;break;
                case 0xFF51:case 0xFF52:case 0xFF53:case 0xFF54:
                    if(!hdmaActive)io[addr-0xFF00]=addr==0xFF52||addr==0xFF54?v&0xF0:addr==0xFF53?v&0x1F:v;
                    break;
                case 0xFF55:startVramDMA(v);break;
                case 0xFF56:io[0x56]=v&0xC1;break;
                case 0xFF68:case 0xFF6A:io[addr-0xFF00]=v&0xBF;break;
                case 0xFF69:case 0xFF6B:{
                    u8& index=io[addr==0xFF69?0x68:0x6A];
                    auto& palette=addr==0xFF69?bgPalette:objPalette;
                    if(!paletteBlocked())palette[index&0x3F]=v;
                    if(index&0x80)index=(u8)(0x80|((index+1)&0x3F));break;
                }
                case 0xFF72:case 0xFF73:case 0xFF74:io[addr-0xFF00]=v;break;
                case 0xFF75:io[0x75]=v&0x70;break;
                case 0xFF70:io[0x70]=v&7;break;
                // KEY0 and OPRI are locked after the boot ROM hand-off.
                default:break;
            }
            return;
        }
        switch(addr){
            case 0xFF00:{const u8 before=joypadRead();io[0]=(io[0]&~0x30)|(v&0x30);joyChanged(before);break;}
            case 0xFF04:{const bool old=timerSignal();if((divCounter&apuDividerMask())&&dividerReset)dividerReset();divCounter=0;if(old&&!timerSignal())advanceTima();break;}
            case 0xFF05:if(!timerJustReloaded){io[5]=v;timerReload=-1;}break;
            case 0xFF06:io[6]=v;if(timerJustReloaded)io[5]=v;break;
            case 0xFF07:{const bool old=timerSignal();io[7]=v&7;if(old&&!timerSignal()&&(!cgbMode||(io[7]&4)))advanceTima();break;}
            case 0xFF0F:io[0x0F]=v|0xE0;break;
            case 0xFF02:io[2]=v&(cgbMode?0x83:0x81);serialBits=0;serialTicks=(v&0x81)==0x81?((cgbMode&&(v&2))?128:4096):0;serialSent=io[1];break;
            case 0xFF14:case 0xFF19:case 0xFF1E:case 0xFF23:{io[addr-0xFF00]=v;if(v&0x80){int channel=addr==0xFF14?0:addr==0xFF19?1:addr==0xFF1E?2:3;++soundTrigger[channel];io[0x26]|=(u8)(1<<channel);}break;}
            case 0xFF26:io[0x26]=(io[0x26]&15)|(v&0x80);if(!(v&0x80)){for(int i=0x10;i<=0x25;++i)io[i]=0;io[0x26]=0;}break;
            case 0xFF40:{bool changed=(io[0x40]^v)&0x80;io[0x40]=v;if(changed&&lcdPower)lcdPower((v&0x80)!=0);
                if(!(v&0x80)&&hdmaActive&&hdmaHblank){hdmaHblank=false;startDmaBlock();}break;}
            case 0xFF41:if(!cgbMode&&(io[0x40]&0x80)&&!statLine&&(ppuMode!=3||io[0x44]==io[0x45]))io[0x0F]|=2;
                io[0x41]=(io[0x41]&0x87)|(v&0x78);refreshStat();break;
            case 0xFF45:io[0x45]=v;refreshStat();break;
            case 0xFF44:break; // LY is read-only
            case 0xFF46:pendingDmaSource=(u16)(v<<8);dmaDelay=8;io[0x46]=v;break;
            default:io[addr-0xFF00]=v;break;
        }
    }
    void tick(int cycles) {
        for(int i=0;i<cycles;++i) {
            if(cameraTicks>0&&(cameraRegisters[0]&1)&&!cpuHalted&&--cameraTicks==0){
                if(eram.size()>=0x100+cameraPending.size())std::copy(cameraPending.begin(),cameraPending.end(),eram.begin()+0x100);cameraRegisters[0]&=(u8)~1;}
            timerJustReloaded=false;
            if(timerReload>=0){if(--timerReload==0){io[5]=io[6];io[0x0F]|=4;timerReload=-1;timerJustReloaded=true;}}
            const bool old=timerSignal();
            ++divCounter;if(old&&!timerSignal())advanceTima();
            // Serial's divider edge precedes the readable DIV phase by one M-cycle.
            const int serialPeriod=cgbMode&&(io[2]&2)?16:512;
            if(serialTicks>0&&((divCounter+4)&(serialPeriod-1))==0){
                if(serialClock)serialClock();else serialPulse(true);
            }
            if(dmaPos<160 && ++dmaTicks==4){
                dmaTicks=0;u16 source=(u16)(dmaSource+dmaPos);
                if(source>=0xE000)source-=0x2000;
                oam[dmaPos]=read(source);++dmaPos;
            }
            if(dmaDelay>0&&--dmaDelay==0){dmaSource=pendingDmaSource;dmaPos=0;dmaTicks=0;}
        }
    }
    std::filesystem::path savePath() const {auto p=romPath;p.replace_extension(".sav");return p;}
    std::filesystem::path rtcPath() const {auto p=romPath;p.replace_extension(".rtc");return p;}
    bool saveBattery() const {
        if(externalBatterySave)return externalBatterySave();
        if(!hasBattery||romPath.empty())return false;
        if(!eram.empty()){
            if(!writeAtomic(savePath(),std::string_view((const char*)eram.data(),eram.size()),true))return false;
        }
        if(hasRTC){
            std::string bytes="GBRTC01";bytes.append((const char*)rtc.data(),5);
            const auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::int64_t stamp=(std::int64_t)now;bytes.append((const char*)&stamp,sizeof(stamp));
            if(!writeAtomic(rtcPath(),bytes,true))return false;
        }
        return true;
    }
    void loadBattery() {
        if(!hasBattery)return;
        if(!eram.empty()) {auto file=loadBytes(savePath(),eram.size());if(file.size()==eram.size())eram=std::move(file);}
        if(!hasRTC)return;
        std::ifstream f(rtcPath(),std::ios::binary);
        char magic[7]{};std::int64_t stamp=0;
        if(f && f.read(magic,7) && std::string(magic,7)=="GBRTC01" && f.read((char*)rtc.data(),5) && f.read((char*)&stamp,sizeof(stamp))){
            const auto now=(std::int64_t)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            auto elapsed=std::clamp<std::int64_t>(now-stamp,0,86400*365LL);
            advanceRTC((u64)elapsed);
        }
    }
};
class CPU {
public:
    Memory& m;
    u8 A = 0, F = 0, B = 0, C = 0, D = 0, E = 0, H = 0, L = 0;
    u16 SP = 0xFFFE, PC = 0x0100;
    bool IME = false;
    bool halted = false;
    bool stopped = false;
    bool haltBug = false;
    int imeDelay = 0;
    bool locked=false;
    int elapsed=0,instructionElapsed=0;
    std::function<void(int)> clock;
    std::function<int()> externalStep;
    void drainStall(){while(clock&&m.cpuStalled()){++elapsed;clock(1);}}
    void idle(int cycles){
        instructionElapsed+=cycles;
        for(int i=0;i<cycles;++i){drainStall();++elapsed;if(clock)clock(1);}
        drainStall();
    }
    u8 read8(u16 addr){idle(4);return m.cpuRead(addr);}
    void write8(u16 addr,u8 value){
        if(addr==0xFF07){idle(3);m.cpuWrite(addr,value);idle(1);}
        else {idle(4);m.cpuWrite(addr,value);}
    }
    int step(){
        if(externalStep)return externalStep();
        elapsed=instructionElapsed=0;drainStall();
        const int cycles=locked?4:execute();
        if(cycles>instructionElapsed)idle(cycles-instructionElapsed);
        if(imeDelay>0&&--imeDelay==0)IME=true;
        return elapsed;
    }

    explicit CPU(Memory& mem) : m(mem) {}

    u16 AF() const { return static_cast<u16>(A << 8 | F); }
    u16 BC() const { return static_cast<u16>(B << 8 | C); }
    u16 DE() const { return static_cast<u16>(D << 8 | E); }
    u16 HL() const { return static_cast<u16>(H << 8 | L); }
    void setAF(u16 v) { A = v >> 8; F = v & 0xF0; }
    void setBC(u16 v) { B = v >> 8; C = v & 0xFF; }
    void setDE(u16 v) { D = v >> 8; E = v & 0xFF; }
    void setHL(u16 v) { H = v >> 8; L = v & 0xFF; }

    bool z() const { return F & 0x80; }
    bool n() const { return F & 0x40; }
    bool h() const { return F & 0x20; }
    bool c() const { return F & 0x10; }
    void setFlags(bool Z, bool N, bool Hf, bool Cf) {
        F = static_cast<u8>((Z ? 0x80 : 0) | (N ? 0x40 : 0) | (Hf ? 0x20 : 0) | (Cf ? 0x10 : 0));
    }

    u8 fetch8() { const u8 v = read8(PC); if (haltBug) haltBug = false; else PC = static_cast<u16>(PC + 1); return v; }
    u16 fetch16() { const u16 lo = fetch8(); const u16 hi = fetch8(); return static_cast<u16>(lo | hi << 8); }

    void push16(u16 v) {
        write8(--SP, static_cast<u8>(v >> 8));
        write8(--SP, static_cast<u8>(v & 0xFF));
    }
    u16 pop16() {
        const u16 lo = read8(SP++);
        const u16 hi = read8(SP++);
        return static_cast<u16>(lo | hi << 8);
    }

    u8 readR8(int r) {
        switch (r & 7) {
            case 0: return B; case 1: return C; case 2: return D; case 3: return E;
            case 4: return H; case 5: return L; case 6: return read8(HL()); default: return A;
        }
    }
    void writeR8(int r, u8 v) {
        switch (r & 7) {
            case 0: B = v; break; case 1: C = v; break; case 2: D = v; break; case 3: E = v; break;
            case 4: H = v; break; case 5: L = v; break; case 6: write8(HL(), v); break; default: A = v; break;
        }
    }

    u16 readR16(int p) const {
        switch (p & 3) { case 0: return BC(); case 1: return DE(); case 2: return HL(); default: return SP; }
    }
    void writeR16(int p, u16 v) {
        switch (p & 3) { case 0: setBC(v); break; case 1: setDE(v); break; case 2: setHL(v); break; default: SP = v; break; }
    }

    void inc8(u8& x) {
        const u8 old = x++;
        const bool Hf = ((old & 0x0F) + 1) > 0x0F;
        F = static_cast<u8>((x == 0 ? 0x80 : 0) | (Hf ? 0x20 : 0) | (F & 0x10));
    }
    void dec8(u8& x) {
        const u8 old = x--;
        const bool Hf = (old & 0x0F) == 0;
        F = static_cast<u8>((x == 0 ? 0x80 : 0) | 0x40 | (Hf ? 0x20 : 0) | (F & 0x10));
    }
    u8 incR8(int r) {
        u8 v = readR8(r); const u8 old = v; ++v;
        F = static_cast<u8>((v == 0 ? 0x80 : 0) | (((old & 0x0F) + 1 > 0x0F) ? 0x20 : 0) | (F & 0x10));
        writeR8(r, v); return v;
    }
    u8 decR8(int r) {
        u8 v = readR8(r); const u8 old = v; --v;
        F = static_cast<u8>((v == 0 ? 0x80 : 0) | 0x40 | ((old & 0x0F) == 0 ? 0x20 : 0) | (F & 0x10));
        writeR8(r, v); return v;
    }

    void addA(u8 x, bool carryIn = false) {
        const int carry = carryIn && c() ? 1 : 0;
        const int result = A + x + carry;
        setFlags((result & 0xFF) == 0, false, ((A & 0x0F) + (x & 0x0F) + carry) > 0x0F, result > 0xFF);
        A = static_cast<u8>(result);
    }
    void subA(u8 x, bool carryIn = false) {
        const int carry = carryIn && c() ? 1 : 0;
        const int result = A - x - carry;
        setFlags((result & 0xFF) == 0, true, ((A & 0x0F) - (x & 0x0F) - carry) < 0, result < 0);
        A = static_cast<u8>(result);
    }
    void cpA(u8 x) {
        const int result = A - x;
        setFlags((result & 0xFF) == 0, true, ((A & 0x0F) < (x & 0x0F)), result < 0);
    }

    u8 rlc(u8 v) { const u8 out = static_cast<u8>((v << 1) | (v >> 7)); setFlags(out == 0, false, false, v & 0x80); return out; }
    u8 rrc(u8 v) { const u8 out = static_cast<u8>((v >> 1) | (v << 7)); setFlags(out == 0, false, false, v & 1); return out; }
    u8 rl(u8 v) { const bool oldC = c(); const u8 out = static_cast<u8>((v << 1) | (oldC ? 1 : 0)); setFlags(out == 0, false, false, v & 0x80); return out; }
    u8 rr(u8 v) { const bool oldC = c(); const u8 out = static_cast<u8>((v >> 1) | (oldC ? 0x80 : 0)); setFlags(out == 0, false, false, v & 1); return out; }
    u8 sla(u8 v) { const u8 out = static_cast<u8>(v << 1); setFlags(out == 0, false, false, v & 0x80); return out; }
    u8 sra(u8 v) { const u8 out = static_cast<u8>((v >> 1) | (v & 0x80)); setFlags(out == 0, false, false, v & 1); return out; }
    u8 srl(u8 v) { const u8 out = static_cast<u8>(v >> 1); setFlags(out == 0, false, false, v & 1); return out; }
    u8 swap(u8 v) { const u8 out = static_cast<u8>((v << 4) | (v >> 4)); setFlags(out == 0, false, false, false); return out; }

    int execCB(u8 op) {
        const int r = op & 7;
        u8 v = readR8(r);
        const int group = op >> 6;
        if (group == 0) {
            switch ((op >> 3) & 7) {
                case 0: v = rlc(v); break; case 1: v = rrc(v); break; case 2: v = rl(v); break; case 3: v = rr(v); break;
                case 4: v = sla(v); break; case 5: v = sra(v); break; case 6: v = swap(v); break; case 7: v = srl(v); break;
            }
            writeR8(r, v);
        } else if (group == 1) {
            const int bit = (op >> 3) & 7;
            setFlags((v & (1 << bit)) == 0, false, true, c());
        } else if (group == 2) {
            v = static_cast<u8>(v & ~(1 << ((op >> 3) & 7))); writeR8(r, v);
        } else {
            v = static_cast<u8>(v | (1 << ((op >> 3) & 7))); writeR8(r, v);
        }
        return r == 6 ? (group == 1 ? 12 : 16) : 8;
    }

    void daa() {
        int adjust = 0;
        bool carry = c();
        if (!n()) {
            if (h() || (A & 0x0F) > 9) adjust |= 0x06;
            if (c() || A > 0x99) { adjust |= 0x60; carry = true; }
            A = static_cast<u8>(A + adjust);
        } else {
            if (h()) adjust |= 0x06;
            if (c()) adjust |= 0x60;
            A = static_cast<u8>(A - adjust);
        }
        F = static_cast<u8>((A == 0 ? 0x80 : 0) | (n() ? 0x40 : 0) | (carry ? 0x10 : 0));
    }

    int serviceInterrupt() {
        if(stopped){if(m.io[0x0F]&0x10)stopped=false;else return 0;}
        const u8 pending = static_cast<u8>(m.ie & m.io[0x0F] & 0x1F);
        if (!pending) return 0;
        if (!IME) { halted = false; stopped = false; return 0; }
        IME = false;
        imeDelay = 0;
        halted = false;
        int bit = 0;
        while (((pending >> bit) & 1) == 0) ++bit;
        idle(8);
        write8(--SP,(u8)(PC>>8));
        const u8 selected=(u8)(m.ie&m.io[0x0F]&0x1F);
        write8(--SP,(u8)PC);
        if(selected){bit=0;while(!(selected&(1<<bit)))++bit;m.io[0x0F]&=(u8)~(1<<bit);}
        haltBug=false;
        PC = selected?static_cast<u16>(0x40 + bit * 8):0;
        return 20;
    }

    int execute() {
        const int irqCycles = serviceInterrupt();
        if (irqCycles) return irqCycles;
        if (stopped || halted) return 4;

        const u8 op = fetch8();

        if (op == 0xCB) return execCB(fetch8());
        if (op == 0x76) { if (!IME && (m.ie & m.io[0x0F] & 0x1F)) haltBug = true; else halted = true; return 4; }

        // LD r,r'
        if (op >= 0x40 && op <= 0x7F) {
            const int dst = (op >> 3) & 7;
            const int src = op & 7;
            writeR8(dst, readR8(src));
            return (dst == 6 || src == 6) ? 8 : 4;
        }

        // ALU A,r
        if (op >= 0x80 && op <= 0xBF) {
            const u8 v = readR8(op & 7);
            switch ((op >> 3) & 7) {
                case 0: addA(v); break; case 1: addA(v, true); break; case 2: subA(v); break; case 3: subA(v, true); break;
                case 4: A &= v; setFlags(A == 0, false, true, false); break;
                case 5: A ^= v; setFlags(A == 0, false, false, false); break;
                case 6: A |= v; setFlags(A == 0, false, false, false); break;
                case 7: cpA(v); break;
            }
            return (op & 7) == 6 ? 8 : 4;
        }

        // LD r,d8
        if ((op & 0xC7) == 0x06) {
            const int r = (op >> 3) & 7;
            writeR8(r, fetch8());
            return r == 6 ? 12 : 8;
        }

        // INC/DEC r
        if ((op & 0xC7) == 0x04) { const int r = (op >> 3) & 7; incR8(r); return r == 6 ? 12 : 4; }
        if ((op & 0xC7) == 0x05) { const int r = (op >> 3) & 7; decR8(r); return r == 6 ? 12 : 4; }

        switch (op) {
            case 0x00: return 4; // NOP
            case 0x01: setBC(fetch16()); return 12;
            case 0x11: setDE(fetch16()); return 12;
            case 0x21: setHL(fetch16()); return 12;
            case 0x31: SP = fetch16(); return 12;

            case 0x02: write8(BC(), A); return 8;
            case 0x12: write8(DE(), A); return 8;
            case 0x22: write8(HL(), A); setHL(static_cast<u16>(HL() + 1)); return 8;
            case 0x32: write8(HL(), A); setHL(static_cast<u16>(HL() - 1)); return 8;
            case 0x0A: A = read8(BC()); return 8;
            case 0x1A: A = read8(DE()); return 8;
            case 0x2A: A = read8(HL()); setHL(static_cast<u16>(HL() + 1)); return 8;
            case 0x3A: A = read8(HL()); setHL(static_cast<u16>(HL() - 1)); return 8;

            case 0x03: setBC(static_cast<u16>(BC() + 1)); return 8;
            case 0x13: setDE(static_cast<u16>(DE() + 1)); return 8;
            case 0x23: setHL(static_cast<u16>(HL() + 1)); return 8;
            case 0x33: ++SP; return 8;
            case 0x0B: setBC(static_cast<u16>(BC() - 1)); return 8;
            case 0x1B: setDE(static_cast<u16>(DE() - 1)); return 8;
            case 0x2B: setHL(static_cast<u16>(HL() - 1)); return 8;
            case 0x3B: --SP; return 8;

            case 0x07: { const u8 old = A; A = static_cast<u8>((A << 1) | (A >> 7)); setFlags(false, false, false, old & 0x80); return 4; }
            case 0x0F: { const u8 old = A; A = static_cast<u8>((A >> 1) | (A << 7)); setFlags(false, false, false, old & 1); return 4; }
            case 0x17: { const bool oldC = c(); const u8 old = A; A = static_cast<u8>((A << 1) | (oldC ? 1 : 0)); setFlags(false, false, false, old & 0x80); return 4; }
            case 0x1F: { const bool oldC = c(); const u8 old = A; A = static_cast<u8>((A >> 1) | (oldC ? 0x80 : 0)); setFlags(false, false, false, old & 1); return 4; }

            case 0x08: { const u16 a = fetch16(); write8(a, SP & 0xFF); write8(static_cast<u16>(a + 1), SP >> 8); return 20; }
            case 0x10: ++PC;if(!m.speedSwitch()){m.write(0xFF04,0);stopped=true;}return 4;
            case 0x18: { const s8 d = static_cast<s8>(fetch8()); PC = static_cast<u16>(PC + d); return 12; }

            case 0x20: { const s8 d = static_cast<s8>(fetch8()); if (!z()) { PC = static_cast<u16>(PC + d); return 12; } return 8; }
            case 0x28: { const s8 d = static_cast<s8>(fetch8()); if (z())  { PC = static_cast<u16>(PC + d); return 12; } return 8; }
            case 0x30: { const s8 d = static_cast<s8>(fetch8()); if (!c()) { PC = static_cast<u16>(PC + d); return 12; } return 8; }
            case 0x38: { const s8 d = static_cast<s8>(fetch8()); if (c())  { PC = static_cast<u16>(PC + d); return 12; } return 8; }

            case 0x27: daa(); return 4;
            case 0x2F: A ^= 0xFF; F = static_cast<u8>((F & 0x90) | 0x60); return 4;
            case 0x37: F = static_cast<u8>((F & 0x80) | 0x10); return 4;
            case 0x3F: F = static_cast<u8>((F & 0x80) | (c() ? 0 : 0x10)); return 4;

            case 0xC3: PC = fetch16(); return 16;
            case 0xC2: { const u16 a = fetch16(); if (!z()) PC = a; return z() ? 12 : 16; }
            case 0xCA: { const u16 a = fetch16(); if (z()) PC = a; return z() ? 16 : 12; }
            case 0xD2: { const u16 a = fetch16(); if (!c()) PC = a; return c() ? 12 : 16; }
            case 0xDA: { const u16 a = fetch16(); if (c()) PC = a; return c() ? 16 : 12; }
            case 0xE9: PC = HL(); return 4;

            case 0xCD: { const u16 a = fetch16(); idle(4); push16(PC); PC = a; return 24; }
            case 0xC4: { const u16 a = fetch16(); if (!z()) { idle(4); push16(PC); PC = a; return 24; } return 12; }
            case 0xCC: { const u16 a = fetch16(); if (z())  { idle(4); push16(PC); PC = a; return 24; } return 12; }
            case 0xD4: { const u16 a = fetch16(); if (!c()) { idle(4); push16(PC); PC = a; return 24; } return 12; }
            case 0xDC: { const u16 a = fetch16(); if (c())  { idle(4); push16(PC); PC = a; return 24; } return 12; }

            case 0xC9: PC = pop16(); return 16;
            case 0xC0: idle(4); if (!z()) { PC = pop16(); return 20; } return 8;
            case 0xC8: idle(4); if (z())  { PC = pop16(); return 20; } return 8;
            case 0xD0: idle(4); if (!c()) { PC = pop16(); return 20; } return 8;
            case 0xD8: idle(4); if (c())  { PC = pop16(); return 20; } return 8;
            case 0xD9: PC = pop16(); IME = true; imeDelay = 0; return 16;

            case 0xC1: setBC(pop16()); return 12;
            case 0xD1: setDE(pop16()); return 12;
            case 0xE1: setHL(pop16()); return 12;
            case 0xF1: setAF(pop16()); return 12;
            case 0xC5: idle(4); push16(BC()); return 16;
            case 0xD5: idle(4); push16(DE()); return 16;
            case 0xE5: idle(4); push16(HL()); return 16;
            case 0xF5: idle(4); push16(AF()); return 16;

            case 0xC7: idle(4); push16(PC); PC = 0x00; return 16;
            case 0xCF: idle(4); push16(PC); PC = 0x08; return 16;
            case 0xD7: idle(4); push16(PC); PC = 0x10; return 16;
            case 0xDF: idle(4); push16(PC); PC = 0x18; return 16;
            case 0xE7: idle(4); push16(PC); PC = 0x20; return 16;
            case 0xEF: idle(4); push16(PC); PC = 0x28; return 16;
            case 0xF7: idle(4); push16(PC); PC = 0x30; return 16;
            case 0xFF: idle(4); push16(PC); PC = 0x38; return 16;


            case 0x09: addHL(BC()); return 8;
            case 0x19: addHL(DE()); return 8;
            case 0x29: addHL(HL()); return 8;
            case 0x39: addHL(SP); return 8;


            case 0xEA: { const u16 a = fetch16(); write8(a, A); return 16; }
            case 0xFA: { const u16 a = fetch16(); A = read8(a); return 16; }
            case 0xE0: { const u8 a = fetch8(); write8(static_cast<u16>(0xFF00 | a), A); return 12; }
            case 0xF0: { const u8 a = fetch8(); A = read8(static_cast<u16>(0xFF00 | a)); return 12; }
            case 0xE2: write8(static_cast<u16>(0xFF00 | C), A); return 8;
            case 0xF2: A = read8(static_cast<u16>(0xFF00 | C)); return 8;

            case 0xF9: SP = HL(); return 8;
            case 0xF8: { const s8 d = static_cast<s8>(fetch8()); const u16 result = static_cast<u16>(SP + d); setFlags(false, false, ((SP & 0x0F) + (static_cast<u8>(d) & 0x0F)) > 0x0F, ((SP & 0xFF) + static_cast<u8>(d)) > 0xFF); setHL(result); return 12; }
            case 0xE8: { const s8 d = static_cast<s8>(fetch8()); const u16 result = static_cast<u16>(SP + d); setFlags(false, false, ((SP & 0x0F) + (static_cast<u8>(d) & 0x0F)) > 0x0F, ((SP & 0xFF) + static_cast<u8>(d)) > 0xFF); SP = result; return 16; }

            case 0xC6: addA(fetch8()); return 8;
            case 0xCE: addA(fetch8(), true); return 8;
            case 0xD6: subA(fetch8()); return 8;
            case 0xDE: subA(fetch8(), true); return 8;
            case 0xE6: A &= fetch8(); setFlags(A == 0, false, true, false); return 8;
            case 0xEE: A ^= fetch8(); setFlags(A == 0, false, false, false); return 8;
            case 0xF6: A |= fetch8(); setFlags(A == 0, false, false, false); return 8;
            case 0xFE: cpA(fetch8()); return 8;

            case 0xF3: IME = false; imeDelay = 0; return 4;
            case 0xFB: if(!imeDelay)imeDelay = 2; return 4;

            default:
                std::cerr << "Unimplemented opcode 0x" << std::hex << int(op) << " at PC=0x" << int(static_cast<u16>(PC - 1)) << std::dec << "\n";
                locked = true;
                return 4;
        }
    }

    void addHL(u16 value) {
        const u32 result = HL() + value;
        const bool Z = z();
        setFlags(Z, false, ((HL() & 0x0FFF) + (value & 0x0FFF)) > 0x0FFF, result > 0xFFFF);
        setHL(static_cast<u16>(result));
    }
};


class PPU {
public:
    Memory& m;
    std::array<u32,GB_W*GB_H> pixels{}, drawing{};
    int dot=0, palette=0;
    bool lcdWasOn=true;
    u64 frameNumber=0;
    struct Sprite {int id=0,x=0,y=0,tile=0;u8 attr=0;};
    struct Pipeline {
        int line=0,windowLine=0,selectedCount=0,x=0,stall=0,scx=0,lastSpriteTile=-100000,windowX=0;
        bool windowYTriggered=false,windowActive=false,windowDrawn=false,firstLine=false;
        std::array<Sprite,10> sprites{};
        std::array<bool,10> fetched{};
    } state;
    static constexpr std::array<std::array<u32,4>,4> palettes{{
        {{0xFF9BBD0F,0xFF8BAC0F,0xFF306230,0xFF0F380F}},
        {{0xFFFFFFFF,0xFFAAAAAA,0xFF555555,0xFF000000}},
        {{0xFFDCEAF6,0xFF8CA7C2,0xFF3E6991,0xFF192F4A}},
        {{0xFFFFE8B8,0xFFD6A078,0xFF89506B,0xFF34172A}}
    }};
    explicit PPU(Memory& mem):m(mem){reset();}
    u32 shade(int value)const{return palettes[palette&3][value&3];}
    u32 blankColor()const{return m.dmgCompatibility&&!m.cgbMode?color(m.bgPalette,0,0):m.cgbMode?0xFFFFFFFF:shade(0);}
    u32 color(const std::array<u8,64>& ram,int paletteNumber,int raw)const{
        const int address=(paletteNumber&7)*8+(raw&3)*2;
        const u16 rgb=(u16)(ram[address]|(ram[address+1]<<8));
        const auto expand=[](u32 value){return (value<<3)|(value>>2);};
        return 0xFF000000|(expand(rgb&31)<<16)|(expand((rgb>>5)&31)<<8)|expand((rgb>>10)&31);
    }
    void reset(){
        dot=0;state={};frameNumber=0;lcdWasOn=(m.io[0x40]&0x80)!=0;
        m.io[0x44]=m.ppuCompareLY=0;m.ppuDot=0;m.ppuMode=m.ppuStatMode=lcdWasOn?2:0;
        pixels.fill(blankColor());drawing=pixels;beginLine();syncStat();
    }
    void power(bool enabled){
        dot=enabled?-2:0;state={};state.firstLine=enabled;lcdWasOn=enabled;m.io[0x44]=0;m.ppuMode=m.ppuStatMode=0;
        m.ppuDot=dot;if(enabled)m.ppuCompareLY=0;
        if(!enabled){pixels.fill(blankColor());drawing=pixels;}
        else beginLine();
        syncStat();
    }
    void beginLine(){
        state.selectedCount=0;state.x=0;state.windowActive=state.windowDrawn=false;
        state.fetched.fill(false);state.lastSpriteTile=-100000;
        if(state.line==m.io[0x4A])state.windowYTriggered=true;
    }
    struct BackgroundPixel {int raw=0;u8 attr=0;};
    BackgroundPixel backgroundAt(bool window,int px,int py)const {
        const u8 lcdc=m.io[0x40];
        const int mapBase=(window?(lcdc&0x40):(lcdc&8))?0x1C00:0x1800;
        const int mapAddress=mapBase+((py&255)/8)*32+(px&255)/8;
        const u8 tile=m.vramByte(0,(u16)mapAddress);
        const u8 attr=m.cgbMode?m.vramByte(1,(u16)mapAddress):0;
        const int offset=(lcdc&0x10)?tile*16:0x1000+(int)(s8)tile*16;
        const int row=((attr&0x40)?7-(py&7):(py&7))*2;
        const int bit=(attr&0x20)?(px&7):7-(px&7);
        const unsigned bank=(attr>>3)&1;
        return {((m.vramByte(bank,(u16)(offset+row))>>bit)&1)|
                (((m.vramByte(bank,(u16)(offset+row+1))>>bit)&1)<<1),attr};
    }
    int tileAt(bool window,int px,int py)const{return backgroundAt(window,px,py).raw;}
    void scanSprite(int id){
        if(m.dmaPos<160)return;
        const int height=(m.io[0x40]&4)?16:8;
        const int y=(int)m.oam[id*4]-16;
        if(state.selectedCount<10&&state.line>=y&&state.line<y+height){
            state.sprites[state.selectedCount++]={id,(int)m.oam[id*4+1]-8,y,(int)m.oam[id*4+2],m.oam[id*4+3]};
        }
    }
    void pixel(){
        const int x=state.x,y=state.line;
        const u8 lcdc=m.io[0x40];
        const bool bgEnabled=m.cgbMode||(lcdc&1);
        const auto bg=bgEnabled?backgroundAt(state.windowActive,
            state.windowActive?x-state.windowX:x+m.io[0x43],
            state.windowActive?state.windowLine:y+m.io[0x42]):BackgroundPixel{};
        u32 output=(m.dmgCompatibility&&!m.cgbMode)?color(m.bgPalette,0,bgEnabled?((m.io[0x47]>>(2*bg.raw))&3):0):m.cgbMode?color(m.bgPalette,bg.attr&7,bg.raw):
            shade(bgEnabled?((m.io[0x47]>>(2*bg.raw))&3):0);
        if(lcdc&2){
            // Select the winning OBJ first, then apply its BG priority. A hidden
            // winner still masks lower-priority OBJs (important in Mario games).
            // Sprites remain X-sorted for fetch timing, but CGB drawing priority
            // is determined exclusively by the original OAM index.
            const Sprite* winner=nullptr;
            int winnerRaw=0;
            for(int i=0;i<state.selectedCount;++i){
                const auto& s=state.sprites[i];
                if(x<s.x||x>=s.x+8)continue;
                const int height=(lcdc&4)?16:8;
                int row=y-s.y;
                if(s.attr&0x40)row=height-1-row;
                int tile=height==16?(s.tile&0xFE):s.tile;
                tile+=row/8;
                const int off=tile*16+(row&7)*2;
                const int bit=(s.attr&0x20)?x-s.x:7-(x-s.x);
                const unsigned bank=m.cgbMode?((s.attr>>3)&1):0;
                const int raw=((m.vramByte(bank,(u16)off)>>bit)&1)|
                    (((m.vramByte(bank,(u16)(off+1))>>bit)&1)<<1);
                if(!raw)continue;
                if(!winner||s.id<winner->id){winner=&s;winnerRaw=raw;}
                if(!m.cgbMode)break;
            }
            if(winner){
                const bool behind=m.cgbMode?
                    ((lcdc&1)&&bg.raw&&((bg.attr|winner->attr)&0x80)):
                    (bg.raw&&(winner->attr&0x80));
                if(!behind)output=(m.dmgCompatibility&&!m.cgbMode)?color(m.objPalette,(winner->attr&0x10)?1:0,(m.io[(winner->attr&0x10)?0x49:0x48]>>(winnerRaw*2))&3):m.cgbMode?color(m.objPalette,winner->attr&7,winnerRaw):
                    shade((m.io[(winner->attr&0x10)?0x49:0x48]>>(winnerRaw*2))&3);
            }
        }
        drawing[y*GB_W+x]=output;
    }
    void transferDot(){
        if(state.stall>0){--state.stall;return;}
        const int wx=(int)m.io[0x4B]-7;
        if(!state.windowActive&&state.windowYTriggered&&(m.io[0x40]&0x20)&&
           (m.cgbMode||(m.io[0x40]&1))&&
           wx<160&&state.x>=std::max(0,wx)){
            state.windowActive=state.windowDrawn=true;state.windowX=wx;
            state.lastSpriteTile=-100000;state.stall=5;return;
        }
        if(m.io[0x40]&2){
            for(int i=0;i<state.selectedCount;++i){
                const auto& s=state.sprites[i];
                if(state.fetched[i]||s.x>=160||s.x>state.x)continue;
                state.fetched[i]=true;
                const int pos=state.windowActive?s.x-state.windowX:s.x+state.scx;
                const int key=(pos>=0?pos/8:(pos-7)/8)+(state.windowActive?64:0);
                const int penalty=6+(state.lastSpriteTile==key?0:s.x==-8?5:std::max(0,5-(pos&7)));
                state.lastSpriteTile=key;state.stall=penalty-1;return;
            }
        }
        pixel();
        ++state.x;
        if(state.x==GB_W-4){m.ppuStatMode=0;syncStat();}
        if(state.x==GB_W){m.ppuMode=0;syncStat();m.hblankDMA();}
    }
    void syncStat(){m.refreshStat();}
    void tick(int cycles){
        if(!(m.io[0x40]&0x80)){if(lcdWasOn)power(false);return;}
        if(!lcdWasOn)power(true);
        for(int t=0;t<cycles;++t){
            ++dot;
            m.ppuDot=dot;
            if(dot==4){m.ppuCompareLY=state.line==153?0:(u8)state.line;syncStat();}
            if(state.line<GB_H){
                const int oamEnd=state.firstLine?76:84;
                if(!state.firstLine&&dot==4){m.ppuMode=2;syncStat();}
                if(dot>(state.firstLine?0:4)&&dot<=oamEnd&&!(dot&1))scanSprite((dot-(state.firstLine?0:4))/2-1);
                if(dot==oamEnd){
                    std::sort(state.sprites.begin(),state.sprites.begin()+state.selectedCount,
                        [](const Sprite& a,const Sprite& b){return a.x==b.x?a.id<b.id:a.x<b.x;});
                    state.scx=m.io[0x43];state.stall=12+(state.scx&7);
                    // The first OBJ fetch overlaps a background startup dot.
                    if((m.io[0x40]&2)&&state.selectedCount&&state.sprites[0].x<160)--state.stall;
                    m.ppuMode=m.ppuStatMode=3;syncStat();
                }else if(m.ppuMode==3)transferDot();
            }else if(state.line==153&&dot==4){m.io[0x44]=m.ppuCompareLY=0;syncStat();}
            if(dot==(state.firstLine?448:456)){
                dot=0;
                m.ppuDot=0;
                state.firstLine=false;
                if(state.line<GB_H&&state.windowDrawn)++state.windowLine;
                ++state.line;
                m.ppuCompareLY=0xFF; // LY comparison is inactive for the first four dots.
                if(state.line==144){
                    pixels=drawing;++frameNumber;m.io[0x0F]|=1;
                    if((m.io[0x41]&0x20)&&!m.statLine)m.io[0x0F]|=2;
                    m.ppuMode=m.ppuStatMode=1;
                }
                else if(state.line==154){state.line=0;state.windowLine=0;state.windowYTriggered=false;m.ppuMode=0;m.ppuStatMode=2;}
                else if(state.line<144){m.ppuMode=0;m.ppuStatMode=2;}
                m.io[0x44]=(u8)state.line;
                if(state.line<144)beginLine();
                syncStat();
            }
        }
    }
};

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
    void updateCapacitor(){capacitorCharge=std::pow(m.isCGBHardware()?0.998943:0.999958,(double)GB_CLOCK/sampleRate);}
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
        state={};clearQueue();updateCapacitor();scopePosition=0;for(auto& channel:scope)channel.fill(0);
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
    int sweepCalculation(){
        const int delta=state.sweepShadow>>(m.io[0x10]&7);
        if(m.io[0x10]&8)state.sweepNegated=true;
        const int value=state.sweepShadow+((m.io[0x10]&8)?-delta:delta);
        if(value>2047)state.channels[0].enabled=false;
        return value;
    }
    void trigger(int i){
        auto& c=state.channels[i];
        c.enabled=c.dac;c.started=true;
        if(!c.length){c.length=i==2?256:64;if(c.lengthEnabled&&(state.frameStep&1))--c.length;}
        c.timer=reload(i)+(i==2?6:0);
        if(i<2)c.timer=(c.timer&~3)|(state.channels[i].timer&3);
        if(i==2){
            c.phase=0;state.waveAccess=0;
            if(m.isCGBHardware())state.waveByte=0;
        }else{
            const int env=m.io[envelopeRegister(i)];
            c.volume=env>>4;c.envelopeTimer=(env&7)?(env&7):8;
            if(state.frameStep==7)++c.envelopeTimer;
            c.envelopeRunning=true;
        }
        if(i==0){
            state.sweepShadow=period(0);state.sweepTimer=(m.io[0x10]>>4)&7;
            if(!state.sweepTimer)state.sweepTimer=8;
            state.sweepEnabled=(m.io[0x10]&0x77)!=0;state.sweepNegated=false;
            if(m.io[0x10]&7)sweepCalculation();
        }
        if(i==3)state.lfsr=0x7FFF;
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
                state.frameStep=0;state.sweepEnabled=false;state.waveBuffer=0;
            }else if(!old&&on){
                state.frameStep=0;state.waveBuffer=0;
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
                // Common DMG manual-volume ("zombie envelope") behaviour.
                if(!(old&7)&&c.envelopeRunning)++c.volume;
                if(!(old&8)&&(!m.isCGBHardware()||(old&7)||!c.envelopeRunning))c.volume+=2;
                if((old^value)&8)c.volume=16-c.volume;
                c.volume&=15;
            }
            c.dac=(value&0xF8)!=0;if(!c.dac)c.enabled=false;
        }
        if(reg==0x1A){state.channels[2].dac=(value&0x80)!=0;if(!state.channels[2].dac)state.channels[2].enabled=false;}
        if(reg==0x14||reg==0x19||reg==0x1E||reg==0x23){
            const int i=reg==0x14?0:reg==0x19?1:reg==0x1E?2:3;
            auto& c=state.channels[i];const bool enable=(value&0x40)!=0;
            if(!c.lengthEnabled&&enable&&(state.frameStep&1)&&c.length>0)
                if(--c.length==0)c.enabled=false;
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
        const int step=state.frameStep;
        if(!(step&1))for(auto& c:state.channels)
            if(c.lengthEnabled&&c.length>0&&--c.length==0)c.enabled=false;
        if(step==2||step==6){
            if(--state.sweepTimer<=0){
                const int pace=(m.io[0x10]>>4)&7;state.sweepTimer=pace?pace:8;
                if(state.sweepEnabled&&pace){
                    const int value=sweepCalculation();
                    if(value<=2047&&(m.io[0x10]&7)){
                        state.sweepShadow=value;m.io[0x13]=(u8)(value&255);m.io[0x14]=(u8)((m.io[0x14]&0xF8)|(value>>8));
                        sweepCalculation();
                    }
                }
            }
        }
        if(step==7){
            for(int i:{0,1,3}){
                auto& c=state.channels[i];
                if(c.envelopeRunning&&--c.envelopeTimer<=0){
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
            return ((duty[m.io[i==0?0x11:0x16]>>6]>>c.phase)&1)?c.volume:0;
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
        if(state.waveAccess>0)--state.waveAccess;
        if(m.io[0x26]&0x80){
            for(int i=0;i<4;++i){
                auto& c=state.channels[i];
                if(!c.started)continue;
                if(--c.timer<=0){
                    c.timer=reload(i);
                    if(i<2)c.phase=(c.phase+1)&7;
                    else if(i==2){
                        if(c.enabled){c.phase=(c.phase+1)&31;state.waveByte=c.phase/2;
                            state.waveBuffer=m.io[0x30+state.waveByte];state.waveAccess=2;}
                    }else if((m.io[0x22]>>4)<14){
                        const int bit=(state.lfsr^(state.lfsr>>1))&1;
                        state.lfsr=(u16)((state.lfsr>>1)|(bit<<14));
                        if(m.io[0x22]&8)state.lfsr=(u16)((state.lfsr&~0x40)|(bit<<6));
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

class GameBoy {
public:
    Memory mem;
    CPU cpu;
    PPU ppu;
    APU apu;
    HardwareModel model=HardwareModel::Auto;
    int currentPalette=0,frameDebt=0;
    u64 totalCycles=0;
    u32 clockRate=GB_CLOCK;
    std::shared_ptr<void> backendOwner;
    std::function<bool()> backendReset;
    std::function<void()> backendRefresh;
    std::function<std::vector<u8>()> backendState,backendBess;
    std::function<bool(std::span<const u8>)> backendRestore;
    std::function<void()> backendCommitRegisters;
    std::function<void(bool)> backendPrinter;
    std::function<bool(GBSInspect&)> backendInspect;
    std::function<void(std::span<const u32>,unsigned,unsigned,unsigned)> backendPrint;
    std::function<std::vector<u8>()> auxiliaryState;
    std::function<bool(std::span<const u8>)> auxiliaryRestore;
    std::array<u8,4> multiplayerInput{};
    std::vector<u32> accessoryPixels;
    unsigned accessoryWidth=160,accessoryHeight=144;
    GameBoy():cpu(mem),ppu(mem),apu(mem){
        cpu.clock=[this](int cycles){tick(cycles);};
        mem.soundWrite=[this](u16 addr,u8 value){apu.write(addr,value);};
        mem.soundRead=[this](u16 addr){return apu.read(addr);};
        mem.soundPCM=[this](u16 addr){return apu.pcm(addr);};
        mem.dividerReset=[this](){apu.sequencer();};
        mem.lcdPower=[this](bool on){ppu.power(on);};
        reset();
    }
    GameBoy(const GameBoy&)=delete;
    GameBoy& operator=(const GameBoy&)=delete;
    void tick(int cycles){
        for(int i=0;i<cycles;++i){
            if(!cpu.stopped){
                const unsigned mask=mem.apuDividerMask();
                const bool apuBit=(mem.divCounter&mask)!=0;
                if(mem.speedSwitchTicks>0)--mem.speedSwitchTicks;else mem.tick(1);
                if(apuBit&&!(mem.divCounter&mask))apu.sequencer();
            }
            if(mem.doubleSpeed&&++mem.dotPhase<2)continue;
            mem.dotPhase=0;++totalCycles;
            if(cpu.stopped)continue;
            mem.cpuHalted=cpu.halted;
            // DMA starts at the PPU HBlank edge, and moves one byte per two dots.
            mem.tickBaseDot();ppu.tick(1);apu.tick();
        }
    }
    void reset(){
        if(backendReset&&!mem.rom.empty()){frameDebt=0;totalCycles=0;ppu.frameNumber=0;apu.clearQueue();if(!backendReset())mem.loadError="Hardware backend initialization failed.";return;}
        mem.cgbHardware=(mem.cgbROM&&model!=HardwareModel::DMG)||model==HardwareModel::CGB;
        mem.dmgCompatibility=mem.cgbHardware&&!mem.cgbROM;
        mem.cgbMode=mem.cgbHardware;
        mem.resetIO();
        mem.romBank=1;mem.bankHigh=0;mem.ramBank=0;mem.mbcMode=0;mem.ramEnabled=mem.mbc==0||mem.mbc==101;
        mem.latchWrite=0;mem.rtcLatched=false;mem.latchedRTC=mem.rtc;
        mem.bootROMCGB=mem.cgbMode;
        mem.bootROM=mem.cgbMode?mem.cgbBootROM:mem.dmgBootROM;
        mem.bootROMMapped=!mem.bootROM.empty();
        if(mem.dmgCompatibility&&!mem.bootROMMapped){
            mem.cgbMode=false;mem.io[0x4C]=4;
            static constexpr std::array<u16,4> bg={0x7FFF,0x32BF,0x00D0,0x0000},obj={0x7FFF,0x421F,0x001F,0x0000};
            for(int i=0;i<4;++i){mem.bgPalette[i*2]=(u8)bg[i];mem.bgPalette[i*2+1]=(u8)(bg[i]>>8);
                for(int p=0;p<2;++p){mem.objPalette[p*8+i*2]=(u8)obj[i];mem.objPalette[p*8+i*2+1]=(u8)(obj[i]>>8);}}
        }
        if(mem.cgbHardware){cpu.setAF(0x1180);cpu.setBC(0x0000);cpu.setDE(0xFF56);cpu.setHL(0x000D);}
        else {cpu.setAF(0x01B0);cpu.setBC(0x0013);cpu.setDE(0x00D8);cpu.setHL(0x014D);}
        cpu.SP=0xFFFE;cpu.PC=0x100;cpu.IME=false;cpu.imeDelay=0;
        cpu.halted=false;cpu.stopped=false;cpu.haltBug=false;
        cpu.locked=false;cpu.elapsed=cpu.instructionElapsed=0;frameDebt=0;totalCycles=0;
        mem.divCounter=mem.cgbMode?0x1EA0:0xABC8;
        if(mem.bootROMMapped){
            // Deterministic power-on state: the boot code initializes the
            // cartridge hand-off registers, LCD, APU and divider itself.
            cpu.setAF(0);cpu.setBC(0);cpu.setDE(0);cpu.setHL(0);cpu.SP=0;cpu.PC=0;
            mem.io.fill(0);mem.io[0]=0xCF;mem.io[0x0F]=0xE0;
            mem.io[0x55]=0xFF;mem.divCounter=0;
        }
        ppu.palette=currentPalette;ppu.reset();apu.reset();
    }
    bool bootROMConfigured(HardwareModel requested)const{
        if(requested==HardwareModel::DMG)return !mem.dmgBootROM.empty();
        if(requested==HardwareModel::CGB)return !mem.cgbBootROM.empty();
        return !mem.dmgBootROM.empty()||!mem.cgbBootROM.empty();
    }
    bool loadBootROM(const std::filesystem::path& path){
        auto image=Memory::loadBytes(path,0x900);
        mem.loadError.clear();
        if(image.size()==0x100){mem.dmgBootROM=std::move(image);mem.dmgBootROMPath=path;return true;}
        if(image.size()==0x800){
            // A packed CGB dump omits the cartridge header hole 0100-01FF.
            image.insert(image.begin()+0x100,0x100,0);
        }
        if(image.size()==0x900){mem.cgbBootROM=std::move(image);mem.cgbBootROMPath=path;return true;}
        mem.loadError="Boot ROM must be 256 bytes (DMG), or 2048/2304 bytes (CGB).";return false;
    }
    void clearBootROM(HardwareModel requested=HardwareModel::Auto){
        if(requested!=HardwareModel::CGB){mem.dmgBootROM.clear();mem.dmgBootROMPath.clear();}
        if(requested!=HardwareModel::DMG){mem.cgbBootROM.clear();mem.cgbBootROMPath.clear();}
    }
    bool modelCompatible(HardwareModel requested){
        mem.loadError.clear();
        if(mem.rom.size()>0x143){
            if((requested==HardwareModel::DMG||requested==HardwareModel::Pocket||requested==HardwareModel::Light||requested==HardwareModel::SGB||requested==HardwareModel::SGB2||requested==HardwareModel::SGBPAL)&&mem.rom[0x143]==0xC0){mem.loadError="This cartridge requires Game Boy Color.";return false;}
        }
        return true;
    }
    bool setModel(HardwareModel requested){
        if(!modelCompatible(requested))return false;
        model=requested;reset();return true;
    }
    bool load(const std::filesystem::path& path,HardwareModel requested=HardwareModel::Auto){
        // Validate model compatibility before replacing the running cartridge.
        const auto data=Memory::loadBytes(path,8*1024*1024);
        if(data.size()>0x143){
            if((requested==HardwareModel::DMG||requested==HardwareModel::Pocket||requested==HardwareModel::Light||requested==HardwareModel::SGB||requested==HardwareModel::SGB2||requested==HardwareModel::SGBPAL)&&data[0x143]==0xC0){mem.loadError="This cartridge requires Game Boy Color.";return false;}
        }
        mem.saveBattery();
        if(!mem.loadROM(path))return false;
        model=requested;
        reset();return mem.loadError.empty();
    }
    void runFrame(){
        int t=frameDebt;
        while(t<GB_FRAME){
            const u64 before=totalCycles;cpu.step();t+=(int)(totalCycles-before);
        }
        frameDebt=t-GB_FRAME;
        if(backendRefresh)backendRefresh();
    }
    void setPalette(int p){currentPalette=(p+4)%4;ppu.palette=currentPalette;}
    u32 romHash() const {u32 h=2166136261u;for(u8 x:mem.rom){h^=x;h*=16777619u;}return h;}
    std::filesystem::path statePath()const{auto p=mem.romPath;p.replace_extension(".state");return p;}
    static constexpr std::size_t state6ExtensionSize=48+64+4+3584;
    std::vector<u8> serializeState(bool version5=false)const{
        if(backendState)return backendState();
        if(mem.rom.empty())return {};
        std::ostringstream f(std::ios::binary|std::ios::out);
        u32 checksum=2166136261u;
        auto write=[&](const char* data,std::streamsize size){
            f.write(data,size);
            for(std::streamsize i=0;i<size;++i){checksum^=(u8)data[i];checksum*=16777619u;}
        };
        auto put=[&](auto v){write((const char*)&v,sizeof(v));};
        write(version5?"GBSTATE5":"GBSTATE6",8);put(romHash());
        for(u8 v:{cpu.A,cpu.F,cpu.B,cpu.C,cpu.D,cpu.E,cpu.H,cpu.L})put(v);
        put(cpu.SP);put(cpu.PC);put((u8)cpu.IME);put((u8)cpu.halted);put((u8)cpu.stopped);put((u8)cpu.haltBug);put((std::int32_t)cpu.imeDelay);
        put(mem.ie);put(mem.divCounter);put((std::int32_t)mem.timerReload);
        put((std::int32_t)mem.serialTicks);put((std::int32_t)mem.dmaTicks);put((std::int32_t)mem.dmaPos);put(mem.dmaSource);
        put((std::int32_t)mem.ppuMode);put((u8)mem.statLine);put((u8)mem.ramEnabled);
        put(mem.romBank);put(mem.bankHigh);put(mem.ramBank);put(mem.mbcMode);put(mem.latchWrite);
        put((u8)mem.rtcLatched);put(mem.rtcCycles);
        put((u8)mem.cgbMode);put((u8)mem.doubleSpeed);put((u8)mem.hdmaActive);put((u8)mem.hdmaHblank);
        put(mem.hdmaSource);put(mem.hdmaDest);put((std::int32_t)mem.hdmaBlocks);put((std::int32_t)mem.hdmaBytes);
        put((std::int32_t)mem.hdmaDot);put((std::int32_t)mem.dotPhase);put((std::int32_t)mem.speedSwitchTicks);
        auto bytes=[&](const auto& arr){if(!arr.empty())write((const char*)arr.data(),(std::streamsize)arr.size()*sizeof(arr[0]));};
        bytes(mem.vram);bytes(mem.wram);bytes(mem.oam);bytes(mem.io);bytes(mem.hram);
        bytes(mem.bgPalette);bytes(mem.objPalette);
        bytes(mem.rtc);bytes(mem.latchedRTC);bytes(mem.soundTrigger);
        std::uint32_t ext=(u32)mem.eram.size();put(ext);bytes(mem.eram);
        put((u8)cpu.locked);put((u8)mem.timerJustReloaded);put((std::int32_t)mem.dmaDelay);
        put(mem.pendingDmaSource);put(mem.serialSent);put(mem.ppuCompareLY);put((std::int32_t)mem.ppuStatMode);
        put((std::int32_t)frameDebt);put(totalCycles);put(ppu.frameNumber);
        // Preserve the historical native layout while making its padding
        // deterministic. Struct assignment may copy uninitialized padding.
        auto layout=[&]<class T>(const T&,auto fields){
            std::array<u8,sizeof(T)> data{};
            auto field=[&](std::size_t at,const auto& value){std::memcpy(data.data()+at,&value,sizeof(value));};
            fields(field);write((const char*)data.data(),data.size());
        };
        layout(ppu.state,[&](auto field){
            const auto& p=ppu.state;
#define GB_PPU_FIELD(name) field(offsetof(PPU::Pipeline,name),p.name)
            GB_PPU_FIELD(line);GB_PPU_FIELD(windowLine);GB_PPU_FIELD(selectedCount);GB_PPU_FIELD(x);
            GB_PPU_FIELD(stall);GB_PPU_FIELD(scx);GB_PPU_FIELD(lastSpriteTile);GB_PPU_FIELD(windowX);
            GB_PPU_FIELD(windowYTriggered);GB_PPU_FIELD(windowActive);GB_PPU_FIELD(windowDrawn);GB_PPU_FIELD(firstLine);
            for(std::size_t i=0;i<p.sprites.size();++i){const auto at=offsetof(PPU::Pipeline,sprites)+i*sizeof(PPU::Sprite);const auto& s=p.sprites[i];
                field(at+offsetof(PPU::Sprite,id),s.id);field(at+offsetof(PPU::Sprite,x),s.x);
                field(at+offsetof(PPU::Sprite,y),s.y);field(at+offsetof(PPU::Sprite,tile),s.tile);field(at+offsetof(PPU::Sprite,attr),s.attr);
            }
            GB_PPU_FIELD(fetched);
#undef GB_PPU_FIELD
        });
        layout(apu.state,[&](auto field){
            const auto& a=apu.state;
            for(std::size_t i=0;i<a.channels.size();++i){const auto at=offsetof(APU::State,channels)+i*sizeof(APU::Channel);const auto& c=a.channels[i];
#define GB_CHANNEL_FIELD(name) field(at+offsetof(APU::Channel,name),c.name)
                GB_CHANNEL_FIELD(timer);GB_CHANNEL_FIELD(length);GB_CHANNEL_FIELD(phase);GB_CHANNEL_FIELD(volume);GB_CHANNEL_FIELD(envelopeTimer);
                GB_CHANNEL_FIELD(enabled);GB_CHANNEL_FIELD(dac);GB_CHANNEL_FIELD(lengthEnabled);GB_CHANNEL_FIELD(envelopeRunning);GB_CHANNEL_FIELD(started);
#undef GB_CHANNEL_FIELD
            }
#define GB_APU_FIELD(name) field(offsetof(APU::State,name),a.name)
            GB_APU_FIELD(frameStep);GB_APU_FIELD(sweepShadow);GB_APU_FIELD(sweepTimer);GB_APU_FIELD(waveAccess);GB_APU_FIELD(waveByte);
            GB_APU_FIELD(sweepEnabled);GB_APU_FIELD(sweepNegated);GB_APU_FIELD(lfsr);GB_APU_FIELD(waveBuffer);GB_APU_FIELD(sampleClock);
            GB_APU_FIELD(mixDivider);GB_APU_FIELD(mixCount);GB_APU_FIELD(sumLeft);GB_APU_FIELD(sumRight);GB_APU_FIELD(capLeft);GB_APU_FIELD(capRight);
#undef GB_APU_FIELD
        });
        put((std::int32_t)ppu.dot);put((u8)ppu.lcdWasOn);put((std::int32_t)currentPalette);
        write((const char*)ppu.pixels.data(),(std::streamsize)ppu.pixels.size()*sizeof(u32));
        write((const char*)ppu.drawing.data(),(std::streamsize)ppu.drawing.size()*sizeof(u32));
        put((u8)mem.bootROMMapped);put((u8)mem.bootROMCGB);
        u32 bootHash=2166136261u;
        if(mem.bootROMMapped)for(u8 b:mem.bootROM){bootHash^=b;bootHash*=16777619u;}
        put(mem.bootROMMapped?bootHash:0u);
        put((u8)((mem.right?1:0)|(mem.left?2:0)|(mem.up?4:0)|(mem.down?8:0)|
                 (mem.a?16:0)|(mem.b?32:0)|(mem.select?64:0)|(mem.start?128:0)));
        if(!version5){
            std::array<u8,state6ExtensionSize> ext{};std::size_t index=0;
            auto number=[&](u32 v,unsigned size){for(unsigned i=0;i<size;++i)ext[index++]=(u8)(v>>(i*8));};
            for(bool flag:{mem.cgbHardware,mem.dmgCompatibility,mem.rumbleActive,mem.hucIRMode,mem.hucIRLight,
                          mem.motion.enabled,mem.motion.latchReady,mem.motion.writeEnabled})number(flag?1:0,1);
            number((u32)mem.serialBits,1);number(mem.motion.pins,1);
            number((u32)mem.motion.phase,4);number((u32)mem.motion.bits,4);number(mem.motion.command,2);
            number(mem.motion.readWord,2);number(mem.motion.x,2);number(mem.motion.y,2);number(mem.motion.shift,4);
            number((u16)mem.motion.tiltX,2);number((u16)mem.motion.tiltY,2);number((u32)apu.sampleRate,4);
            index=48;for(u8 value:mem.cameraRegisters)number(value,1);number((u32)mem.cameraTicks,4);for(u8 value:mem.cameraPending)number(value,1);
            bytes(ext);
        }
        f.write((const char*)&checksum,sizeof(checksum));
        if(!f)return {};
        const auto result=f.str();return {result.begin(),result.end()};
    }
    bool saveState(const std::filesystem::path& path)const{
        const auto bytes=serializeState();
        return !bytes.empty()&&Memory::writeAtomic(path,std::string_view((const char*)bytes.data(),bytes.size()));
    }
    bool saveState()const{return saveState(statePath());}
    bool loadState(const std::filesystem::path& path){return deserializeState(Memory::loadBytes(path,400000));}
    bool loadState(){return loadState(statePath());}
    bool deserializeState(std::span<const u8> bytes){
        if(backendRestore)return backendRestore(bytes);
        if(mem.rom.empty())return false;
        // Reject truncated/wrong-version/different-ROM states before mutating live objects.
        if(bytes.size()<120 || bytes.size()>400000 || mem.eram.size()>131072)return false;
        u32 storedChecksum=0;std::memcpy(&storedChecksum,bytes.data()+bytes.size()-4,4);
        u32 actualChecksum=2166136261u;
        for(std::size_t i=0;i<bytes.size()-4;++i){actualChecksum^=bytes[i];actualChecksum*=16777619u;}
        if(actualChecksum!=storedChecksum)return false;
        GameBoy tmp;
        tmp.mem.rom=mem.rom;tmp.mem.eram=mem.eram;tmp.mem.romPath=mem.romPath;
        tmp.mem.mbc=mem.mbc;tmp.mem.hasBattery=mem.hasBattery;tmp.mem.hasRTC=mem.hasRTC;tmp.mem.cgbROM=mem.cgbROM;
        tmp.mem.cgbMode=mem.cgbMode;tmp.mem.cgbHardware=mem.cgbHardware;tmp.mem.dmgCompatibility=mem.dmgCompatibility;tmp.model=model;
        std::size_t off=0;
        auto pull=[&](auto& val)->bool {if(off+sizeof(val)>bytes.size())return false;std::memcpy(&val,bytes.data()+off,sizeof(val));off+=sizeof(val);return true;};
        auto array=[&](auto& arr)->bool {const auto n=arr.size()*sizeof(arr[0]);if(off+n>bytes.size())return false;if(n)std::memcpy(arr.data(),bytes.data()+off,n);off+=n;return true;};
        const std::string version((const char*)bytes.data(),8);
        const bool legacy=version=="GBSTATE3";
        const bool modern=version=="GBSTATE6",extended=version=="GBSTATE5"||modern;
        if((!legacy&&!extended&&version!="GBSTATE4")||(legacy&&mem.cgbMode))return false;off=8;
        u32 hash=0;if(!pull(hash)||hash!=romHash())return false;
        if(!pull(tmp.cpu.A)||!pull(tmp.cpu.F)||!pull(tmp.cpu.B)||!pull(tmp.cpu.C)||!pull(tmp.cpu.D)||!pull(tmp.cpu.E)||!pull(tmp.cpu.H)||!pull(tmp.cpu.L))return false;
        u8 flag=0;
        if(!pull(tmp.cpu.SP)||!pull(tmp.cpu.PC)||!pull(flag))return false;tmp.cpu.IME=flag!=0;
        if(!pull(flag))return false;tmp.cpu.halted=flag!=0;
        if(!pull(flag))return false;tmp.cpu.stopped=flag!=0;
        if(!pull(flag))return false;tmp.cpu.haltBug=flag!=0;
        std::int32_t n=0;if(!pull(n))return false;tmp.cpu.imeDelay=n;
        if(!pull(tmp.mem.ie)||!pull(tmp.mem.divCounter)||!pull(n))return false;tmp.mem.timerReload=n;
        if(!pull(n))return false;tmp.mem.serialTicks=n;
        if(!pull(n))return false;tmp.mem.dmaTicks=n;
        if(!pull(n))return false;tmp.mem.dmaPos=n;
        if(!pull(tmp.mem.dmaSource)||!pull(n))return false;tmp.mem.ppuMode=n;
        if(!pull(flag))return false;tmp.mem.statLine=flag!=0;
        if(!pull(flag))return false;tmp.mem.ramEnabled=flag!=0;
        if(!pull(tmp.mem.romBank)||!pull(tmp.mem.bankHigh)||!pull(tmp.mem.ramBank)||!pull(tmp.mem.mbcMode)||!pull(tmp.mem.latchWrite))return false;
        if(!pull(flag))return false;tmp.mem.rtcLatched=flag!=0;
        if(!pull(tmp.mem.rtcCycles))return false;
        if(!legacy){
            if(!pull(flag)||flag>1||((flag!=0)!=mem.cgbMode&&!(mem.cgbHardware&&!mem.cgbROM)))return false;tmp.mem.cgbMode=flag!=0;
            if(!pull(flag)||flag>1)return false;tmp.mem.doubleSpeed=flag!=0;
            if(!pull(flag)||flag>1)return false;tmp.mem.hdmaActive=flag!=0;
            if(!pull(flag)||flag>1)return false;tmp.mem.hdmaHblank=flag!=0;
            if(!pull(tmp.mem.hdmaSource)||!pull(tmp.mem.hdmaDest)||!pull(n))return false;tmp.mem.hdmaBlocks=n;
            if(!pull(n))return false;tmp.mem.hdmaBytes=n;
            if(!pull(n))return false;tmp.mem.hdmaDot=n;
            if(!pull(n))return false;tmp.mem.dotPhase=n;
            if(!pull(n))return false;tmp.mem.speedSwitchTicks=n;
        }
        if(legacy){
            constexpr std::size_t oldBytes=8192;
            if(off+oldBytes*2>bytes.size())return false;
            std::memcpy(tmp.mem.vram.data(),bytes.data()+off,oldBytes);off+=oldBytes;
            std::memcpy(tmp.mem.wram.data(),bytes.data()+off,oldBytes);off+=oldBytes;
        }else if(!array(tmp.mem.vram)||!array(tmp.mem.wram))return false;
        if(!array(tmp.mem.oam)||!array(tmp.mem.io)||!array(tmp.mem.hram))return false;
        if(!legacy&&(!array(tmp.mem.bgPalette)||!array(tmp.mem.objPalette)))return false;
        if(!array(tmp.mem.rtc)||!array(tmp.mem.latchedRTC)||!array(tmp.mem.soundTrigger))return false;
        u32 ext=0;if(!pull(ext)||ext!=mem.eram.size()||!array(tmp.mem.eram))return false;
        if(!pull(flag))return false;tmp.cpu.locked=flag!=0;
        if(!pull(flag))return false;tmp.mem.timerJustReloaded=flag!=0;
        if(!pull(n))return false;tmp.mem.dmaDelay=n;
        if(!pull(tmp.mem.pendingDmaSource)||!pull(tmp.mem.serialSent)||!pull(tmp.mem.ppuCompareLY)||!pull(n))return false;tmp.mem.ppuStatMode=n;
        if(!pull(n))return false;tmp.frameDebt=n;
        if(!pull(tmp.totalCycles)||!pull(tmp.ppu.frameNumber))return false;
        // Legacy states contain native-layout pipeline/APU structs. Inspect
        // boolean object bytes before constructing the structs, since values
        // outside 0/1 are invalid C++ bool representations on supported builds.
        auto boolean=[&](std::size_t at){return at<bytes.size()&&bytes[at]<=1;};
        if(off+sizeof(PPU::Pipeline)>bytes.size())return false;
        for(std::size_t at:{offsetof(PPU::Pipeline,windowYTriggered),offsetof(PPU::Pipeline,windowActive),
                           offsetof(PPU::Pipeline,windowDrawn),offsetof(PPU::Pipeline,firstLine)})
            if(!boolean(off+at))return false;
        for(std::size_t i=0;i<10;++i)if(!boolean(off+offsetof(PPU::Pipeline,fetched)+i*sizeof(bool)))return false;
        if(!pull(tmp.ppu.state)||off+sizeof(APU::State)>bytes.size())return false;
        for(std::size_t at:{offsetof(APU::State,sweepEnabled),offsetof(APU::State,sweepNegated)})
            if(!boolean(off+at))return false;
        for(std::size_t i=0;i<4;++i)for(std::size_t at:{offsetof(APU::Channel,enabled),offsetof(APU::Channel,dac),
            offsetof(APU::Channel,lengthEnabled),offsetof(APU::Channel,envelopeRunning),offsetof(APU::Channel,started)})
            if(!boolean(off+offsetof(APU::State,channels)+i*sizeof(APU::Channel)+at))return false;
        if(!pull(tmp.apu.state))return false;
        if(!pull(n))return false;tmp.ppu.dot=n;
        if(!pull(flag))return false;tmp.ppu.lcdWasOn=flag!=0;
        if(!pull(n)||n<0||n>3)return false;tmp.currentPalette=n;tmp.ppu.palette=n;
        const auto len=tmp.ppu.pixels.size()*sizeof(u32);
        if(off+len*2+(extended?7:0)+(modern?state6ExtensionSize:0)+4!=bytes.size()||tmp.ppu.dot< -2||tmp.ppu.dot>=456||tmp.mem.ppuMode<0||tmp.mem.ppuMode>3||tmp.mem.ppuStatMode<0||tmp.mem.ppuStatMode>3||tmp.cpu.imeDelay<0||tmp.cpu.imeDelay>2)return false;
        if(tmp.mem.dmaPos<0||tmp.mem.dmaPos>160||tmp.mem.dmaTicks<0||tmp.mem.dmaTicks>3||
           tmp.mem.dmaDelay<0||tmp.mem.dmaDelay>8||tmp.frameDebt<0||tmp.frameDebt>(legacy?24:8224)||
           tmp.ppu.state.line<0||tmp.ppu.state.line>153||tmp.ppu.state.selectedCount<0||tmp.ppu.state.selectedCount>10||
           tmp.ppu.state.x<0||tmp.ppu.state.x>160||tmp.ppu.state.windowLine<0||tmp.ppu.state.windowLine>144||
           tmp.mem.timerReload< -1||tmp.mem.timerReload>4||tmp.mem.serialTicks<0||tmp.mem.serialTicks>4096||
           tmp.apu.state.frameStep<0||tmp.apu.state.frameStep>7||tmp.apu.state.waveByte<0||tmp.apu.state.waveByte>15||
           tmp.apu.state.sampleClock>=GB_CLOCK||tmp.mem.rtcCycles>=GB_CLOCK)return false;
        if((tmp.cpu.F&15)||tmp.mem.bankHigh>3||tmp.mem.mbcMode>1||tmp.mem.romBank>511||
           tmp.ppu.state.stall<0||tmp.ppu.state.stall>32||tmp.ppu.state.scx<0||tmp.ppu.state.scx>255||
           tmp.ppu.state.windowX< -7||tmp.ppu.state.windowX>248||
           tmp.ppu.state.lastSpriteTile< -100000||tmp.ppu.state.lastSpriteTile>1024||
           (tmp.mem.ppuMode==3&&(tmp.ppu.state.line>=144||tmp.ppu.state.x>=160))||
           tmp.apu.state.sweepShadow<0||tmp.apu.state.sweepShadow>2047||tmp.apu.state.sweepTimer<0||tmp.apu.state.sweepTimer>8||
           tmp.apu.state.waveAccess<0||tmp.apu.state.waveAccess>2||tmp.apu.state.mixDivider<0||tmp.apu.state.mixDivider>3||
           tmp.apu.state.mixCount<0||tmp.apu.state.mixCount>32)return false;
        for(int i=0;i<tmp.ppu.state.selectedCount;++i){const auto& sprite=tmp.ppu.state.sprites[i];
            if(sprite.id<0||sprite.id>=40||sprite.x< -8||sprite.x>247||sprite.y< -16||sprite.y>239||sprite.tile<0||sprite.tile>255)return false;}
        for(double sample:{tmp.apu.state.sumLeft,tmp.apu.state.sumRight,tmp.apu.state.capLeft,tmp.apu.state.capRight})
            if(!std::isfinite(sample)||std::abs(sample)>1000000000.0)return false;
        if(tmp.mem.hdmaBlocks<0||tmp.mem.hdmaBlocks>128||tmp.mem.hdmaBytes<0||tmp.mem.hdmaBytes>16||
           tmp.mem.hdmaDot<0||tmp.mem.hdmaDot>1||tmp.mem.hdmaDest<0x8000||tmp.mem.hdmaDest>0xA000||
           tmp.mem.hdmaDest+tmp.mem.hdmaBytes>0xA000||
           tmp.mem.dotPhase<0||tmp.mem.dotPhase>1||(!tmp.mem.doubleSpeed&&tmp.mem.dotPhase)||
           tmp.mem.speedSwitchTicks<0||tmp.mem.speedSwitchTicks>8200||
           (tmp.mem.hdmaActive&&(tmp.mem.hdmaBlocks==0||tmp.mem.hdmaDest==0xA000))||
           (tmp.mem.hdmaBytes&&(!tmp.mem.hdmaActive||tmp.mem.hdmaBlocks==0))||
           (tmp.mem.hdmaActive&&!tmp.mem.hdmaHblank&&!tmp.mem.hdmaBytes)||
           (!tmp.mem.cgbMode&&(tmp.mem.doubleSpeed||tmp.mem.hdmaActive||tmp.mem.hdmaBytes||tmp.mem.speedSwitchTicks)))return false;
        if(!legacy&&((tmp.mem.io[0x4F]&~1)||(tmp.mem.io[0x70]&~7)||
           (tmp.mem.io[0x68]&0x40)||(tmp.mem.io[0x6A]&0x40)||(tmp.mem.io[0x4D]&~1)))return false;
        for(int i=0;i<4;++i){const auto& c=tmp.apu.state.channels[i];
            if(c.phase<0||c.phase>(i==2?31:7)||c.volume<0||c.volume>15||c.length<0||c.length>(i==2?256:64)||c.timer<0||c.timer>4000000||c.envelopeTimer<0||c.envelopeTimer>9)return false;}
        std::memcpy(tmp.ppu.pixels.data(),bytes.data()+off,len);
        std::memcpy(tmp.ppu.drawing.data(),bytes.data()+off+len,len);
        off+=len*2;
        if(extended){
            if(!pull(flag)||flag>1)return false;tmp.mem.bootROMMapped=flag!=0;
            if(!pull(flag)||flag>1)return false;tmp.mem.bootROMCGB=flag!=0;
            u32 bootHash=0;if(!pull(bootHash))return false;
            if(tmp.mem.bootROMMapped){
                if(tmp.mem.bootROMCGB!=tmp.mem.isCGBHardware())return false;
                auto matches=[&](const std::vector<u8>& image){
                    if(image.size()!=(tmp.mem.bootROMCGB?0x900u:0x100u))return false;
                    u32 hash=2166136261u;for(u8 b:image){hash^=b;hash*=16777619u;}return hash==bootHash;
                };
                const auto& configured=tmp.mem.bootROMCGB?mem.cgbBootROM:mem.dmgBootROM;
                if(matches(mem.bootROM))tmp.mem.bootROM=mem.bootROM;
                else if(matches(configured))tmp.mem.bootROM=configured;
                else return false; // A boot snapshot needs its exact external image.
            }else if(bootHash)return false;
            if(!pull(flag))return false;
            tmp.mem.right=(flag&1)!=0;tmp.mem.left=(flag&2)!=0;tmp.mem.up=(flag&4)!=0;tmp.mem.down=(flag&8)!=0;
            tmp.mem.a=(flag&16)!=0;tmp.mem.b=(flag&32)!=0;tmp.mem.select=(flag&64)!=0;tmp.mem.start=(flag&128)!=0;
        }
        if(modern){
            std::size_t index=off;auto number=[&](unsigned size){u32 v=0;for(unsigned i=0;i<size;++i)v|=(u32)bytes[index++]<<(i*8);return v;};
            for(unsigned i=0;i<8;++i)if(bytes[index+i]>1)return false;
            tmp.mem.cgbHardware=number(1)!=0;tmp.mem.dmgCompatibility=number(1)!=0;tmp.mem.rumbleActive=number(1)!=0;
            tmp.mem.hucIRMode=number(1)!=0;tmp.mem.hucIRLight=number(1)!=0;tmp.mem.motion.enabled=number(1)!=0;
            tmp.mem.motion.latchReady=number(1)!=0;tmp.mem.motion.writeEnabled=number(1)!=0;
            tmp.mem.serialBits=(int)number(1);tmp.mem.motion.pins=(u8)number(1);tmp.mem.motion.phase=(int)number(4);tmp.mem.motion.bits=(int)number(4);
            tmp.mem.motion.command=(u16)number(2);tmp.mem.motion.readWord=(u16)number(2);tmp.mem.motion.x=(u16)number(2);tmp.mem.motion.y=(u16)number(2);
            tmp.mem.motion.shift=number(4);tmp.mem.motion.tiltX=(std::int16_t)number(2);tmp.mem.motion.tiltY=(std::int16_t)number(2);
            tmp.apu.sampleRate=(int)number(4);
            if(tmp.mem.cgbHardware!=mem.cgbHardware||tmp.mem.dmgCompatibility!=mem.dmgCompatibility||tmp.mem.serialBits>8||
                (tmp.mem.motion.pins&~0xC2)||tmp.mem.motion.phase<0||tmp.mem.motion.phase>4||tmp.mem.motion.bits<0||tmp.mem.motion.bits>16||
                (tmp.apu.sampleRate!=44100&&tmp.apu.sampleRate!=48000&&tmp.apu.sampleRate!=96000))return false;
            while(index<off+48)if(bytes[index++])return false;
            for(auto& value:tmp.mem.cameraRegisters)value=(u8)number(1);tmp.mem.cameraTicks=(int)number(4);for(auto& value:tmp.mem.cameraPending)value=(u8)number(1);
            if((tmp.mem.cameraRegisters[0]&~7)||tmp.mem.cameraTicks<0||tmp.mem.cameraTicks>5000000)return false;
        }else {const int period=tmp.mem.cgbMode&&(tmp.mem.io[2]&2)?16:512;tmp.mem.serialBits=tmp.mem.serialTicks?8-tmp.mem.serialTicks/period:0;}
        // References in CPU/PPU/APU must remain bound to this GameBoy's Memory.
        mem.vram=tmp.mem.vram;mem.wram=tmp.mem.wram;mem.oam=tmp.mem.oam;mem.io=tmp.mem.io;mem.hram=tmp.mem.hram;
        mem.bgPalette=tmp.mem.bgPalette;mem.objPalette=tmp.mem.objPalette;
        mem.bootROMMapped=tmp.mem.bootROMMapped;mem.bootROMCGB=tmp.mem.bootROMCGB;
        if(mem.bootROMMapped)mem.bootROM=std::move(tmp.mem.bootROM);
        mem.right=tmp.mem.right;mem.left=tmp.mem.left;mem.up=tmp.mem.up;mem.down=tmp.mem.down;
        mem.a=tmp.mem.a;mem.b=tmp.mem.b;mem.select=tmp.mem.select;mem.start=tmp.mem.start;
        mem.doubleSpeed=tmp.mem.doubleSpeed;mem.hdmaActive=tmp.mem.hdmaActive;mem.hdmaHblank=tmp.mem.hdmaHblank;
        mem.hdmaSource=tmp.mem.hdmaSource;mem.hdmaDest=tmp.mem.hdmaDest;
        mem.hdmaBlocks=tmp.mem.hdmaBlocks;mem.hdmaBytes=tmp.mem.hdmaBytes;mem.hdmaDot=tmp.mem.hdmaDot;
        mem.dotPhase=tmp.mem.dotPhase;mem.speedSwitchTicks=tmp.mem.speedSwitchTicks;
        mem.eram=std::move(tmp.mem.eram);mem.ie=tmp.mem.ie;mem.divCounter=tmp.mem.divCounter;
        mem.timerReload=tmp.mem.timerReload;mem.serialTicks=tmp.mem.serialTicks;
        mem.dmaTicks=tmp.mem.dmaTicks;mem.dmaPos=tmp.mem.dmaPos;mem.dmaSource=tmp.mem.dmaSource;
        mem.ppuMode=tmp.mem.ppuMode;mem.statLine=tmp.mem.statLine;mem.ramEnabled=tmp.mem.ramEnabled;
        mem.romBank=tmp.mem.romBank;mem.bankHigh=tmp.mem.bankHigh;mem.ramBank=tmp.mem.ramBank;
        mem.mbcMode=tmp.mem.mbcMode;mem.latchWrite=tmp.mem.latchWrite;mem.rtc=tmp.mem.rtc;
        mem.latchedRTC=tmp.mem.latchedRTC;mem.soundTrigger=tmp.mem.soundTrigger;
        mem.rtcLatched=tmp.mem.rtcLatched;mem.rtcCycles=tmp.mem.rtcCycles;
        mem.cgbMode=tmp.mem.cgbMode;mem.cgbHardware=tmp.mem.cgbHardware;mem.dmgCompatibility=tmp.mem.dmgCompatibility;
        mem.rumbleActive=tmp.mem.rumbleActive;mem.hucIRMode=tmp.mem.hucIRMode;mem.hucIRLight=tmp.mem.hucIRLight;
        mem.motion=tmp.mem.motion;mem.serialBits=tmp.mem.serialBits;
        mem.cameraRegisters=tmp.mem.cameraRegisters;mem.cameraTicks=tmp.mem.cameraTicks;mem.cameraPending=tmp.mem.cameraPending;
        cpu.A=tmp.cpu.A;cpu.F=tmp.cpu.F;cpu.B=tmp.cpu.B;cpu.C=tmp.cpu.C;
        cpu.D=tmp.cpu.D;cpu.E=tmp.cpu.E;cpu.H=tmp.cpu.H;cpu.L=tmp.cpu.L;
        cpu.SP=tmp.cpu.SP;cpu.PC=tmp.cpu.PC;cpu.IME=tmp.cpu.IME;
        cpu.locked=tmp.cpu.locked;cpu.elapsed=cpu.instructionElapsed=0;
        cpu.halted=tmp.cpu.halted;cpu.stopped=tmp.cpu.stopped;cpu.haltBug=tmp.cpu.haltBug;cpu.imeDelay=tmp.cpu.imeDelay;
        mem.cpuHalted=cpu.halted;
        ppu.dot=tmp.ppu.dot;ppu.lcdWasOn=tmp.ppu.lcdWasOn;ppu.pixels=tmp.ppu.pixels;
        ppu.drawing=tmp.ppu.drawing;ppu.state=tmp.ppu.state;ppu.frameNumber=tmp.ppu.frameNumber;
        mem.pendingDmaSource=tmp.mem.pendingDmaSource;mem.dmaDelay=tmp.mem.dmaDelay;mem.timerJustReloaded=tmp.mem.timerJustReloaded;
        mem.serialSent=tmp.mem.serialSent;mem.ppuStatMode=tmp.mem.ppuStatMode;
        mem.ppuCompareLY=tmp.mem.ppuCompareLY;mem.ppuDot=ppu.dot;
        frameDebt=tmp.frameDebt;totalCycles=tmp.totalCycles;apu.state=tmp.apu.state;apu.sampleRate=tmp.apu.sampleRate;apu.clearQueue();apu.updateCapacitor();
        mem.serialOutput.clear();setPalette(tmp.currentPalette);return true;
    }
};
