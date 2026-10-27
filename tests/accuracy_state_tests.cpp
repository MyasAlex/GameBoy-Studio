#include "../src/portable_state.hpp"
#include <cstdlib>

#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';std::abort();}}while(false)

struct Fixtures {
    std::filesystem::path directory=std::filesystem::temp_directory_path()/
        ("gb-accuracy-states-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixtures(){std::filesystem::create_directories(directory);}
    ~Fixtures(){std::error_code error;std::filesystem::remove_all(directory,error);}
    std::filesystem::path cartridge(bool color){
        std::vector<u8> rom(32768);rom[0x100]=0xC3;rom[0x101]=0x50;rom[0x102]=1;
        rom[0x150]=0x18;rom[0x151]=0xFE;rom[0x143]=color?0x80:0;
        const auto path=directory/(color?"color.gbc":"mono.gb");
        CHECK(Memory::writeAtomic(path,{(const char*)rom.data(),rom.size()}));return path;
    }
};

static std::unique_ptr<GameBoy> console(const std::filesystem::path& rom,HardwareModel model){
    auto g=std::make_unique<GameBoy>();CHECK(g->load(rom,model));return g;
}

static void startVideoAndSound(GameBoy& g){
    auto& m=g.mem;m.write(0xFF40,0);m.write(0xFF47,0xE4);m.write(0xFF48,0xE4);m.write(0xFF49,0xE4);
    for(unsigned tile=0;tile<4;++tile)for(unsigned row=0;row<8;++row){
        m.vram[tile*16+row*2]=(u8)(0xA5^(row*17+tile*23));m.vram[tile*16+row*2+1]=(u8)(0x5A^(row*9+tile*29));}
    for(unsigned index=0;index<1024;++index){m.vram[0x1800+index]=(u8)(index&3);m.vram[0x1C00+index]=(u8)((index+1)&3);}
    for(unsigned index=0;index<3;++index){m.oam[index*4]=16;m.oam[index*4+1]=(u8)(16+index*24);m.oam[index*4+2]=(u8)index;m.oam[index*4+3]=(u8)(index&1?0x20:0);}
    for(unsigned index=0;index<64;++index){m.bgPalette[index]=(u8)(index*13);m.objPalette[index]=(u8)(index*17);}
    m.write(0xFF26,0);m.write(0xFF26,0x80);m.write(0xFF11,0x80);m.write(0xFF12,0xF3);
    m.write(0xFF13,0xF9);m.write(0xFF14,0x87);m.write(0xFF16,0x40);m.write(0xFF17,0xA2);
    m.write(0xFF18,0xF3);m.write(0xFF19,0x87);
    for(unsigned index=0;index<16;++index)m.write((u16)(0xFF30+index),(u8)(index*17));
    m.write(0xFF1A,0x80);m.write(0xFF1C,0x20);m.write(0xFF1D,0xF7);m.write(0xFF1E,0x87);
    m.write(0xFF21,0xC2);m.write(0xFF22,0x15);m.write(0xFF23,0x80);m.write(0xFF24,0x77);m.write(0xFF25,0xFF);
    m.write(0xFF42,5);m.write(0xFF43,3);m.write(0xFF4A,0);m.write(0xFF4B,55);m.write(0xFF40,0xF3);
}

static std::vector<u8> peripheralState(const GameBoy& g){
    native_state::Codec codec;native_state::writeAccuracy(g.ppu,codec);native_state::writeAccuracy(g.apu,codec);return codec.output;
}

static void exactCycleReplay(Fixtures& fixtures){
    for(const auto& [model,fast]:std::array<std::pair<HardwareModel,bool>,4>{{
            {HardwareModel::DMG,false},{HardwareModel::CGBD,false},
            {HardwareModel::CGBE,false},{HardwareModel::CGBE,true}}}){
        const auto rom=fixtures.cartridge(model!=HardwareModel::DMG);
        auto source=console(rom,model),restored=console(rom,model);startVideoAndSound(*source);
        source->mem.doubleSpeed=fast;
        std::vector<std::vector<u8>> phases;
        unsigned captures=0;
        // Several snapshots are taken inside mode 3, while fetched pixels and
        // object data are pending. They must preserve future bus/audio output,
        // not just CPU registers and the already displayed framebuffer.
        for(unsigned dots=0;dots<1500&&captures<16;++dots){
            source->tick(1);
            if(source->mem.ppuMode!=3||source->ppu.state.line>=144||dots%7)continue;
            const auto snapshot=source->serializeState();CHECK(!snapshot.empty());
            CHECK(std::string(reinterpret_cast<const char*>(snapshot.data()),8)=="GBSTATE8");
            const auto phase=peripheralState(*source);
            if(std::find(phases.begin(),phases.end(),phase)==phases.end())phases.push_back(phase);
            CHECK(restored->deserializeState(snapshot)&&restored->serializeState()==snapshot);
            source->apu.clearQueue();restored->apu.clearQueue();
            std::vector<std::array<u32,3>> leftReads,rightReads;
            source->mem.ppuBusObserver=[&](u16 addr,u8 value,bool blocked){leftReads.push_back({addr,value,blocked?1u:0u});};
            restored->mem.ppuBusObserver=[&](u16 addr,u8 value,bool blocked){rightReads.push_back({addr,value,blocked?1u:0u});};
            for(unsigned step=0;step<140;++step){source->cpu.step();restored->cpu.step();}
            CHECK(source->serializeState()==restored->serializeState());CHECK(leftReads==rightReads);
            CHECK(source->apu.count==restored->apu.count);
            CHECK(std::equal(source->apu.queue.begin(),source->apu.queue.begin()+source->apu.count,restored->apu.queue.begin()));
            source->mem.ppuBusObserver={};restored->mem.ppuBusObserver={};
            CHECK(source->deserializeState(snapshot));++captures;
        }
        CHECK(captures==16&&phases.size()>=8);
        const auto bess=saves::exportBess(*source);CHECK(!bess.empty());
        CHECK(saves::importBess(*restored,bess)&&restored->serializeState()==source->serializeState());
        for(unsigned step=0;step<500;++step){source->cpu.step();restored->cpu.step();}
        CHECK(source->serializeState()==restored->serializeState());
    }
    std::cout<<"Native8/BESS3 mid-fetch replay preserves CPU, pixels, bus reads and audio PASS\n";
}

static void earlyWindowReplay(Fixtures& fixtures){
    for(const auto& [model,fast]:std::array<std::pair<HardwareModel,bool>,4>{{
            {HardwareModel::DMG,false},{HardwareModel::CGBD,false},
            {HardwareModel::CGBE,false},{HardwareModel::CGBE,true}}})for(u8 wx:{u8{0},u8{1}}){
        const auto rom=fixtures.cartridge(model!=HardwareModel::DMG);
        auto source=console(rom,model),native=console(rom,model),portable=console(rom,model);
        startVideoAndSound(*source);source->mem.doubleSpeed=fast;
        source->mem.write(0xFF40,0);source->mem.write(0xFF43,7);source->mem.write(0xFF4B,wx);
        source->mem.write(0xFF40,0xF1); // Disable OBJs to retain the full initial window delay.
        unsigned captures=0;int maximum=0,previousCapture=-1;bool bufferedPixels=false;
        for(unsigned dots=0;dots<1500&&captures<(wx?4u:3u);++dots){
            source->tick(1);const auto& fetch=source->ppu.accuracy;
            if(source->mem.ppuMode!=3||!fetch.windowFetch)continue;
            maximum=std::max(maximum,fetch.startup);
            if(fetch.startup<=12||(fetch.startup-13)%4||fetch.startup==previousCapture)continue;
            previousCapture=fetch.startup;bufferedPixels|=fetch.fifoSize>0;
            // The old startup limit was12. Capture the extended left-edge
            // fetch while clipped window pixels still occupy the FIFO.
            const auto snapshot=source->serializeState(),bess=saves::exportBess(*source);
            CHECK(!snapshot.empty()&&!bess.empty());
            CHECK(native->deserializeState(snapshot)&&native->serializeState()==snapshot);
            CHECK(saves::importBess(*portable,bess)&&portable->serializeState()==snapshot);
            std::array<std::vector<std::array<u32,3>>,3> reads;
            const std::array<GameBoy*,3> consoles={source.get(),native.get(),portable.get()};
            for(unsigned index=0;index<consoles.size();++index){
                consoles[index]->apu.clearQueue();
                consoles[index]->mem.ppuBusObserver=[&,index](u16 address,u8 value,bool blocked){reads[index].push_back({address,value,blocked?1u:0u});};
            }
            for(unsigned step=0;step<140;++step)for(auto* g:consoles)g->cpu.step();
            const auto future=source->serializeState();
            for(unsigned index=1;index<consoles.size();++index){
                CHECK(consoles[index]->serializeState()==future&&reads[index]==reads[0]);
                CHECK(consoles[index]->apu.count==source->apu.count);
                CHECK(std::equal(source->apu.queue.begin(),source->apu.queue.begin()+source->apu.count,consoles[index]->apu.queue.begin()));
            }
            for(auto* g:consoles)g->mem.ppuBusObserver={};
            CHECK(source->deserializeState(snapshot));++captures;
        }
        CHECK(captures==(wx?4u:3u)&&maximum==(wx?25:24)&&bufferedPixels);
    }
    std::cout<<"Native8/BESS3 clipped-window startup replay preserves FIFO, pixels, bus reads and audio PASS\n";
}

static void checksum(std::vector<u8>& bytes){saves::set32(bytes,bytes.size()-4,saves::hash(std::span(bytes).first(bytes.size()-4)));}

static std::pair<std::size_t,std::size_t> exactBlock(const std::vector<u8>& file){
    std::size_t at=saves::le32(file,file.size()-8);
    while(at+8<=file.size()-8){
        const auto size=saves::le32(file,at+4);CHECK(size<=file.size()-8-at-8);
        if(!std::memcmp(file.data()+at,"GBSP",4))return {at+8,size};
        at+=8+size;
    }
    CHECK(false);return {};
}

static void corruptionAndProfiles(Fixtures& fixtures){
    const auto rom=fixtures.cartridge(true);
    auto source=console(rom,HardwareModel::CGB),alias=console(rom,HardwareModel::CGBE),other=console(rom,HardwareModel::CGBD);
    startVideoAndSound(*source);for(unsigned i=0;i<105;++i)source->tick(1);
    const auto snapshot=source->serializeState(),stable=other->serializeState();
    CHECK(alias->deserializeState(snapshot));CHECK(!other->deserializeState(snapshot)&&other->serializeState()==stable);
    const auto bess=saves::exportBess(*source);CHECK(saves::importBess(*alias,bess));
    CHECK(!saves::importBess(*other,bess)&&other->serializeState()==stable);

    const auto v5=source->serializeState(true);
    const std::size_t accuracySizeAt=v5.size()-4+GameBoy::state6ExtensionSize;
    const auto accuracyAt=accuracySizeAt+4;
    CHECK(accuracyAt+saves::le32(snapshot,accuracySizeAt)+8==snapshot.size());
    for(std::size_t offset:{accuracyAt,accuracyAt+4,accuracyAt+8}){
        auto invalid=snapshot;invalid[offset]=0xFF;checksum(invalid);
        const auto before=source->serializeState();CHECK(!source->deserializeState(invalid)&&source->serializeState()==before);
    }
    // Range validation of newly serialized device latches runs before any
    // external progress callback can commit changes.
    std::vector<u8> progress={1,2};unsigned calls=0;
    source->auxiliaryState=[&](){return progress;};source->auxiliaryRestore=[&](std::span<const u8> bytes){++calls;progress.assign(bytes.begin(),bytes.end());return true;};
    const auto previous=source->ppu.accuracy;source->ppu.accuracy.fifoSize=17;
    const auto invalid=source->serializeState();source->ppu.accuracy=previous;
    const auto before=source->serializeState();CHECK(!source->deserializeState(invalid));CHECK(calls==0&&source->serializeState()==before);
    auto truncated=before;truncated.resize(truncated.size()-1);CHECK(!source->deserializeState(truncated)&&source->serializeState()==before);

    // Both loaders must reject invalid boolean representations and impossible
    // device latches even when the attacker recomputes the payload checksum.
    auto invalidBoolean=before;invalidBoolean[24]=2;checksum(invalidBoolean);
    CHECK(!source->deserializeState(invalidBoolean)&&source->serializeState()==before&&calls==0);
    const auto portable=saves::exportBess(*source);const auto [payload,size]=exactBlock(portable);
    native_state::Codec oldFields;saves::visit(*source,oldFields,false);
    const std::size_t profileAt=payload+12+oldFields.output.size(),ppuAt=profileAt+4;
    native_state::Codec ppuFields;native_state::writeAccuracy(source->ppu,ppuFields);
    const std::size_t apuAt=ppuAt+ppuFields.output.size();
    CHECK(apuAt<payload+size-4);
    auto rejectPortable=[&](auto mutate){
        auto malformed=portable;mutate(malformed);
        saves::set32(malformed,payload+size-4,saves::hash(std::span(malformed).subspan(payload,size-4)));
        CHECK(!saves::importBess(*source,malformed)&&source->serializeState()==before&&calls==0);
    };
    rejectPortable([&](auto& bytes){bytes[payload+24]=2;}); // IME
    rejectPortable([&](auto& bytes){bytes[profileAt]=0xFF;});
    rejectPortable([&](auto& bytes){bytes[ppuAt]=2;}); // fetch-active boolean
    rejectPortable([&](auto& bytes){bytes[apuAt]=4;}); // shared pulse-divider phase
    rejectPortable([&](auto& bytes){
        auto bad=source->ppu.accuracy;bad.fifoSize=17;native_state::Codec codec;PPU::visitAccuracy(bad,codec);
        CHECK(codec.output.size()==ppuFields.output.size());std::copy(codec.output.begin(),codec.output.end(),bytes.begin()+ppuAt);
    });
    rejectPortable([&](auto& bytes){
        auto bad=source->apu.accuracy;bad.noise.prescaler=0;native_state::Codec codec;APU::visitAccuracy(bad,codec);
        CHECK(apuAt+codec.output.size()==payload+size-4);std::copy(codec.output.begin(),codec.output.end(),bytes.begin()+apuAt);
    });
    // Unselected entries need not contain valid OAM identities. A forged
    // pending fetch into one of them must reject before its ID is dereferenced.
    const auto pipeline=source->ppu.state;const auto fetch=source->ppu.accuracy;
    source->ppu.state.selectedCount=0;source->ppu.state.sprites[9].id=1000000;
    source->ppu.accuracy.active=true;source->ppu.accuracy.objIndex=9;source->ppu.accuracy.objPhase=0;
    const auto invalidSprite=source->serializeState();
    native_state::Codec spritePayload;u32 portableVersion=0x33505347,romHash=source->romHash(),bootHash=0;
    spritePayload.fields(portableVersion,romHash,bootHash);saves::visit(*source,spritePayload);saves::put32(spritePayload.output,saves::hash(spritePayload.output));
    source->ppu.state=pipeline;source->ppu.accuracy=fetch;
    CHECK(!source->deserializeState(invalidSprite)&&source->serializeState()==before&&calls==0);
    auto invalidSpriteBess=portable;CHECK(spritePayload.output.size()==size);
    std::copy(spritePayload.output.begin(),spritePayload.output.end(),invalidSpriteBess.begin()+payload);
    CHECK(!saves::importBess(*source,invalidSpriteBess)&&source->serializeState()==before&&calls==0);
    std::cout<<"Native8 hardware-profile identity and transactional accuracy validation PASS\n";
}

static void legacyStates(Fixtures& fixtures){
    const auto rom=fixtures.cartridge(false);auto source=console(rom,HardwareModel::DMG),restored=console(rom,HardwareModel::DMG);
    source->mem.write(0xFF40,0);source->mem.wram[17]=0xA5;source->mem.a=true;
    for(unsigned i=0;i<67;++i)source->cpu.step();
    const auto historical=source->serializeState(true),current=source->serializeState();
    CHECK(restored->deserializeState(historical));CHECK(restored->cpu.PC==source->cpu.PC&&restored->mem.wram==source->mem.wram);
    // Native4 predates the seven-byte external-boot/button extension of v5.
    auto v4=historical;v4.resize(v4.size()-11);v4[7]='4';saves::put32(v4,saves::hash(v4));
    CHECK(restored->deserializeState(v4)&&restored->mem.wram[17]==0xA5);
    std::vector<u8> v6(current.begin(),current.begin()+historical.size()-4+GameBoy::state6ExtensionSize);
    v6[7]='6';saves::put32(v6,saves::hash(v6));
    CHECK(restored->deserializeState(v6)&&restored->mem.wram[17]==0xA5&&restored->mem.a);
    auto v7=v6;v7.resize(v7.size()-4);v7[7]='7';saves::put32(v7,3);v7.insert(v7.end(),{4,5,6});saves::put32(v7,saves::hash(v7));
    std::vector<u8> progress;
    restored->auxiliaryState=[&](){return progress;};restored->auxiliaryRestore=[&](std::span<const u8> bytes){progress.assign(bytes.begin(),bytes.end());return true;};
    CHECK(restored->deserializeState(v7)&&progress==std::vector<u8>({4,5,6}));

    // Convert only the exact-state block to the old field-encoded GBSP2
    // format. Its standard BESS memory offsets remain unchanged.
    auto oldBess=saves::exportBess(*source);native_state::Codec codec;
    u32 version=0x32505347,hash=source->romHash(),boot=0;codec.fields(version,hash,boot);saves::visit(*source,codec,false);saves::put32(codec.output,saves::hash(codec.output));
    std::size_t at=saves::le32(oldBess,oldBess.size()-8);bool replaced=false;
    while(at+8<=oldBess.size()-8){const auto size=saves::le32(oldBess,at+4);
        if(!std::memcmp(oldBess.data()+at,"GBSP",4)){
            oldBess.erase(oldBess.begin()+at+8,oldBess.begin()+at+8+size);oldBess.insert(oldBess.begin()+at+8,codec.output.begin(),codec.output.end());saves::set32(oldBess,at+4,(u32)codec.output.size());replaced=true;break;}
        at+=8+size;
    }
    CHECK(replaced&&saves::importBess(*restored,oldBess));
    CHECK(restored->mem.wram==source->mem.wram&&restored->cpu.PC==source->cpu.PC);
    std::cout<<"Legacy native4/5/6/7 and field-encoded GBSP2 import PASS\n";
}

int main(){Fixtures fixtures;exactCycleReplay(fixtures);earlyWindowReplay(fixtures);corruptionAndProfiles(fixtures);legacyStates(fixtures);}
