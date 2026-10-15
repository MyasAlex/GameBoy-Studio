#include "../src/network_tools.hpp"
#include "../src/accuracy_core.hpp"
#include "../src/cheat_engine.hpp"
#include "../src/studio_tools.hpp"
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main(int argc,char** argv){std::cout<<std::unitbuf;CHECK(argc==3);
    const auto dir=std::filesystem::temp_directory_path()/("gb-accuracy-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
    const auto rom=Memory::loadBytes(argv[1]);const auto file=dir/"demo.gb";CHECK(Memory::writeAtomic(file,{(const char*)rom.data(),rom.size()}));
    for(auto model:{HardwareModel::DMG,HardwareModel::Pocket,HardwareModel::Light,HardwareModel::CGB0,HardwareModel::CGBA,HardwareModel::CGBB,HardwareModel::CGBC,HardwareModel::CGBD,HardwareModel::CGBE,HardwareModel::AGB,HardwareModel::SGB,HardwareModel::SGB2,HardwareModel::SGBPAL}){
        auto a=std::make_unique<GameBoy>(),b=std::make_unique<GameBoy>();CHECK(accurate::enable(*a)&&accurate::enable(*b));CHECK(a->load(file,model)&&b->load(file,model));
        for(int n=0;n<4;++n)a->runFrame();a->mem.write(0xC023,0xA5);a->apu.setSampleRate(96000);a->mem.a=true;
        const auto state=a->serializeState();CHECK(!state.empty()&&b->deserializeState(state));if(a->serializeState()!=b->serializeState()){const auto x=a->serializeState(),y=b->serializeState();std::cerr<<"State mismatch model "<<(int)model<<" sizes "<<x.size()<<" "<<y.size()<<"\n";unsigned shown=0;for(std::size_t i=0;i<std::min(x.size(),y.size())&&shown<30;++i)if(x[i]!=y[i]){std::cerr<<i<<":"<<(int)x[i]<<"/"<<(int)y[i]<<" ";++shown;}std::cerr<<"\n";return 1;}
        for(int n=0;n<3;++n){a->runFrame();b->runFrame();}if(a->serializeState()!=b->serializeState()){const auto x=a->serializeState(),y=b->serializeState();std::cerr<<"Continuation mismatch model "<<(int)model<<"\n";unsigned shown=0;for(std::size_t i=0;i<std::min(x.size(),y.size())&&shown<20;++i)if(x[i]!=y[i]){std::cerr<<i<<":"<<(int)x[i]<<"/"<<(int)y[i]<<" ";++shown;}std::cerr<<"\n";return 1;}
        const auto bess=saves::exportBess(*a);CHECK(saves::importBess(*b,bess));CHECK(a->serializeState()==b->serializeState());
        auto bad=state;bad[100]^=1;const auto previous=b->serializeState();CHECK(!b->deserializeState(bad)&&previous==b->serializeState());
        std::cout<<"Model "<<(int)model<<" exact state/BESS continuation PASS\n";
    }
    auto a=std::make_unique<GameBoy>(),b=std::make_unique<GameBoy>();CHECK(accurate::enable(*a)&&accurate::enable(*b)&&a->load(file)&&b->load(file));
    a->mem.write(0xFF40,0);b->mem.write(0xFF40,0);a->cpu.PC=b->cpu.PC=0xC000;a->mem.write(0xC000,0x18);a->mem.write(0xC001,0xFE);b->mem.write(0xC000,0x18);b->mem.write(0xC001,0xFE);
    devices::LocalLink cable;cable.connect(*a,*b);a->mem.write(0xFF01,0xA5);b->mem.write(0xFF01,0x3C);b->mem.write(0xFF02,0x80);a->mem.write(0xFF02,0x81);cable.runFrame();
    CHECK(a->mem.read(0xFF01)==0x3C&&b->mem.read(0xFF01)==0xA5);CHECK((a->mem.io[0x0F]&8)&&(b->mem.io[0x0F]&8));
    const auto startA=a->serializeState(),startB=b->serializeState();devices::Rollback rollback(*a,*b,cable);for(int n=0;n<7;++n)rollback.advance(0,0);CHECK(rollback.correct(2,16,false));
    const auto replayA=a->serializeState(),replayB=b->serializeState();CHECK(a->deserializeState(startA)&&b->deserializeState(startB));cable.synchronize();
    for(int n=0;n<7;++n){InputMovie::setJoypad(a->mem,n==2?16:0);InputMovie::setJoypad(b->mem,0);cable.runFrame();}CHECK(replayA==a->serializeState()&&replayB==b->serializeState());cable.disconnect();
    std::cout<<"Accurate serial and rollback replay PASS\n";
    studio::Debugger debugger(*a);a->cpu.PC=0xC100;a->mem.write(0xC100,0xFA);a->mem.write(0xC101,0x23);a->mem.write(0xC102,0xC0);debugger.addBusWatchpoint(0xC023,studio::BusAccess::Read);
    CHECK(debugger.run(1).reason==studio::StopReason::Watchpoint);CHECK(debugger.busHit()&&!debugger.busHit()->write);
    cheats::Engine cheats(a->mem);CHECK(cheats.add("016300C0"));cheats.applyRAM();CHECK(a->mem.read(0xC000)==99);
    a->mem.write(0xFF40,0);a->cpu.PC=0x100;CHECK(cheats.add("C91-00F"));a->cpu.SP=0xC100;a->mem.write(0xC100,0x23);a->mem.write(0xC101,0xC1);a->cpu.step();CHECK(a->cpu.PC==0xC123);
    std::cout<<"Backend debugger, RAM cheats and executed ROM substitution PASS\n";
    cheats.setEnabled(false);debugger.clearBusWatchpoints();
    // PPU bus events originate from the actual VRAM fetcher, not CPU reads.
    a->mem.write(0xFF40,0);a->mem.pokeVRAM(0,0x1800,0);a->mem.pokeVRAM(0,0,0xA5);a->mem.write(0xC000,0x18);a->mem.write(0xC001,0xFE);a->mem.write(0xFFFF,0);a->cpu.PC=0xC000;a->mem.write(0xFF40,0x91);
    debugger.addBusWatchpoint(0x8000,studio::BusAccess::Read);debugger.setPPUWatches(true);CHECK(debugger.run(10000).reason==studio::StopReason::Watchpoint);CHECK(debugger.busHit()&&debugger.busHit()->ppu&&debugger.busHit()->address==0x8000);
    GBSInspect detail{};CHECK(a->backendInspect&&a->backendInspect(detail));CHECK(detail.bg_size<=16&&detail.obj_size<=16&&detail.fetcher<=6&&detail.selected_objects<=10);debugger.clearBusWatchpoints();
    std::cout<<"Actual PPU VRAM bus watch and native FIFO/APU inspection PASS\n";
    // Full accurate-backend TCP synchronization after both host cores have advanced.
    auto c=std::make_unique<GameBoy>(),d=std::make_unique<GameBoy>();CHECK(accurate::enable(*c)&&accurate::enable(*d)&&c->load(file)&&d->load(file));
    devices::LocalLink hostLink,guestLink;networking::Cable host,guest;unsigned port=0;
    for(unsigned candidate=28810;candidate<28840;++candidate)if(host.host(*a,*b,hostLink,candidate)){port=candidate;break;}CHECK(port&&guest.join(*c,*d,guestLink,"127.0.0.1",port));
    auto synchronize=[&](){const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(5);while((!host.connected()||!guest.connected())&&std::chrono::steady_clock::now()<limit){host.poll();guest.poll();std::this_thread::yield();}return host.connected()&&guest.connected();};
    CHECK(synchronize());CHECK(a->serializeState()==c->serializeState()&&b->serializeState()==d->serializeState());
    for(unsigned f=0;f<18;++f){unsigned attempts=0;while(host.frameNumber()<=f||guest.frameNumber()<=f){CHECK(host.poll()&&guest.poll());if(host.frameNumber()<=f)host.advance((u8)(host.frameNumber()*7));if(guest.frameNumber()<=f)guest.advance((u8)(guest.frameNumber()*11));CHECK(++attempts<20000);std::this_thread::yield();}}
    for(int i=0;i<3000;++i){CHECK(host.poll()&&guest.poll());std::this_thread::yield();}if(a->serializeState()!=c->serializeState()||b->serializeState()!=d->serializeState()){for(auto pair:{std::pair{a.get(),c.get()},std::pair{b.get(),d.get()}}){const auto x=pair.first->serializeState(),y=pair.second->serializeState();unsigned shown=0;for(std::size_t i=0;i<std::min(x.size(),y.size())&&shown<15;++i)if(x[i]!=y[i]){std::cerr<<i<<":"<<(int)x[i]<<"/"<<(int)y[i]<<" ";++shown;}std::cerr<<"\n";}return 1;}
    guest.close();CHECK(!host.poll()&&host.active());CHECK(guest.join(*c,*d,guestLink,"127.0.0.1",port));CHECK(synchronize());CHECK(a->serializeState()==c->serializeState()&&b->serializeState()==d->serializeState());host.close();guest.close();
    std::cout<<"Accurate TCP state synchronization, late input convergence and automatic host reconnect PASS\n";
    // MBC6 exercises both independently banked windows and persistent flash.
    auto cartridge=std::vector<u8>(131072);for(unsigned bank=0;bank<16;++bank)std::fill_n(cartridge.begin()+bank*8192,8192,(u8)bank);
    std::copy(rom.begin(),rom.begin()+0x150,cartridge.begin());cartridge[0x147]=0x20;cartridge[0x149]=3;
    const auto mbcFile=dir/"mbc6.gb";CHECK(Memory::writeAtomic(mbcFile,{(const char*)cartridge.data(),cartridge.size()}));
    auto m=std::make_unique<GameBoy>();CHECK(accurate::enable(*m)&&m->load(mbcFile));m->mem.write(0x2000,7);m->mem.write(0x3000,11);
    CHECK(m->mem.read(0x4000)==7&&m->mem.read(0x6000)==11);m->mem.write(0,10);m->mem.write(0x400,3);m->mem.write(0x800,5);m->mem.write(0xA000,0xA3);m->mem.write(0xB000,0xB5);
    m->mem.write(0x400,5);CHECK(m->mem.read(0xA000)==0xB5);m->mem.write(0x400,3);CHECK(m->mem.read(0xA000)==0xA3);
    m->mem.write(0xC00,1);m->mem.write(0x2800,8);
    auto flashWrite=[&](unsigned bank,u16 addr,u8 value){m->mem.write(0x2000,(u8)bank);m->mem.write(addr,value);};
    auto command=[&](u8 value){flashWrite(2,0x5555,0xAA);flashWrite(1,0x4AAA,0x55);flashWrite(2,0x5555,value);};
    command(0x90);m->mem.write(0x2000,0);CHECK(m->mem.read(0x4000)==0xC2&&m->mem.read(0x4001)==0x81);m->mem.write(0x4000,0xF0);
    command(0xA0);m->mem.write(0x2000,16);for(int i=0;i<128;++i)m->mem.write((u16)(0x4000+i),(u8)(i^0x5A));m->mem.write(0x407F,0);CHECK(m->mem.read(0x4000)&0x80);m->mem.write(0x4000,0xF0);
    CHECK(m->mem.read(0x4000)==0x5A&&m->mem.read(0x407F)==(127^0x5A));const auto flashState=m->serializeState();CHECK(m->mem.saveBattery());
    m->mem.write(0x2000,1);CHECK(m->deserializeState(flashState)&&m->mem.read(0x4000)==0x5A);auto n=std::make_unique<GameBoy>();CHECK(accurate::enable(*n)&&n->load(mbcFile));n->mem.write(0xC00,1);n->mem.write(0x2800,8);n->mem.write(0x2000,16);CHECK(n->mem.read(0x4000)==0x5A);
    const auto portable=saves::exportBess(*m);CHECK(saves::importBess(*n,portable));CHECK(m->serializeState()==n->serializeState());
    command(0x80);flashWrite(2,0x5555,0xAA);flashWrite(1,0x4AAA,0x55);flashWrite(16,0x4000,0x30);m->mem.write(0x4000,0xF0);CHECK(m->mem.read(0x4000)==0xFF);
    std::cout<<"MBC6 dual ROM/RAM windows, JEDEC ID, flash program/erase/battery/BESS PASS\n";
    std::filesystem::remove_all(dir);return 0;
}
