#pragma once
// GB Studio Beta - Game Boy DMG emulator core. Original project upgraded for Win32.
// No copyrighted ROM or Nintendo boot ROM is included.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using u8 = std::uint8_t;
using s8 = std::int8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
static constexpr int GB_W=160, GB_H=144, GB_CLOCK=4194304, GB_FRAME=70224;

class Memory {
public:
    std::vector<u8> rom, eram;
    std::array<u8,8192> vram{};
    std::array<u8,8192> wram{};
    std::array<u8,160> oam{};
    std::array<u8,128> io{};
    std::array<u8,127> hram{};
    std::array<u32,4> soundTrigger{};
    u8 ie=0;
    u16 divCounter=0;
    int timerReload=-1, serialTicks=0, dmaTicks=0, dmaPos=160;
    u16 dmaSource=0;
    int ppuMode=2;
    bool statLine=false;
    bool right=false,left=false,up=false,down=false,a=false,b=false,select=false,start=false;
    bool ramEnabled=false, hasBattery=false, hasRTC=false, cgbROM=false;
    int mbc=0;
    u16 romBank=1;
    u8 bankHigh=0, ramBank=0, mbcMode=0, latchWrite=0;
    std::array<u8,5> rtc{}, latchedRTC{};
    bool rtcLatched=false;
    u64 rtcCycles=0;
    std::filesystem::path romPath;
    std::string gameTitle;

