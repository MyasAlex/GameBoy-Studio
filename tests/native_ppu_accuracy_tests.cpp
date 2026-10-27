#include "../src/gameboy.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';std::abort();}}while(false)

static void solid(Memory& m,int tile,int raw){for(int row=0;row<8;++row){m.vram[tile*16+row*2]=(raw&1)?255:0;m.vram[tile*16+row*2+1]=(raw&2)?255:0;}}
static void prepare(Memory& m,PPU& p){m.io[0x40]=0x91;m.io[0x47]=m.io[0x48]=0xE4;m.vram.fill(0);m.oam.fill(0);m.io[0x42]=m.io[0x43]=0;solid(m,0,1);solid(m,1,2);solid(m,2,3);for(int i=0;i<32;++i)m.vram[0x1800+i]=(u8)(i%3);p.reset();}
template<class Predicate> static void until(PPU& p,Predicate condition){for(int i=0;i<1000&&!condition();++i)p.tick(1);CHECK(condition());}
static void bitplaneBusLatches(){Memory m;PPU p(m);prepare(m,p);until(p,[&](){return m.ppuMode==3&&p.accuracy.fetchPhase==4;});CHECK(p.accuracy.low==255);m.vram[0]=0;until(p,[&](){return p.state.x>=8;});for(int i=0;i<8;++i)CHECK(p.drawing[i]==p.shade(1));}
static void horizontalScrollLatches(){
    for(int changed:{3,8}){Memory m;PPU p(m);prepare(m,p);until(p,[&](){return m.ppuMode==3&&p.accuracy.fetchPhase==4;});m.io[0x43]=(u8)changed;until(p,[&](){return p.state.x>=16;});
        for(int i=0;i<8;++i)CHECK(p.drawing[i]==p.shade(1));for(int i=8;i<16;++i)CHECK(p.drawing[i]==p.shade(changed==8?3:2));}
}
static void verticalPlaneLatches(){
    for(bool color:{false,true}){Memory m;PPU p(m);prepare(m,p);m.cgbMode=m.cgbHardware=color;m.hardwareModel=color?HardwareModel::CGBD:HardwareModel::DMG;
        // Distinct rows make independently sampled low/high bitplane addresses visible.
        m.vram[0]=255;m.vram[1]=0;m.vram[2]=0;m.vram[3]=255;
        m.bgPalette[2]=0x1F;m.bgPalette[3]=0; // CGB color1 red.
        p.reset();until(p,[&](){return m.ppuMode==3&&p.accuracy.fetchPhase==4;});m.io[0x42]=1;until(p,[&](){return p.state.x>=1;});
        CHECK(p.drawing[0]==(color?0xFFFF0000:p.shade(3)));}
}
static void spriteBusLatches(){Memory m;PPU p(m);prepare(m,p);solid(m,0,0);solid(m,1,1);m.io[0x40]|=2;m.oam[0]=16;m.oam[1]=8;m.oam[2]=1;p.reset();
    until(p,[&](){return p.accuracy.objRaw[0]!=0;});for(int row=0;row<8;++row)m.vram[16+row*2]=0;until(p,[&](){return p.state.x>=8;});for(int i=0;i<8;++i)CHECK(p.drawing[i]==p.shade(1));}
static void disabledBackgroundIsWhite(){Memory m;PPU p(m);prepare(m,p);m.io[0x40]&=~1;m.io[0x47]=0xFF;p.reset();
    until(p,[&](){return p.state.x>=8;});for(int i=0;i<8;++i)CHECK(p.drawing[i]==p.shade(0));}
