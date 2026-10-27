#include "../src/gameboy.hpp"
#include <cstdlib>

#define CHECK(expr) do { if(!(expr)){std::cerr<<"CHECK FAILED: "<<#expr<<" at line "<<__LINE__<<'\n';std::abort();} } while(false)

struct Scratch {
    std::filesystem::path path=std::filesystem::temp_directory_path()/
        ("gbstudio-native-regression-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch(){std::filesystem::create_directories(path);}
    ~Scratch(){std::error_code error;std::filesystem::remove_all(path,error);}
    void write(const std::filesystem::path& file,const std::vector<u8>& bytes){
        CHECK(Memory::writeAtomic(file,{reinterpret_cast<const char*>(bytes.data()),bytes.size()}));
    }
};

static void cartridge(GameBoy& g,bool color){
    g.mem.rom.assign(32768,0);g.mem.rom[0x100]=0x18;g.mem.rom[0x101]=0xFE;
    g.mem.rom[0x143]=color?0x80:0;g.mem.cgbROM=color;g.reset();
}

static void testExplicitHardwareFamilies(){
    const HardwareModel colorModels[]={HardwareModel::CGB,HardwareModel::CGB0,HardwareModel::CGBA,
        HardwareModel::CGBB,HardwareModel::CGBC,HardwareModel::CGBD,HardwareModel::CGBE,HardwareModel::AGB};
    const HardwareModel monoModels[]={HardwareModel::DMG,HardwareModel::Pocket,HardwareModel::Light};
    for(auto model:colorModels){
        GameBoy g;cartridge(g,false);CHECK(g.setModel(model));
        CHECK(g.mem.cgbHardware&&g.mem.dmgCompatibility&&!g.mem.cgbMode&&g.cpu.A==0x11);
        g.mem.cgbBootROM.assign(0x900,0);g.mem.dmgBootROM.assign(0x100,0);
        CHECK(g.bootROMConfigured(model));g.reset();
        CHECK(g.mem.bootROMMapped&&g.mem.bootROMCGB&&g.mem.cgbMode);
        g.clearBootROM(model);CHECK(g.mem.cgbBootROM.empty()&&!g.mem.dmgBootROM.empty());
    }
    for(auto model:monoModels){
        GameBoy g;cartridge(g,true);CHECK(g.setModel(model));
        CHECK(!g.mem.cgbHardware&&!g.mem.cgbMode&&!g.mem.dmgCompatibility);
        CHECK(g.cpu.A==(model==HardwareModel::DMG?0x01:0xFF));
        CHECK(g.mem.read(0xFF4F)==0xFF);
        g.mem.cgbBootROM.assign(0x900,0);g.mem.dmgBootROM.assign(0x100,0);g.reset();
        CHECK(g.mem.bootROMMapped&&!g.mem.bootROMCGB);
        g.clearBootROM(model);CHECK(g.mem.dmgBootROM.empty()&&!g.mem.cgbBootROM.empty());
        g.mem.rom[0x143]=0xC0;CHECK(!g.setModel(model));
    }
}

static std::vector<u8> clockRecord(const std::array<u8,5>& registers,std::int64_t stamp){
    std::vector<u8> bytes={'G','B','R','T','C','0','1'};
    bytes.insert(bytes.end(),registers.begin(),registers.end());
    const auto* time=reinterpret_cast<const u8*>(&stamp);bytes.insert(bytes.end(),time,time+sizeof(stamp));
    return bytes;
}

static void testRtcFileValidation(Scratch& scratch){
    Memory m;m.hasBattery=m.hasRTC=true;m.romPath=scratch.path/"clock.gb";
    m.rtc={7,6,5,4,0x40};const auto original=m.rtc;
    const auto now=static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    const std::array<u8,5> incoming={12,13,14,15,0x40};
    auto bytes=clockRecord(incoming,now);bytes.resize(16);scratch.write(m.rtcPath(),bytes);
    m.loadBattery();CHECK(m.rtc==original);
    bytes=clockRecord(incoming,std::numeric_limits<std::int64_t>::min());scratch.write(m.rtcPath(),bytes);
    m.loadBattery();CHECK(m.rtc==original);
    auto invalid=incoming;invalid[0]=60;bytes=clockRecord(invalid,now);scratch.write(m.rtcPath(),bytes);
    m.loadBattery();CHECK(m.rtc==original);
    bytes=clockRecord(incoming,now);bytes.push_back(0);scratch.write(m.rtcPath(),bytes);
    m.loadBattery();CHECK(m.rtc==original);
    bytes=clockRecord(incoming,now);scratch.write(m.rtcPath(),bytes);m.loadBattery();CHECK(m.rtc==incoming);
    // A clock set in the future must not overflow elapsed-time subtraction.
    invalid=incoming;invalid[4]=0;bytes=clockRecord(invalid,std::numeric_limits<std::int64_t>::max());
    scratch.write(m.rtcPath(),bytes);m.loadBattery();CHECK(m.rtc==invalid);
}

static void testDefaultCompatibilityPalette(){
    GameBoy g;cartridge(g,false);CHECK(g.setModel(HardwareModel::CGB));
    CHECK(g.mem.dmgCompatibility&&!g.mem.cgbMode);
    // Palette ID00's RGB555 values, converted by the documented bit replication.
    const std::array<u32,4> background={0xFFFFFFFF,0xFF7BFF31,0xFF0063C6,0xFF000000};
    const std::array<u32,4> objects={0xFFFFFFFF,0xFFFF8484,0xFF943939,0xFF000000};
    g.mem.io[0x40]=0x91;g.mem.io[0x47]=g.mem.io[0x48]=g.mem.io[0x49]=0xE4;
    // The first four pixels of each tile have raw shades 0,1,2,3.
    for(int row=0;row<8;++row)for(int tile=0;tile<2;++tile){
        g.mem.vram[tile*16+row*2]=0x55;g.mem.vram[tile*16+row*2+1]=0x33;
    }
    for(int x=0;x<4;++x){
        g.ppu.state.line=0;g.ppu.state.x=x;g.ppu.pixel();CHECK(g.ppu.drawing[x]==background[x]);
    }
    g.mem.io[0x40]|=2;g.ppu.state.selectedCount=1;
    for(u8 attr:{0,0x10}){
        g.ppu.state.sprites[0]={0,0,0,1,attr};
        for(int x=1;x<4;++x){g.ppu.state.x=x;g.ppu.pixel();CHECK(g.ppu.drawing[x]==objects[x]);}
    }
    // Supplied firmware owns the palettes. The fallback must not replace its
    // color RAM either at power-on or when it enters compatibility mode.
    g.mem.cgbBootROM.assign(0x900,0);g.reset();CHECK(g.mem.cgbMode&&g.mem.bootROMMapped);
    CHECK(std::all_of(g.mem.bgPalette.begin(),g.mem.bgPalette.end(),[](u8 b){return b==0xFF;}));
    g.mem.bgPalette[0]=0x12;g.mem.objPalette[2]=0x34;
    g.mem.write(0xFF4C,4);g.mem.write(0xFF50,0x11);
    CHECK(!g.mem.cgbMode&&g.mem.bgPalette[0]==0x12&&g.mem.objPalette[2]==0x34);
}

static void checksum(std::vector<u8>& bytes){
    u32 sum=2166136261u;for(std::size_t i=0;i<bytes.size()-4;++i){sum^=bytes[i];sum*=16777619u;}
    std::memcpy(bytes.data()+bytes.size()-4,&sum,4);
}

static void testAuxiliaryStates(Scratch& scratch){
    GameBoy g;cartridge(g,false);g.mem.romPath=scratch.path/"state.gb";g.runFrame();
    const auto legacy=g.serializeState();CHECK(std::string(reinterpret_cast<const char*>(legacy.data()),8)=="GBSTATE8");
    std::vector<u8> progress={1,2,3};unsigned restores=0;
    g.auxiliaryState=[&](){return progress;};
    g.auxiliaryRestore=[&](std::span<const u8> bytes){
        ++restores;progress.assign(bytes.begin(),bytes.end());return progress.empty()||progress[0]!=42;
    };
    const auto state=g.serializeState();CHECK(std::string(reinterpret_cast<const char*>(state.data()),8)=="GBSTATE8");
    progress={8,9};g.mem.wram[0]=0x5A;CHECK(g.deserializeState(state));
    CHECK(progress==std::vector<u8>({1,2,3})&&g.mem.wram[0]==0&&restores==1);
    GameBoy clone;cartridge(clone,false);CHECK(clone.deserializeState(state));CHECK(clone.serializeState()==legacy);
    // A rejected callback deliberately mutates its data. Its old data and the
    // complete CPU/memory state must be restored by the failed transaction.
    progress={42};const auto rejected=g.serializeState();progress={7};g.mem.wram[0]=0xA5;
    const auto before=g.serializeState();CHECK(!g.deserializeState(rejected));CHECK(g.serializeState()==before);
    // Even with a valid checksum, a forged extension size must never call the
    // external loader or modify native state.
    auto corrupt=state;const u32 oversized=GameBoy::auxiliaryStateLimit+1;
    std::memcpy(corrupt.data()+corrupt.size()-11,&oversized,4);checksum(corrupt);
    const auto calls=restores;CHECK(!g.deserializeState(corrupt));CHECK(restores==calls&&g.serializeState()==before);
    progress.assign(512*1024,0x33);CHECK(g.saveState());progress={5};
    CHECK(g.loadState()&&progress.size()==512*1024&&progress.front()==0x33);
    progress.assign(GameBoy::auxiliaryStateLimit+1,0);CHECK(g.serializeState().empty());
    progress={9};CHECK(g.deserializeState(legacy)&&progress==std::vector<u8>({9}));
}

static void testUnsupportedControllers(Scratch& scratch){
    GameBoy g;cartridge(g,false);const auto prior=g.serializeState();
    for(u8 mapper:{0x0B,0x0C,0x0D,0x20,0xFE}){
        auto rom=g.mem.rom;rom[0x147]=mapper;const auto path=scratch.path/"unsupported.gb";scratch.write(path,rom);
        CHECK(!g.load(path)&&!g.mem.loadError.empty()&&g.serializeState()==prior);
    }
}

static void testPpuObservation(){
    GameBoy observed,plain;cartridge(observed,false);cartridge(plain,false);
    unsigned video=0,objects=0;
    observed.mem.ppuBusObserver=[&](u16 address,u8,bool blocked){
        CHECK(!blocked);if(address>=0x8000&&address<0xA000)++video;
        else if(address>=0xFE00&&address<0xFEA0)++objects;
        else CHECK(false);
    };
    // Tile-map inspection does not execute the emulated PPU bus.
    observed.ppu.backgroundAt(false,0,0);CHECK(video==0&&objects==0);
    observed.tick(456);plain.tick(456);
    CHECK(video>0&&objects>0&&observed.serializeState()==plain.serializeState());
}

int main(){
    Scratch scratch;testExplicitHardwareFamilies();testRtcFileValidation(scratch);testDefaultCompatibilityPalette();
    testAuxiliaryStates(scratch);testUnsupportedControllers(scratch);testPpuObservation();
    std::cout<<"Native models, RTC validation, transactional progress, mapper rejection and PPU observation PASS\n";
}
