#include "../src/portable_state.hpp"
#include <random>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main(int argc,char** argv){CHECK(argc==2);auto g=std::make_unique<GameBoy>();CHECK(g->load(argv[1]));g->runFrame();const auto seed=g->serializeState(),bess=saves::exportBess(*g);std::mt19937 random(0x100ACCU);unsigned rejected=0,accepted=0;
    for(unsigned n=0;n<512;++n){auto data=n&1?seed:bess;
        for(unsigned j=0;j<1+random()%4;++j)data[random()%data.size()]^=(u8)(1+random()%255);
        if((n&1)&&n%3==0)saves::set32(data,data.size()-4,saves::hash(std::span(data).first(data.size()-4)));
        const auto before=g->serializeState();if(!(n&1?g->deserializeState(data):saves::importBess(*g,data))){++rejected;CHECK(before==g->serializeState());}else {++accepted;for(unsigned i=0;i<16;++i)g->cpu.step();}
        CHECK(g->deserializeState(seed));
    }
    for(std::size_t end:{std::size_t(0),std::size_t(8),std::size_t(63),seed.size()-1}){const auto before=g->serializeState();CHECK(!g->deserializeState(std::span(seed).first(end))&&g->serializeState()==before);}
    std::cout<<"Native GBSTATE/BESS loader fuzz: "<<accepted<<" accepted, "<<rejected<<" rejected atomically; truncation PASS\n";
    // Progress extensions must round-trip and reject atomically in both formats.
    std::vector<u8> progress={1,2,3};
    g->auxiliaryState=[&](){return progress;};
    g->auxiliaryRestore=[&](std::span<const u8> bytes){progress.assign(bytes.begin(),bytes.end());return progress.empty()||progress[0]!=0xFF;};
    const auto nativeAux=g->serializeState(),bessAux=saves::exportBess(*g);CHECK(!nativeAux.empty()&&!bessAux.empty());
    progress={4,5};g->mem.write(0xC123,0xAA);CHECK(g->deserializeState(nativeAux)&&progress==std::vector<u8>({1,2,3}));
    progress={6,7};g->mem.write(0xC123,0xBB);CHECK(saves::importBess(*g,bessAux)&&progress==std::vector<u8>({1,2,3}));
    const auto stable=g->serializeState();auto invalid=bessAux;std::size_t at=saves::le32(invalid,invalid.size()-8);bool found=false;
    while(at+8<=invalid.size()-8){const auto size=saves::le32(invalid,at+4);CHECK(size<=invalid.size()-8-at-8);
        if(!std::memcmp(invalid.data()+at,"GBAU",4)){CHECK(size>=5);invalid[at+8]=0xFF;saves::set32(invalid,at+8+size-4,saves::hash(std::span(invalid).subspan(at+8,size-4)));found=true;break;}at+=8+size;}
    CHECK(found&&!saves::importBess(*g,invalid)&&g->serializeState()==stable&&progress==std::vector<u8>({1,2,3}));
    invalid=bessAux;at=saves::le32(invalid,invalid.size()-8);
    while(at+8<=invalid.size()-8){const auto size=saves::le32(invalid,at+4);if(!std::memcmp(invalid.data()+at,"GBAU",4)){invalid[at+8]^=1;break;}at+=8+size;}
    CHECK(!saves::importBess(*g,invalid)&&g->serializeState()==stable);
    std::cout<<"Native and BESS auxiliary progress roundtrip, checksum and transactional rejection PASS\n";

    // Camera RAM appears in both the standard BESS buffers and GBSP. Together
    // with the maximum legal auxiliary payload this exceeds the native cap.
    auto cameraRom=Memory::loadBytes(argv[1]);cameraRom[0x147]=0xFC;
    const auto cameraPath=std::filesystem::temp_directory_path()/("gb-native-large-state-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".gb");
    CHECK(Memory::writeAtomic(cameraPath,{(const char*)cameraRom.data(),cameraRom.size()}));
    auto camera=std::make_unique<GameBoy>();CHECK(camera->load(cameraPath)&&camera->mem.eram.size()==131072);
    std::vector<u8> maximumProgress(GameBoy::auxiliaryStateLimit,0x5A);
    camera->auxiliaryState=[&](){return maximumProgress;};
    camera->auxiliaryRestore=[&](std::span<const u8> bytes){maximumProgress.assign(bytes.begin(),bytes.end());return true;};
    camera->mem.eram.back()=0xA5;camera->runFrame();
    const auto cameraNative=camera->serializeState(),cameraBess=saves::exportBess(*camera);
    CHECK(!cameraNative.empty()&&cameraNative.size()<=GameBoy::stateSizeLimit&&cameraBess.size()>GameBoy::stateSizeLimit);
    camera->mem.eram.back()=0;maximumProgress={0};
    CHECK(saves::importBess(*camera,cameraBess)&&camera->serializeState()==cameraNative);
    const auto statePath=std::filesystem::path(cameraPath.string()+".bess");
    CHECK(saves::save(*camera,statePath));camera->mem.eram.back()=0;maximumProgress={1};
    CHECK(saves::load(*camera,statePath)&&camera->serializeState()==cameraNative);
    std::filesystem::remove(statePath);std::filesystem::remove(cameraPath);
    std::cout<<"Maximum auxiliary payload with Camera RAM: BESS memory/disk roundtrip PASS\n";

}
