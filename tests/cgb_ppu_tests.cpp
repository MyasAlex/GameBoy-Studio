#include "../src/gameboy.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(expr) do { if(!(expr)) { std::cerr<<"CHECK FAILED: "<<#expr<<" at "<<__FILE__<<":"<<__LINE__<<"\n"; std::abort(); } } while(false)

static void setColor(std::array<u8,64>& ram,int palette,int index,u16 rgb){
    const int address=palette*8+index*2;
    ram[address]=(u8)rgb;ram[address+1]=(u8)(rgb>>8);
}
static void solidTile(Memory& m,int bank,int tile,int raw){
    for(int row=0;row<8;++row){
        m.vram[bank*8192+tile*16+row*2]=(raw&1)?0xFF:0;
        m.vram[bank*8192+tile*16+row*2+1]=(raw&2)?0xFF:0;
    }
}
static u32 renderPixel(PPU& p,int x=0,int y=0){
    p.state.x=x;p.state.line=y;p.pixel();return p.drawing[y*160+x];
}
static void initColor(Memory& m,PPU& p){
    m.cgbMode=true;m.io[0x40]=0x93;
    m.io[0x42]=m.io[0x43]=0;m.io[0x47]=0xE4;
    m.vram.fill(0);m.oam.fill(0);m.bgPalette.fill(0);m.objPalette.fill(0);
    setColor(m.bgPalette,0,0,0x7FFF);setColor(m.bgPalette,0,1,0x03E0);
    setColor(m.objPalette,0,1,0x001F);setColor(m.objPalette,0,2,0x7C00);
    solidTile(m,0,0,1);solidTile(m,0,4,1);solidTile(m,0,5,2);
    p.reset();
}

static void testRgb555(){
    Memory m;PPU p(m);m.cgbMode=true;
    setColor(m.bgPalette,7,3,0x001F);CHECK(p.color(m.bgPalette,7,3)==0xFFFF0000);
    setColor(m.bgPalette,7,3,0x03E0);CHECK(p.color(m.bgPalette,7,3)==0xFF00FF00);
    setColor(m.bgPalette,7,3,0x7C00);CHECK(p.color(m.bgPalette,7,3)==0xFF0000FF);
    setColor(m.bgPalette,7,3,0x4210);CHECK(p.color(m.bgPalette,7,3)==0xFF848484);
    setColor(m.bgPalette,7,3,0xC210);CHECK(p.color(m.bgPalette,7,3)==0xFF848484);
    p.power(false);CHECK(p.pixels[0]==0xFFFFFFFF);
}

static void testBackgroundAttributes(){
    Memory m;PPU p(m);initColor(m,p);m.io[0x40]=0x91;
    // The tile index belongs to bank 0, attributes to bank 1, independently of VBK.
    m.vram[0x1800]=2;m.vram[8192+0x1800]=0x08|5;m.io[0x4F]=1;
    setColor(m.bgPalette,5,1,0x001F);setColor(m.bgPalette,5,2,0x7C00);
    m.vram[8192+2*16]=0x80; // Upper-left color 1.
    m.vram[8192+2*16+15]=0x01; // Lower-right color 2.
    CHECK(renderPixel(p)==0xFFFF0000);
    CHECK(renderPixel(p,7,7)==0xFF0000FF);
    m.vram[8192+0x1800]|=0x60;
    CHECK(renderPixel(p)==0xFF0000FF);
    CHECK(renderPixel(p,7,7)==0xFFFF0000);
    // Signed tile addressing must use $9000 as origin, in the selected tile bank.
    m.io[0x40]&=(u8)~0x10;m.vram[0x1800]=0xFF;m.vram[8192+0x1800]=0x0D;
    m.vram[8192+0x0FF0]=0x80;
    CHECK(renderPixel(p)==0xFFFF0000);
    // CGB LCDC.0 only controls BG/OBJ priority; background pixels remain visible.
    m.io[0x40]&=(u8)~1;CHECK(renderPixel(p)==0xFFFF0000);
}

