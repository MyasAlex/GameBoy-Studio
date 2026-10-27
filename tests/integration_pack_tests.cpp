#include "../src/portable_state.hpp"
#include "../src/link_devices.hpp"
#include "../src/cheat_engine.hpp"
#include "../src/lua_tools.hpp"
#include "../src/media_tools.hpp"
#include <iostream>
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main(int argc,char** argv){
    CHECK(argc==2||argc==3);const bool retain=argc==3;
    const auto dir=retain?std::filesystem::path(argv[2]):std::filesystem::temp_directory_path()/("gb-integration-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
    const auto original=Memory::loadBytes(argv[1]);
    auto a=std::make_unique<GameBoy>(),b=std::make_unique<GameBoy>();CHECK(a->load(argv[1])&&b->load(argv[1]));
    for(auto* g:{a.get(),b.get()}){g->mem.write(0xFF40,0);g->mem.write(0xC000,0x18);g->mem.write(0xC001,0xFE);g->cpu.PC=0xC000;}
    devices::LocalLink cable;cable.connect(*a,*b);a->mem.write(0xFF01,0xA5);b->mem.write(0xFF01,0x3C);b->mem.write(0xFF02,0x80);a->mem.write(0xFF02,0x81);
    for(int i=0;i<90;++i){a->cpu.step();b->cpu.step();}const auto pair=devices::savePair(*a,*b,cable);cable.runFrame();const auto finalA=a->serializeState(),finalB=b->serializeState();
    CHECK(devices::loadPair(*a,*b,cable,pair));cable.runFrame();CHECK(finalA==a->serializeState()&&finalB==b->serializeState());
    auto bad=pair;bad.back()^=1;CHECK(!devices::loadPair(*a,*b,cable,bad)&&finalA==a->serializeState()&&finalB==b->serializeState());cable.disconnect();
    std::cout<<"Mid-transfer paired restore and corrupt-pair atomic rejection PASS\n";
    // Replay must use captured camera images rather than a changing live input.
    for(u8 type:{u8{0x22},u8{0xFC}}){auto rom=std::vector<u8>(131072);std::copy(original.begin(),original.end(),rom.begin());rom[0x147]=type;rom[0x149]=3;
        const auto path=dir/(type==0x22?"motion.gb":"camera.gb");CHECK(Memory::writeAtomic(path,{(const char*)rom.data(),rom.size()}));auto g=std::make_unique<GameBoy>();CHECK(g->load(path));
        g->mem.write(0xFF40,0);g->mem.write(0xC000,0x18);g->mem.write(0xC001,0xFE);g->cpu.PC=0xC000;unsigned captures=0;
        g->mem.cameraImage=[&](){std::array<u8,128*112> pixels{};++captures;for(std::size_t i=0;i<pixels.size();++i)pixels[i]=(u8)(i+23*captures);return pixels;};
        auto trigger=[&](){g->mem.write(0,10);g->mem.write(0x4000,16);g->mem.write(0xA000,0);g->mem.write(0xA000,1);};
        InputMovie movie;CHECK(movie.startRecording(*g));std::vector<std::vector<u8>> states;
        for(int frame=0;frame<14;++frame){InputMovie::setJoypad(g->mem,(u8)(frame*17));g->mem.motion.tiltX=(std::int16_t)(frame*7-40);g->mem.motion.tiltY=(std::int16_t)(50-frame*6);CHECK(movie.beforeFrame(*g));if(type==0xFC&&(frame==2||frame==8))trigger();g->runFrame();states.push_back(g->serializeState());}
        const unsigned liveCalls=captures;movie.stop();const auto recording=dir/(type==0x22?"motion.gbmovie":"camera.gbmovie");CHECK(movie.save(recording)&&movie.startPlayback(*g,recording));
        for(int frame=0;frame<14;++frame){CHECK(movie.beforeFrame(*g));if(type==0xFC&&(frame==2||frame==8))trigger();g->runFrame();if(g->serializeState()!=states[frame]){auto x=g->serializeState();const auto& y=states[frame];std::cerr<<"Movie type "<<(int)type<<" frame "<<frame<<" sizes "<<x.size()<<"/"<<y.size()<<"\n";unsigned shown=0;for(std::size_t i=0;i<std::min(x.size(),y.size())&&shown<20;++i)if(x[i]!=y[i]){std::cerr<<i<<":"<<(int)y[i]<<"/"<<(int)x[i]<<" ";++shown;}std::cerr<<"\n";return 1;}}CHECK(captures==liveCalls);movie.stop();
        if(type==0xFC)CHECK(liveCalls>0);std::cout<<(type==0x22?"Motion":"Camera")<<" movie exact continuation PASS\n";
    }
    auto colorRom=original;colorRom[0x143]=0x80;const auto colorPath=dir/"color.gbc";CHECK(Memory::writeAtomic(colorPath,{(const char*)colorRom.data(),colorRom.size()}));auto g=std::make_unique<GameBoy>();CHECK(g->load(colorPath,HardwareModel::CGBE));g->mem.write(0xFF70,2);cheats::Engine cheats(g->mem);CHECK(cheats.add("95AB23D1"));cheats.applyRAM();CHECK((g->mem.read(0xFF70)&7)==2);g->mem.write(0xFF70,5);CHECK(g->mem.read(0xD123)==0xAB);g->mem.write(0xFF70,2);CHECK(g->mem.read(0xD123)!=0xAB);cheats.setEnabled(false);
    scripting::Plugins plugins(*g);const auto one=dir/"one.lua",two=dir/"two.lua";CHECK(Memory::writeAtomic(one,"function on_frame() gb.write(0xC020,gb.read(0xC020)+1) end"));CHECK(Memory::writeAtomic(two,"function on_frame() gb.write(0xC021,gb.read(0xC021)+3) end"));
    g->mem.write(0xC020,0);g->mem.write(0xC021,0);CHECK(plugins.toggle(one)&&plugins.toggle(two)&&plugins.onFrame());CHECK(g->mem.read(0xC020)==1&&g->mem.read(0xC021)==3);CHECK(plugins.toggle(one)&&plugins.onFrame());CHECK(g->mem.read(0xC020)==1&&g->mem.read(0xC021)==6);plugins.stop();
    std::cout<<"Physical bank cheats and isolated concurrent plugins PASS\n";
    // Native cartridge output signals must reach the facade and survive states.
    auto accessoryRom=original;accessoryRom[0x147]=0xFF;accessoryRom[0x149]=3;const auto irPath=dir/"infrared.gb";CHECK(Memory::writeAtomic(irPath,{(const char*)accessoryRom.data(),accessoryRom.size()}));auto infrared=std::make_unique<GameBoy>();CHECK(infrared->load(irPath));infrared->mem.write(0,14);infrared->mem.write(0xA000,1);CHECK(infrared->mem.hucIRLight);const auto irState=infrared->serializeState();infrared->mem.write(0xA000,0);CHECK(!infrared->mem.hucIRLight&&infrared->deserializeState(irState)&&infrared->mem.hucIRLight);
    accessoryRom[0x147]=0x1E;const auto rumblePath=dir/"rumble.gb";CHECK(Memory::writeAtomic(rumblePath,{(const char*)accessoryRom.data(),accessoryRom.size()}));auto rumble=std::make_unique<GameBoy>();CHECK(rumble->load(rumblePath));rumble->mem.write(0x4000,8);CHECK(rumble->mem.rumbleActive);const auto rumbleState=rumble->serializeState();rumble->mem.write(0x4000,0);CHECK(!rumble->mem.rumbleActive&&rumble->deserializeState(rumbleState)&&rumble->mem.rumbleActive);
    std::cout<<"Native HuC1 IR and MBC5 rumble output / state restore PASS\n";
    media::AviWriter video;CHECK(video.open(dir/"pcm.avi",48000));std::vector<media::Frame> samples(800);for(unsigned i=0;i<samples.size();++i)samples[i]={(std::int16_t)(i*31-12000),(std::int16_t)(12000-i*23)};
    for(int i=0;i<12;++i){CHECK(video.append(g->ppu.pixels)&&video.appendAudio(samples));}CHECK(video.close());
    if(!retain)std::filesystem::remove_all(dir);return 0;
}
