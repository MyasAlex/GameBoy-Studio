#include "../src/session.hpp"
#include <random>

static void require(bool result,const char* message) {if(!result)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    try {
        require(argc==2,"demo path required");
        std::mt19937 random(42);
        for(std::size_t size:{0u,1u,2u,127u,128u,129u,130u,131u,32768u,400000u}) {
            std::vector<u8> bytes(size);for(auto& b:bytes)b=static_cast<u8>(random());
            for(int variant=0;variant<3;++variant) {
                if(variant==1)std::fill(bytes.begin(),bytes.end(),0);
                if(variant==2)for(std::size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<u8>((i/131)%9);
                const auto compressed=RewindBuffer::pack(bytes);std::vector<u8> decoded;
                require(RewindBuffer::unpack(compressed,size,decoded)&&decoded==bytes,"RLE roundtrip");
                if(!compressed.empty()) {
                    auto damaged=compressed;damaged.pop_back();decoded={0x42};
                    require(!RewindBuffer::unpack(damaged,size,decoded)&&decoded==std::vector<u8>{0x42},"Truncated packet rejection is transactional");
                }
            }
        }
        std::vector<u8> output={0xAA};
        require(!RewindBuffer::unpack(std::vector<u8>{255,0},1,output),"Packet expansion bound");
        require(output==std::vector<u8>{0xAA},"Rejected expansion preserves output");
        GameBoy gb;require(gb.load(argv[1]),"load demo");
        RewindBuffer rewind;rewind.configure(30,2);rewind.capture(gb);
        gb.runFrame();rewind.capture(gb);gb.runFrame();rewind.capture(gb);
        const auto checkpoint=gb.serializeState();
        gb.runFrame();rewind.capture(gb);gb.runFrame();rewind.capture(gb);
        require(rewind.rewind(gb)&&gb.serializeState()==checkpoint,"Exact rewind including CPU/PPU/APU/DMA");
        rewind.configure(0);gb.runFrame();rewind.capture(gb);require(rewind.size()==0&&!rewind.rewind(gb),"Rewind disabled");
        rewind.configure(1,1);
        for(int i=0;i<100;++i){gb.runFrame();rewind.capture(gb);}
        require(rewind.size()<=61&&rewind.bytes()<=64*1024*1024,"Rewind bounded by time and memory");
        gb.apu.setChannelMuted(1,true);
        InputMovie movie;require(movie.startRecording(gb),"start input recording");
        for(int i=0;i<45;++i) {
            InputMovie::setJoypad(gb.mem,static_cast<u8>(i%9==0?128:(i%5==0?17:0)));
            require(movie.beforeFrame(gb),"record input");gb.runFrame();
        }
        const auto result=gb.serializeState();movie.stop();
        const auto folder=std::filesystem::temp_directory_path()/"gbstudio-session-tests";
        std::filesystem::create_directories(folder);const auto path=folder/"input.gbmovie";
        require(movie.save(path),"movie save");
        gb.apu.setChannelMuted(1,false);
        require(movie.startPlayback(gb,path),"movie load seed state");
        require(gb.apu.mutedChannels[1],"Movie restores recorded mixer settings");
        for(int i=0;i<45;++i){require(movie.beforeFrame(gb),"playback input");gb.runFrame();}
        require(gb.serializeState()==result,"Recorded playback restores exact emulation result");
        require(!movie.beforeFrame(gb)&&movie.mode()==InputMovie::Mode::Idle,"Playback ends without an extra frame");
        auto damaged=Memory::loadBytes(path);damaged.back()^=1;
        require(Memory::writeAtomic(path,std::string_view(reinterpret_cast<const char*>(damaged.data()),damaged.size())),"write bad movie");
        const auto before=gb.serializeState();
        require(!movie.startPlayback(gb,path)&&gb.serializeState()==before,"Corrupted movie rejected without state changes");
        std::error_code ec;std::filesystem::remove(path,ec);std::filesystem::remove(folder,ec);
        std::cout<<"Session tests PASS: bounded RLE rewind, exact input movie replay, corruption rejection\n";
        return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