static void clippedWindowTimingAndPixels(){
    for(int wx=0;wx<7;++wx)for(int fine=0;fine<8;++fine){Memory m;PPU p(m);prepare(m,p);
        m.io[0x40]=0xF1;m.io[0x43]=(u8)fine;m.io[0x4A]=0;m.io[0x4B]=(u8)wx;
        // A repeating 0,1,2,3 pattern exposes which offscreen window pixels
        // were clipped. SCX changes the delay but never scrolls the window.
        for(int row=0;row<8;++row){m.vram[16+row*2]=0x55;m.vram[17+row*2]=0x33;}
        for(int i=0;i<32;++i)m.vram[0x1C00+i]=1;
        p.reset();p.tick(456);CHECK(p.state.line==1);
        until(p,[&](){return m.ppuMode==3;});const int start=p.dot;
        until(p,[&](){return m.ppuMode==0;});CHECK(p.state.line==1);
        // Pan Docs: 172 base dots, six for window restart, fine SCX;
        // WX=0 shortens the transfer by one dot when fine SCX is nonzero.
        CHECK(p.dot-start==178+fine-((wx==0&&fine)?1:0));
        for(int x=0;x<8;++x)CHECK(p.drawing[160+x]==p.shade((7-wx+x)&3));
    }
}
static void windowReactivationReadCycle(){
    // Hardware measurements in Mealybug's m3_wx_4_change_sprites distinguish
    // a tile-map-read reactivation from an ordinary WX comparison. A color-
    // zero pixel also lets an OBJ with its behind-BG flag become visible.
    for(int phase=0;phase<8;++phase)for(bool object:{false,true}){
        Memory m;PPU p(m);prepare(m,p);m.io[0x40]=0xF3;m.io[0x47]=0x1B;
        m.io[0x4B]=23;m.ppuMode=3;p.state.x=16;p.state.windowActive=true;
        p.state.windowYTriggered=p.state.windowDrawn=true;
        auto& a=p.accuracy;a.active=a.windowFetch=true;a.windowWX=7;
        a.fetchPhase=phase;a.fifoSize=4;a.bgRaw.fill(3);a.bgAttributes.fill(0);
        if(object){a.objRaw[16]=1;a.objAttrs[16]=0x80;}
        unsigned mapReads=0;m.ppuBusObserver=[&](u16 address,u8,bool){if(address>=0x9800&&address<0xA000)++mapReads;};
        a.mapAddress=0x1800;p.transferDot();
        CHECK(p.state.x==17&&a.windowWX==23);
        CHECK(p.drawing[16]==p.shade(phase==1?(object?1:3):0));
        CHECK((mapReads!=0)==(phase==1));
        // The same comparison cannot prepend another glitch pixel next dot.
        p.transferDot();CHECK(p.drawing[17]==p.shade(0));
    }
}
static void statInterruptEdges(){
    // Mooneye's hardware measurements distinguish the CGB's early mode-2
    // source at VBlank from the ordinary visible-line mode-2 edge.
    for(bool color:{false,true}){Memory m;PPU p(m);prepare(m,p);m.cgbMode=m.cgbHardware=color;
        m.hardwareModel=color?HardwareModel::CGBE:HardwareModel::DMG;m.io[0x41]=0x20;p.reset();
        p.tick(142*456+440);CHECK(p.state.line==142&&p.dot==440);m.io[0x0F]=0;
        p.tick(12);CHECK(!(m.io[0x0F]&2));CHECK(m.ppuMode==0);
        p.tick(4);CHECK(p.state.line==143);CHECK(m.io[0x0F]&2);
        p.tick(440);m.io[0x0F]=0;p.tick(11);CHECK(!(m.io[0x0F]&3));
        p.tick(1);CHECK(bool(m.io[0x0F]&2)==color);CHECK(!(m.io[0x0F]&1));CHECK(m.ppuMode==0);
        p.tick(4);CHECK(p.state.line==144);CHECK(m.ppuMode==1);CHECK((m.io[0x0F]&3)==3);
    }
}
int main(){bitplaneBusLatches();horizontalScrollLatches();verticalPlaneLatches();spriteBusLatches();disabledBackgroundIsWhite();clippedWindowTimingAndPixels();windowReactivationReadCycle();statInterruptEdges();std::cout<<"Native PPU tile/plane/scroll, OBJ fetch latches, clipped window and STAT edge tests PASS\n";}
