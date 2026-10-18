#include "../src/gameboy.hpp"
#include <cassert>
#include <cstdlib>
#define CHECK(expr) do { if(!(expr)) { std::cerr<<"CHECK FAILED: "<<#expr<<" at "<<__FILE__<<":"<<__LINE__<<"\n"; std::abort(); } } while(false)
#include <iostream>
#include <filesystem>
#include <set>

static void testCartridges(){
    const auto path=std::filesystem::temp_directory_path()/"gbstudio-test-cart.gb";
    auto test=[&](int type,int banks,int target,bool ram){
        std::vector<u8> data((std::size_t)banks*0x4000);
        for(int b=0;b<banks;++b)std::fill(data.begin()+b*0x4000,data.begin()+(b+1)*0x4000,(u8)b);
        data[0x147]=(u8)type;data[0x149]=(u8)(ram?3:0);
        {std::ofstream f(path,std::ios::binary);f.write((char*)data.data(),(std::streamsize)data.size());}
        Memory m;CHECK(m.loadROM(path));
        CHECK(m.read(0x4000)==1);
        if(type==1||type==3||type==0x13||type==0x1B){m.write(0x2000,(u8)target);CHECK(m.read(0x4000)==target%banks);}
        if(ram){m.write(0x0000,0x0A);m.write(0xA000,42);CHECK(m.read(0xA000)==42);}
        std::filesystem::remove(path);
    };
    test(0,2,1,false);test(1,8,3,true);test(3,8,3,true);
    test(0x13,8,5,true);test(0x1B,8,5,true);
    // MBC2: 512 x 4-bit built-in RAM and A8 gated bank register.
    std::vector<u8> data(8*0x4000);
    for(int bank=0;bank<8;++bank)std::fill(data.begin()+bank*16384,data.begin()+(bank+1)*16384,(u8)bank);
    data[0x147]=0x06;
    {std::ofstream f(path,std::ios::binary);f.write((char*)data.data(),(std::streamsize)data.size());}
    Memory m;CHECK(m.loadROM(path));m.write(0x2100,3);
    CHECK(m.read(0x4000)==3);
    m.write(0x0000,0x0A);m.write(0xA000,0xAB);
    CHECK(m.read(0xA200)==0xFB);
    std::filesystem::remove(path);

}
static void testSingleBankHomebrew(){
    const auto path=std::filesystem::temp_directory_path()/"gbstudio-single-bank.gb";
    std::vector<u8> image(16384,0);
    image[0x123]=0x5A;image.back()=0xA5;
    Memory memory;
    for(u8 type:{u8(0x00),u8(0x08),u8(0x09)}){
        image[0x147]=type;image[0x149]=type?2:0;
        CHECK(Memory::writeAtomic(path,{(const char*)image.data(),image.size()}));
        CHECK(memory.loadROM(path)&&memory.rom==image&&memory.mbc==0);
        // Legacy unbanked addressing mirrors its sole physical bank.
        CHECK(memory.read(0x0123)==0x5A&&memory.read(0x4123)==0x5A);
        CHECK(memory.read(0x3FFF)==0xA5&&memory.read(0x7FFF)==0xA5);
    }
    const auto previous=memory.rom;
    for(u8 type:{u8(0x01),u8(0x03),u8(0x13),u8(0x1B)}){
        image[0x147]=type;
        CHECK(Memory::writeAtomic(path,{(const char*)image.data(),image.size()}));
        CHECK(!memory.loadROM(path)&&memory.rom==previous);
    }
    image.resize(16383);image[0x147]=0;
    CHECK(Memory::writeAtomic(path,{(const char*)image.data(),image.size()}));
    CHECK(!memory.loadROM(path)&&memory.rom==previous);
    std::filesystem::remove(path);
}
static void testTimer(){
    Memory m;
    m.write(0xFF06,42);m.write(0xFF05,0xFF);m.write(0xFF07,0x05);
    m.tick(16);CHECK(m.read(0xFF05)==0);
    m.tick(4);CHECK(m.read(0xFF05)==42);CHECK(m.read(0xFF0F)&4);
    m.write(0xFF04,123);CHECK(m.read(0xFF04)==0);
}
static void testPpu(){
    Memory m;PPU p(m);p.tick(456*144);
    CHECK(m.read(0xFF44)==144);CHECK(m.read(0xFF0F)&1);
    p.tick(456*10);CHECK(m.read(0xFF44)==0);
    CHECK(m.ppuStatMode==2);
    p.tick(4);CHECK(m.ppuMode==2);
}
static void testCpuAllDocumented(){
    const std::set<int> illegal={0xD3,0xDB,0xDD,0xE3,0xE4,0xEB,0xEC,0xED,0xF4,0xFC,0xFD};
    int passed=0;
    for(int op=0;op<256;++op){
        if(illegal.contains(op))continue;
        Memory m;CPU c(m);
        c.PC=0xC000;c.SP=0xD000;
        m.wram[0]=(u8)op;m.wram[1]=0;m.wram[2]=0;
        c.step();
        if(op!=0x76 && op!=0x10 && c.halted){
            std::cerr<<"Unimplemented valid opcode 0x"<<std::hex<<op<<std::dec<<"\n";std::abort();
        }
        ++passed;
    }
    CHECK(passed==245);
}
static void testCBGroup(){
    for(int op=0;op<256;++op){
        Memory m;CPU c(m);c.PC=0xC000;c.SP=0xD000;
        c.setHL(0xC100);m.wram[0x100]=0xAA;
        m.wram[0]=0xCB;m.wram[1]=(u8)op;
        const int expected=(op&7)==6 ? (op>=0x40&&op<0x80 ? 12 : 16) : 8;
        CHECK(c.step()==expected);
    }
}
static void testJoypad(){
    Memory m;
    const u8 old=m.joypadRead();
    m.write(0xFF00,0x20); // select dpad
    m.right=true;m.joyChanged(old);
    CHECK((m.joypadRead()&1)==0);
    CHECK(m.io[0x0F]&0x10);
}
static void testStatesAndDemo(const std::filesystem::path& demo){
    GameBoy g;CHECK(g.load(demo));
    for(int i=0;i<60;++i)g.runFrame();
    CHECK(g.mem.oam[1]==80 && g.mem.oam[0]==96);
    CHECK(g.cpu.PC>=0x150&&g.cpu.PC<0x300 && !g.cpu.locked);
    const u32 px=g.ppu.pixels[83*160+74];
    CHECK(px==PPU::palettes[0][3]);
    CHECK(g.saveState());
    const u16 pc=g.cpu.PC;
    const auto oam=g.mem.oam;
    g.mem.right=true;
    for(int i=0;i<10;++i)g.runFrame();
    CHECK(g.mem.oam[1]!=80);
    CHECK(g.loadState());
    CHECK(g.cpu.PC==pc && g.mem.oam==oam);
    CHECK(g.ppu.pixels[83*160+74]==px);
    std::filesystem::remove(g.statePath());
}
static void testInterruptSequence(){
    GameBoy g;
    g.cpu.PC=0xC000;g.mem.ie=1;g.mem.io[0x0F]=1;
    g.mem.wram[0]=0xFB;g.mem.wram[1]=0;g.mem.wram[2]=0;
    CHECK(g.cpu.step()==4&&!g.cpu.IME);
    CHECK(g.cpu.step()==4&&g.cpu.IME&&g.cpu.PC==0xC002);
    CHECK(g.cpu.step()==20&&g.cpu.PC==0x40);
    CHECK(g.mem.read(0xFFFC)==2&&g.mem.read(0xFFFD)==0xC0);
    g.reset();g.cpu.PC=0xC000;g.mem.ie=1;g.mem.io[0x0F]=1;
    g.mem.wram[0]=0x76;g.mem.wram[1]=0x3E;g.mem.wram[2]=0x12;
    g.cpu.step();CHECK(g.cpu.haltBug&&!g.cpu.halted);
    g.cpu.step();CHECK(g.cpu.A==0x3E&&g.cpu.PC==0xC002);
    g.reset();g.cpu.PC=0xC000;g.mem.io[0x0F]=0;
    g.mem.wram[0]=0x10;g.mem.wram[1]=0;g.mem.wram[2]=0;
    g.cpu.step();CHECK(g.cpu.stopped&&g.mem.divCounter==0);
    const int dot=g.ppu.dot;g.cpu.step();CHECK(g.mem.divCounter==0&&g.ppu.dot==dot);
    g.mem.write(0xFF00,0x20);const u8 before=g.mem.joypadRead();g.mem.right=true;g.mem.joyChanged(before);
    g.cpu.step();CHECK(!g.cpu.stopped&&g.cpu.PC==0xC003);
}
static void testVideoBusAndDma(){
    GameBoy g;g.mem.write(0xFF40,0);
    g.mem.vram[0]=0x35;g.mem.wram[0]=0x42;g.mem.oam[0]=0x19;
    g.mem.write(0xFF46,0x80);
    g.tick(4);CHECK(g.mem.cpuRead(0xFE00)==0x19);
    g.tick(4);CHECK(g.mem.cpuRead(0xFE00)==0xFF);
    CHECK(g.mem.cpuRead(0xC000)==0x42&&g.mem.cpuRead(0x8000)==0xFF);
    CHECK(g.mem.cpuRead(0xFF46)==0x80);
    g.tick(640);CHECK(g.mem.oam[0]==0x35&&g.mem.dmaPos==160);
    g.mem.write(0xFF40,0x91);g.tick(80);
    CHECK(g.mem.ppuMode==3&&g.mem.cpuRead(0x8000)==0xFF);
    g.mem.cpuWrite(0x8000,0xAA);CHECK(g.mem.vram[0]==0x35);
}
static void testFrameClockAndAudio(){
    GameBoy g;
    g.mem.rom.assign(32768,0);
    g.mem.rom[0x100]=0xC6;g.mem.rom[0x101]=1;
    g.mem.rom[0x102]=0x18;g.mem.rom[0x103]=0xFC;
    for(u64 n=1;n<=100;++n){g.runFrame();CHECK(g.totalCycles>=n*GB_FRAME&&g.totalCycles<n*GB_FRAME+24);}
    g.reset();g.mem.write(0xFF26,0x80);g.mem.write(0xFF16,0x80);
    g.mem.write(0xFF17,0xF0);g.mem.write(0xFF18,0x80);g.mem.write(0xFF19,0x87);
    g.mem.write(0xFF24,0x77);g.mem.write(0xFF25,0x22);
    g.tick(GB_FRAME);
    CHECK(g.apu.available()==(u64)GB_FRAME*APU::rate/GB_CLOCK);
    std::vector<std::int16_t> sound(g.apu.available()*2);g.apu.samples(sound.data(),(int)sound.size()/2);
    CHECK(std::any_of(sound.begin(),sound.end(),[](auto x){return x!=0;}));
    CHECK(g.apu.available()==0);
}
static void testRtc(){
    Memory m;m.hasRTC=true;m.rtc={59,59,23,255,1};m.advanceRTC(1);
    CHECK(m.rtc[0]==0&&m.rtc[1]==0&&m.rtc[2]==0&&m.rtc[3]==0&&m.rtc[4]==0x80);
    m.rtc[4]|=0x40;m.advanceRTC(3600);CHECK(m.rtc[2]==0);
    m.rtc[4]&=~0x40;m.advanceRTC(86400*30+3723);
    CHECK(m.rtc[3]==30&&m.rtc[2]==1&&m.rtc[1]==2&&m.rtc[0]==3);
}
static void testStateIntegrity(const std::filesystem::path& demo){
    const auto original=Memory::loadBytes(demo);
    const auto temp=std::filesystem::temp_directory_path()/"gbstudio-state-test.gb";
    CHECK(Memory::writeAtomic(temp,std::string_view((const char*)original.data(),original.size())));
    GameBoy g;CHECK(g.load(temp));for(int i=0;i<5;++i)g.runFrame();
    CHECK(g.saveState());const auto initial=Memory::loadBytes(g.statePath());
    for(int i=0;i<17;++i)g.runFrame();CHECK(g.saveState());
    const auto expected=Memory::loadBytes(g.statePath());
    CHECK(Memory::writeAtomic(g.statePath(),std::string_view((const char*)initial.data(),initial.size())));
    CHECK(g.loadState());for(int i=0;i<17;++i)g.runFrame();CHECK(g.saveState());
    CHECK(Memory::loadBytes(g.statePath())==expected);
    auto corrupt=expected;corrupt[100]^=1;
    CHECK(Memory::writeAtomic(g.statePath(),std::string_view((const char*)corrupt.data(),corrupt.size())));
    const auto pc=g.cpu.PC;const auto pixels=g.ppu.pixels;
    CHECK(!g.loadState()&&g.cpu.PC==pc&&g.ppu.pixels==pixels);
    CHECK(Memory::writeAtomic(g.statePath(),std::string_view((const char*)expected.data(),expected.size())));
    g.mem.rom[0]^=1;CHECK(!g.loadState());g.mem.rom[0]^=1;
    g.mem.eram.assign(131072,0x5A);CHECK(g.saveState());
    g.mem.eram[0]=0;CHECK(g.loadState()&&g.mem.eram[0]==0x5A);
    std::filesystem::remove(g.statePath());std::filesystem::remove(temp);
}
int main(int argc,char** argv){
    testCartridges();testSingleBankHomebrew();testTimer();testPpu();testCpuAllDocumented();testCBGroup();testJoypad();
    testInterruptSequence();testVideoBusAndDma();testFrameClockAndAudio();testRtc();
    if(argc>1){testStatesAndDemo(argv[1]);testStateIntegrity(argv[1]);}
    std::cout<<"ALL CORE TESTS PASSED\n";
}