    Memory() { resetIO(); }
    void resetIO() {
        io.fill(0); hram.fill(0); vram.fill(0); wram.fill(0); oam.fill(0);
        soundTrigger.fill(0); divCounter=0; ie=0; timerReload=-1; serialTicks=0; dmaTicks=0; dmaPos=160;
        ppuMode=2; statLine=false;
        io[0x00]=0xCF; io[0x0F]=0xE1;
        io[0x10]=0x80; io[0x11]=0xBF; io[0x12]=0xF3; io[0x14]=0xBF;
        io[0x16]=0x3F; io[0x19]=0xBF; io[0x1A]=0x7F; io[0x1B]=0xFF;
        io[0x1C]=0x9F; io[0x1E]=0xBF; io[0x20]=0xFF; io[0x23]=0xBF;
        io[0x24]=0x77; io[0x25]=0xF3; io[0x26]=0xF1;
        io[0x40]=0x91; io[0x41]=0x85; io[0x47]=0xFC;
        io[0x48]=0xFF; io[0x49]=0xFF;
    }
    static std::vector<u8> loadBytes(const std::filesystem::path& path) {
        std::ifstream f(path,std::ios::binary);
        return f ? std::vector<u8>(std::istreambuf_iterator<char>(f),{}) : std::vector<u8>{};
    }
    bool loadROM(const std::filesystem::path& path) {
        auto data=loadBytes(path);
        if(data.size()<0x150 || data.size()>8*1024*1024) return false;
        if(data[0x143]==0xC0){std::cerr<<"CGB-only cartridge requires Color support (not implemented).\n";return false;}
        const u8 type=data[0x147];
        int controller=-1;
        switch(type) {
            case 0x00: case 0x08: case 0x09: controller=0;break;
            case 0x01: case 0x02: case 0x03: controller=1;break;
            case 0x05: case 0x06: controller=2;break;
            case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: controller=3;break;
            case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: controller=5;break;
            default: std::cerr << "Unsupported cartridge type: " << std::hex << (int)type << std::dec << '\n';return false;
        }
        rom=std::move(data); romPath=path; mbc=controller;
        romBank=1; bankHigh=0; ramBank=0; mbcMode=0; ramEnabled=(mbc==0);
        hasBattery=(type==0x03 || type==0x06 || type==0x09 || type==0x0F || type==0x10 || type==0x13 || type==0x1B || type==0x1E);
        hasRTC=(type==0x0F || type==0x10); cgbROM=(rom[0x143]&0x80)!=0;
        std::size_t size=0;
        switch(rom[0x149]) { case 1:size=2048;break; case 2:size=8192;break; case 3:size=32768;break;case 4:size=131072;break;case 5:size=65536;break; default:break; }
        if(mbc==2) size=512;
        eram.assign(size,0xFF);
        gameTitle.clear(); for(int i=0x134;i<0x144 && rom[i];++i) { if(rom[i]<32||rom[i]>126)break;gameTitle+=(char)rom[i]; }
        rtc.fill(0);latchedRTC.fill(0);rtcCycles=0;rtcLatched=false;
        loadBattery();
        return true;
    }
    std::size_t banks() const {return std::max<std::size_t>(2,(rom.size()+0x3FFF)/0x4000);}
    std::size_t bank0() const {return mbc==1&&mbcMode ? ((std::size_t)bankHigh<<5)%banks() : 0;}
    std::size_t bankN() const {
        std::size_t n=1;
        if(mbc==0) n=1;
        if(mbc==1) { n=(romBank&31)|((std::size_t)bankHigh<<5); if((n&31)==0)++n; }
        if(mbc==2) n=romBank&15;
        if(mbc==3) n=romBank&127;
        if(mbc==5) n=romBank&511;
        return n%banks();
    }
    std::size_t extBank() const {
        if(mbc==1) return mbcMode ? (bankHigh&3):0;
        if(mbc==3 || mbc==5) return ramBank & (mbc==5?15:7);
        return 0;
    }
    void refreshStat() {
        u8& stat=io[0x41];
        stat=(u8)((stat&0xF8)|(ppuMode&3));
        if(io[0x44]==io[0x45])stat|=4;else stat&=(u8)~4;
        const bool line=((stat&4)&&(stat&0x40)) || (ppuMode==0&&(stat&8)) ||
                        (ppuMode==1&&(stat&0x10)) || (ppuMode==2&&(stat&0x20));
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
        if(addr<0x4000) return rom.empty()?0xFF:rom[(bank0()*16384+addr)%rom.size()];
        if(addr<0x8000) return rom.empty()?0xFF:rom[(bankN()*16384+addr-0x4000)%rom.size()];
        if(addr<0xA000) return vram[addr-0x8000];
        if(addr<0xC000) {
            if(!ramEnabled)return 0xFF;
            if(mbc==3 && ramBank>=8) return hasRTC && ramBank<=12 ? (rtcLatched?latchedRTC:rtc)[ramBank-8] : 0xFF;
            if(eram.empty())return 0xFF;
            std::size_t offset=(extBank()*8192+addr-0xA000)%eram.size();
            return mbc==2 ? (u8)(0xF0|(eram[(addr-0xA000)&511]&15)) : eram[offset];
        }
        if(addr<0xE000)return wram[addr-0xC000];
        if(addr<0xFE00)return wram[addr-0xE000];
        if(addr<0xFEA0)return oam[addr-0xFE00];
        if(addr<0xFF00)return 0xFF;
        if(addr==0xFFFF)return ie;
        if(addr>=0xFF80)return hram[addr-0xFF80];
        switch(addr) {
            case 0xFF00:return joypadRead();
            case 0xFF04:return (u8)(divCounter>>8);
            case 0xFF07:return io[7]|0xF8;
            case 0xFF0F:return io[0x0F]|0xE0;
            case 0xFF41:return io[0x41]|0x80;
            case 0xFF44:return io[0x44];
            case 0xFF26:return (io[0x26]&0x8F)|0x70;
            default:return io[addr-0xFF00];
        }
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
    void rtcSecond() {
        if(!hasRTC || (rtc[4]&0x40))return;
        if(++rtc[0]<60)return;
        rtc[0]=0;
        if(++rtc[1]<60)return;
        rtc[1]=0;
        if(++rtc[2]<24)return;
        rtc[2]=0;
        u16 d=(u16)(rtc[3]|((rtc[4]&1)<<8));++d;
        if(d>=512){d=0;rtc[4]|=0x80;}
        rtc[3]=d&255; rtc[4]=(rtc[4]&0xFE)|((d>>8)&1);
    }
    void write(u16 addr,u8 v) {
        if(addr<0x8000) {
            if(mbc==0)return;
            if(mbc==2) {
                if(addr<0x4000) { if(addr&0x100) {romBank=v&15;if(!romBank)romBank=1;}else ramEnabled=((v&15)==10); }
                return;
            }
            if(addr<0x2000){ramEnabled=(v&15)==10;return;}
            if(addr<0x4000){
                if(mbc==1)romBank=v&31;
                if(mbc==3){romBank=v&127;if(!romBank)romBank=1;}
                if(mbc==5){if(addr<0x3000)romBank=(romBank&0x100)|v;else romBank=(romBank&255)|((v&1)<<8);}
                return;
            }
            if(addr<0x6000){if(mbc==1)bankHigh=v&3;else ramBank=(mbc==5)?(v&15):v;return;}
            if(mbc==1)mbcMode=v&1;
            if(mbc==3){if(latchWrite==0&&v==1){latchedRTC=rtc;rtcLatched=true;} latchWrite=v;}
            return;
        }
        if(addr<0xA000){vram[addr-0x8000]=v;return;}
        if(addr<0xC000){
            if(!ramEnabled)return;
            if(mbc==3&&ramBank>=8){if(hasRTC&&ramBank<=12){int n=ramBank-8;rtc[n]=v;if(n==0)rtc[n]%=60;if(n==1)rtc[n]%=60;if(n==2)rtc[n]%=24;}return;}
            if(eram.empty())return;
            if(mbc==2)eram[(addr-0xA000)&511]=v&15;
            else eram[(extBank()*8192+addr-0xA000)%eram.size()]=v;
            return;
        }
        if(addr<0xE000){wram[addr-0xC000]=v;return;}
        if(addr<0xFE00){wram[addr-0xE000]=v;return;}
        if(addr<0xFEA0){oam[addr-0xFE00]=v;return;}
        if(addr<0xFF00)return;
        if(addr==0xFFFF){ie=v;return;}
        if(addr>=0xFF80){hram[addr-0xFF80]=v;return;}
        switch(addr){
            case 0xFF00:{const u8 before=joypadRead();io[0]=(io[0]&~0x30)|(v&0x30);joyChanged(before);break;}
            case 0xFF04:{const bool old=timerSignal();divCounter=0;if(old&&!timerSignal())advanceTima();break;}
            case 0xFF05:io[5]=v;timerReload=-1;break;
            case 0xFF06:io[6]=v;break;
            case 0xFF07:{const bool old=timerSignal();io[7]=v&7;if(old&&!timerSignal())advanceTima();break;}
            case 0xFF0F:io[0x0F]=v|0xE0;break;
            case 0xFF02:io[2]=v;if(v&0x80)serialTicks=4096;break;
            case 0xFF14:case 0xFF19:case 0xFF1E:case 0xFF23:{io[addr-0xFF00]=v;if(v&0x80){int channel=addr==0xFF14?0:addr==0xFF19?1:addr==0xFF1E?2:3;++soundTrigger[channel];io[0x26]|=(u8)(1<<channel);}break;}
            case 0xFF26:io[0x26]=(io[0x26]&15)|(v&0x80);if(!(v&0x80)){for(int i=0x10;i<=0x25;++i)io[i]=0;io[0x26]=0;}break;
            case 0xFF41:io[0x41]=(io[0x41]&0x87)|(v&0x78);refreshStat();break;
            case 0xFF45:io[0x45]=v;refreshStat();break;
            case 0xFF44:break; // LY is read-only
            case 0xFF46:dmaSource=(u16)(v<<8);dmaPos=0;dmaTicks=0;io[0x46]=v;break;
            default:io[addr-0xFF00]=v;break;
        }
    }
    void tick(int cycles) {
        for(int i=0;i<cycles;++i) {
            if(timerReload>=0){if(--timerReload==0){io[5]=io[6];io[0x0F]|=4;timerReload=-1;}}
            const bool old=timerSignal();++divCounter;if(old&&!timerSignal())advanceTima();
            if(serialTicks>0&&--serialTicks==0){io[2]&=~0x80;io[1]=0xFF;io[0x0F]|=8;}
            if(dmaPos<160 && ++dmaTicks==4){dmaTicks=0;oam[dmaPos]=read((u16)(dmaSource+dmaPos));++dmaPos;}
            if(hasRTC && ++rtcCycles>=GB_CLOCK){rtcCycles-=GB_CLOCK;rtcSecond();}
        }
    }
    std::filesystem::path savePath() const {auto p=romPath;p.replace_extension(".sav");return p;}
    std::filesystem::path rtcPath() const {auto p=romPath;p.replace_extension(".rtc");return p;}
    bool saveBattery() const {
        if(!hasBattery||romPath.empty())return false;
        if(!eram.empty()){
            std::ofstream f(savePath(),std::ios::binary|std::ios::trunc);
            if(!f)return false;
            f.write((const char*)eram.data(),(std::streamsize)eram.size());if(!f)return false;
        }
        if(hasRTC){
            std::ofstream f(rtcPath(),std::ios::binary|std::ios::trunc);
            if(!f)return false;
            f.write("GBRTC01",7); f.write((const char*)rtc.data(),5);
            const auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::int64_t stamp=(std::int64_t)now;f.write((const char*)&stamp,sizeof(stamp));
        }
        return true;
    }
    void loadBattery() {
        if(!hasBattery)return;
        if(!eram.empty()) {auto file=loadBytes(savePath());if(file.size()==eram.size())eram=std::move(file);}
        if(!hasRTC)return;
        std::ifstream f(rtcPath(),std::ios::binary);
        char magic[7]{};std::int64_t stamp=0;
        if(f && f.read(magic,7) && std::string(magic,7)=="GBRTC01" && f.read((char*)rtc.data(),5) && f.read((char*)&stamp,sizeof(stamp))){
            const auto now=(std::int64_t)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            auto elapsed=std::clamp<std::int64_t>(now-stamp,0,86400*365LL);
            if(!(rtc[4]&0x40))for(std::int64_t i=0;i<elapsed;++i)rtcSecond();
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

    u8 fetch8() { const u8 v = m.read(PC); if (haltBug) haltBug = false; else PC = static_cast<u16>(PC + 1); return v; }
    u16 fetch16() { const u16 lo = fetch8(); const u16 hi = fetch8(); return static_cast<u16>(lo | hi << 8); }

    void push16(u16 v) {
        m.write(--SP, static_cast<u8>(v >> 8));
        m.write(--SP, static_cast<u8>(v & 0xFF));
    }
    u16 pop16() {
        const u16 lo = m.read(SP++);
        const u16 hi = m.read(SP++);
        return static_cast<u16>(lo | hi << 8);
    }

    u8 readR8(int r) {
        switch (r & 7) {
            case 0: return B; case 1: return C; case 2: return D; case 3: return E;
            case 4: return H; case 5: return L; case 6: return m.read(HL()); default: return A;
        }
    }
    void writeR8(int r, u8 v) {
        switch (r & 7) {
            case 0: B = v; break; case 1: C = v; break; case 2: D = v; break; case 3: E = v; break;
            case 4: H = v; break; case 5: L = v; break; case 6: m.write(HL(), v); break; default: A = v; break;
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
        F = static_cast<u8>((old + 1 == 0 ? 0x80 : 0) | (Hf ? 0x20 : 0) | (F & 0x10));
    }
    void dec8(u8& x) {
        const u8 old = x--;
        const bool Hf = (old & 0x0F) == 0;
        F = static_cast<u8>((old - 1 == 0 ? 0x80 : 0) | 0x40 | (Hf ? 0x20 : 0) | (F & 0x10));
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
        if(stopped && (m.io[0x0F]&0x10))stopped=false;
        const u8 pending = static_cast<u8>(m.ie & m.io[0x0F] & 0x1F);
        if (!pending) return 0;
        if (!IME) { halted = false; stopped = false; return 0; }
        IME = false;
        halted = false;
        int bit = 0;
        while (((pending >> bit) & 1) == 0) ++bit;
        m.io[0x0F] = static_cast<u8>(m.io[0x0F] & ~(1 << bit));
        push16(PC);
        PC = static_cast<u16>(0x40 + bit * 8);
        return 20;
    }

    int step() {
        const int irqCycles = serviceInterrupt();
        if (irqCycles) return irqCycles;
        if (stopped || halted) return 4;

        if (imeDelay > 0) {
            --imeDelay;
            if (imeDelay == 0) IME = true;
        }

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

            case 0x02: m.write(BC(), A); return 8;
            case 0x12: m.write(DE(), A); return 8;
            case 0x22: m.write(HL(), A); setHL(static_cast<u16>(HL() + 1)); return 8;
            case 0x32: m.write(HL(), A); setHL(static_cast<u16>(HL() - 1)); return 8;
            case 0x0A: A = m.read(BC()); return 8;
            case 0x1A: A = m.read(DE()); return 8;
            case 0x2A: A = m.read(HL()); setHL(static_cast<u16>(HL() + 1)); return 8;
            case 0x3A: A = m.read(HL()); setHL(static_cast<u16>(HL() - 1)); return 8;

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

            case 0x08: { const u16 a = fetch16(); m.write(a, SP & 0xFF); m.write(static_cast<u16>(a + 1), SP >> 8); return 20; }
            case 0x10: fetch8(); stopped = true; return 4;
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

            case 0xCD: { const u16 a = fetch16(); push16(PC); PC = a; return 24; }
            case 0xC4: { const u16 a = fetch16(); if (!z()) { push16(PC); PC = a; return 24; } return 12; }
            case 0xCC: { const u16 a = fetch16(); if (z())  { push16(PC); PC = a; return 24; } return 12; }
            case 0xD4: { const u16 a = fetch16(); if (!c()) { push16(PC); PC = a; return 24; } return 12; }
            case 0xDC: { const u16 a = fetch16(); if (c())  { push16(PC); PC = a; return 24; } return 12; }

            case 0xC9: PC = pop16(); return 16;
            case 0xC0: if (!z()) { PC = pop16(); return 20; } return 8;
            case 0xC8: if (z())  { PC = pop16(); return 20; } return 8;
            case 0xD0: if (!c()) { PC = pop16(); return 20; } return 8;
            case 0xD8: if (c())  { PC = pop16(); return 20; } return 8;
            case 0xD9: PC = pop16(); IME = true; imeDelay = 0; return 16;

            case 0xC1: setBC(pop16()); return 12;
            case 0xD1: setDE(pop16()); return 12;
            case 0xE1: setHL(pop16()); return 12;
            case 0xF1: setAF(pop16()); return 12;
            case 0xC5: push16(BC()); return 16;
            case 0xD5: push16(DE()); return 16;
            case 0xE5: push16(HL()); return 16;
            case 0xF5: push16(AF()); return 16;

            case 0xC7: push16(PC); PC = 0x00; return 16;
            case 0xCF: push16(PC); PC = 0x08; return 16;
            case 0xD7: push16(PC); PC = 0x10; return 16;
            case 0xDF: push16(PC); PC = 0x18; return 16;
            case 0xE7: push16(PC); PC = 0x20; return 16;
            case 0xEF: push16(PC); PC = 0x28; return 16;
            case 0xF7: push16(PC); PC = 0x30; return 16;
            case 0xFF: push16(PC); PC = 0x38; return 16;


            case 0x09: addHL(BC()); return 8;
            case 0x19: addHL(DE()); return 8;
            case 0x29: addHL(HL()); return 8;
            case 0x39: addHL(SP); return 8;


            case 0xEA: { const u16 a = fetch16(); m.write(a, A); return 16; }
            case 0xFA: { const u16 a = fetch16(); A = m.read(a); return 16; }
            case 0xE0: { const u8 a = fetch8(); m.write(static_cast<u16>(0xFF00 | a), A); return 12; }
            case 0xF0: { const u8 a = fetch8(); A = m.read(static_cast<u16>(0xFF00 | a)); return 12; }
            case 0xE2: m.write(static_cast<u16>(0xFF00 | C), A); return 8;
            case 0xF2: A = m.read(static_cast<u16>(0xFF00 | C)); return 8;

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
            case 0xFB: imeDelay = 2; return 4;

            default:
                std::cerr << "Unimplemented opcode 0x" << std::hex << int(op) << " at PC=0x" << int(static_cast<u16>(PC - 1)) << std::dec << "\n";
                halted = true;
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
    std::array<u32,GB_W*GB_H> pixels{};
    int dot=0, palette=0;
    bool lcdWasOn=true;
    static constexpr std::array<std::array<u32,4>,4> palettes{{
        {{0xFF9BBD0F,0xFF8BAC0F,0xFF306230,0xFF0F380F}}, // Classic green, dark to light
        {{0xFFE9F0EC,0xFFAFBEC0,0xFF586B70,0xFF182022}}, // Grey
        {{0xFFDCEAF6,0xFF8CA7C2,0xFF3E6991,0xFF192F4A}}, // Blue
        {{0xFFFFE8B8,0xFFD6A078,0xFF89506B,0xFF34172A}}  // Amber
    }};
    explicit PPU(Memory& mem):m(mem){pixels.fill(palettes[0][0]);syncStat();}
    u32 shade(int value)const{return palettes[palette&3][value&3];}
    int tileAt(bool win,int px,int py)const {
        const u8 lcdc=m.io[0x40];
        int mapBase=(win ? (lcdc&0x40):(lcdc&0x08)) ? 0x1C00:0x1800;
        int tx=(px&255)/8,ty=(py&255)/8;
        const u8 t=m.vram[mapBase+ty*32+tx];
        int offset=(lcdc&0x10)?(int)t*16:(0x1000+(int)(s8)t*16);
        int row=py&7,bit=7-(px&7);
        const u8 low=m.vram[offset+row*2],high=m.vram[offset+row*2+1];
        return ((high>>bit)&1)*2+((low>>bit)&1);
    }
    void renderLine(int y) {
        if(y<0||y>=GB_H)return;
        const u8 lcdc=m.io[0x40],bgp=m.io[0x47];
        const int wx=(int)m.io[0x4B]-7,wy=(int)m.io[0x4A];
        std::array<u8,GB_W> bg{};
        const bool bgOn=lcdc&1, winOn=bgOn&&(lcdc&0x20)&&y>=wy&&wx<GB_W;
        for(int x=0;x<GB_W;++x){
            const bool win=winOn&&x>=wx;
            const int raw=bgOn?(win?tileAt(true,x-wx,y-wy):tileAt(false,x+m.io[0x43],y+m.io[0x42])):0;
            bg[x]=(u8)raw;
            pixels[y*GB_W+x]=shade((bgp>>(raw*2))&3);
        }
        if(!(lcdc&2))return;
        const int sprH=(lcdc&4)?16:8;
        struct Sprite {int id,x,y;};
        std::vector<Sprite> visible;visible.reserve(10);
        for(int id=0;id<40;++id){
            const int y0=(int)m.oam[id*4]-16;
            if(y>=y0&&y<y0+sprH){visible.push_back({id,(int)m.oam[id*4+1]-8,y0});if(visible.size()==10)break;}
        }
        // DMG: sprite with smaller X wins; for equal X smaller OAM index wins.
        std::sort(visible.begin(),visible.end(),[](const Sprite& a,const Sprite& b){
            return a.x==b.x ? a.id>b.id : a.x>b.x;
        });
        for(int k=0;k<(int)visible.size();++k){
            const auto [id,sx,sy]=visible[k];
            int tile=m.oam[id*4+2];u8 attr=m.oam[id*4+3];
            int yy=y-sy;if(attr&0x40)yy=sprH-1-yy;
            if(sprH==16)tile&=0xFE;
            tile+=yy/8;
            const int off=tile*16+(yy&7)*2;
            const u8 lo=m.vram[off],hi=m.vram[off+1];
            const u8 pal=m.io[(attr&0x10)?0x49:0x48];
            for(int px=0;px<8;++px){
                int x=sx+px;if(x<0||x>=GB_W)continue;
                const int bit=(attr&0x20)?px:7-px;
                int raw=((hi>>bit)&1)*2+((lo>>bit)&1);
                if(!raw || ((attr&0x80) && bg[x]))continue;
                pixels[y*GB_W+x]=shade((pal>>(2*raw))&3);
            }
        }
    }
    void syncStat(){m.refreshStat();}
    void tick(int cycles){
        if(!(m.io[0x40]&0x80)){
            if(lcdWasOn){dot=0;m.io[0x44]=0;m.ppuMode=0;m.statLine=false;
                pixels.fill(shade(0));syncStat();lcdWasOn=false;}
            return;
        }
        if(!lcdWasOn){lcdWasOn=true;dot=0;m.io[0x44]=0;m.ppuMode=2;syncStat();}
        for(int t=0;t<cycles;++t){
            ++dot;
            if(m.io[0x44]<144){
                if(dot==80){m.ppuMode=3;syncStat();}
                else if(dot==252){renderLine(m.io[0x44]);m.ppuMode=0;syncStat();}
            }
            if(dot>=456){
                dot=0;u8 ly=++m.io[0x44];
                if(ly==144){m.io[0x0F]|=1;m.ppuMode=1;}
                else if(ly==154){m.io[0x44]=0;m.ppuMode=2;}
                else if(ly<144)m.ppuMode=2;
                syncStat();
            }
        }
    }
};

// Deliberately approximate APU: four channels, basic pulse/wave/noise, NOT cycle-accurate.
class APU {
public:
    Memory& m;
    static constexpr int rate=44100;
    std::array<double,4> phase{};
    std::array<u32,4> seen{};
    std::array<double,4> envelopeTime{};
    std::array<int,4> envelopeVol{};
    std::array<bool,4> active{};
    u16 lfsr=0x7FFF;
    explicit APU(Memory& memory):m(memory){}
    void reset(){phase.fill(0);seen.fill(0);active.fill(false);envelopeTime.fill(0);envelopeVol.fill(0);lfsr=0x7FFF;}
    void samples(std::int16_t* stereo,int frames){
        static constexpr std::array<std::array<int,8>,4> duties{{{{0,0,0,0,0,0,0,1}},{{1,0,0,0,0,0,0,1}},{{1,0,0,0,0,1,1,1}},{{0,1,1,1,1,1,1,0}}}};
        for(int j=0;j<frames;++j){
            double ch[4]{};
            if(m.io[0x26]&0x80){
                for(int c=0;c<4;++c){
                    if(seen[c]!=m.soundTrigger[c]){
                        seen[c]=m.soundTrigger[c];active[c]=true;phase[c]=0;
                        int envReg=(c==0)?0x12:(c==1)?0x17:0x21;
                        envelopeVol[c]=(m.io[envReg]>>4)&15;envelopeTime[c]=0;
                        if(c==3)lfsr=0x7FFF;
                    }
                    if(!active[c])continue;
                    if(c==0||c==1){
                        int reg=c==0?0x13:0x18, hi=c==0?0x14:0x19;
                        const int period=m.io[reg]|((m.io[hi]&7)<<8);
                        const double hz=131072.0/std::max(1,2048-period);
                        phase[c]+=hz/rate;phase[c]-=std::floor(phase[c]);
                        const int duty=(m.io[c==0?0x11:0x16]>>6)&3;
                        ch[c]=(duties[duty][std::min(7,(int)(phase[c]*8))]?1.0:-1.0)*envelopeVol[c]/15.0;
                    } else if(c==2){
                        if(!(m.io[0x1A]&0x80))continue;
                        int period=m.io[0x1D]|((m.io[0x1E]&7)<<8);
                        const double hz=65536.0/std::max(1,2048-period);
                        phase[c]+=hz/rate;phase[c]-=std::floor(phase[c]);
                        const int ix=std::min(31,(int)(phase[c]*32));
                        const u8 byte=m.io[0x30+ix/2];
                        const int nibble=(ix&1)?(byte&15):(byte>>4);
                        const int vol=(m.io[0x1C]>>5)&3;
                        const double scale=vol==0?0.0:vol==1?1.0:vol==2?0.5:0.25;
                        ch[c]=((nibble/7.5)-1.0)*scale;
                    }else{
                        const int nr43=m.io[0x22];
                        static constexpr int divisors[8]={8,16,32,48,64,80,96,112};
                        const double hz=(double)524288.0/divisors[nr43&7]/(1<<(nr43>>4));
                        phase[c]+=hz/rate;
                        while(phase[c]>=1.0){phase[c]-=1.0;int bit=(lfsr^(lfsr>>1))&1;lfsr=(lfsr>>1)|(bit<<14);if(nr43&8)lfsr=(lfsr&~(1<<6))|(bit<<6);}
                        ch[c]=((lfsr&1)?-1.0:1.0)*envelopeVol[c]/15.0;
                    }
                    if(c!=2){
                        const int envReg=c==0?0x12:c==1?0x17:0x21;
                        const int env=m.io[envReg],pace=env&7;
                        if(pace && (envelopeTime[c]+=1.0/rate)>=pace/64.0){
                            envelopeTime[c]-=pace/64.0;
                            envelopeVol[c]=std::clamp(envelopeVol[c]+((env&8)?1:-1),0,15);
                        }
                    }
                }
            }
            int l=0,r=0;for(int c=0;c<4;++c){if(m.io[0x25]&(1<<(c+4)))l+= (int)(ch[c]*2000);if(m.io[0x25]&(1<<c))r+=(int)(ch[c]*2000);}
            l=l*((m.io[0x24]>>4&7)+1)/8;r=r*((m.io[0x24]&7)+1)/8;
            stereo[j*2]=(std::int16_t)std::clamp(l,-32000,32000);
            stereo[j*2+1]=(std::int16_t)std::clamp(r,-32000,32000);
        }
    }
};

class GameBoy {
public:
    Memory mem;
    CPU cpu;
    PPU ppu;
    APU apu;
    int currentPalette=0;
    GameBoy():cpu(mem),ppu(mem),apu(mem){reset();}
    void reset(){
        mem.resetIO();
        mem.romBank=1;mem.bankHigh=0;mem.ramBank=0;mem.mbcMode=0;mem.ramEnabled=mem.mbc==0;
        cpu.setAF(0x01B0);cpu.setBC(0x0013);cpu.setDE(0x00D8);cpu.setHL(0x014D);
        cpu.SP=0xFFFE;cpu.PC=0x100;cpu.IME=false;cpu.imeDelay=0;
        cpu.halted=false;cpu.stopped=false;cpu.haltBug=false;
        ppu.dot=0;ppu.lcdWasOn=true;ppu.palette=currentPalette;
        ppu.pixels.fill(ppu.shade(0));ppu.syncStat();apu.reset();
    }
    bool load(const std::filesystem::path& path){
        mem.saveBattery();
        if(!mem.loadROM(path))return false;
        reset();return true;
    }
    void runFrame(){
        int t=0;
        while(t<GB_FRAME){
            const int c=cpu.step();
            mem.tick(c);ppu.tick(c);t+=c;
        }
    }
    void setPalette(int p){currentPalette=(p+4)%4;ppu.palette=currentPalette;}
    u32 romHash() const {u32 h=2166136261u;for(u8 x:mem.rom){h^=x;h*=16777619u;}return h;}
    std::filesystem::path statePath()const{auto p=mem.romPath;p.replace_extension(".state");return p;}
    bool saveState()const{
        if(mem.rom.empty())return false;
        std::ofstream f(statePath(),std::ios::binary|std::ios::trunc);if(!f)return false;
        auto put=[&](auto v){f.write((const char*)&v,sizeof(v));};
        f.write("GBSTATE2",8);put(romHash());
        for(u8 v:{cpu.A,cpu.F,cpu.B,cpu.C,cpu.D,cpu.E,cpu.H,cpu.L})put(v);
        put(cpu.SP);put(cpu.PC);put((u8)cpu.IME);put((u8)cpu.halted);put((u8)cpu.stopped);put((u8)cpu.haltBug);put((std::int32_t)cpu.imeDelay);
        put(mem.ie);put(mem.divCounter);put((std::int32_t)mem.timerReload);
        put((std::int32_t)mem.serialTicks);put((std::int32_t)mem.dmaTicks);put((std::int32_t)mem.dmaPos);put(mem.dmaSource);
        put((std::int32_t)mem.ppuMode);put((u8)mem.statLine);put((u8)mem.ramEnabled);
        put(mem.romBank);put(mem.bankHigh);put(mem.ramBank);put(mem.mbcMode);put(mem.latchWrite);
        put((u8)mem.rtcLatched);put(mem.rtcCycles);
        auto bytes=[&](const auto& arr){if(!arr.empty())f.write((const char*)arr.data(),(std::streamsize)arr.size()*sizeof(arr[0]));};
        bytes(mem.vram);bytes(mem.wram);bytes(mem.oam);bytes(mem.io);bytes(mem.hram);
        bytes(mem.rtc);bytes(mem.latchedRTC);bytes(mem.soundTrigger);
        std::uint32_t ext=(u32)mem.eram.size();put(ext);bytes(mem.eram);
        put((std::int32_t)ppu.dot);put((u8)ppu.lcdWasOn);put((std::int32_t)currentPalette);
        f.write((const char*)ppu.pixels.data(),(std::streamsize)ppu.pixels.size()*sizeof(u32));
        return (bool)f;
    }
    bool loadState(){
        if(mem.rom.empty())return false;
        // Reject truncated/wrong-version/different-ROM states before mutating live objects.
        const auto bytes=Memory::loadBytes(statePath());
        if(bytes.size()<120 || bytes.size()>200000 || mem.eram.size()>131072)return false;
        GameBoy tmp;
        tmp.mem.rom=mem.rom;tmp.mem.eram=mem.eram;tmp.mem.romPath=mem.romPath;
        tmp.mem.mbc=mem.mbc;tmp.mem.hasBattery=mem.hasBattery;tmp.mem.hasRTC=mem.hasRTC;tmp.mem.cgbROM=mem.cgbROM;
        std::size_t off=0;
        auto pull=[&](auto& val)->bool {if(off+sizeof(val)>bytes.size())return false;std::memcpy(&val,bytes.data()+off,sizeof(val));off+=sizeof(val);return true;};
        auto array=[&](auto& arr)->bool {const auto n=arr.size()*sizeof(arr[0]);if(off+n>bytes.size())return false;if(n)std::memcpy(arr.data(),bytes.data()+off,n);off+=n;return true;};
        if(std::string((const char*)bytes.data(),8)!="GBSTATE2")return false;off=8;
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
        if(!array(tmp.mem.vram)||!array(tmp.mem.wram)||!array(tmp.mem.oam)||!array(tmp.mem.io)||!array(tmp.mem.hram))return false;
        if(!array(tmp.mem.rtc)||!array(tmp.mem.latchedRTC)||!array(tmp.mem.soundTrigger))return false;
        u32 ext=0;if(!pull(ext)||ext!=mem.eram.size()||!array(tmp.mem.eram))return false;
        if(!pull(n))return false;tmp.ppu.dot=n;
        if(!pull(flag))return false;tmp.ppu.lcdWasOn=flag!=0;
        if(!pull(n)||n<0||n>3)return false;tmp.currentPalette=n;tmp.ppu.palette=n;
        const auto len=tmp.ppu.pixels.size()*sizeof(u32);
        if(off+len!=bytes.size()||tmp.ppu.dot<0||tmp.ppu.dot>=456||tmp.mem.ppuMode<0||tmp.mem.ppuMode>3||tmp.cpu.imeDelay<0||tmp.cpu.imeDelay>2)return false;
        std::memcpy(tmp.ppu.pixels.data(),bytes.data()+off,len);
        // References in CPU/PPU/APU must remain bound to this GameBoy's Memory.
        mem.vram=tmp.mem.vram;mem.wram=tmp.mem.wram;mem.oam=tmp.mem.oam;mem.io=tmp.mem.io;mem.hram=tmp.mem.hram;
        mem.eram=std::move(tmp.mem.eram);mem.ie=tmp.mem.ie;mem.divCounter=tmp.mem.divCounter;
        mem.timerReload=tmp.mem.timerReload;mem.serialTicks=tmp.mem.serialTicks;
        mem.dmaTicks=tmp.mem.dmaTicks;mem.dmaPos=tmp.mem.dmaPos;mem.dmaSource=tmp.mem.dmaSource;
        mem.ppuMode=tmp.mem.ppuMode;mem.statLine=tmp.mem.statLine;mem.ramEnabled=tmp.mem.ramEnabled;
        mem.romBank=tmp.mem.romBank;mem.bankHigh=tmp.mem.bankHigh;mem.ramBank=tmp.mem.ramBank;
        mem.mbcMode=tmp.mem.mbcMode;mem.latchWrite=tmp.mem.latchWrite;mem.rtc=tmp.mem.rtc;
        mem.latchedRTC=tmp.mem.latchedRTC;mem.soundTrigger=tmp.mem.soundTrigger;
        mem.rtcLatched=tmp.mem.rtcLatched;mem.rtcCycles=tmp.mem.rtcCycles;
        cpu.A=tmp.cpu.A;cpu.F=tmp.cpu.F;cpu.B=tmp.cpu.B;cpu.C=tmp.cpu.C;
        cpu.D=tmp.cpu.D;cpu.E=tmp.cpu.E;cpu.H=tmp.cpu.H;cpu.L=tmp.cpu.L;
        cpu.SP=tmp.cpu.SP;cpu.PC=tmp.cpu.PC;cpu.IME=tmp.cpu.IME;
        cpu.halted=tmp.cpu.halted;cpu.stopped=tmp.cpu.stopped;cpu.haltBug=tmp.cpu.haltBug;cpu.imeDelay=tmp.cpu.imeDelay;
        ppu.dot=tmp.ppu.dot;ppu.lcdWasOn=tmp.ppu.lcdWasOn;ppu.pixels=tmp.ppu.pixels;
        setPalette(tmp.currentPalette);apu.reset();return true;
    }
};
