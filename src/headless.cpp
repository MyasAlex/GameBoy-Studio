#include "gameboy.hpp"
int main(int argc, char** argv){
    if(argc<2){std::cout<<"Headless test: gb_headless path/to/ROM.gb [frames]\n";return 0;}
    GameBoy gb;
    if(!gb.load(argv[1])){std::cerr<<"Could not load ROM\n";return 1;}
    int frames=argc>2?std::clamp(std::atoi(argv[2]),1,10000):120;
    for(int i=0;i<frames;++i) gb.runFrame();
    std::cout<<"frames="<<frames<<" PC="<<std::hex<<gb.cpu.PC<<std::dec
        <<" LY="<<(int)gb.mem.io[0x44]<<" ROM="<<gb.mem.gameTitle<<"\n";
    std::ofstream f("frame.ppm",std::ios::binary);f<<"P6\n160 144\n255\n";
    for(u32 px:gb.ppu.pixels){char rgb[3]={(char)(px>>16),(char)(px>>8),(char)px};f.write(rgb,3);}
    return 0;
}
