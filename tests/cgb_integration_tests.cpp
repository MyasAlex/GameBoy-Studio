#include "../src/gameboy.hpp"
#include <cstdlib>
#include <iostream>
#include <set>

#define CHECK(expr) do { if(!(expr)) { std::cerr << "CHECK FAILED: " << #expr << " at " << __FILE__ << ":" << __LINE__ << '\n'; std::abort(); } } while(false)

static void colorMode(GameBoy& gb){
    gb.mem.cgbROM=true;gb.reset();
    CHECK(gb.mem.cgbMode);
    gb.mem.write(0xFF40,0);
}

static void testBankedMemory(){
    GameBoy gb;colorMode(gb);auto& m=gb.mem;
    CHECK(m.read(0xFF4F)==0xFE);
    m.write(0x8000,0x12);m.write(0xFF4F,1);m.write(0x8000,0x34);
    CHECK(m.read(0xFF4F)==0xFF&&m.read(0x8000)==0x34);
    m.write(0xFF4F,0);CHECK(m.read(0x8000)==0x12);
    m.write(0xC020,0xA5);CHECK(m.read(0xE020)==0xA5);
    for(u8 bank=1;bank<8;++bank){m.write(0xFF70,bank);m.write(0xD020,(u8)(0x80+bank));}
    for(u8 bank=1;bank<8;++bank){
        m.write(0xFF70,bank);CHECK(m.read(0xD020)==0x80+bank);
        CHECK(m.read(0xF020)==0x80+bank&&m.read(0xC020)==0xA5);
    }
    m.write(0xFF70,0);CHECK(m.read(0xD020)==0x81);
    m.write(0xF020,0x55);m.write(0xFF70,1);CHECK(m.read(0xD020)==0x55);
    GameBoy dmg;dmg.mem.write(0xFF40,0);
    dmg.mem.write(0xFF4F,1);dmg.mem.write(0xFF70,7);
    CHECK(dmg.mem.read(0xFF4F)==0xFF&&dmg.mem.read(0xFF70)==0xFF);
}

static void testPalettePorts(){
    GameBoy gb;colorMode(gb);auto& m=gb.mem;
    for(u16 index:{(u16)0xFF68,(u16)0xFF6A}){
        m.write(index,0x80);
        for(int i=0;i<64;++i)m.write((u16)(index+1),(u8)(i*3));
        CHECK((m.read(index)&0xBF)==0x80);
        for(int i=0;i<64;++i){m.write(index,(u8)i);CHECK(m.read((u16)(index+1))==(u8)(i*3));}
    }
    // On CGB the auto-increment still occurs on blocked mode-3 writes.
    m.write(0xFF68,0x80);m.write(0xFF40,0x91);m.ppuMode=3;
    CHECK(m.read(0xFF69)==0xFF);m.write(0xFF69,0x7A);
    CHECK((m.read(0xFF68)&0xBF)==0x81);
    m.write(0xFF40,0);m.write(0xFF68,0);CHECK(m.read(0xFF69)==0);
}

static void setupDma(Memory& m,u16 source,u16 destination){
    m.write(0xFF51,(u8)(source>>8));m.write(0xFF52,(u8)source);
    m.write(0xFF53,(u8)(destination>>8));m.write(0xFF54,(u8)destination);
}

static void testGeneralDma(){
    GameBoy gb;colorMode(gb);auto& m=gb.mem;
    for(int i=0;i<32;++i)m.write((u16)(0xC000+i),(u8)(i^0xA5));
    m.write(0xFF4F,1);setupDma(m,0xC003,0x80A7);m.write(0xFF55,1);
    CHECK(m.cpuDmaStalled());gb.tick(64);CHECK(m.read(0xFF55)==0xFF&&!m.cpuDmaStalled());
    for(int i=0;i<32;++i)CHECK(m.read((u16)(0x80A0+i))==(u8)(i^0xA5));
    m.write(0xFF4F,0);CHECK(m.read(0x80A0)==0);
    // 32 base dots per block are independent of double-speed CPU clocks.
    m.doubleSpeed=true;m.write(0xFF4F,1);setupDma(m,0xC000,0x8100);m.write(0xFF55,0);
    gb.tick(32);CHECK(m.cpuDmaStalled());gb.tick(32);CHECK(!m.cpuDmaStalled());
    for(int i=0;i<16;++i)CHECK(m.read((u16)(0x8100+i))==(u8)(i^0xA5));
}

