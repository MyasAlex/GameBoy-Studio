#include "../src/gameboy.hpp"
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';std::abort();}}while(false)

static void bufferedEiHalt(){
    auto storage=std::make_unique<GameBoy>();auto& g=*storage;
    g.mem.rom.assign(32768,0);g.mem.rom[0x100]=0xFB;g.mem.rom[0x101]=0x76;
    g.mem.rom[0x40]=0xD9;g.mem.rom[0x48]=0xD9;g.mem.rom[0x50]=0xD9;
    g.reset();g.mem.write(0xFF40,0);g.mem.ie=7;g.mem.io[0x0F]=3;
    CHECK(g.cpu.step()==4&&!g.cpu.IME&&g.cpu.PC==0x101);
    CHECK(g.cpu.step()==4&&g.cpu.IME&&g.cpu.haltBug&&g.cpu.PC==0x102);
    CHECK(g.cpu.step()==20&&g.cpu.PC==0x40&&!g.cpu.haltBug);
    CHECK(g.mem.read(0xFFFC)==1&&g.mem.read(0xFFFD)==1);
    CHECK(g.cpu.step()==16&&g.cpu.PC==0x101);
    CHECK(g.cpu.step()==20&&g.cpu.PC==0x48);
    CHECK(g.cpu.step()==16&&g.cpu.PC==0x101);
    CHECK(g.cpu.step()==4&&g.cpu.halted&&g.cpu.PC==0x102);
    g.mem.io[0x0F]|=4;
    CHECK(g.cpu.step()==20&&g.cpu.PC==0x50);
    CHECK(g.cpu.step()==16&&g.cpu.PC==0x102);
}

static void interruptDisableBoundary(){
    // An interrupt raised during DI's opcode fetch remains pending, but DI
    // completes and suppresses acknowledgement. Mooneye di_timing exercises
    // this hardware boundary; polling again after fetching the opcode would
    // incorrectly discard DI and push a return address here.
    for(int bit=0;bit<5;++bit)for(int arrival=1;arrival<=4;++arrival){
        auto storage=std::make_unique<GameBoy>();auto& g=*storage;
        g.mem.rom.assign(32768,0);g.mem.rom[0x100]=0xF3;g.reset();
        g.mem.write(0xFF40,0);g.mem.ie=(u8)(1<<bit);g.mem.io[0x0F]=0;
        g.cpu.IME=true;int dots=0;
        g.cpu.clock=[&](int cycles){
            dots+=cycles;if(dots>=arrival)g.mem.io[0x0F]|=(u8)(1<<bit);
        };
        CHECK(g.cpu.step()==4&&g.cpu.PC==0x101&&!g.cpu.IME);
        CHECK(g.cpu.SP==0xFFFE&&(g.mem.io[0x0F]&(1<<bit)));
        CHECK(g.cpu.step()==4&&g.cpu.PC==0x102&&g.cpu.SP==0xFFFE);
    }

    // DI cancels a pending EI. Consecutive EI instructions retain the first
    // EI's enable boundary instead of postponing it by another instruction.
    for(bool cancel:{false,true}){
        auto storage=std::make_unique<GameBoy>();auto& g=*storage;
        g.mem.rom.assign(32768,0);g.mem.rom[0x100]=0xFB;
        g.mem.rom[0x101]=cancel?0xF3:0xFB;g.reset();
        g.mem.write(0xFF40,0);g.mem.ie=4;g.mem.io[0x0F]=4;
        CHECK(g.cpu.step()==4&&!g.cpu.IME);
        CHECK(g.cpu.step()==4&&g.cpu.IME==!cancel&&g.cpu.imeDelay==0);
        if(cancel)CHECK(g.cpu.step()==4&&g.cpu.PC==0x103&&g.cpu.SP==0xFFFE);
        else{
            CHECK(g.cpu.step()==20&&g.cpu.PC==0x50);
            CHECK(g.mem.read(0xFFFC)==2&&g.mem.read(0xFFFD)==1);
        }
    }
}

static void immediateHblankBlock(){
    // HBlank transfer mode persists while LCD is disabled. Only its first
    // block starts immediately; subsequent blocks require an HBlank edge.
    for(bool lcd:{false,true})for(bool fast:{false,true}){
        auto storage=std::make_unique<GameBoy>();auto& g=*storage;auto& m=g.mem;
        m.cgbROM=true;g.reset();m.write(0xFF40,0);m.doubleSpeed=fast;
        if(lcd){m.write(0xFF40,0x91);m.ppuMode=0;m.io[0x44]=7;}
        for(int i=0;i<64;++i)m.wram[0x100+i]=(u8)(i+1);
        m.write(0xFF51,0xC1);m.write(0xFF52,0x0F);
        m.write(0xFF53,0x68);m.write(0xFF54,0x0F);
        m.write(0xFF55,0x83);
        CHECK(m.hdmaActive&&m.hdmaHblank&&m.cpuDmaStalled());
        // Drive base dots directly to keep this test independent of PPU mode
        // advancement. It checks the transfer controller's state and data.
        for(int i=0;i<32;++i)m.tickBaseDot();
        CHECK(m.hdmaActive&&!m.cpuDmaStalled()&&m.read(0xFF55)==2);
        for(int i=0;i<16;++i)CHECK(m.vram[0x800+i]==i+1);
        for(int i=16;i<64;++i)CHECK(m.vram[0x800+i]==0);
        m.write(0xFF55,0);
        CHECK(!m.hdmaActive&&!m.cpuDmaStalled()&&m.read(0xFF55)==0x80);
    }
}

