#include "../src/gameboy.hpp"
#include <cstdlib>

#define CHECK(expr) do { if(!(expr)) { std::cerr<<"CHECK FAILED: "<<#expr<<" at "<<__FILE__<<":"<<__LINE__<<"\n"; std::abort(); } } while(false)

// Every fixture is synthetic. No commercial cartridge or manufacturer boot
// image is included, fetched or required by these regression tests.
class Fixtures {
public:
    std::filesystem::path directory=std::filesystem::temp_directory_path()/
        ("gbstudio-core-extension-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixtures(){std::filesystem::create_directories(directory);}
    ~Fixtures(){std::error_code ec;std::filesystem::remove_all(directory,ec);}
    std::filesystem::path file(std::string name,const std::vector<u8>& bytes){
        const auto path=directory/name;
        CHECK(Memory::writeAtomic(path,std::string_view((const char*)bytes.data(),bytes.size())));return path;
    }
    std::filesystem::path cartridge(std::string name,u8 cgb=0,u8 mapper=0,int banks=2){
        std::vector<u8> bytes((std::size_t)banks*0x4000,0);
        for(int bank=0;bank<banks;++bank)std::fill(bytes.begin()+bank*0x4000,bytes.begin()+(bank+1)*0x4000,(u8)bank);
        bytes[0x143]=cgb;bytes[0x147]=mapper;bytes[0x149]=mapper==0x03?3:0;
        // Cartridge runs a safe, stable loop at its conventional entry point.
        bytes[0x100]=0x18;bytes[0x101]=0xFE;
        return file(std::move(name),bytes);
    }
};

static void checksum(std::vector<u8>& bytes){
    u32 hash=2166136261u;for(std::size_t i=0;i<bytes.size()-4;++i){hash^=bytes[i];hash*=16777619u;}
    std::memcpy(bytes.data()+bytes.size()-4,&hash,4);
}

static void testDmgBoot(Fixtures& f){
    const auto cart=f.cartridge("dmg.gb");
    std::vector<u8> boot(256,0);
    // Boot at 0000 sets SP and A, then branches to the final unmap instruction.
    boot[0]=0x31;boot[1]=0xFE;boot[2]=0xFF; // LD SP,FFFE
    boot[3]=0x3E;boot[4]=0x42;              // LD A,42
    boot[5]=0xC3;boot[6]=0xFE;boot[7]=0;    // JP 00FE
    boot[0xFE]=0xE0;boot[0xFF]=0x50;        // LDH (FF50),A
    const auto image=f.file("synthetic-dmg.bin",boot);
    GameBoy g;CHECK(g.load(cart));CHECK(g.cpu.PC==0x100);
    CHECK(g.loadBootROM(image));CHECK(g.bootROMConfigured(HardwareModel::DMG));
    CHECK(g.cpu.PC==0x100&&!g.mem.bootROMMapped); // Configure without interrupting the cartridge.
    g.reset();CHECK(g.cpu.PC==0&&g.mem.bootROMMapped&&g.cpu.SP==0);
    CHECK(g.mem.read(0)==0x31&&g.mem.read(0x100)==0x18);
    CHECK(g.mem.io[0x40]==0&&!(g.mem.read(0xFF26)&0x80)&&g.mem.divCounter==0);
    g.mem.write(0xFF50,0);CHECK(g.mem.bootROMMapped);
    for(int i=0;i<4;++i)g.cpu.step();
    CHECK(g.cpu.PC==0x100&&g.cpu.SP==0xFFFE&&g.cpu.A==0x42&&!g.mem.bootROMMapped);
    CHECK(g.mem.read(0)==0&&g.mem.read(0xFF50)==0xFF);
    g.mem.write(0xFF50,0);CHECK(!g.mem.bootROMMapped);
    g.reset();CHECK(g.mem.bootROMMapped);
    g.clearBootROM(HardwareModel::DMG);CHECK(!g.bootROMConfigured(HardwareModel::DMG));
    CHECK(g.mem.bootROMMapped&&g.mem.read(0)==0x31); // Live image remains stable.
    g.reset();CHECK(!g.mem.bootROMMapped&&g.cpu.PC==0x100);
}

static void testCgbBoot(Fixtures& f){
    const auto cart=f.cartridge("cgb.gbc",0x80);
    std::vector<u8> packed(0x800,0xA5);packed[0]=0xC3;packed[1]=0;packed[2]=2;
    const auto image=f.file("synthetic-cgb-packed.bin",packed);
    GameBoy g;CHECK(g.load(cart));CHECK(g.loadBootROM(image));g.reset();
    CHECK(g.mem.cgbMode&&g.mem.bootROMCGB&&g.mem.bootROMMapped);
    CHECK(g.mem.bootROM.size()==0x900&&g.mem.read(0)==0xC3);
    CHECK(g.mem.read(0x100)==0x18&&g.mem.read(0x1FF)==0);
    CHECK(g.mem.read(0x200)==0xA5&&g.mem.read(0x8FF)==0xA5&&g.mem.read(0x900)==0);
    g.cpu.step();CHECK(g.cpu.PC==0x200); // CPU can fetch the second boot segment.
    g.mem.write(0xFF4C,0x80);g.mem.write(0xFF6C,1);
    CHECK(g.mem.io[0x4C]==0x80&&g.mem.io[0x6C]==1);
    g.mem.write(0xFF50,0x11);CHECK(!g.mem.bootROMMapped);
    g.mem.write(0xFF4C,0);g.mem.write(0xFF6C,0);
    CHECK(g.mem.io[0x4C]==0x80&&g.mem.io[0x6C]==1);
    std::vector<u8> expanded(0x900,0x5A);expanded[0x100]=0xDD;
    CHECK(g.loadBootROM(f.file("synthetic-cgb-expanded.bin",expanded)));g.reset();
    CHECK(g.mem.read(0)==0x5A&&g.mem.read(0x100)==0x18&&g.mem.read(0x200)==0x5A);
    const auto configured=g.mem.cgbBootROM;
    CHECK(!g.loadBootROM(f.file("invalid.bin",std::vector<u8>(255))));
    CHECK(g.mem.cgbBootROM==configured&&!g.mem.loadError.empty());
    g.clearBootROM();CHECK(!g.bootROMConfigured(HardwareModel::Auto));
}

static void testSnapshots(Fixtures& f){
    const auto cart=f.cartridge("snapshots.gb");
    GameBoy g;CHECK(g.load(cart));g.runFrame();
    g.mem.right=g.mem.b=true;g.mem.wram[42]=0x87;g.mem.hram[12]=0x9A;
    const auto initial=g.serializeState();CHECK(!initial.empty());
    CHECK(std::string((const char*)initial.data(),8)=="GBSTATE6");
    for(int i=0;i<6;++i)g.runFrame();const auto expected=g.serializeState();
    CHECK(g.deserializeState(initial));CHECK(g.mem.right&&g.mem.b&&!g.mem.a);
    for(int i=0;i<6;++i)g.runFrame();CHECK(g.serializeState()==expected);
    const auto state=f.directory/"slot-7.state";CHECK(g.saveState(state));
    g.mem.wram[42]=0;CHECK(g.loadState(state));CHECK(g.mem.wram[42]==0x87);
    CHECK(g.serializeState()==expected);
    const auto unchanged=g.serializeState();
    auto corrupt=expected;corrupt[120]^=1;CHECK(!g.deserializeState(corrupt));
    CHECK(g.serializeState()==unchanged);
    auto truncated=expected;truncated.resize(truncated.size()-1);CHECK(!g.deserializeState(truncated));
    CHECK(g.serializeState()==unchanged);
    auto otherROM=Memory::loadBytes(cart);otherROM[42]=0xFF;
    GameBoy other;CHECK(other.load(f.file("other.gb",otherROM)));CHECK(!other.deserializeState(expected));
    // Version 4 has exactly the existing layout, without the seven-byte v5 tail.
    auto version4=g.serializeState(true);version4.erase(version4.end()-11,version4.end()-4);version4[7]='4';checksum(version4);
    CHECK(g.deserializeState(version4));CHECK(!g.mem.bootROMMapped);
    // Version 3 predates CGB RAM banking and color palettes. Reconstruct its
    // documented binary layout from the DMG v4 fixture, then verify migration.
    auto version3=version4;
    constexpr std::size_t v4Arrays=102,oldRam=8192;
    const auto paletteOffset=v4Arrays+16384+32768+160+128+127;
    version3.erase(version3.begin()+paletteOffset,version3.begin()+paletteOffset+128);
    version3.erase(version3.begin()+v4Arrays+16384+oldRam,version3.begin()+v4Arrays+16384+32768);
    version3.erase(version3.begin()+v4Arrays+oldRam,version3.begin()+v4Arrays+16384);
    version3.erase(version3.begin()+74,version3.begin()+v4Arrays);
    version3[7]='3';checksum(version3);
    CHECK(g.deserializeState(version3)&&g.mem.wram[42]==0x87&&g.mem.hram[12]==0x9A);
    // Existing host mixer preferences survive reset and restore.
    g.apu.setChannelMuted(1,true);g.reset();CHECK(g.apu.mutedChannels[1]);
    CHECK(g.deserializeState(expected)&&g.apu.mutedChannels[1]);
}

static void testBootSnapshots(Fixtures& f){
    const auto cart=f.cartridge("boot-snapshot.gb");
    std::vector<u8> boot(256,0);boot[0]=0x18;boot[1]=0xFE;
    const auto image=f.file("snapshot-boot.bin",boot);
    GameBoy g;CHECK(g.load(cart));CHECK(g.loadBootROM(image));g.reset();g.cpu.step();
    const auto snapshot=g.serializeState();CHECK(g.mem.bootROMMapped);
    g.mem.write(0xFF50,1);g.cpu.PC=0x100;
    CHECK(g.deserializeState(snapshot));CHECK(g.mem.bootROMMapped&&g.cpu.PC==0);
    GameBoy missing;CHECK(missing.load(cart));const auto original=missing.serializeState();
    CHECK(!missing.deserializeState(snapshot));CHECK(missing.serializeState()==original);
    boot[2]=0x43;CHECK(missing.loadBootROM(f.file("wrong-boot.bin",boot)));missing.reset();
    CHECK(!missing.deserializeState(snapshot));
    CHECK(missing.loadBootROM(image)); // Configured matching image may restore a boot snapshot.
    CHECK(missing.deserializeState(snapshot)&&missing.mem.read(2)==0);
}

static void testMalformedStates(Fixtures& f){
    GameBoy g;CHECK(g.load(f.cartridge("malformed.gb")));g.runFrame();
    const auto valid=g.serializeState();
    constexpr std::size_t pipelineOffset=102+16384+32768+160+128+127+128+5+5+16+4+14+4+8+8;
    constexpr std::size_t apuOffset=pipelineOffset+sizeof(PPU::Pipeline);
    auto rejects=[&](std::vector<u8> bytes){
        checksum(bytes);CHECK(!g.deserializeState(bytes));CHECK(g.serializeState()==valid);
    };
    auto invalid=valid;invalid[pipelineOffset+offsetof(PPU::Pipeline,windowActive)]=0xFF;rejects(invalid);
    invalid=valid;invalid[apuOffset+offsetof(APU::State,channels)+offsetof(APU::Channel,enabled)]=0x02;rejects(invalid);
    invalid=valid;const double nan=std::numeric_limits<double>::quiet_NaN();
    std::memcpy(invalid.data()+apuOffset+offsetof(APU::State,capLeft),&nan,sizeof(nan));rejects(invalid);
    invalid=valid;const int extreme=std::numeric_limits<int>::max();
    std::memcpy(invalid.data()+pipelineOffset+offsetof(PPU::Pipeline,stall),&extreme,sizeof(extreme));rejects(invalid);
    invalid=valid;invalid[13]|=1;rejects(invalid); // Impossible low bits in the CPU flags register.
    invalid=valid;const int tooManySamples=std::numeric_limits<int>::max();
    std::memcpy(invalid.data()+apuOffset+offsetof(APU::State,mixCount),&tooManySamples,sizeof(tooManySamples));rejects(invalid);
    // A checksum-valid state cannot make Mode 3 write outside the framebuffer.
    invalid=valid;const int mode=3,line=144;
    std::memcpy(invalid.data()+53,&mode,sizeof(mode));
    std::memcpy(invalid.data()+pipelineOffset+offsetof(PPU::Pipeline,line),&line,sizeof(line));rejects(invalid);
}

static void testSafeModelsAndMappers(Fixtures& f){
    GameBoy g;CHECK(g.load(f.cartridge("color-only.gbc",0xC0)));
    g.runFrame();const auto state=g.serializeState();const auto path=g.mem.romPath;
    CHECK(!g.setModel(HardwareModel::DMG));CHECK(g.serializeState()==state&&g.mem.romPath==path);
    GameBoy compatible;CHECK(compatible.load(f.cartridge("dmg-only.gb"),HardwareModel::CGB));CHECK(compatible.mem.dmgCompatibility&&!compatible.mem.cgbMode);
    CHECK(!g.load(f.file("unsupported.gb",std::vector<u8>(100))));CHECK(g.serializeState()==state);
    CHECK(g.setModel(HardwareModel::CGB)&&g.mem.cgbMode&&g.cpu.PC==0x100);
    CHECK(g.load(f.cartridge("mapper-reset.gb",0,0x03,64)));
    CHECK(!g.mem.mbc1Multicart); // Blank synthetic logos do not identify multicarts.
    g.mem.write(0x4000,1);g.mem.write(0x2000,0);CHECK(g.mem.read(0x4000)==33);
    g.mem.write(0x6000,1);CHECK(g.mem.read(0)==32);
    g.mem.write(0x0000,0x0A);g.mem.write(0xA000,0x55);g.mem.latchWrite=1;g.mem.rtcLatched=true;
    g.reset();CHECK(g.mem.read(0)==0&&g.mem.read(0x4000)==1&&!g.mem.ramEnabled);
    CHECK(g.mem.latchWrite==0&&!g.mem.rtcLatched&&g.mem.eram[8192]==0x55);
    auto multi=Memory::loadBytes(g.mem.romPath);
    std::fill(multi.begin()+0x104,multi.begin()+0x134,0x35);
    std::copy(multi.begin()+0x104,multi.begin()+0x134,multi.begin()+0x40104);
    CHECK(g.load(f.file("multicart.gb",multi)));CHECK(g.mem.mbc1Multicart);
    g.mem.write(0x4000,2);g.mem.write(0x2000,0x10);CHECK(g.mem.read(0x4000)==32);
    g.mem.write(0x2000,0);CHECK(g.mem.read(0x4000)==33);
    g.mem.write(0x6000,1);CHECK(g.mem.read(0)==32);
}

static void testBatteryBackups(Fixtures& f){
    GameBoy g;CHECK(g.load(f.cartridge("battery.gb",0,0x03,4)));
    g.mem.eram[0]=0x11;CHECK(g.mem.saveBattery());
    g.mem.eram[0]=0x22;CHECK(g.mem.saveBattery());
    auto backup=g.mem.savePath();backup+=".bak";
    CHECK(Memory::loadBytes(g.mem.savePath())[0]==0x22&&Memory::loadBytes(backup)[0]==0x11);
    g.mem.eram[0]=0x33;CHECK(g.mem.saveBattery());CHECK(Memory::loadBytes(backup)[0]==0x22);
}

static void testBoundedFileLoading(Fixtures& f){
    // Seek to the last byte to create large logical files without constructing
    // or shipping large byte fixtures. All allocations must be rejected by the
    // file-length check before a loader reads the image.
    auto sparse=[&](std::string name,std::uint64_t size){
        const auto path=f.directory/name;std::ofstream file(path,std::ios::binary|std::ios::trunc);
        CHECK(file&&size>0);file.seekp((std::streamoff)(size-1));file.put(0);file.close();
        CHECK(std::filesystem::file_size(path)==size);return path;
    };
    const auto cart=f.cartridge("bounded-running.gb");
    std::vector<u8> boot(256,0);boot[0]=0x18;boot[1]=0xFE;
    GameBoy g;CHECK(g.load(cart));CHECK(g.loadBootROM(f.file("bounded-valid-boot.bin",boot)));
    g.reset();g.cpu.step();const auto before=g.serializeState();const auto bootConfig=g.mem.dmgBootROM;
    CHECK(!g.loadState(sparse("oversized.state",400001))&&g.serializeState()==before);
    CHECK(!g.loadBootROM(sparse("oversized-boot.bin",2305))&&g.mem.dmgBootROM==bootConfig&&g.serializeState()==before);
    const auto oversizedROM=sparse("oversized.gb",8*1024*1024+1);
    CHECK(!g.load(oversizedROM)&&g.mem.romPath==cart&&g.serializeState()==before);
    CHECK(!g.mem.loadROM(oversizedROM)&&g.mem.romPath==cart&&g.serializeState()==before);
    CHECK(Memory::loadBytes(sparse("oversized-default.bin",32*1024*1024+1)).empty());
    CHECK(Memory::loadBytes(sparse("huge-logical.bin",4ull*1024*1024*1024+1),400000).empty());
    const auto boundary=f.file("exact-bound.bin",std::vector<u8>(2304,0xAB));
    CHECK(Memory::loadBytes(boundary,2304).size()==2304&&Memory::loadBytes(boundary,2303).empty());
    CHECK(Memory::loadBytes(f.directory/"missing.bin").empty());
    CHECK(Memory::loadBytes(f.directory).empty()); // Seek/read failures are not valid byte images.
}

int main(){
    Fixtures f;testDmgBoot(f);testCgbBoot(f);testSnapshots(f);testBootSnapshots(f);testMalformedStates(f);
    testSafeModelsAndMappers(f);testBatteryBackups(f);testBoundedFileLoading(f);
    std::cout<<"ALL CORE EXTENSION TESTS PASSED\n";
}
