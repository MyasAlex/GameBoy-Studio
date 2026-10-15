#include "../src/network_tools.hpp"
#include "../src/cheat_engine.hpp"
#include "../src/link_devices.hpp"
#include "../src/studio_tools.hpp"
#include "../src/media_tools.hpp"
#include "../src/archive_loader.hpp"
#include "../src/lua_tools.hpp"
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';std::exit(1);}}while(false)
static bool equalStates(const GameBoy& a,const GameBoy& b,int mode,const char* phase){
    const auto left=a.serializeState(),right=b.serializeState();if(left==right)return true;
    std::cerr<<"State mismatch mode="<<mode<<" phase="<<phase<<" bytes="<<left.size()<<'/'<<right.size()<<'\n';
    unsigned printed=0;for(std::size_t i=0;i<std::min(left.size(),right.size());++i)if(left[i]!=right[i]&&printed++<24)
        std::cerr<<"  offset="<<i<<" expected="<<(unsigned)left[i]<<" actual="<<(unsigned)right[i]<<'\n';
    std::cerr<<"  differing bytes="<<printed<<" field-encoded portable equal="<<(saves::portable(a)==saves::portable(b))<<'\n';return false;
}
static void poisonNativePadding(GameBoy& g){
    auto fill=[](auto& object,std::size_t from,std::size_t to){if(to>from)std::memset((u8*)&object+from,0xA5,to-from);};
    for(auto& sprite:g.ppu.state.sprites)fill(sprite,offsetof(PPU::Sprite,attr)+sizeof(sprite.attr),sizeof(sprite));
    fill(g.ppu.state,offsetof(PPU::Pipeline,fetched)+sizeof(g.ppu.state.fetched),sizeof(g.ppu.state));
    for(auto& channel:g.apu.state.channels)fill(channel,offsetof(APU::Channel,started)+sizeof(channel.started),sizeof(channel));
    fill(g.apu.state,offsetof(APU::State,waveBuffer)+sizeof(g.apu.state.waveBuffer),offsetof(APU::State,sampleClock));
    fill(g.apu.state,offsetof(APU::State,mixCount)+sizeof(g.apu.state.mixCount),offsetof(APU::State,sumLeft));
}
static void portableStates(const std::filesystem::path& dmg,const std::filesystem::path& cgb){
    for(int mode=0;mode<3;++mode){GameBoy a,b;const auto cart=mode==1?cgb:dmg;const auto model=mode==2?HardwareModel::CGB:HardwareModel::Auto;
        CHECK(a.load(cart,model)&&b.load(cart,model));for(int n=0;n<7;++n)a.runFrame();a.mem.wram[100]=0x42;a.mem.a=true;a.apu.setSampleRate(96000);
        const auto unpoisoned=a.serializeState();poisonNativePadding(a);CHECK(a.serializeState()==unpoisoned);
        const auto bytes=saves::exportBess(a);CHECK(!bytes.empty());std::string error;CHECK(saves::importBess(b,bytes,&error));
        CHECK(equalStates(a,b,mode,"BESS import"));for(int n=0;n<5;++n){a.runFrame();b.runFrame();}CHECK(equalStates(a,b,mode,"continued execution"));
        const auto old=b.serializeState();auto bad=bytes;bad[bad.size()-100]^=0x10;CHECK(!saves::importBess(b,bad));CHECK(b.serializeState()==old);
        bad=bytes;saves::set32(bad,bad.size()-8,0xFFFFFFFF);CHECK(!saves::importBess(b,bad)&&b.serializeState()==old);
        // Remove our extension to exercise independent BESS CORE/MBC importing.
        auto foreign=bytes;std::size_t at=saves::le32(bytes,bytes.size()-8);
        while(at<foreign.size()-8){const u32 size=saves::le32(foreign,at+4);if(!std::memcmp(foreign.data()+at,"GBSP",4)){foreign.erase(foreign.begin()+at,foreign.begin()+at+8+size);break;}at+=8+size;}
        CHECK(saves::importBess(b,foreign,&error));CHECK(b.mem.wram[100]==0x42);b.runFrame();CHECK(!b.cpu.locked);
    }
    std::cout<<"Portable/BESS exact replay and foreign import PASS\n";
}
static void serial(const std::filesystem::path& rom){
    GameBoy a,b;CHECK(a.load(rom)&&b.load(rom));devices::LocalLink link;link.connect(a,b);
    a.mem.io[0x0F]=b.mem.io[0x0F]=0;a.mem.write(0xFF01,0xA5);b.mem.write(0xFF01,0x3C);
    b.mem.write(0xFF02,0x80);a.mem.write(0xFF02,0x81);
    link.runFrame();CHECK(a.mem.read(0xFF01)==0x3C&&b.mem.read(0xFF01)==0xA5);CHECK((a.mem.io[0x0F]&8)&&(b.mem.io[0x0F]&8));
    a.mem.write(0xFF01,0x96);b.mem.write(0xFF01,0x69);b.mem.write(0xFF02,0x80);a.mem.write(0xFF02,0x81);
    for(int n=0;n<3;++n)a.mem.serialClock();const auto partialA=a.serializeState(),partialB=b.serializeState();
    for(int n=0;n<5;++n)a.mem.serialClock();CHECK(a.mem.io[1]==0x69&&b.mem.io[1]==0x96);
    CHECK(a.deserializeState(partialA)&&b.deserializeState(partialB));for(int n=0;n<5;++n)a.mem.serialClock();CHECK(a.mem.io[1]==0x69&&b.mem.io[1]==0x96);
    link.disconnect();a.mem.write(0xFF02,0x81);a.runFrame();CHECK(a.mem.io[1]==0xFF);
    link.connect(a,b);a.mem.cgbMode=b.mem.cgbMode=true;b.mem.io[0x56]=1;a.mem.io[0x56]=0xC0;CHECK((a.mem.read(0xFF56)&2)==0);
    b.mem.io[0x56]=0;CHECK(a.mem.read(0xFF56)&2);
    std::cout<<"Link serial, mid-byte states, disconnected cable, IR PASS\n";
}
static void cheating(const std::filesystem::path& rom){
    GameBoy g;CHECK(g.load(rom));const u8 original=g.mem.read(0x100);cheats::Engine engine(g.mem);
    CHECK(engine.add("C91-00F","ROM substitute"));CHECK(g.mem.read(0x100)==0xC9);engine.setEnabled(false);CHECK(g.mem.read(0x100)==original);engine.setEnabled(true);
    CHECK(engine.add("016300C0","99 at C000"));engine.applyRAM();CHECK(g.mem.read(0xC000)==99);g.mem.write(0xC000,1);engine.applyRAM();CHECK(g.mem.read(0xC000)==99);
    CHECK(!engine.add("unsupported")&&!engine.add("01990080"));cheats::Search search;search.begin(g.mem);g.mem.write(0xC123,17);search.filter(g.mem,cheats::Compare::Changed);
    CHECK(search.results().size()==1&&search.results()[0]==0xC123);
    g.mem.write(0xFF40,0);g.mem.wram[0]=0xFA;g.mem.wram[1]=0x10;g.mem.wram[2]=0xC0;g.mem.wram[3]=0xEA;g.mem.wram[4]=0x10;g.mem.wram[5]=0xC0;
    g.cpu.PC=0xC000;studio::Debugger debugger(g);debugger.addBusWatchpoint(0xC010,studio::BusAccess::Read);debugger.setProfiling(true);
    auto result=debugger.run(3);CHECK(result.reason==studio::StopReason::Watchpoint&&result.steps==1);CHECK(debugger.busHit()&&!debugger.busHit()->write);
    debugger.clearBusWatchpoints();debugger.addBusWatchpoint(0xC010,studio::BusAccess::Write);result=debugger.run(3);
    CHECK(result.reason==studio::StopReason::Watchpoint&&result.steps==1&&debugger.busHit()->write);CHECK(debugger.opcodeCount[0xFA]==1&&debugger.opcodeCount[0xEA]==1);
    std::cout<<"Working cheats, RAM search, read/same-value write watchpoints PASS\n";
}
static void eeprom(){
    GameBoy g;g.mem.mbc=7;g.mem.eram.assign(256,0xFF);g.mem.write(0,0xA);g.mem.write(0x4000,0x40);
    g.mem.motion.tiltX=112;g.mem.motion.tiltY=-112;g.mem.write(0xA000,0x55);g.mem.write(0xA010,0xAA);
    CHECK(g.mem.read(0xA020)==0x40&&g.mem.read(0xA030)==0x82);CHECK(g.mem.read(0xA040)==0x60&&g.mem.read(0xA050)==0x81);
    auto bit=[&](bool b){g.mem.write(0xA080,(u8)(0x80|(b?2:0)));g.mem.write(0xA080,(u8)(0xC0|(b?2:0)));};
    auto command=[&](unsigned c){g.mem.write(0xA080,0);bit(true);for(int i=9;i>=0;--i)bit((c>>i)&1);};
    command(0xC0);CHECK(g.mem.motion.writeEnabled);command(0x105);for(int i=15;i>=0;--i)bit((0xABCD>>i)&1);
    CHECK(g.mem.eram[10]==0xAB&&g.mem.eram[11]==0xCD);command(0x205);u16 value=0;
    for(int i=0;i<16;++i){value=(u16)((value<<1)|(g.mem.read(0xA080)&1));bit(false);}CHECK(value==0xABCD);
    command(0);CHECK(!g.mem.motion.writeEnabled);command(0x305);CHECK(g.mem.eram[10]==0xAB);
    std::cout<<"MBC7 accelerometer and EEPROM command sequence PASS\n";
}
static void printer(const std::filesystem::path& dir){
    devices::Printer p;auto send=[&](u8 command,const std::vector<u8>& payload){std::vector<u8> packet={0x88,0x33,command,0,(u8)payload.size(),(u8)(payload.size()>>8)};
        packet.insert(packet.end(),payload.begin(),payload.end());u16 sum=0;for(std::size_t i=2;i<packet.size();++i)sum+=packet[i];packet.push_back((u8)sum);packet.push_back((u8)(sum>>8));
        for(u8 b:packet)p.transfer(b);CHECK(p.transfer(0)==0x81);return p.transfer(0);};
    CHECK(send(1,{})==0);std::vector<u8> tiles(640);for(std::size_t i=0;i<tiles.size();i+=2)tiles[i]=0xFF;
    CHECK(send(4,tiles)==8);send(4,{});send(2,{1,0,0xE4,0x40});CHECK(p.pageCount()==1&&p.saveLast(dir/"printer.bmp"));
    std::cout<<"Printer packet/checksum, tiles and BMP output PASS\n";
}
static void audio(const std::filesystem::path& dir){
    for(int rate:{44100,48000,96000}){GameBoy g;CHECK(g.apu.setSampleRate(rate));unsigned frames=0;for(int i=0;i<GB_CLOCK;++i){g.apu.tick();if(g.apu.available()>=512){frames+=(unsigned)g.apu.available();g.apu.clearQueue();}}
        frames+=(unsigned)g.apu.available();CHECK(frames==(unsigned)rate);}
    std::vector<host_audio::StereoFrame> frames(8195);for(std::size_t n=0;n<frames.size();++n)frames[n]={(std::int16_t)((n*17)%60000-30000),(std::int16_t)(30000-(n*13)%60000)};
    media::FlacWriter flac;CHECK(flac.open(dir/"test.flac",96000)&&flac.append(frames)&&flac.close());
    host_audio::WavWriter wav;CHECK(wav.open(dir/"test.wav",96000)&&wav.append(frames)&&wav.close());
    std::ofstream raw(dir/"expected.pcm",std::ios::binary);for(const auto& frame:frames){const u8 bytes[]={(u8)frame.left,(u8)((u16)frame.left>>8),(u8)frame.right,(u8)((u16)frame.right>>8)};raw.write((const char*)bytes,4);}
    std::cout<<"44.1/48/96 kHz clocking and WAV/FLAC encoding PASS (independent decode required)\n";
}
static void luaTests(const std::filesystem::path& rom,const std::filesystem::path& dir){
    GameBoy g;CHECK(g.load(rom));scripting::Lua lua(g);auto path=dir/"script.lua";CHECK(Memory::writeAtomic(path,"assert(os == nil and io == nil and package == nil and pcall == nil and xpcall == nil and setmetatable == nil)\nfunction on_frame() gb.write(0xC023, gb.read(0xC023)+1) end\n"));
    CHECK(lua.load(path)&&lua.onFrame()&&lua.onFrame()&&g.mem.read(0xC023)==2);
    CHECK(Memory::writeAtomic(path,"while true do end\n"));CHECK(!lua.load(path)&&!lua.active());
    std::cout<<"Lua frame hook, bounded execution and restricted libraries PASS\n";
}
static void cameraTests(const std::filesystem::path& demo,const std::filesystem::path& dir){
    auto rom=Memory::loadBytes(demo);rom[0x147]=0xFC;const auto path=dir/"camera.gb";CHECK(Memory::writeAtomic(path,{(const char*)rom.data(),rom.size()}));
    auto g=std::make_unique<GameBoy>();CHECK(g->load(path)&&g->mem.mbc==252&&g->mem.eram.size()==131072);
    g->mem.cameraImage=[](){std::array<u8,128*112> pixels{};for(unsigned y=0;y<112;++y)for(unsigned x=0;x<128;++x)pixels[y*128+x]=(u8)(x*2);return pixels;};
    g->mem.write(0x4000,16);for(unsigned cell=0;cell<16;++cell){g->mem.write((u16)(0xA006+cell*3),64);g->mem.write((u16)(0xA007+cell*3),128);g->mem.write((u16)(0xA008+cell*3),192);}
    g->mem.write(0xA000,1);CHECK(g->mem.read(0xA000)&1);g->mem.write(0x4000,0);CHECK(g->mem.read(0xA100)==0);
    g->mem.tick(500);const auto state=g->serializeState();g->mem.tick(g->mem.cameraTicks);CHECK(g->mem.eram[0x100]==0xFF&&g->mem.eram[0x101]==0xFF&&g->mem.eram[0x100+15*16]==0);
    const auto complete=g->serializeState();CHECK(g->deserializeState(state));g->mem.tick(g->mem.cameraTicks);CHECK(g->serializeState()==complete);
    g->mem.write(0x4000,16);CHECK((g->mem.read(0xA000)&1)==0);g->mem.write(0xA000,1);g->mem.tick(400);g->mem.write(0xA000,0);const int remaining=g->mem.cameraTicks;g->mem.tick(400);CHECK(g->mem.cameraTicks==remaining);g->mem.write(0xA000,1);g->mem.tick(remaining);CHECK(g->mem.cameraTicks==0);
    std::cout<<"Camera banking, thresholds/2bpp, busy timing, pause/resume and capture states PASS\n";
}
static void networkTests(const std::filesystem::path& rom){
    GameBoy a,b,c,d;CHECK(a.load(rom)&&b.load(rom)&&c.load(rom)&&d.load(rom));devices::LocalLink hostLink,guestLink;networking::Cable host,guest;
    a.mem.wram[45]=0x77;b.mem.wram[46]=0x66;
    unsigned port=0;for(unsigned candidate=28761;candidate<28781;++candidate)if(host.host(a,b,hostLink,candidate)){port=candidate;break;}CHECK(port&&guest.join(c,d,guestLink,"127.0.0.1",port));
    for(int i=0;i<4000&&(!host.connected()||!guest.connected());++i){CHECK(host.poll()&&guest.poll());std::this_thread::yield();}
    CHECK(host.connected()&&guest.connected()&&c.mem.wram[45]==0x77&&d.mem.wram[46]==0x66);
    // Different input changes every frame, deliberately delivered after prediction.
    for(unsigned frame=0;frame<24;++frame){unsigned attempts=0;while(host.frameNumber()<=frame||guest.frameNumber()<=frame){
            CHECK(host.poll()&&guest.poll());if(host.frameNumber()<=frame)host.advance((u8)(host.frameNumber()*7));
            if(guest.frameNumber()<=frame)guest.advance((u8)(guest.frameNumber()*13));CHECK(++attempts<20000);std::this_thread::yield();}}
    for(int i=0;i<1000;++i){CHECK(host.poll()&&guest.poll());std::this_thread::yield();}
    CHECK(a.serializeState()==c.serializeState()&&b.serializeState()==d.serializeState());host.close();guest.close();
    std::cout<<"TCP synchronization, predicted inputs and exact rollback convergence PASS\n";
}
int main(int argc,char** argv){CHECK(argc>=3);const auto dir=argc>3?std::filesystem::path(argv[3]):std::filesystem::temp_directory_path()/("gbstudio-feature-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);portableStates(argv[1],argv[2]);serial(argv[1]);cheating(argv[1]);eeprom();printer(dir);audio(dir);cameraTests(argv[1],dir);networkTests(argv[1]);luaTests(argv[1],dir);
    media::AviWriter video;GameBoy g;CHECK(g.load(argv[1])&&video.open(dir/"test.avi"));for(int n=0;n<10;++n){g.runFrame();CHECK(video.append(g.ppu.pixels));}CHECK(video.close());
    std::cout<<"ALL FEATURE PACK TESTS PASSED: "<<dir<<'\n';return 0;
}
