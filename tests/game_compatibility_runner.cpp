// Runs supplied games through the AccuracyCore bridge used by the GUI.
// Evidence is a smoke check, never a claim that a complete game was verified.
#include "../src/studio_tools.hpp"
#include "../src/accuracy_core.hpp"
#include <chrono>
#include <set>

namespace {
struct Event { unsigned frame, buttons; };
void setButtons(GameBoy& gb, unsigned value) {
    auto& m=gb.mem;
    m.right=value&1; m.left=value&2; m.up=value&4; m.down=value&8;
    m.a=value&16; m.b=value&32; m.select=value&64; m.start=value&128;
}
u32 imageHash(const GameBoy& gb) {
    return studio::crc32({reinterpret_cast<const u8*>(gb.ppu.pixels.data()),gb.ppu.pixels.size()*sizeof(u32)});
}
void screenshot(const GameBoy& gb, const std::filesystem::path& path) {
    std::ofstream file(path,std::ios::binary);
    file<<"P6\n160 144\n255\n";
    for(u32 pixel:gb.ppu.pixels) {
        const char rgb[]={(char)(pixel>>16),(char)(pixel>>8),(char)pixel};
        file.write(rgb,3);
    }
    if(!file) throw std::runtime_error("Cannot save screenshot");
}
struct Audio {
    u64 samples=0,nonzero=0;
    unsigned peak=0;
    long double squares=0;
    u32 hash=2166136261u;
    std::vector<std::int16_t> clip;
    void append(std::span<const std::int16_t> values, bool keep) {
        for(auto value:values) {
            ++samples; nonzero+=value!=0;
            peak=std::max(peak,(unsigned)std::abs((int)value));
            squares+=(long double)value*value;
            for(int shift:{0,8}) { hash^=(u8)((u16)value>>shift); hash*=16777619u; }
        }
        if(keep) clip.insert(clip.end(),values.begin(),values.end());
    }
    void consume(GameBoy& gb, bool keep, Audio* mirror=nullptr) {
        std::vector<std::int16_t> values(gb.apu.count);
        if(values.empty()) return;
        gb.apu.samples(values.data(),(int)values.size()/2);
        append(values,keep);
        if(mirror) mirror->append(values,true);
    }
    void wav(const std::filesystem::path& path, int rate) const {
        std::ofstream file(path,std::ios::binary);
        auto put=[&](u32 value,int size) { for(int i=0;i<size;++i) file.put((char)(value>>(i*8))); };
        file.write("RIFF",4); put(36+(u32)clip.size()*2,4);
        file.write("WAVEfmt ",8); put(16,4); put(1,2); put(2,2);
        put(rate,4); put(rate*4,4); put(4,2); put(16,2);
        file.write("data",4); put((u32)clip.size()*2,4);
        for(auto value:clip) put((u16)value,2);
        if(!file) throw std::runtime_error("Cannot save PCM clip");
    }
};
struct AudioDifference {
    u64 firstSamples=0,secondSamples=0,firstMismatch=0;
    unsigned maxError=0;
    double rmsError=0,tailRMSError=0;
    bool exact=false;
    AudioDifference(const Audio& first,const Audio& second,int rate) {
        firstSamples=first.samples; secondSamples=second.samples;
        exact=first.clip==second.clip&&first.samples==second.samples&&!first.clip.empty();
        const auto count=std::min(first.clip.size(),second.clip.size());
        firstMismatch=count;
        long double squareError=0,tailError=0;
        const auto tail=std::min(count,(std::size_t)rate/2); // quarter second of stereo PCM
        for(std::size_t i=0;i<count;++i) {
            const unsigned error=(unsigned)std::abs((int)first.clip[i]-second.clip[i]);
            if(error&&firstMismatch==count) firstMismatch=i;
            maxError=std::max(maxError,error);
            squareError+=(long double)error*error;
            if(i>=count-tail) tailError+=(long double)error*error;
        }
        rmsError=count?std::sqrt(squareError/count):0;
        tailRMSError=tail?std::sqrt(tailError/tail):0;
    }
};
const char* boolean(bool value) { return value?"true":"false"; }
}