static void testHblankDma(){
    GameBoy gb;colorMode(gb);auto& m=gb.mem;
    for(int i=0;i<32;++i)m.write((u16)(0xC100+i),(u8)(0x20+i));
    m.write(0xFF40,0x91);m.ppuMode=2;setupDma(m,0xC100,0x8200);m.write(0xFF55,0x81);
    CHECK(!m.cpuDmaStalled()&&m.read(0xFF55)==1);
    m.hblankDMA();CHECK(m.cpuDmaStalled());gb.tick(32);
    CHECK(!m.cpuDmaStalled()&&m.read(0xFF55)==0);
    m.write(0xFF40,0); // LCD shutdown turns the remaining transfer into GDMA.
    CHECK(m.cpuDmaStalled());gb.tick(32);CHECK(m.read(0xFF55)==0xFF);
    for(int i=0;i<32;++i)CHECK(m.read((u16)(0x8200+i))==(u8)(0x20+i));
    m.write(0xFF40,0x91);setupDma(m,0xC100,0x8300);m.write(0xFF55,0x81);
    m.hblankDMA();gb.tick(32);m.write(0xFF55,0);
    CHECK(!m.hdmaActive&&(m.read(0xFF55)&0x80));
    CHECK(m.read(0x8300)==0x20&&m.read(0x8310)==0);
}

static void testSpeedAndFrame(){
    GameBoy gb;colorMode(gb);auto& m=gb.mem;
    gb.cpu.PC=0xC000;m.write(0xC000,0x10);m.write(0xC001,0);
    m.write(0xFF4D,1);CHECK(m.read(0xFF4D)==0x7F);
    gb.cpu.step();CHECK(m.doubleSpeed&&!gb.cpu.stopped&&m.read(0xFF4D)==0xFE);
    // Drain the documented speed-switch pause before checking a simple NOP.
    while(m.speedSwitchTicks>0)gb.cpu.step();
    gb.cpu.PC=0xC002;m.write(0xC002,0);const u64 before=gb.totalCycles;
    const u16 divider=m.divCounter;CHECK(gb.cpu.step()==4);
    CHECK(gb.totalCycles-before==2&&(u16)(m.divCounter-divider)==4);
    gb.reset();m.write(0xFF40,0);
    gb.mem.rom.assign(32768,0);gb.cpu.PC=0x100;m.doubleSpeed=true;
    gb.runFrame();CHECK(gb.totalCycles>=GB_FRAME&&gb.totalCycles<GB_FRAME+4);
    CHECK(gb.apu.available()==(u64)GB_FRAME*APU::rate/GB_CLOCK);
}

static void testCgbDmaAndSerialClocks(){
    GameBoy gb;colorMode(gb);auto& m=gb.mem;m.doubleSpeed=true;
    for(int i=0;i<160;++i)m.write((u16)(0xC000+i),(u8)(0x40+i));
    m.write(0xFF46,0xC0);gb.tick(647);CHECK(m.dmaPos==159);
    gb.tick(1);CHECK(m.dmaPos==160);
    for(int i=0;i<160;++i)CHECK(m.oam[i]==(u8)(0x40+i));
    m.divCounter=0xFFFC;m.io[0x0F]=0;m.write(0xFF01,'F');m.write(0xFF02,0x83);
    const u64 before=gb.totalCycles;gb.tick(127);CHECK(m.serialOutput.empty());
    gb.tick(1);CHECK(m.serialOutput=="F"&&(m.read(0xFF0F)&8));
    CHECK(gb.totalCycles-before==64&&m.read(0xFF01)==0xFF&&!(m.read(0xFF02)&0x80));
}

static void checksumState(std::vector<u8>& state){
    u32 checksum=2166136261u;
    for(std::size_t i=0;i<state.size()-4;++i){checksum^=state[i];checksum*=16777619u;}
    std::memcpy(state.data()+state.size()-4,&checksum,4);
}

static void writeState(const std::filesystem::path& path,const std::vector<u8>& bytes){
    CHECK(Memory::writeAtomic(path,std::string_view((const char*)bytes.data(),bytes.size())));
}

static void testMalformedState(GameBoy& gb,const std::vector<u8>& valid){
    // A valid checksum must not bypass structural/mode validation.
    constexpr std::size_t cgbTuple=74;
    const auto pc=gb.cpu.PC;const auto vram=gb.mem.vram;const auto pixels=gb.ppu.pixels;
    for(const auto [offset,value]:std::array<std::pair<std::size_t,u8>,5>{{
            {cgbTuple,0},{cgbTuple+1,2},{cgbTuple+6,0xFF},
            {cgbTuple+12,17},{cgbTuple+20,2}}}){
        auto invalid=valid;invalid[offset]=value;
        // Invalid destination 0x7fff, rather than just an unaligned valid address.
        if(offset==cgbTuple+6)invalid[offset+1]=0x7F;
        checksumState(invalid);writeState(gb.statePath(),invalid);
        CHECK(!gb.loadState());CHECK(gb.cpu.PC==pc&&gb.mem.vram==vram&&gb.ppu.pixels==pixels);
    }
    writeState(gb.statePath(),valid);
}

