#pragma once
// Internal module: included after Memory and CPU by gameboy.hpp.
class PPU {
public:
    Memory& m;
    std::array<u32,GB_W*GB_H> pixels{}, drawing{};
    int dot=0, palette=0;
    bool lcdWasOn=true;
    u64 frameNumber=0;
    struct Sprite {int id=0,x=0,y=0,tile=0;u8 attr=0;};
    struct Pipeline {
        int line=0,windowLine=0,selectedCount=0,x=0,stall=0,scx=0,lastSpriteTile=-100000,windowX=0;
        bool windowYTriggered=false,windowActive=false,windowDrawn=false,firstLine=false;
        std::array<Sprite,10> sprites{};
        std::array<bool,10> fetched{};
    } state;
    // Pixel data and fetch bus latches are independent of the CPU-visible
    // registers. State is explicit so mid-fetch snapshots continue exactly.
    struct AccuracyState {
        bool active=false,windowFetch=false,pushed=false;
        int fetchPhase=0,fetchTileX=0,fetchY=0,mapAddress=0,tileAddress=0;
        u8 tile=0,attributes=0,low=0,high=0;
        int fifoRead=0,fifoSize=0,discard=0,startup=0,windowWX=0;
        std::array<u8,16> bgRaw{},bgAttributes{};
        int objIndex=-1,objPhase=0,objDelay=0,objAddress=0;
        u8 objLow=0,objHigh=0,objAttributes=0;
        std::array<u8,160> objRaw{},objAttrs{},objIds{};
        std::array<u8,2> scyHistory{};
    } accuracy;
    template<class Codec> static void visitAccuracy(AccuracyState& a,Codec& c){
        c.fields(a.active,a.windowFetch,a.pushed,a.fetchPhase,a.fetchTileX,a.fetchY,a.mapAddress,a.tileAddress,
            a.tile,a.attributes,a.low,a.high,a.fifoRead,a.fifoSize,a.discard,a.startup,a.windowWX);
        c.array(a.bgRaw);c.array(a.bgAttributes);
        c.fields(a.objIndex,a.objPhase,a.objDelay,a.objAddress,a.objLow,a.objHigh,a.objAttributes);
        c.array(a.objRaw);c.array(a.objAttrs);c.array(a.objIds);c.array(a.scyHistory);
    }
    template<class Codec> void visitAccuracy(Codec& c){visitAccuracy(accuracy,c);}
    bool accuracyValid()const{
        const auto& a=accuracy;
        // Only selected sprites have initialized and validated OAM identities.
        // A pending fetch must never address an unused entry of that array.
        if(state.selectedCount<0||state.selectedCount>10||a.objIndex>=state.selectedCount)return false;
        if(a.fetchPhase<0||a.fetchPhase>7||a.fetchTileX<0||a.fetchTileX>31||a.fetchY<0||a.fetchY>255||
           a.mapAddress<0||a.mapAddress>=8192||a.tileAddress<0||a.tileAddress>=8192||
           a.fifoRead<0||a.fifoRead>15||a.fifoSize<0||a.fifoSize>16||a.discard<0||a.discard>7||
           a.startup<0||a.startup>25||a.windowWX<0||a.windowWX>255||a.objIndex< -1||a.objIndex>9||
           a.objPhase<0||a.objPhase>6||a.objDelay<0||a.objDelay>16||a.objAddress<0||a.objAddress>=8192)return false;
        for(u8 raw:a.bgRaw)if(raw>3)return false;
        for(u8 raw:a.objRaw)if(raw>3)return false;
        for(u8 id:a.objIds)if(id>=40&&id!=255)return false;
        return true;
    }
    void restoreLegacyAccuracy(){
        accuracy={};accuracy.objIds.fill(255);accuracy.scyHistory.fill(m.io[0x42]);
        if(m.ppuMode==3){accuracy.active=true;accuracy.fetchTileX=((state.x+(state.scx&7))/8)&31;}
    }
    static constexpr std::array<std::array<u32,4>,4> palettes{{
        {{0xFF9BBD0F,0xFF8BAC0F,0xFF306230,0xFF0F380F}},
        {{0xFFFFFFFF,0xFFAAAAAA,0xFF555555,0xFF000000}},
        {{0xFFDCEAF6,0xFF8CA7C2,0xFF3E6991,0xFF192F4A}},
        {{0xFFFFE8B8,0xFFD6A078,0xFF89506B,0xFF34172A}}
    }};
    explicit PPU(Memory& mem):m(mem){reset();}
    u32 shade(int value)const{return palettes[palette&3][value&3];}
    u32 blankColor()const{return m.dmgCompatibility&&!m.cgbMode?color(m.bgPalette,0,0):m.cgbMode?0xFFFFFFFF:shade(0);}
    u32 color(const std::array<u8,64>& ram,int paletteNumber,int raw)const{
        const int address=(paletteNumber&7)*8+(raw&3)*2;
        const u16 rgb=(u16)(ram[address]|(ram[address+1]<<8));
        const auto expand=[](u32 value){return (value<<3)|(value>>2);};
        return 0xFF000000|(expand(rgb&31)<<16)|(expand((rgb>>5)&31)<<8)|expand((rgb>>10)&31);
    }
    void reset(){
        dot=0;state={};accuracy={};accuracy.objIds.fill(255);accuracy.scyHistory.fill(m.io[0x42]);frameNumber=0;lcdWasOn=(m.io[0x40]&0x80)!=0;
        m.io[0x44]=m.ppuCompareLY=0;m.ppuDot=0;m.ppuMode=m.ppuStatMode=lcdWasOn?2:0;
        pixels.fill(blankColor());drawing=pixels;beginLine();syncStat();
    }
    void power(bool enabled){
        dot=enabled?-2:0;state={};accuracy={};accuracy.objIds.fill(255);accuracy.scyHistory.fill(m.io[0x42]);state.firstLine=enabled;lcdWasOn=enabled;m.io[0x44]=0;m.ppuMode=m.ppuStatMode=0;
        m.ppuDot=dot;if(enabled)m.ppuCompareLY=0;
        if(!enabled){pixels.fill(blankColor());drawing=pixels;}
        else beginLine();
        syncStat();
    }
    void beginLine(){
        state.selectedCount=0;state.x=0;state.windowActive=state.windowDrawn=false;accuracy.active=false;accuracy.objIndex=-1;
        state.fetched.fill(false);state.lastSpriteTile=-100000;
        if(state.line==m.io[0x4A])state.windowYTriggered=true;
    }
    struct BackgroundPixel {int raw=0;u8 attr=0;};
    // Trace the native fetcher's reads. Debugger lookups may opt out of tracing.
    u8 readVram(unsigned bank,u16 offset,bool trace=true)const{
        const u8 value=m.vramByte(bank,offset);
        if(trace&&m.ppuBusObserver)m.ppuBusObserver((u16)(0x8000|(offset&0x1FFF)),value,false);
        return value;
    }
    u8 readOam(unsigned offset)const{
        const u8 value=m.oam[offset];
        if(m.ppuBusObserver)m.ppuBusObserver((u16)(0xFE00+offset),value,false);
        return value;
    }
    BackgroundPixel backgroundAt(bool window,int px,int py,bool trace=false)const {
        const u8 lcdc=m.io[0x40];
        const int mapBase=(window?(lcdc&0x40):(lcdc&8))?0x1C00:0x1800;
        const int mapAddress=mapBase+((py&255)/8)*32+(px&255)/8;
        const u8 tile=readVram(0,(u16)mapAddress,trace);
        const u8 attr=m.cgbMode?readVram(1,(u16)mapAddress,trace):0;
        const int offset=(lcdc&0x10)?tile*16:0x1000+(int)(s8)tile*16;
        const int row=((attr&0x40)?7-(py&7):(py&7))*2;
        const int bit=(attr&0x20)?(px&7):7-(px&7);
        const unsigned bank=(attr>>3)&1;
        return {((readVram(bank,(u16)(offset+row),trace)>>bit)&1)|
                (((readVram(bank,(u16)(offset+row+1),trace)>>bit)&1)<<1),attr};
    }
    int tileAt(bool window,int px,int py)const{return backgroundAt(window,px,py).raw;}
    void scanSprite(int id){
        if(m.dmaPos<160)return;
        const int height=(m.io[0x40]&4)?16:8;
        const int y=(int)readOam(id*4)-16;
        if(state.selectedCount<10&&state.line>=y&&state.line<y+height){
            state.sprites[state.selectedCount++]={id,(int)readOam(id*4+1)-8,y,0,0};
        }
    }
    void pixel(){
        const int x=state.x,y=state.line;
        const u8 lcdc=m.io[0x40];
        const bool bgEnabled=m.cgbMode||(lcdc&1);
        const auto bg=bgEnabled?backgroundAt(state.windowActive,
            state.windowActive?x-state.windowX:x+m.io[0x43],
            state.windowActive?state.windowLine:y+m.io[0x42],true):BackgroundPixel{};
        u32 output=(m.dmgCompatibility&&!m.cgbMode)?color(m.bgPalette,0,bgEnabled?((m.io[0x47]>>(2*bg.raw))&3):0):m.cgbMode?color(m.bgPalette,bg.attr&7,bg.raw):
            shade(bgEnabled?((m.io[0x47]>>(2*bg.raw))&3):0);
        if(lcdc&2){
            // Select the winning OBJ first, then apply its BG priority. A hidden
            // winner still masks lower-priority OBJs (important in Mario games).
            // Sprites remain X-sorted for fetch timing, but CGB drawing priority
            // is determined exclusively by the original OAM index.
            const Sprite* winner=nullptr;
            int winnerRaw=0;
            for(int i=0;i<state.selectedCount;++i){
                const auto& s=state.sprites[i];
                if(x<s.x||x>=s.x+8)continue;
                const int height=(lcdc&4)?16:8;
                int row=y-s.y;
                if(s.attr&0x40)row=height-1-row;
                int tile=height==16?(s.tile&0xFE):s.tile;
                tile+=row/8;
                const int off=tile*16+(row&7)*2;
                const int bit=(s.attr&0x20)?x-s.x:7-(x-s.x);
                const unsigned bank=m.cgbMode?((s.attr>>3)&1):0;
                const int raw=((readVram(bank,(u16)off)>>bit)&1)|
                    (((readVram(bank,(u16)(off+1))>>bit)&1)<<1);
                if(!raw)continue;
                if(!winner||s.id<winner->id){winner=&s;winnerRaw=raw;}
                if(!m.cgbMode)break;
            }
            if(winner){
                const bool behind=m.cgbMode?
                    ((lcdc&1)&&bg.raw&&((bg.attr|winner->attr)&0x80)):
                    (bg.raw&&(winner->attr&0x80));
                if(!behind)output=(m.dmgCompatibility&&!m.cgbMode)?color(m.objPalette,(winner->attr&0x10)?1:0,(m.io[(winner->attr&0x10)?0x49:0x48]>>(winnerRaw*2))&3):m.cgbMode?color(m.objPalette,winner->attr&7,winnerRaw):
                    shade((m.io[(winner->attr&0x10)?0x49:0x48]>>(winnerRaw*2))&3);
            }
        }
        drawing[y*GB_W+x]=output;
    }
    void startTransfer(){
        const auto history=accuracy.scyHistory;accuracy={};accuracy.scyHistory=history;accuracy.objIds.fill(255);
        accuracy.active=true;accuracy.discard=state.scx&7;accuracy.startup=12;
        const int wx=(int)m.io[0x4B]-7;
        if(wx<0&&state.windowYTriggered&&(m.io[0x40]&0x20)&&(m.cgbMode||(m.io[0x40]&1))){
            // The clipped left part of the window is fetched before visible
            // X=0, instead of extending transfer by seven minus WX dots.
            state.windowActive=state.windowDrawn=true;state.windowX=wx;
            accuracy.windowFetch=true;accuracy.windowWX=m.io[0x4B];
            accuracy.startup+=6+(state.scx&7)-((wx==-7&&(state.scx&7))?1:0);
            accuracy.discard=-wx;
        }
        if((m.io[0x40]&2)&&state.selectedCount&&state.sprites[0].x<160)--accuracy.startup;
        state.stall=accuracy.startup;
    }
    int fetchScrollY()const{return m.isCGBHardware()?accuracy.scyHistory[0]:m.io[0x42];}
    bool latchesFetchY()const{
        return m.hardwareModel==HardwareModel::CGBD||m.hardwareModel==HardwareModel::CGBE||
               m.hardwareModel==HardwareModel::CGB||m.hardwareModel==HardwareModel::AGB;
    }
    int planeAddress(int plane)const{
        const auto& a=accuracy;
        int y=a.windowFetch?state.windowLine:latchesFetchY()?a.fetchY:((state.line+fetchScrollY())&255);
        if(a.attributes&0x40)y=7-y;
        const int base=(m.io[0x40]&0x10)?a.tile*16:0x1000+(int)(s8)a.tile*16;
        return (base+(y&7)*2+plane)&0x1FFF;
    }
    void pushBackground(){
        auto& a=accuracy;
        for(int i=0;i<8;++i){const int bit=(a.attributes&0x20)?i:7-i;
            const int at=(a.fifoRead+a.fifoSize+i)&15;
            a.bgRaw[at]=(u8)(((a.low>>bit)&1)|(((a.high>>bit)&1)<<1));a.bgAttributes[at]=a.attributes;}
        a.fifoSize+=8;a.pushed=true;
    }
    void fetchBackgroundDot(){
        auto& a=accuracy;
        switch(a.fetchPhase){
            case 0:{
                // Window disable is sampled between tiles, rather than changing
                // pixels which have already been fetched into the FIFO.
                if(a.windowFetch&&!(m.io[0x40]&0x20)){
                    a.windowFetch=false;state.windowActive=false;
                    a.fetchTileX=((state.x+a.fifoSize+7)/8)&31;
                }
                a.fetchY=a.windowFetch?state.windowLine:(state.line+fetchScrollY())&255;
                const int x=a.windowFetch?a.fetchTileX:((m.io[0x43]>>3)+a.fetchTileX)&31;
                const bool highMap=a.windowFetch?(m.io[0x40]&0x40):(m.io[0x40]&8);
                a.mapAddress=(highMap?0x1C00:0x1800)+((a.fetchY&255)/8)*32+x;a.pushed=false;break;}
            case 1:a.tile=readVram(0,(u16)a.mapAddress);a.attributes=m.cgbMode?readVram(1,(u16)a.mapAddress):0;break;
            case 2:a.tileAddress=planeAddress(0);break;
            case 3:a.low=readVram((a.attributes>>3)&1,(u16)a.tileAddress);break;
            case 4:a.tileAddress=planeAddress(1);break;
            case 5:a.high=readVram((a.attributes>>3)&1,(u16)a.tileAddress);
                if(a.fifoSize==0)pushBackground();break;
            case 6:break;
            case 7:
                if(!a.pushed){if(a.fifoSize)return;pushBackground();}
                a.fetchTileX=(a.fetchTileX+1)&31;break;
        }
        a.fetchPhase=(a.fetchPhase+1)&7;
    }
    int objectAddress(const Sprite& sprite)const{
        const int height=(m.io[0x40]&4)?16:8;
        int row=state.line-sprite.y;if(accuracy.objAttributes&0x40)row=height-1-row;
        const int tile=(height==16?sprite.tile&0xFE:sprite.tile)+((row&15)/8);
        return (tile*16+(row&7)*2)&0x1FFF;
    }
    void fetchObjectDot(){
        auto& a=accuracy;auto& sprite=state.sprites[a.objIndex];
        if(!m.isCGBHardware()&&!(m.io[0x40]&2)){a.objIndex=-1;state.stall=0;return;}
        if(a.objDelay){fetchBackgroundDot();--a.objDelay;state.stall=a.objDelay+6;return;}
        switch(++a.objPhase){
            case 1:sprite.tile=readOam(sprite.id*4+2);a.objAttributes=readOam(sprite.id*4+3);sprite.attr=a.objAttributes;break;
            case 2:a.objAddress=objectAddress(sprite);break;
            case 3:a.objAddress=objectAddress(sprite);a.objLow=readVram(m.cgbMode?(a.objAttributes>>3)&1:0,(u16)a.objAddress);break;
            case 4:a.objAddress=objectAddress(sprite);break;
            case 5:a.objHigh=readVram(m.cgbMode?(a.objAttributes>>3)&1:0,(u16)(a.objAddress+1));break;
            case 6:
                for(int i=0;i<8;++i){const int x=sprite.x+i;if(x<state.x||x<0||x>=160)continue;
                    const int bit=(a.objAttributes&0x20)?i:7-i;const u8 raw=(u8)(((a.objLow>>bit)&1)|(((a.objHigh>>bit)&1)<<1));
                    if(!raw)continue;if(a.objRaw[x]&&(!m.cgbMode||a.objIds[x]<sprite.id))continue;
                    a.objRaw[x]=raw;a.objAttrs[x]=a.objAttributes;a.objIds[x]=(u8)sprite.id;
                }
                a.objIndex=-1;break;
        }
        state.stall=a.objIndex<0?0:6-a.objPhase;
    }
    void fetchedPixel(u8 raw,u8 attr){
        const int x=state.x;const auto& a=accuracy;const u8 lcdc=m.io[0x40];
        const bool bgEnabled=m.cgbMode||(lcdc&1);
        if(!bgEnabled)raw=0;
        const int bgShade=bgEnabled?(m.io[0x47]>>(2*raw))&3:0;
        u32 output=m.cgbMode?color(m.bgPalette,attr&7,raw):m.dmgCompatibility?color(m.bgPalette,0,bgShade):shade(bgShade);
        if((lcdc&2)&&a.objRaw[x]){const u8 obj=a.objAttrs[x];const bool behind=m.cgbMode?
                ((lcdc&1)&&raw&&((attr|obj)&0x80)):(raw&&(obj&0x80));
            if(!behind){const int objShade=(m.io[(obj&0x10)?0x49:0x48]>>(2*a.objRaw[x]))&3;
                output=m.cgbMode?color(m.objPalette,obj&7,a.objRaw[x]):m.dmgCompatibility?color(m.objPalette,(obj&0x10)?1:0,objShade):shade(objShade);}}
        drawing[state.line*160+x]=output;
    }
    void transferDot(){
        auto& a=accuracy;if(!a.active)startTransfer();
        if(a.objIndex>=0){fetchObjectDot();return;}
        if(a.startup){
            fetchBackgroundDot();
            if(a.windowFetch&&state.windowX<0&&a.discard&&a.fifoSize){
                a.fifoRead=(a.fifoRead+1)&15;--a.fifoSize;--a.discard;
            }
            --a.startup;state.stall=a.startup;return;
        }
        const int wx=(int)m.io[0x4B]-7;
        if(!a.windowFetch&&state.windowYTriggered&&(m.io[0x40]&0x20)&&
           (m.cgbMode||(m.io[0x40]&1))&&wx<160&&state.x==std::max(0,wx)){
            if(state.windowDrawn)++state.windowLine;
            state.windowActive=state.windowDrawn=true;state.windowX=wx;
            a.windowFetch=true;a.windowWX=m.io[0x4B];a.fetchPhase=0;a.fetchTileX=0;a.fifoRead=a.fifoSize=0;
            a.discard=wx<0?-wx:0;a.startup=0;state.lastSpriteTile=-100000;return;
        }
        if(a.windowFetch&&m.io[0x4B]!=a.windowWX&&wx>=0&&state.x==wx){
            // The reactivation glitch is qualified by the window tile-map
            // read, rather than firing on every pixel whose X matches WX.
            // The inserted color-zero pixel has the lowest BG priority.
            if(a.fetchPhase==1&&a.fifoSize<16){a.fifoRead=(a.fifoRead+15)&15;a.bgRaw[a.fifoRead]=a.bgAttributes[a.fifoRead]=0;++a.fifoSize;}
            a.windowWX=m.io[0x4B];
        }
        if((m.io[0x40]&2)||m.isCGBHardware()){
            for(int i=0;i<state.selectedCount;++i){const auto& sprite=state.sprites[i];
                if(state.fetched[i]||sprite.x>=160||sprite.x>state.x)continue;
                state.fetched[i]=true;const int pos=a.windowFetch?sprite.x-state.windowX:sprite.x+state.scx;
                const int key=(pos>=0?pos/8:(pos-7)/8)+(a.windowFetch?64:0);
                const int penalty=6+(state.lastSpriteTile==key?0:sprite.x==-8?5:std::max(0,5-(pos&7)));
                state.lastSpriteTile=key;a.objIndex=i;a.objPhase=0;a.objDelay=penalty-6;fetchObjectDot();return;
            }
        }
        fetchBackgroundDot();if(!a.fifoSize)return;
        const u8 raw=a.bgRaw[a.fifoRead],attr=a.bgAttributes[a.fifoRead];a.fifoRead=(a.fifoRead+1)&15;--a.fifoSize;
        if(a.discard){--a.discard;return;}
        fetchedPixel(raw,attr);++state.x;
        if(state.x==GB_W-4){m.ppuStatMode=0;syncStat();}
        if(state.x==GB_W){m.ppuMode=0;a.active=false;syncStat();m.hblankDMA();}
    }
    void syncStat(){m.refreshStat();}
    void tick(int cycles){
        if(!(m.io[0x40]&0x80)){if(lcdWasOn)power(false);return;}
        if(!lcdWasOn)power(true);
        for(int t=0;t<cycles;++t){
            ++dot;
            m.ppuDot=dot;
            if(dot==4){m.ppuCompareLY=state.line==153?0:(u8)state.line;syncStat();}
            // CGB asserts its mode-2 STAT source one M-cycle before VBlank.
            // The readable mode remains HBlank until the line boundary.
            if(m.isCGBHardware()&&state.line==143&&dot==452){m.ppuStatMode=2;syncStat();}
            if(state.line<GB_H){
                const int oamEnd=state.firstLine?76:84;
                if(!state.firstLine&&dot==4){m.ppuMode=2;syncStat();}
                if(dot>(state.firstLine?0:4)&&dot<=oamEnd&&!(dot&1))scanSprite((dot-(state.firstLine?0:4))/2-1);
                if(dot==oamEnd){
                    std::sort(state.sprites.begin(),state.sprites.begin()+state.selectedCount,
                        [](const Sprite& a,const Sprite& b){return a.x==b.x?a.id<b.id:a.x<b.x;});
                    state.scx=m.io[0x43];startTransfer();
                    m.ppuMode=m.ppuStatMode=3;syncStat();
                }else if(m.ppuMode==3)transferDot();
            }else if(state.line==153&&dot==4){m.io[0x44]=m.ppuCompareLY=0;syncStat();}
            if(dot==(state.firstLine?448:456)){
                dot=0;
                m.ppuDot=0;
                state.firstLine=false;
                if(state.line<GB_H&&state.windowDrawn)++state.windowLine;
                ++state.line;
                m.ppuCompareLY=0xFF; // LY comparison is inactive for the first four dots.
                if(state.line==144){
                    pixels=drawing;++frameNumber;m.io[0x0F]|=1;
                    if((m.io[0x41]&0x20)&&!m.statLine)m.io[0x0F]|=2;
                    m.ppuMode=m.ppuStatMode=1;
                }
                else if(state.line==154){state.line=0;state.windowLine=0;state.windowYTriggered=false;m.ppuMode=0;m.ppuStatMode=2;}
                else if(state.line<144){m.ppuMode=0;m.ppuStatMode=2;}
                m.io[0x44]=(u8)state.line;
                if(state.line<144)beginLine();
                syncStat();
            }
            accuracy.scyHistory[1]=accuracy.scyHistory[0];accuracy.scyHistory[0]=m.io[0x42];
        }
    }
};
