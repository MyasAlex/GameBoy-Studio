#include "../src/accuracy_core.hpp"
#include <random>
extern "C" {void gb_cov_begin();unsigned gb_cov_novel();unsigned gb_cov_count();}
// Real PC-coverage feedback retains inputs that discover new native-loader paths.
// It complements the ordinary ROM/archive/state mutation regression suites.
int main(int argc,char** argv){
    if(argc<3||argc>4){std::cerr<<"Usage: gb_coverage_fuzzer ROM.gb OUTPUT_DIRECTORY [seconds=30]\n";return 2;}
    const int seconds=argc==4?std::atoi(argv[3]):30;if(seconds<1||seconds>3600)return 2;
    const std::filesystem::path folder=argv[2];std::filesystem::create_directories(folder);
    auto g=std::make_unique<GameBoy>();if(!accurate::enable(*g)||!g->load(argv[1]))return 2;g->runFrame();
    const auto seed=g->serializeState();auto portable=saves::exportBess(*g),foreign=portable;
    // Exercise the standard BESS importer without Studio's exact-state wrapper
    // or SameBoy's native prefix; embedded standard buffer offsets stay valid.
    std::fill_n(foreign.begin(),8,u8{0});std::size_t at=saves::le32(foreign,foreign.size()-8);
    while(at+8<=foreign.size()-8){const auto length=saves::le32(foreign,at+4);if(length>foreign.size()-8-at-8)break;if(!std::memcmp(foreign.data()+at,"GSTU",4))std::memcpy(foreign.data()+at,"TEST",4);if(!std::memcmp(foreign.data()+at,"END ",4))break;at+=8+length;}
    std::vector<std::vector<u8>> corpus={seed,portable,foreign};std::size_t corpusBytes=seed.size()+portable.size()+foreign.size();
    std::mt19937 random(0x20261013);unsigned iterations=0,accepted=0,rejected=0,newInputs=0;
    const auto start=std::chrono::steady_clock::now(),deadline=start+std::chrono::seconds(seconds);
    while(std::chrono::steady_clock::now()<deadline){auto data=corpus[random()%corpus.size()];
        if(random()%8==0)data.resize(random()%data.size());else for(unsigned j=0;j<1+random()%8;++j){const auto offset=random()%3==0?random()%std::min<std::size_t>(2048,data.size()):random()%data.size();data[offset]^=(u8)(1+random()%255);}
        if(data.size()>=68&&!std::memcmp(data.data(),"GBSACC1",7)&&random()%2)saves::set32(data,data.size()-4,saves::hash(std::span(data).first(data.size()-4)));
        // The last input is written before the parser runs, including on crashes.
        if(!Memory::writeAtomic(folder/"last-input.bin",{(const char*)data.data(),data.size()}))return 2;
        const auto before=g->serializeState();gb_cov_begin();const bool ok=g->deserializeState(data);const bool novel=gb_cov_novel()!=0;
        if(ok){++accepted;for(unsigned j=0;j<4;++j)g->cpu.step();}else {++rejected;if(before!=g->serializeState()){std::cerr<<"Rejected state changed the machine; last-input.bin is the reproducer.\n";return 1;}}
        if(novel&&!data.empty()&&corpus.size()<64&&corpusBytes+data.size()<32*1024*1024){corpusBytes+=data.size();corpus.push_back(data);++newInputs;Memory::writeAtomic(folder/("corpus-"+std::to_string(newInputs)+".bin"),{(const char*)data.data(),data.size()});}
        if(!g->deserializeState(seed)){std::cerr<<"Seed could not restore after mutation.\n";return 1;}++iterations;
    }
    const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::ostringstream report;
    report<<"{\"status\":\"PASS\",\"seconds\":"<<elapsed<<",\"iterations\":"<<iterations<<",\"accepted\":"<<accepted<<",\"rejected\":"<<rejected<<",\"coverage_slots\":"<<gb_cov_count()<<",\"new_corpus_inputs\":"<<newInputs<<",\"corpus_inputs\":"<<corpus.size()<<",\"scope\":\"GCC trace-pc native state/BESS loader feedback; finite run, hashed coverage slots; not exhaustive\"}\n";
    if(!Memory::writeAtomic(folder/"report.json",report.str()))return 2;std::cout<<report.str();return 0;
}
