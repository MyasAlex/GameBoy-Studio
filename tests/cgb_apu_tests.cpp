#include "../src/gameboy.hpp"
#include <cstdlib>
#define CHECK(expr) do { if(!(expr)){std::cerr<<"CHECK FAILED: "<<#expr<<" at line "<<__LINE__<<'\n';std::abort();} } while(false)

static void waveAccessAndRetrigger(){
    for(bool color:{false,true}){
        GameBoy g;g.mem.cgbROM=color;g.reset();
        g.mem.write(0xFF26,0x80);g.mem.write(0xFF1A,0x80);
        for(int i=0;i<16;++i)g.mem.write((u16)(0xFF30+i),(u8)(0x80+i));
        g.mem.write(0xFF1C,0x20);g.mem.write(0xFF1E,0x80);
        g.apu.state.waveByte=9;g.apu.state.waveAccess=0;
        CHECK(g.mem.read(0xFF3F)==(color?0x89:0xFF));
        g.mem.write(0xFF3F,0x42);CHECK(g.mem.io[0x39]==(color?0x42:0x89));
        g.apu.state.channels[2].phase=17;g.apu.state.channels[2].timer=2;
        const u8 before=g.mem.io[0x30];g.mem.write(0xFF1E,0x80);
        if(color){CHECK(g.mem.io[0x30]==before);CHECK(g.apu.state.waveByte==0&&g.mem.read(0xFF3C)==before);}
        else CHECK(g.mem.io[0x30]==0x88);
    }
}
static void powerAndPcm(){
    for(bool color:{false,true}){
        GameBoy g;g.mem.cgbROM=color;g.reset();
        g.mem.write(0xFF11,0x3E);CHECK(g.apu.state.channels[0].length==2);
        g.mem.write(0xFF26,0);CHECK(g.apu.state.channels[0].length==(color?0:2));
        g.mem.write(0xFF11,0x3D);CHECK(g.apu.state.channels[0].length==(color?0:3));
        g.mem.write(0xFF26,0x80);g.mem.io[0x11]=0;g.mem.io[0x16]=0;
        auto& c0=g.apu.state.channels[0];auto& c1=g.apu.state.channels[1];
        c0.enabled=true;c0.volume=9;c0.phase=7;c1.enabled=true;c1.volume=6;c1.phase=7;
        CHECK(g.apu.pcm(0xFF76)==0x69);
        if(color)CHECK(g.mem.read(0xFF76)==0x69);
        else CHECK(g.mem.read(0xFF76)==0xFF);
    }
}
static void stableAudioAtDoubleSpeed(){
    GameBoy normal,fast;normal.mem.cgbROM=fast.mem.cgbROM=true;
    normal.reset();fast.reset();fast.mem.doubleSpeed=true;
    // Align divider phases without creating different DIV-reset falling edges.
    normal.mem.divCounter=fast.mem.divCounter=0;
    normal.tick(GB_FRAME);fast.tick(GB_FRAME*2);
    CHECK(normal.apu.available()==fast.apu.available());
    CHECK(normal.apu.state.frameStep==fast.apu.state.frameStep);
    CHECK(normal.ppu.frameNumber==fast.ppu.frameNumber);
    CHECK(normal.ppu.dot==fast.ppu.dot);
}
int main(){waveAccessAndRetrigger();powerAndPcm();stableAudioAtDoubleSpeed();std::cout<<"ALL CGB APU TESTS PASSED\n";}
