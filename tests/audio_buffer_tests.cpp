#include "../src/audio_buffer.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#define CHECK(expr) do {if(!(expr)){std::cerr<<"CHECK FAILED: "<<#expr<<" line "<<__LINE__<<'\n';std::abort();}} while(false)
using host_audio::StereoFrame;
static void testRing() {
    host_audio::StereoRing<8> ring;std::array<StereoFrame,12> frames{};
    for(unsigned n=0;n<frames.size();++n)frames[n]={(std::int16_t)n,(std::int16_t)-((int)n)};
    CHECK(ring.push(frames)==8);CHECK(ring.push(frames)==0);CHECK(ring.size()==8);
    std::array<StereoFrame,3> first{};CHECK(ring.pop(first)==3);CHECK(first[2].left==2&&first[2].right==-2);
    CHECK(ring.push(std::span<const StereoFrame>(frames).subspan(8,3))==3);
    std::array<StereoFrame,8> rest{};CHECK(ring.pop(rest)==8);
    for(unsigned n=0;n<rest.size();++n)CHECK(rest[n].left==(int)n+3&&rest[n].right==-(int)(n+3));
    CHECK(ring.pop(rest)==0);CHECK(ring.size()==0);
    CHECK(ring.push(frames)==8);ring.discardConsumer();CHECK(ring.size()==0);
    CHECK(ring.push(std::span<const StereoFrame>(frames).first(2))==2);ring.resetStopped();CHECK(ring.size()==0);
}
static void testConcurrentOrder() {
    host_audio::StereoRing<256> ring;
    constexpr unsigned total=500000;
    std::atomic<bool> start=false;
    std::thread producer([&](){while(!start.load(std::memory_order_acquire))std::this_thread::yield();
        std::array<StereoFrame,31> block{};
        for(unsigned next=0;next<total;){const auto size=std::min<unsigned>((unsigned)block.size(),total-next);
            for(unsigned n=0;n<size;++n){const unsigned value=next+n;block[n]={(std::int16_t)(value&0xFFFF),(std::int16_t)(value>>16)};}
            const auto written=ring.push(std::span<const StereoFrame>(block).first(size));next+=(unsigned)written;if(!written)std::this_thread::yield();}
    });
    std::thread consumer([&](){start.store(true,std::memory_order_release);std::array<StereoFrame,19> block{};
        for(unsigned next=0;next<total;){const auto read=ring.pop(block);if(!read){std::this_thread::yield();continue;}
            for(std::size_t n=0;n<read;++n){const unsigned value=(unsigned)(std::uint16_t)block[n].left|((unsigned)(std::uint16_t)block[n].right<<16);CHECK(value==next);++next;}}
    });
    producer.join();consumer.join();CHECK(ring.size()==0);
}
static void testRamps() {
    host_audio::StereoRing<256> ring;host_audio::SmoothRenderer renderer;
    std::array<StereoFrame,128> source{},output{};source.fill({16384,-16384});CHECK(ring.push(source)==source.size());
    CHECK(renderer.render(ring,output,true)==0);
    CHECK(output[0].left==256&&output[0].right==-256);CHECK(output[63].left==16384&&output[127].right==-16384);
    CHECK(renderer.render(ring,output,true)==128);CHECK(output[0].left>0&&output[0].left<16384);CHECK(output[63].left==0&&output.back().right==0);
    CHECK(ring.push(source)==source.size());CHECK(renderer.render(ring,output,false)==0);CHECK(ring.size()==0);
    CHECK(output.back().left==0);
    CHECK(ring.push(source)==source.size());CHECK(renderer.render(ring,output,true)==0);renderer.restart();
    CHECK(renderer.render(ring,output,false)==0);CHECK(output[0].left>0&&output[63].left==0);
    for(unsigned n=0;n<20;++n){CHECK(ring.push(source)==source.size());renderer.render(ring,output,true,0);}CHECK(output.back().left==0);
    for(unsigned n=0;n<20;++n){CHECK(ring.push(source)==source.size());renderer.render(ring,output,true,50);}CHECK(output.back().left==8192);
}
static std::vector<unsigned char> readFile(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);return std::vector<unsigned char>(std::istreambuf_iterator<char>(stream),{});
}
static unsigned le32(const std::vector<unsigned char>& b,std::size_t pos){return b[pos]|((unsigned)b[pos+1]<<8)|((unsigned)b[pos+2]<<16)|((unsigned)b[pos+3]<<24);}
static unsigned le16(const std::vector<unsigned char>& b,std::size_t pos){return b[pos]|((unsigned)b[pos+1]<<8);}
static void testWav() {
    const auto stamp=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory=std::filesystem::temp_directory_path()/std::filesystem::path(std::u8string(u8"gbstudio-audio-тест-")+std::u8string(stamp.begin(),stamp.end()));
    // Build a unique directory without reusing a user's existing output.
    CHECK(std::filesystem::create_directory(directory));
    const auto file=directory/"record.wav";
    const std::array<StereoFrame,3> source={{{0x1234,-1},{-32768,32767},{0,42}}};
    host_audio::WavWriter writer;CHECK(writer.open(file));CHECK(writer.recording());CHECK(writer.append(source));CHECK(writer.dataBytes()==12);
    CHECK(writer.close());CHECK(!writer.recording());CHECK(writer.close());
    const auto bytes=readFile(file);CHECK(bytes.size()==56);
    CHECK(std::string(bytes.begin(),bytes.begin()+4)=="RIFF");CHECK(std::string(bytes.begin()+8,bytes.begin()+12)=="WAVE");
    CHECK(le32(bytes,4)==48&&le32(bytes,16)==16&&le16(bytes,20)==1&&le16(bytes,22)==2);
    CHECK(le32(bytes,24)==44100&&le32(bytes,28)==176400&&le16(bytes,32)==4&&le16(bytes,34)==16&&le32(bytes,40)==12);
    CHECK(bytes[44]==0x34&&bytes[45]==0x12&&bytes[46]==0xFF&&bytes[47]==0xFF);
    CHECK(bytes[48]==0&&bytes[49]==0x80&&bytes[50]==0xFF&&bytes[51]==0x7F);
    CHECK(!writer.open(file));CHECK(readFile(file)==bytes);CHECK(!writer.recording());
    CHECK(!writer.open(directory/"missing"/"bad.wav"));CHECK(!writer.lastError().empty());
    const auto capped=directory/"capped.wav";host_audio::WavWriter shortWriter(16);
    CHECK(shortWriter.open(capped));CHECK(shortWriter.append(source));CHECK(!shortWriter.append(source));CHECK(!shortWriter.close());
    const auto shortBytes=readFile(capped);CHECK(shortBytes.size()==56&&le32(shortBytes,40)==12);
    const auto empty=directory/"empty.wav";CHECK(writer.open(empty));CHECK(writer.close());CHECK(readFile(empty).size()==44);
    CHECK(!writer.append(source));
    CHECK(std::filesystem::remove(file));CHECK(std::filesystem::remove(capped));CHECK(std::filesystem::remove(empty));
    CHECK(std::filesystem::remove(directory));
}
int main(){testRing();testConcurrentOrder();testRamps();testWav();std::cout<<"Audio buffer/WAV tests passed\n";}
