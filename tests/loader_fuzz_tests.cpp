#include "../src/archive_loader.hpp"
#include <random>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';std::exit(1);}}while(false)
int main(int argc,char** argv){CHECK(argc>=2);const auto folder=std::filesystem::temp_directory_path()/("gbstudio-fuzz-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(folder);
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove_all(path,e);}} cleanup{folder};
    std::mt19937 random(0x09F00D);auto g=std::make_unique<GameBoy>();CHECK(g->load(argv[1]));g->runFrame();const auto native=g->serializeState(),bess=saves::exportBess(*g);
    unsigned rejected=0,accepted=0;
    for(unsigned n=0;n<800;++n){auto bytes=(n&1)?native:bess;
        for(unsigned i=0;i<1+random()%4;++i)bytes[random()%bytes.size()]^=(u8)(1+(random()%255));
        if(!(n&1)&&n%3==0){const u32 hash=saves::hash(std::span<const u8>(bytes).first(bytes.size()-4));saves::set32(bytes,bytes.size()-4,hash);}
        const auto before=g->serializeState();const bool ok=(n&1)?g->deserializeState(bytes):saves::importBess(*g,bytes);
        if(!ok){++rejected;CHECK(before==g->serializeState());}else {++accepted;for(unsigned i=0;i<8;++i)g->cpu.step();}
        CHECK(g->deserializeState(native));
    }
    const auto rom=Memory::loadBytes(argv[1]);const auto path=folder/"input.bin";
    for(unsigned n=0;n<250;++n){std::vector<u8> bytes;if(n%3==0){bytes=rom;for(unsigned i=0;i<8;++i)bytes[random()%bytes.size()]=(u8)random();}
        else {bytes.resize(random()%8192);for(auto& b:bytes)b=(u8)random();if(n%4==0&&bytes.size()>4){bytes[0]='P';bytes[1]='K';bytes[2]=3;bytes[3]=4;}if(n%4==1&&bytes.size()>4){bytes[0]=0x1F;bytes[1]=0x8B;bytes[2]=8;}}
        CHECK(Memory::writeAtomic(path,{(const char*)bytes.data(),bytes.size()}));const auto result=archives::read(path);CHECK(!result||result.rom.size()<=8*1024*1024);
        Memory memory;memory.loadROM(path);
    }
    // Valid ZIP + corrupt CRC proves actual decompression and integrity checks.
    mz_zip_archive zip{};CHECK(mz_zip_writer_init_heap(&zip,0,0));CHECK(mz_zip_writer_add_mem(&zip,"game.gb",rom.data(),rom.size(),MZ_BEST_COMPRESSION));void* data=nullptr;size_t length=0;CHECK(mz_zip_writer_finalize_heap_archive(&zip,&data,&length));
    std::vector<u8> zipped((u8*)data,(u8*)data+length);mz_free(data);mz_zip_writer_end(&zip);CHECK(Memory::writeAtomic(path,{(char*)zipped.data(),zipped.size()}));CHECK(archives::read(path).rom==rom);
    // Truncation must never return a partially decoded cartridge.
    zipped.resize(zipped.size()-8);CHECK(Memory::writeAtomic(path,{(char*)zipped.data(),zipped.size()}));CHECK(!archives::read(path));
    mz_ulong compressed=mz_compressBound((mz_ulong)rom.size());std::vector<u8> deflate(compressed);CHECK(mz_compress2(deflate.data(),&compressed,rom.data(),(mz_ulong)rom.size(),9)==MZ_OK);
    std::vector<u8> gzip={0x1F,0x8B,8,0,0,0,0,0,0,255};gzip.insert(gzip.end(),deflate.begin()+2,deflate.begin()+compressed-4);
    saves::put32(gzip,(u32)mz_crc32(0,rom.data(),rom.size()));saves::put32(gzip,(u32)rom.size());CHECK(Memory::writeAtomic(path,{(char*)gzip.data(),gzip.size()}));CHECK(archives::read(path).rom==rom);
    gzip[gzip.size()-8]^=1;CHECK(Memory::writeAtomic(path,{(char*)gzip.data(),gzip.size()}));CHECK(!archives::read(path));
    std::cout<<"Loader fuzz PASS: 800 state mutations ("<<accepted<<" valid, "<<rejected<<" rejected atomically), 250 ROM/archive mutations, valid/truncated ZIP\n";
}
