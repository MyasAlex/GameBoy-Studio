#include "../src/network_tools.hpp"
#include "../src/portable_state.hpp"
#include "../src/cheat_engine.hpp"
#include "../src/studio_tools.hpp"
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main(int argc,char** argv){std::cout<<std::unitbuf;CHECK(argc==3);
    const auto dir=std::filesystem::temp_directory_path()/("gb-native-integration-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
    const auto rom=Memory::loadBytes(argv[1]);const auto file=dir/"demo.gb";CHECK(Memory::writeAtomic(file,{(const char*)rom.data(),rom.size()}));
    for(auto model:{HardwareModel::DMG,HardwareModel::Pocket,HardwareModel::Light,HardwareModel::CGB0,HardwareModel::CGBA,HardwareModel::CGBB,HardwareModel::CGBC,HardwareModel::CGBD,HardwareModel::CGBE,HardwareModel::AGB}){
        auto a=std::make_unique<GameBoy>(),b=std::make_unique<GameBoy>();CHECK(a->load(file,model)&&b->load(file,model));
        for(int n=0;n<4;++n)a->runFrame();a->mem.write(0xC023,0xA5);a->apu.setSampleRate(96000);a->mem.a=true;
        const auto state=a->serializeState();CHECK(!state.empty()&&b->deserializeState(state));if(a->serializeState()!=b->serializeState()){const auto x=a->serializeState(),y=b->serializeState();std::cerr<<"State mismatch model "<<(int)model<<" sizes "<<x.size()<<" "<<y.size()<<"\n";unsigned shown=0;for(std::size_t i=0;i<std::min(x.size(),y.size())&&shown<30;++i)if(x[i]!=y[i]){std::cerr<<i<<":"<<(int)x[i]<<"/"<<(int)y[i]<<" ";++shown;}std::cerr<<"\n";return 1;}
        for(int n=0;n<3;++n){a->runFrame();b->runFrame();}if(a->serializeState()!=b->serializeState()){const auto x=a->serializeState(),y=b->serializeState();std::cerr<<"Continuation mismatch model "<<(int)model<<"\n";unsigned shown=0;for(std::size_t i=0;i<std::min(x.size(),y.size())&&shown<20;++i)if(x[i]!=y[i]){std::cerr<<i<<":"<<(int)x[i]<<"/"<<(int)y[i]<<" ";++shown;}std::cerr<<"\n";return 1;}
        const auto bess=saves::exportBess(*a);CHECK(saves::importBess(*b,bess));CHECK(a->serializeState()==b->serializeState());
        auto bad=state;bad[100]^=1;const auto previous=b->serializeState();CHECK(!b->deserializeState(bad)&&previous==b->serializeState());
        std::cout<<"Model "<<(int)model<<" exact state/BESS continuation PASS\n";
    }
    auto a=std::make_unique<GameBoy>(),b=std::make_unique<GameBoy>();CHECK(a->load(file)&&b->load(file));
    a->mem.write(0xFF40,0);b->mem.write(0xFF40,0);a->cpu.PC=b->cpu.PC=0xC000;a->mem.write(0xC000,0x18);a->mem.write(0xC001,0xFE);b->mem.write(0xC000,0x18);b->mem.write(0xC001,0xFE);
    devices::LocalLink cable;cable.connect(*a,*b);a->mem.write(0xFF01,0xA5);b->mem.write(0xFF01,0x3C);b->mem.write(0xFF02,0x80);a->mem.write(0xFF02,0x81);cable.runFrame();
    CHECK(a->mem.read(0xFF01)==0x3C&&b->mem.read(0xFF01)==0xA5);CHECK((a->mem.io[0x0F]&8)&&(b->mem.io[0x0F]&8));
    const auto startA=a->serializeState(),startB=b->serializeState();devices::Rollback rollback(*a,*b,cable);for(int n=0;n<7;++n)rollback.advance(0,0);CHECK(rollback.correct(2,16,false));
    const auto replayA=a->serializeState(),replayB=b->serializeState();CHECK(a->deserializeState(startA)&&b->deserializeState(startB));cable.synchronize();
    for(int n=0;n<7;++n){InputMovie::setJoypad(a->mem,n==2?16:0);InputMovie::setJoypad(b->mem,0);cable.runFrame();}CHECK(replayA==a->serializeState()&&replayB==b->serializeState());cable.disconnect();
    std::cout<<"Native serial and rollback replay PASS\n";
    studio::Debugger debugger(*a);a->cpu.PC=0xC100;a->mem.write(0xC100,0xFA);a->mem.write(0xC101,0x23);a->mem.write(0xC102,0xC0);debugger.addBusWatchpoint(0xC023,studio::BusAccess::Read);
    CHECK(debugger.run(1).reason==studio::StopReason::Watchpoint);CHECK(debugger.busHit()&&!debugger.busHit()->write);
    cheats::Engine cheats(a->mem);CHECK(cheats.add("016300C0"));cheats.applyRAM();CHECK(a->mem.read(0xC000)==99);
    a->mem.write(0xFF40,0);a->cpu.PC=0x100;CHECK(cheats.add("C91-00F"));a->cpu.SP=0xC100;a->mem.write(0xC100,0x23);a->mem.write(0xC101,0xC1);a->cpu.step();CHECK(a->cpu.PC==0xC123);
    std::cout<<"Native debugger, RAM cheats and executed ROM substitution PASS\n";
    cheats.setEnabled(false);debugger.clearBusWatchpoints();
    // PPU events originate from the native renderer's actual VRAM reads.
    a->mem.write(0xFF40,0);a->mem.pokeVRAM(0,0x1800,0);a->mem.pokeVRAM(0,0,0xA5);a->mem.write(0xC000,0x18);a->mem.write(0xC001,0xFE);a->mem.write(0xFFFF,0);a->cpu.PC=0xC000;a->mem.write(0xFF40,0x91);
    debugger.addBusWatchpoint(0x8000,studio::BusAccess::Read);debugger.setPPUWatches(true);CHECK(debugger.run(10000).reason==studio::StopReason::Watchpoint);CHECK(debugger.busHit()&&debugger.busHit()->ppu&&debugger.busHit()->address==0x8000);
    CHECK(a->ppu.state.selectedCount>=0&&a->ppu.state.selectedCount<=10);debugger.clearBusWatchpoints();
    std::cout<<"Native PPU VRAM bus watch PASS\n";
    // Native TCP synchronization after both host cores have advanced.
    auto c=std::make_unique<GameBoy>(),d=std::make_unique<GameBoy>();CHECK(c->load(file)&&d->load(file));
    devices::LocalLink hostLink,guestLink;networking::Cable host,guest;unsigned port=0;
    // Separate test processes may use the same network namespace. Vary the
    // candidate range so another run cannot occupy our port during reconnect.
    const unsigned firstCandidate=30000+(unsigned)((u64)std::chrono::steady_clock::now().time_since_epoch().count()%20000);
    for(unsigned attempt=0;attempt<30;++attempt){const unsigned candidate=30000+(firstCandidate-30000+attempt)%20000;
        if(host.host(*a,*b,hostLink,candidate)){port=candidate;break;}}
    CHECK(port);
    {networking::Cable competing;devices::LocalLink competingLink;
        CHECK(!competing.host(*c,*d,competingLink,port)&&!competing.active()&&!competing.lastErrorText().empty());}
    CHECK(guest.join(*c,*d,guestLink,"127.0.0.1",port));
    auto networkStatus=[&](){std::cerr<<"Host: "<<host.status()<<" ("<<host.lastErrorText()<<"); guest: "<<guest.status()<<" ("<<guest.lastErrorText()<<")\n";};
    auto synchronize=[&](){
        const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(15);
        while((!host.connected()||!guest.connected())&&std::chrono::steady_clock::now()<limit){
            const bool hostOk=host.poll(),guestOk=guest.poll();
            // A disconnected peer may report failure while scheduling its
            // automatic retry. Terminal failures still abort immediately.
            if((!hostOk&&!host.active())||(!guestOk&&!guest.active())){networkStatus();return false;}
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if(!host.connected()||!guest.connected()){networkStatus();return false;}return true;
    };
    CHECK(synchronize());CHECK(a->serializeState()==c->serializeState()&&b->serializeState()==d->serializeState());
    for(unsigned f=0;f<18;++f){unsigned attempts=0;while(host.frameNumber()<=f||guest.frameNumber()<=f){CHECK(host.poll()&&guest.poll());if(host.frameNumber()<=f)host.advance((u8)(host.frameNumber()*7));if(guest.frameNumber()<=f)guest.advance((u8)(guest.frameNumber()*11));CHECK(++attempts<20000);std::this_thread::yield();}}
    for(int i=0;i<3000;++i){CHECK(host.poll()&&guest.poll());std::this_thread::yield();}if(a->serializeState()!=c->serializeState()||b->serializeState()!=d->serializeState()){for(auto pair:{std::pair{a.get(),c.get()},std::pair{b.get(),d.get()}}){const auto x=pair.first->serializeState(),y=pair.second->serializeState();unsigned shown=0;for(std::size_t i=0;i<std::min(x.size(),y.size())&&shown<15;++i)if(x[i]!=y[i]){std::cerr<<i<<":"<<(int)x[i]<<"/"<<(int)y[i]<<" ";++shown;}std::cerr<<"\n";}return 1;}
    guest.close();CHECK(!host.poll()&&host.active());CHECK(guest.join(*c,*d,guestLink,"127.0.0.1",port));CHECK(synchronize());CHECK(a->serializeState()==c->serializeState()&&b->serializeState()==d->serializeState());host.close();guest.close();
    std::cout<<"Native TCP state synchronization, late input convergence and automatic host reconnect PASS\n";
    // Unsupported controllers must fail explicitly before emulation starts.
    auto cartridge=rom;cartridge[0x147]=0x20;
    const auto mbcFile=dir/"mbc6.gb";CHECK(Memory::writeAtomic(mbcFile,{(const char*)cartridge.data(),cartridge.size()}));
    auto m=std::make_unique<GameBoy>();CHECK(!m->load(mbcFile)&&!m->mem.loadError.empty());
    std::cout<<"Unsupported MBC6 explicit rejection PASS\n";
    std::filesystem::remove_all(dir);return 0;
}