static void testLegacyDmgState(const std::vector<u8>& cgbDemo){
    const auto temp=std::filesystem::temp_directory_path()/"gbstudio-legacy-regression.gb";
    auto dmgDemo=cgbDemo;dmgDemo[0x143]=0;
    CHECK(Memory::writeAtomic(temp,std::string_view((const char*)dmgDemo.data(),dmgDemo.size())));
    GameBoy gb;CHECK(gb.load(temp)&&!gb.mem.cgbMode);
    gb.cpu.PC=0xC100;gb.mem.write(0xC100,0x18);gb.mem.write(0xC101,0xFE);
    gb.mem.write(0xC020,0x5A);gb.mem.write(0xFF40,0);
    for(int i=0;i<3;++i)gb.runFrame();CHECK(gb.saveState());
    const auto historical=gb.serializeState(true);
    auto v4=historical;
    // v5 adds a seven-byte boot/input extension just before the checksum.
    // Remove that extension before projecting the legacy v3 memory layout.
    if(std::string((const char*)v4.data(),8)=="GBSTATE5")v4.erase(v4.end()-11,v4.end()-4);
    // The legacy v3 wire format has no CGB tuple, one 8KiB VRAM bank,
    // 8KiB WRAM, or 128 CGB palette bytes; the rest is unchanged.
    constexpr std::size_t tuple=74,arrays=tuple+28;
    constexpr std::size_t oam=arrays+16384+32768,palette=oam+160+128+127;
    std::vector<u8> legacy;
    auto append=[&](std::size_t begin,std::size_t end){legacy.insert(legacy.end(),v4.begin()+begin,v4.begin()+end);};
    append(0,tuple);append(arrays,arrays+8192);append(arrays+16384,arrays+16384+8192);
    append(oam,palette);append(palette+128,v4.size());
    std::memcpy(legacy.data(),"GBSTATE3",8);checksumState(legacy);writeState(gb.statePath(),legacy);
    const u64 cycles=gb.totalCycles;const u16 pc=gb.cpu.PC;
    gb.mem.write(0xC020,0);gb.cpu.PC=0x1234;gb.runFrame();
    CHECK(gb.loadState()&&gb.cpu.PC==pc&&gb.totalCycles==cycles&&gb.mem.read(0xC020)==0x5A);
    // Version 3 cannot retain the modern fetcher/divider latches. Its explicit
    // legacy representation still roundtrips the CPU, RAM and old pipeline.
    CHECK(gb.serializeState(true)==historical);
    CHECK(gb.saveState());
    std::filesystem::remove(gb.statePath());std::filesystem::remove(temp);
}

static void testDemoAndState(const std::filesystem::path& demo){
    // Work on a disposable cartridge copy so tests never overwrite user saves.
    const auto temp=std::filesystem::temp_directory_path()/"gbstudio-cgb-regression.gbc";
    const auto bytes=Memory::loadBytes(demo);CHECK(!bytes.empty());
    CHECK(Memory::writeAtomic(temp,std::string_view((const char*)bytes.data(),bytes.size())));
    GameBoy gb;CHECK(gb.load(temp)&&gb.mem.cgbMode);
    for(int i=0;i<60;++i)gb.runFrame();
    CHECK(!gb.cpu.locked&&gb.mem.oam[0]==121&&gb.mem.oam[1]==84);
    std::set<u32> colors(gb.ppu.pixels.begin(),gb.ppu.pixels.end());CHECK(colors.size()>20);
    gb.mem.right=true;for(int i=0;i<4;++i)gb.runFrame();CHECK(gb.mem.oam[1]>84);
    gb.mem.right=false;
    gb.mem.doubleSpeed=true;gb.mem.write(0xFF70,7);gb.mem.write(0xD123,0xA7);
    gb.mem.write(0xFF4F,1);gb.mem.write(0xFF40,0);
    for(int i=0;i<16;++i)gb.mem.write((u16)(0xC100+i),(u8)(0x90+i));
    setupDma(gb.mem,0xC100,0x8400);gb.mem.write(0xFF55,0);gb.tick(12);
    const auto bgPalette=gb.mem.bgPalette;const auto objPalette=gb.mem.objPalette;
    CHECK(gb.saveState());const auto saved=Memory::loadBytes(gb.statePath());
    gb.tick(100);for(int i=0;i<3;++i)gb.runFrame();CHECK(gb.saveState());
    const auto continuation=Memory::loadBytes(gb.statePath());
    CHECK(Memory::writeAtomic(gb.statePath(),std::string_view((const char*)saved.data(),saved.size())));
    gb.mem.bgPalette.fill(0);gb.mem.objPalette.fill(0);gb.mem.vram.fill(0);gb.mem.wram.fill(0);
    CHECK(gb.loadState()&&gb.mem.doubleSpeed&&gb.mem.read(0xD123)==0xA7&&gb.mem.cpuDmaStalled());
    CHECK(gb.mem.bgPalette==bgPalette&&gb.mem.objPalette==objPalette);
    testMalformedState(gb,saved);
    gb.tick(100);for(int i=0;i<3;++i)gb.runFrame();CHECK(gb.saveState());
    CHECK(Memory::loadBytes(gb.statePath())==continuation);
    std::filesystem::remove(gb.statePath());std::filesystem::remove(temp);
    testLegacyDmgState(bytes);
}

int main(int argc,char** argv){
    testBankedMemory();testPalettePorts();testGeneralDma();testHblankDma();testSpeedAndFrame();testCgbDmaAndSerialClocks();
    if(argc>1)testDemoAndState(argv[1]);
    std::cout << "ALL CGB INTEGRATION TESTS PASSED\n";
}
