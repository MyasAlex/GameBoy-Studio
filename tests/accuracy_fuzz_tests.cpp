#include "../src/accuracy_core.hpp"
#include <random>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main(int argc,char** argv){CHECK(argc==2);auto g=std::make_unique<GameBoy>();CHECK(accurate::enable(*g)&&g->load(argv[1]));g->runFrame();const auto seed=g->serializeState(),bess=saves::exportBess(*g);std::mt19937 random(0x100ACCU);unsigned rejected=0,accepted=0;
    for(unsigned n=0;n<512;++n){auto data=n&1?seed:bess;
        for(unsigned j=0;j<1+random()%4;++j)data[random()%data.size()]^=(u8)(1+random()%255);
        if((n&1)&&n%3==0)saves::set32(data,data.size()-4,saves::hash(std::span(data).first(data.size()-4)));
        const auto before=g->serializeState();if(!g->deserializeState(data)){++rejected;CHECK(before==g->serializeState());}else {++accepted;for(unsigned i=0;i<16;++i)g->cpu.step();}
        CHECK(g->deserializeState(seed));
    }
    for(std::size_t end:{std::size_t(0),std::size_t(8),std::size_t(63),seed.size()-1}){const auto before=g->serializeState();CHECK(!g->deserializeState(std::span(seed).first(end))&&g->serializeState()==before);}
    std::cout<<"Accurate loader fuzz: "<<accepted<<" accepted, "<<rejected<<" rejected atomically; truncation PASS\n";
}
