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
    CHECK(m.ppuMode==2);
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
    CHECK(g.cpu.PC>=0x193&&g.cpu.PC<0x1C8);
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
int main(int argc,char** argv){
    testCartridges();testTimer();testPpu();testCpuAllDocumented();testCBGroup();testJoypad();
    if(argc>1)testStatesAndDemo(argv[1]);
    std::cout<<"ALL CORE TESTS PASSED\n";
}
