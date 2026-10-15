#include "../src/gameboy.hpp"
#include <cstdlib>

#define CHECK(expr) do { if(!(expr)){std::cerr<<"CHECK FAILED: "<<#expr<<" at "<<__FILE__<<":"<<__LINE__<<'\n';std::abort();} } while(false)

static bool writeBytes(const std::filesystem::path& path,const std::vector<u8>& bytes){
    return Memory::writeAtomic(path,std::string_view((const char*)bytes.data(),bytes.size()));
}
static void checksum(std::vector<u8>& bytes){
    u32 hash=2166136261u;for(std::size_t i=0;i<bytes.size()-4;++i){hash^=bytes[i];hash*=16777619u;}
    std::memcpy(bytes.data()+bytes.size()-4,&hash,4);
}
static std::filesystem::path scratch(){
    auto path=std::filesystem::temp_directory_path()/("gbstudio-memory-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path);return path;
}
static void testBootModels(){
    const auto directory=scratch();const auto romPath=directory/"boot.gbc";
    std::vector<u8> rom(32768);rom[0x143]=0x80;CHECK(writeBytes(romPath,rom));
    GameBoy gb;CHECK(gb.load(romPath)&&gb.mem.cgbMode);
    CHECK(gb.cpu.AF()==0x1180&&gb.cpu.BC()==0&&gb.cpu.DE()==0xFF56&&gb.cpu.HL()==0x000D);
    CHECK(!gb.mem.doubleSpeed&&gb.mem.read(0xFF4D)==0x7E&&gb.mem.read(0xFF4F)==0xFE);
    CHECK(gb.load(romPath,HardwareModel::DMG)&&!gb.mem.cgbMode&&gb.cpu.A==1);
    rom[0x143]=0xC0;CHECK(writeBytes(romPath,rom));const auto hash=gb.romHash();
    CHECK(!gb.load(romPath,HardwareModel::DMG)&&gb.romHash()==hash);
    CHECK(gb.load(romPath,HardwareModel::CGB)&&gb.mem.cgbMode);
    rom[0x143]=0;CHECK(writeBytes(romPath,rom));CHECK(gb.load(romPath,HardwareModel::CGB)&&gb.mem.dmgCompatibility&&!gb.mem.cgbMode);
    CHECK(gb.load(romPath)&&!gb.mem.cgbMode);
    std::filesystem::remove(romPath);std::filesystem::remove(directory);
}
static void testSerialAndTimer(){
    Memory slow;slow.divCounter=0;slow.write(0xFF01,'S');slow.write(0xFF02,0x83);
    CHECK(slow.read(0xFF02)==0xFF);slow.tick(4091);CHECK(slow.serialOutput.empty());
    slow.tick(1);CHECK(slow.serialOutput=="S"&&(slow.io[0x0F]&8));
    Memory fast;fast.cgbMode=true;fast.resetIO();fast.divCounter=0;
    fast.write(0xFF01,'F');fast.write(0xFF02,0x83);fast.tick(123);CHECK(fast.serialOutput.empty());
    fast.tick(1);CHECK(fast.serialOutput=="F"&&fast.read(0xFF01)==0xFF&&!(fast.read(0xFF02)&0x80));
    // CGB disables the timer without the DMG falling-edge TIMA increment.
    for(Memory* m:{&slow,&fast}){m->divCounter=8;m->io[7]=5;m->io[5]=0;m->write(0xFF07,0);}
    CHECK(slow.io[5]==1&&fast.io[5]==0);
    GameBoy gb;gb.mem.cgbROM=true;gb.reset();gb.mem.write(0xFF40,0);
    gb.mem.doubleSpeed=true;gb.mem.divCounter=0;gb.mem.write(0xFF01,'D');gb.mem.write(0xFF02,0x83);
    gb.tick(124);CHECK(gb.totalCycles==62&&gb.mem.serialOutput=="D");
    gb.mem.hasRTC=true;gb.mem.rtcCycles=GB_CLOCK-2;gb.tick(2);CHECK(gb.mem.rtc[0]==0);
    gb.tick(2);CHECK(gb.mem.rtc[0]==1); // RTC advances on base dots, not CPU clocks.
}
static void testDmaBusTiming(){
    for(bool fast:{false,true}){
        GameBoy gb;gb.mem.cgbROM=true;gb.reset();auto& m=gb.mem;m.write(0xFF40,0);m.doubleSpeed=fast;
        for(int i=0;i<16;++i)m.write((u16)(0xC100+i),(u8)(0x20+i));
        m.write(0xFF51,0xC1);m.write(0xFF52,0);m.write(0xFF53,0);m.write(0xFF54,0);
        gb.cpu.PC=0xC000;gb.cpu.A=0;m.write(0xC000,0xE0);m.write(0xC001,0x55);m.write(0xC002,0);
        CHECK(gb.cpu.step()==12&&m.cpuDmaStalled());
        const u64 before=gb.totalCycles;const u16 divider=m.divCounter;
        CHECK(gb.cpu.step()==(fast?68:36));CHECK(gb.totalCycles-before==(fast?34:36));
        CHECK((u16)(m.divCounter-divider)==(fast?68:36)&&!m.cpuDmaStalled()&&gb.cpu.PC==0xC003);
        for(int i=0;i<16;++i)CHECK(m.read((u16)(0x8000+i))==0x20+i);
    }
    GameBoy gb;gb.mem.cgbROM=true;gb.reset();auto& m=gb.mem;m.write(0xFF40,0);
    m.rom.assign(32768,0xA5);m.write(0xC000,0x53);m.write(0xFF46,0xC0);gb.tick(8);
    CHECK(m.cpuRead(0)==0xA5&&m.cpuRead(0xC000)==0xFF&&m.cpuRead(0xFE00)==0xFF);
    m.cpuWrite(0xC000,0x99);CHECK(m.read(0xC000)==0x53);gb.tick(640);
    m.write(0xFF46,0);gb.tick(8);CHECK(m.cpuRead(0)==0xFF&&m.cpuRead(0xC000)==0x53);
    m.cpuWrite(0xC000,0x99);CHECK(m.read(0xC000)==0x99);
}
static void testLegacyAndValidation(const std::filesystem::path& fixture,const std::filesystem::path& demo){
    const auto directory=scratch();const auto romPath=directory/"legacy.gb";
    CHECK(writeBytes(romPath,Memory::loadBytes(demo)));GameBoy gb;CHECK(gb.load(romPath));
    const auto original=Memory::loadBytes(fixture);CHECK(original.size()>100&&std::string((const char*)original.data(),8)=="GBSTATE3");
    CHECK(writeBytes(gb.statePath(),original));CHECK(gb.loadState()&&!gb.mem.cgbMode);
    CHECK(gb.ppu.frameNumber==14); // Demo uploads tiles with the LCD off first.
    CHECK(gb.totalCycles>=17ULL*GB_FRAME&&!gb.cpu.locked);
    const u16 pc=gb.cpu.PC;const auto pixels=gb.ppu.pixels;const auto vram=gb.mem.vram;
    for(int i=0;i<4;++i)gb.runFrame();CHECK(gb.saveState());const auto expected=Memory::loadBytes(gb.statePath());
    CHECK(writeBytes(gb.statePath(),original)&&gb.loadState());
    CHECK(gb.cpu.PC==pc&&gb.ppu.pixels==pixels&&gb.mem.vram==vram);
    for(int i=0;i<4;++i)gb.runFrame();CHECK(gb.saveState());CHECK(Memory::loadBytes(gb.statePath())==expected);
    // A valid checksum is not enough: impossible CGB DMA state must be rejected atomically.
    gb.mem.cgbROM=true;gb.reset();gb.mem.write(0xFF40,0);CHECK(gb.saveState());
    auto corrupt=Memory::loadBytes(gb.statePath());constexpr std::size_t colorStateOffset=74;
    corrupt[colorStateOffset+1]=1;const std::int32_t invalidBytes=16;
    std::memcpy(corrupt.data()+colorStateOffset+12,&invalidBytes,4);checksum(corrupt);
    CHECK(writeBytes(gb.statePath(),corrupt));const auto oldPC=gb.cpu.PC;
    CHECK(!gb.loadState()&&gb.cpu.PC==oldPC&&!gb.mem.doubleSpeed&&!gb.mem.hdmaActive);
    std::filesystem::remove(gb.statePath());std::filesystem::remove(romPath);std::filesystem::remove(directory);
}
int main(int argc,char** argv){
    testBootModels();testSerialAndTimer();testDmaBusTiming();
    if(argc>2)testLegacyAndValidation(argv[1],argv[2]);
    std::cout<<"ALL CGB MEMORY TESTS PASSED\n";
}