int main(int argc,char** argv) {
    try {
        if(argc!=6) {
            std::cerr<<"Usage: gb_game_compatibility_runner ROM OUTDIR dmg|cgb EVENTS.txt FRAMES\n";
            return 2;
        }
        const auto start=std::chrono::steady_clock::now();
        const auto output=std::filesystem::path(argv[2]);
        std::filesystem::create_directories(output);
        const unsigned frames=(unsigned)std::stoul(argv[5]);
        if(frames<1440||frames>100000) throw std::runtime_error("Frame budget must be 1440..100000");
        const unsigned checkpointFrame=frames/2-120,branchFrame=frames/2+120;
        std::vector<Event> events;
        std::ifstream script(argv[4]);
        unsigned frame,value;
        while(script>>frame>>value) {
            if(value>255||(!events.empty()&&frame<=events.back().frame))
                throw std::runtime_error("Events must be ordered and buttons must fit a byte");
            events.push_back({frame,value});
        }
        if(!script.eof()||events.empty()) throw std::runtime_error("Invalid input script");
        auto input=[&](unsigned current) {
            unsigned result=0;
            for(auto event:events) { if(event.frame>current) break; result=event.buttons; }
            return result;
        };
        auto gb=std::make_unique<GameBoy>();
        if(!accurate::enable(*gb)) throw std::runtime_error("Accuracy core unavailable");
        HardwareModel model;
        if(std::string(argv[3])=="dmg") model=HardwareModel::DMG;
        else if(std::string(argv[3])=="cgb") model=HardwareModel::CGB;
        else throw std::runtime_error("Unknown model");
        if(!gb->load(argv[1],model)) throw std::runtime_error(gb->mem.loadError);
        const auto initialBatteryRAM=gb->mem.eram;
        Audio audio,originalContinuation,restoredContinuation;
        std::set<u32> images;
        std::vector<u8> checkpoint;
        u32 idleImage=0,idleRAM=0,activeImage=0,activeRAM=0;
        bool replay=false,restored=false;
        auto run=[&](unsigned current,Audio& target,bool capture,Audio* mirror=nullptr) {
            setButtons(*gb,input(current)); gb->runFrame(); target.consume(*gb,capture,mirror);
        };
        for(unsigned current=0;current<frames;++current) {
            if(current==branchFrame) {
                const auto state=gb->serializeState();
                Audio ignored;
                for(unsigned n=0;n<120;++n) {
                    setButtons(*gb,0); gb->runFrame(); ignored.consume(*gb,false);
                }
                idleImage=imageHash(*gb); idleRAM=studio::crc32(gb->mem.wram);
                if(!gb->deserializeState(state)) throw std::runtime_error("Input branch restore failed");
            }
            if(current==checkpointFrame) {
                checkpoint=gb->serializeState();
                gb->apu.clearQueue(); // Same queue boundary as the subsequent restore.
            }
            const bool inContinuation=current>=checkpointFrame&&current<checkpointFrame+120;
            run(current,audio,current>=frames-240&&current<frames-120,
                inContinuation?&originalContinuation:nullptr);
            if(current%30==29) images.insert(imageHash(*gb));
            if(current==branchFrame+119) {
                activeImage=imageHash(*gb); activeRAM=studio::crc32(gb->mem.wram);
            }
            if(current==179||current==359||current==719||current==1199||current==1799||current+1==frames)
                screenshot(*gb,output/("frame-"+std::to_string(current+1)+".ppm"));
            if(current==checkpointFrame+119) {
                const auto expected=gb->serializeState();
                restored=gb->deserializeState(checkpoint);
                if(!restored) throw std::runtime_error("Save-state reload failed");
                for(unsigned n=checkpointFrame;n<checkpointFrame+120;++n)
                    run(n,restoredContinuation,true);
                replay=gb->serializeState()==expected;
            }
        }
        if(gb->backendRefresh) gb->backendRefresh();
        const AudioDifference difference(originalContinuation,restoredContinuation,gb->apu.sampleRate);
        audio.wav(output/"gameplay-audio.wav",gb->apu.sampleRate);
        std::ofstream cartridge(output/"cartridge.json");
        cartridge<<studio::romReport(*gb);
        bool batterySave=false,batteryReload=false;
        if(gb->mem.hasBattery) {
            const auto ram=gb->mem.eram;
            batterySave=gb->mem.saveBattery();
            auto fresh=std::make_unique<GameBoy>();
            batteryReload=accurate::enable(*fresh)&&fresh->load(argv[1],model)&&fresh->mem.eram==ram;
        }
        const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::ofstream report(output/"run.json");
        report<<"{\n  \"schema\": \"gameboy-studio-game-smoke-v1\",\n  \"rom\": "
              <<studio::jsonString(std::filesystem::path(argv[1]).filename().string())
              <<",\n  \"model\": "<<studio::jsonString(argv[3])
              <<",\n  \"frames\": "<<frames<<",\n  \"emulated_seconds\": "<<frames*double(GB_FRAME)/GB_CLOCK
              <<",\n  \"wall_seconds\": "<<seconds
              <<",\n  \"unique_sampled_frames\": "<<images.size()
              <<",\n  \"input_changes_frame\": "<<boolean(idleImage!=activeImage)
              <<",\n  \"input_changes_wram\": "<<boolean(idleRAM!=activeRAM)
              <<",\n  \"save_state_restored\": "<<boolean(restored)
              <<",\n  \"save_state_continuation_exact\": "<<boolean(replay)
              <<",\n  \"save_state_audio_continuation_exact\": "<<boolean(difference.exact)
              <<",\n  \"audio_replay_difference\": {\"original_sample_count\": "<<difference.firstSamples
              <<", \"restored_sample_count\": "<<difference.secondSamples
              <<", \"first_mismatching_sample\": "<<difference.firstMismatch
              <<", \"max_absolute_error\": "<<difference.maxError
              <<", \"rms_error\": "<<difference.rmsError
              <<", \"last_quarter_second_rms_error\": "<<difference.tailRMSError<<"}"
              <<",\n  \"battery_applicable\": "<<boolean(gb->mem.hasBattery)
              <<",\n  \"battery_ram_bytes\": "<<gb->mem.eram.size()
              <<",\n  \"battery_ram_changed_since_load\": "<<boolean(gb->mem.eram!=initialBatteryRAM)
              <<",\n  \"battery_saved\": "<<boolean(batterySave)
              <<",\n  \"battery_reloaded_ram_exact\": "<<boolean(batteryReload)
              <<",\n  \"audio\": {\"stereo_samples\": "<<audio.samples<<", \"nonzero_samples\": "<<audio.nonzero
              <<", \"peak\": "<<audio.peak<<", \"rms\": "<<(audio.samples?std::sqrt(audio.squares/audio.samples):0)
              <<", \"fnv1a32\": "<<audio.hash<<"}"
              <<",\n  \"final_pc\": "<<gb->cpu.PC<<",\n  \"final_ppu_frames\": "<<gb->ppu.frameNumber
              <<",\n  \"final_frame_crc32\": "<<imageHash(*gb)<<"\n}\n";
        if(!report||!cartridge) throw std::runtime_error("Cannot write report");
        std::cout<<std::filesystem::path(argv[1]).filename().string()<<" "<<argv[3]<<" "<<frames
                 <<" frames, state="<<replay<<" audio="<<difference.exact<<" unique="<<images.size()
                 <<" input="<<(idleImage!=activeImage)<<" "<<seconds<<"s\n";
        // Exit status is only a state/animation smoke assertion; every other
        // observation remains explicit in run.json and the corpus report.
        return replay&&images.size()>1?0:1;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 2;
    }
}