static void compatibilityHardware(){
    for(auto model:{HardwareModel::CGBE,HardwareModel::AGB}){
        auto storage=std::make_unique<GameBoy>();auto& g=*storage;auto& m=g.mem;
        m.rom.assign(32768,0);m.cgbROM=false;g.model=model;g.reset();
        CHECK(m.cgbHardware&&m.dmgCompatibility&&!m.cgbMode);
        CHECK(g.cpu.DE()==8&&g.cpu.HL()==0x7C);
        CHECK(g.cpu.AF()==(model==HardwareModel::AGB?0x1100:0x1180));
        CHECK(g.cpu.BC()==(model==HardwareModel::AGB?0x0100:0));
        for(u8 value:{u8{0},u8{0xA5},u8{0xFF}}){
            m.write(0xFF72,value);m.write(0xFF73,value);m.write(0xFF75,value);
            CHECK(m.read(0xFF72)==value&&m.read(0xFF73)==value);
            CHECK(m.read(0xFF75)==(u8)(0x8F|(value&0x70)));
        }
        m.write(0xFF74,0x55);m.write(0xFF70,7);m.write(0xFF4D,1);
        CHECK(m.read(0xFF74)==0xFF&&m.read(0xFF70)==0xFF&&m.read(0xFF4D)==0xFF);
        // Nintendo-compatible header checksum affects the documented handoff
        // registers; the AGB INC B additionally updates Z/H flags.
        m.rom[0x14B]=1;m.rom[0x134]=0x43;g.reset();
        CHECK(g.cpu.HL()==0x991A&&g.cpu.B==(model==HardwareModel::AGB?0x44:0x43));
        m.rom[0x134]=0xFF;g.reset();
        CHECK(g.cpu.B==(model==HardwareModel::AGB?0:0xFF));
        if(model==HardwareModel::AGB)CHECK(g.cpu.F==0xA0);
    }
    Memory dmg;dmg.write(0xFF72,0x55);CHECK(dmg.read(0xFF72)==0xFF);
}

static void monochromeBootIdentity(){
    for(auto model:{HardwareModel::DMG,HardwareModel::Pocket,HardwareModel::Light,
                   HardwareModel::SGB,HardwareModel::SGB2,HardwareModel::SGBPAL}){
        auto storage=std::make_unique<GameBoy>();auto& g=*storage;
        g.mem.rom.assign(32768,0);g.model=model;
        const bool super=model==HardwareModel::SGB||model==HardwareModel::SGB2||model==HardwareModel::SGBPAL;
        const bool ff=model==HardwareModel::Pocket||model==HardwareModel::Light||model==HardwareModel::SGB2;
        for(u8 checksum:{u8{0},u8{1},u8{0xFF}}){
            g.mem.rom[0x14D]=checksum;g.reset();
            CHECK(g.cpu.A==(ff?0xFF:0x01));
            CHECK(g.cpu.F==(super?0:checksum?0xB0:0x80));
            CHECK(g.cpu.BC()==(super?0x0014:0x0013));
            CHECK(g.cpu.DE()==(super?0x0000:0x00D8));
            CHECK(g.cpu.HL()==(super?0xC060:0x014D));
            CHECK(g.cpu.SP==0xFFFE&&g.cpu.PC==0x0100);
        }
        // A supplied boot image starts from power-on registers and owns its
        // handoff; the native postboot approximation must not run over it.
        g.mem.dmgBootROM.assign(256,0);g.reset();
        CHECK(g.cpu.AF()==0&&g.cpu.BC()==0&&g.cpu.DE()==0&&g.cpu.HL()==0);
        CHECK(g.cpu.SP==0&&g.cpu.PC==0&&g.mem.bootROMMapped);
    }
}

static void singleBankCartridges(){
    const auto folder=std::filesystem::temp_directory_path()/("gb-single-bank-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(folder);
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}} cleanup{folder};
    const auto path=folder/"single.gb";std::vector<u8> image(16384,0);
    image[0x100]=0xC3;image[0x101]=0x50;image[0x102]=1;image[0x150]=0x18;image[0x151]=0xFE;
    image[0x123]=0x5A;image.back()=0xA5;Memory memory;
    for(u8 type:{u8{0x00},u8{0x08},u8{0x09}}){
        image[0x147]=type;image[0x149]=type?2:0;
        CHECK(Memory::writeAtomic(path,{(const char*)image.data(),image.size()}));
        CHECK(memory.loadROM(path)&&memory.rom==image&&memory.mbc==0);
        CHECK(memory.read(0x0123)==0x5A&&memory.read(0x4123)==0x5A);
        CHECK(memory.read(0x3FFF)==0xA5&&memory.read(0x7FFF)==0xA5);
        auto g=std::make_unique<GameBoy>();CHECK(g->load(path));
        CHECK(g->cpu.step()==16&&g->cpu.PC==0x150);
        CHECK(g->cpu.step()==12&&g->cpu.PC==0x150);
    }
    const auto previous=memory.rom,ram=memory.eram;
    for(u8 type:{u8{0x01},u8{0x03},u8{0x13},u8{0x1B},u8{0x22},u8{0xFC}}){
        image[0x147]=type;CHECK(Memory::writeAtomic(path,{(const char*)image.data(),image.size()}));
        CHECK(!memory.loadROM(path)&&memory.rom==previous&&memory.eram==ram&&memory.mbc==0);
    }
    image.resize(16383);image[0x147]=0;CHECK(Memory::writeAtomic(path,{(const char*)image.data(),image.size()}));
    CHECK(!memory.loadROM(path)&&memory.rom==previous&&memory.eram==ram);
}

int main(){bufferedEiHalt();interruptDisableBoundary();immediateHblankBlock();compatibilityHardware();monochromeBootIdentity();singleBankCartridges();
    std::cout<<"Interrupt, startup/compatibility ports and LCD-off/mode0 HDMA regressions PASS\n";}