static void testSpriteAttributesAndPriority(){
    Memory m;PPU p(m);initColor(m,p);
    p.state.selectedCount=1;p.state.sprites[0]={0,0,0,4,0};
    CHECK(renderPixel(p)==0xFFFF0000);
    // Exhaust all BG priority, OBJ priority, master-priority and BG-index combinations.
    for(int bgRaw=0;bgRaw<2;++bgRaw)for(int master=0;master<2;++master)
        for(int bgPriority=0;bgPriority<2;++bgPriority)for(int objPriority=0;objPriority<2;++objPriority){
            solidTile(m,0,0,bgRaw);m.io[0x40]=(u8)(0x92|master);
            m.vram[8192+0x1800]=(u8)(bgPriority<<7);
            p.state.sprites[0].attr=(u8)(objPriority<<7);
            const bool bgWins=bgRaw&&master&&(bgPriority||objPriority);
            CHECK(renderPixel(p)==(bgWins?0xFF00FF00:0xFFFF0000));
        }
    initColor(m,p);p.state.selectedCount=1;
    // Bank 1 OBJ tile uses its three-bit CGB palette, ignoring DMG palette bit 4.
    solidTile(m,1,4,2);setColor(m.objPalette,3,2,0x7C1F);
    p.state.sprites[0]={0,0,0,4,0x1B};CHECK(renderPixel(p)==0xFFFF00FF);
    // Transparent color zero lets lower-priority objects through.
    p.state.selectedCount=2;p.state.sprites[0]={0,0,0,6,0};
    p.state.sprites[1]={1,0,0,5,0};CHECK(renderPixel(p)==0xFF0000FF);
    // CGB chooses original OAM order even though fetches are X-sorted.
    p.state.sprites[0]={1,0,0,5,0};p.state.sprites[1]={0,4,0,4,0};
    CHECK(renderPixel(p,4)==0xFFFF0000);
    // An opaque winning OBJ hidden behind BG still masks the later OBJ.
    p.state.sprites[1].attr=0x80;CHECK(renderPixel(p,4)==0xFF00FF00);
}

static void testSpriteFlipsAndPairs(){
    Memory m;PPU p(m);initColor(m,p);
    p.state.selectedCount=1;p.state.sprites[0]={0,0,0,6,0x08};
    m.vram[8192+6*16]=0x80;m.vram[8192+6*16+15]=1;
    CHECK(renderPixel(p)==0xFFFF0000);
    p.state.sprites[0].attr=0x68;CHECK(renderPixel(p)==0xFF0000FF);
    // In 8x16 mode an odd tile index is rounded down, then the whole pair flips.
    m.io[0x40]|=4;p.state.sprites[0]={0,0,0,7,0x08};
    CHECK(renderPixel(p)==0xFFFF0000);
    m.vram[8192+7*16+15]=0x80;p.state.sprites[0].attr=0x48;
    CHECK(renderPixel(p)==0xFF0000FF);
}

static void testWindowAndOamScan(){
    Memory m;PPU p(m);initColor(m,p);
    m.io[0x40]=0xF0; // Window enabled, map $9C00, BG master priority clear.
    m.io[0x4A]=0;m.io[0x4B]=7;m.vram[0x1C00]=5;
    setColor(m.bgPalette,0,2,0x7C00);p.reset();p.tick(456*144);
    CHECK(p.pixels[0]==0xFF0000FF);CHECK(p.state.windowLine==144);
    // Off-screen X positions still consume one of the ten OAM selection slots.
    initColor(m,p);
    for(int i=0;i<11;++i){m.oam[i*4]=16;m.oam[i*4+1]=(u8)(i==10?8:0);m.oam[i*4+2]=4;}
    p.reset();p.tick(456*144);CHECK(p.pixels[0]==0xFF00FF00);
    m.oam[0]=0;p.reset();p.tick(456*144);CHECK(p.pixels[0]==0xFFFF0000);
}

static void testDmgPreserved(){
    Memory m;PPU p(m);initColor(m,p);m.cgbMode=false;
    m.io[0x47]=0xE4;m.io[0x48]=0xE4;m.io[0x49]=0x1B;
    m.vram[8192+0x1800]=0xEF; // CGB bank, palette and flips must be ignored.
    CHECK(renderPixel(p)==p.shade(1));
    p.state.selectedCount=2;p.state.sprites[0]={1,0,0,5,0};p.state.sprites[1]={0,4,0,4,0};
    CHECK(renderPixel(p,4)==p.shade(2)); // Smaller X wins in DMG mode.
    p.state.selectedCount=1;p.state.sprites[0]={0,0,0,4,0x18};
    CHECK(renderPixel(p)==p.shade(2)); // OBP1 remains active, CGB bank ignored.
    m.io[0x40]&=(u8)~1;p.state.selectedCount=0;CHECK(renderPixel(p)==p.shade(0));
    m.io[0x40]|=0x20;m.io[0x4A]=0;m.io[0x4B]=7;p.reset();p.tick(456*144);
    CHECK(p.state.windowLine==0); // DMG LCDC.0 disables the window.
}

int main(){
    testRgb555();testBackgroundAttributes();testSpriteAttributesAndPriority();
    testSpriteFlipsAndPairs();testWindowAndOamScan();testDmgPreserved();
    std::cout<<"CGB PPU tests passed\n";
}
