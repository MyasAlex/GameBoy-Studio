#pragma once
#include "accuracy_api.h"
#include "portable_state.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

namespace accurate {
inline const GBSApi* api(){
#ifdef _WIN32
    static const GBSApi* table=[]()->const GBSApi* {
        wchar_t name[32768]{};const auto n=GetModuleFileNameW(nullptr,name,32768);if(!n||n>=32768)return nullptr;
        auto path=std::filesystem::path(name).parent_path()/L"gb_accuracy.dll";
        HMODULE dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!dll)return nullptr;auto entry=reinterpret_cast<const GBSApi*(*)()>(GetProcAddress(dll,"gbs_api_v1"));
        if(!entry){FreeLibrary(dll);return nullptr;}auto value=entry();if(value->version!=3||value->struct_size!=sizeof(GBSApi)){FreeLibrary(dll);return nullptr;}return value;
    }();return table;
#elif defined(GB_ACCURACY_LINKED)
    return gbs_api_v1();
#else
    return nullptr;
#endif
}
inline unsigned modelNumber(const GameBoy& g){switch(g.model){
    case HardwareModel::DMG:return 2;case HardwareModel::Pocket:case HardwareModel::Light:return 0x100;
    case HardwareModel::CGB0:return 0x200;case HardwareModel::CGBA:return 0x201;case HardwareModel::CGBB:return 0x202;
    case HardwareModel::CGBC:return 0x203;case HardwareModel::CGBD:return 0x204;case HardwareModel::CGBE:case HardwareModel::CGB:return 0x205;
    case HardwareModel::AGB:return 0x207;case HardwareModel::SGB:return 4;case HardwareModel::SGB2:return 0x101;case HardwareModel::SGBPAL:return 0x44;
    default:return g.mem.cgbROM?0x205:2;
}}
inline bool monochrome(HardwareModel m){return m==HardwareModel::DMG||m==HardwareModel::Pocket||m==HardwareModel::Light||m==HardwareModel::SGB||m==HardwareModel::SGB2||m==HardwareModel::SGBPAL;}
class Engine {
    GameBoy& g;const GBSApi* a;void* ctx=nullptr;GBSView view{};u32 cartridgeHash=0;bool serialClocking=false,serialIncoming=true;u8 serialOutgoing=1;
    static Engine& self(void* p){return *static_cast<Engine*>(p);}
    static u8 onRead(void* p,u16 addr,u8 value,u8 blocked){auto& e=self(p);auto& m=e.g.mem;
        if(addr<0x8000&&m.romReadFilter)value=m.romReadFilter(addr,value);if(m.busObserver)m.busObserver(addr,value,false,blocked!=0);return value;}
    static u8 onWrite(void* p,u16 addr,u8 value,u8 blocked){auto& m=self(p).g.mem;if(m.busObserver)m.busObserver(addr,value,true,blocked!=0);return 1;}
    static void onPPURead(void* p,u16 addr,u8 value,u8 blocked){auto& m=self(p).g.mem;if(m.ppuBusObserver)m.ppuBusObserver(addr,value,blocked!=0);}
    static void onSample(void* p,std::int16_t left,std::int16_t right,const u8* channels){auto& apu=self(p).g.apu;
        if(apu.count+2<=apu.queue.size()){apu.queue[apu.writePos]=left;apu.writePos=(apu.writePos+1)%apu.queue.size();apu.queue[apu.writePos]=right;apu.writePos=(apu.writePos+1)%apu.queue.size();apu.count+=2;}
        for(int i=0;i<4;++i)apu.scope[i][apu.scopePosition]=(std::int16_t)((int)channels[i]*2048-15360);apu.scopePosition=(apu.scopePosition+1)%512;
    }
    static void onFrame(void* p){auto& e=self(p);++e.g.ppu.frameNumber;e.copyPixels();}
    static u8 onSerial(void* p,u8 bit){auto& e=self(p);e.pullView();e.serialClocking=true;e.serialIncoming=true;e.serialOutgoing=bit;
        // Preserve SC's transfer-enable bit for the last clock callback.
        const u8 sc=e.g.mem.io[2];e.g.mem.io[2]|=0x80;e.g.mem.io[1]=(u8)(bit<<7);
        if(e.g.mem.serialClock)e.g.mem.serialClock();e.g.mem.io[2]=sc;e.serialClocking=false;return e.serialIncoming?1:0;
    }
    static void onSerialByte(void* p,u8 byte){auto& m=self(p).g.mem;if(m.serialOutput.size()<65536)m.serialOutput+=(char)byte;}
    static u8 onInfrared(void* p){auto& m=self(p).g.mem;return m.infraredInput&&m.infraredInput()?1:0;}
    static void onRumble(void* p,double amplitude){self(p).g.mem.rumbleActive=amplitude>0.01;}
    static void onCamera(void* p,u8* data){auto& m=self(p).g.mem;if(m.cameraImage){const auto image=m.cameraImage();std::copy(image.begin(),image.end(),data);}else std::fill_n(data,128*112,u8{128});}
    static void onPrint(void* p,const u32* pixels,unsigned height,unsigned top,unsigned bottom){auto& e=self(p);if(e.g.backendPrint)e.g.backendPrint({pixels,(std::size_t)160*height},height,top,bottom);}
    static u32 checksum(std::span<const u8> data){return saves::hash(data);}
    void pullView(){a->view(ctx,&view);auto& m=g.mem;auto& c=g.cpu;
        c.A=view.a;c.F=view.f;c.B=view.b;c.C=view.c;c.D=view.d;c.E=view.e;c.H=view.h;c.L=view.l;c.SP=view.sp;c.PC=view.pc;
        c.IME=view.ime!=0;c.halted=view.halted!=0;c.stopped=view.stopped!=0;c.haltBug=view.halt_bug!=0;
        g.clockRate=view.clock_rate;m.hucIRLight=view.cart_ir!=0;m.rumbleActive=view.rumble!=0;m.divCounter=view.div;m.ie=view.ie;m.cgbHardware=view.cgb!=0;m.dmgCompatibility=view.compat!=0;m.cgbMode=view.cgb&&!view.compat;
        m.doubleSpeed=view.speed!=0;m.bootROMMapped=view.boot!=0;m.ramEnabled=view.ram_enabled!=0;m.ramBank=view.ram_bank;m.romBank=view.romN;
        std::copy_n(view.io,128,m.io.begin());std::copy_n(view.rtc,5,m.rtc.begin());std::copy_n(view.latched_rtc,5,m.latchedRTC.begin());m.serialBits=view.serial_bits;
        m.ppuMode=m.ppuStatMode=m.io[0x41]&3;g.ppu.state.line=view.line;g.ppu.state.x=view.x;
    }
    template<class T>void pull(unsigned access,T& target){std::size_t n=0;const auto* data=static_cast<const u8*>(a->memory(ctx,access,&n));
        if(data)std::copy_n(data,std::min(n,target.size()),target.begin());}
    void copyPixels(){unsigned height=0;const unsigned width=a->screen_size(ctx,&height);const auto* p=a->pixels(ctx);if(!p)return;
        if(width==256&&height==224){g.accessoryWidth=width;g.accessoryHeight=height;g.accessoryPixels.assign(p,p+width*height);
            for(int y=0;y<144;++y)std::copy_n(p+(y+40)*256+48,160,g.ppu.pixels.begin()+y*160);
        }else{g.accessoryWidth=160;g.accessoryHeight=144;g.accessoryPixels.clear();std::copy_n(p,160*144,g.ppu.pixels.begin());}g.ppu.drawing=g.ppu.pixels;
    }
    GBSSettings settings(){GBSSettings s{};s.rate=(u32)g.apu.sampleRate;s.palette=(u8)g.currentPalette;
        auto& m=g.mem;s.buttons[0]=(u8)((m.right?1:0)|(m.left?2:0)|(m.up?4:0)|(m.down?8:0)|(m.a?16:0)|(m.b?32:0)|(m.select?64:0)|(m.start?128:0));
        for(int i=1;i<4;++i)s.buttons[i]=g.multiplayerInput[i];for(int i=0;i<4;++i){s.mute[i]=g.apu.mutedChannels[i]?1:0;s.gain[i]=(u8)g.apu.channelVolume[i];}s.ppu_watch=m.ppuBusObserver?1:0;s.tilt_x=m.motion.tiltX;s.tilt_y=m.motion.tiltY;return s;
    }
public:
    Engine(GameBoy& value,const GBSApi* apiTable):g(value),a(apiTable){GBSCallbacks cb={onRead,onWrite,onSample,onFrame,onSerial,onSerialByte,onInfrared,onRumble,onCamera,onPrint,onPPURead};ctx=a->create(&cb,this);}
    ~Engine(){if(ctx)a->destroy(ctx);}
    bool valid()const{return ctx!=nullptr;}
    void refresh(){pullView();pull(1,g.mem.wram);pull(2,g.mem.eram);pull(3,g.mem.vram);pull(4,g.mem.hram);pull(7,g.mem.oam);pull(8,g.mem.bgPalette);pull(9,g.mem.objPalette);copyPixels();}
    bool reset(){cartridgeHash=g.romHash();const unsigned model=modelNumber(g);auto& m=g.mem;const bool cgb=(model&0xF00)==0x200;
        m.bootROMCGB=cgb;m.bootROM=cgb?m.cgbBootROM:m.dmgBootROM;auto batteryPath=m.romPath;batteryPath+=".sbbattery";auto bytes=Memory::loadBytes(batteryPath,2*1024*1024);
        if(bytes.empty())bytes=m.eram;
        if(!a->reset(ctx,model,m.rom.data(),m.rom.size(),m.bootROM.data(),m.bootROM.size(),bytes.data(),bytes.size()))return false;
        std::size_t n=0;a->memory(ctx,2,&n);if(n>2*1024*1024)return false;m.eram.resize(n);
        m.hasBattery=m.hasBattery||m.mbc==100||m.mbc==103||m.mbc==6;m.hasRTC=m.hasRTC||m.mbc==103;refresh();return true;
    }
    int step(){if(g.cpu.A!=view.a||g.cpu.F!=view.f||g.cpu.B!=view.b||g.cpu.C!=view.c||g.cpu.D!=view.d||g.cpu.E!=view.e||g.cpu.H!=view.h||g.cpu.L!=view.l||g.cpu.PC!=view.pc||g.cpu.SP!=view.sp)commitRegisters();auto s=settings();a->settings(ctx,&s);const unsigned cycles=a->step(ctx);g.totalCycles+=cycles;pullView();return (int)(cycles*(g.mem.doubleSpeed?2:1));}
    u8 read(u16 addr){return a->read(ctx,addr);}
    void write(u16 addr,u8 value){commitRegisters();a->write(ctx,addr,value);pullView();}
    void pulse(bool bit){if(serialClocking)serialIncoming=bit;else {a->serial_pulse(ctx,bit?1:0);pullView();}}
    bool outgoing(){return serialClocking?serialOutgoing!=0:a->serial_out(ctx)!=0;}
    std::size_t bank(bool upper)const{return upper?view.romN:view.rom0;}
    void poke(unsigned access,std::size_t offset,u8 value){std::size_t n=0;auto* data=static_cast<u8*>(a->memory(ctx,access,&n));if(data&&offset<n){data[offset]=value;if(access==8||access==9)a->refresh_palettes(ctx);}}
    void commitRegisters(){GBSView v=view;v.a=g.cpu.A;v.f=g.cpu.F;v.b=g.cpu.B;v.c=g.cpu.C;v.d=g.cpu.D;v.e=g.cpu.E;v.h=g.cpu.H;v.l=g.cpu.L;v.pc=g.cpu.PC;v.sp=g.cpu.SP;a->set_registers(ctx,&v);}
    bool saveBattery(){if(!g.mem.hasBattery||g.mem.romPath.empty())return false;refresh();const auto n=a->battery_size(ctx);if(n>2*1024*1024)return false;
        std::vector<u8> bytes(n);if(!a->save_battery(ctx,bytes.data(),n))return false;auto path=g.mem.romPath;path+=".sbbattery";
        if(!Memory::writeAtomic(path,{(const char*)bytes.data(),bytes.size()},true))return false;
        return g.mem.eram.empty()||Memory::writeAtomic(g.mem.savePath(),{(const char*)g.mem.eram.data(),g.mem.eram.size()},true);
    }
    std::vector<u8> state(){commitRegisters();const auto n=a->state_size(ctx);if(n>2*1024*1024)return {};
        std::vector<u8> bytes={'G','B','S','A','C','C','1',0};saves::put32(bytes,cartridgeHash);saves::put32(bytes,modelNumber(g));
        saves::put32(bytes,(u32)g.apu.sampleRate);saves::put32(bytes,(u32)g.currentPalette);saves::put32(bytes,(u32)g.frameDebt);
        for(u64 value:{g.totalCycles,g.ppu.frameNumber})for(int i=0;i<8;++i)bytes.push_back((u8)(value>>(i*8)));
        saves::put32(bytes,(u32)n);auto inputs=settings();
        bytes.insert(bytes.end(),inputs.buttons,inputs.buttons+4);bytes.insert(bytes.end(),inputs.mute,inputs.mute+4);bytes.insert(bytes.end(),inputs.gain,inputs.gain+4);
        for(u16 tilt:{(u16)inputs.tilt_x,(u16)inputs.tilt_y}){bytes.push_back((u8)tilt);bytes.push_back((u8)(tilt>>8));}
        const auto at=bytes.size();bytes.resize(at+n);a->save_state(ctx,bytes.data()+at);
        for(u32 pixel:g.ppu.pixels)saves::put32(bytes,pixel);const auto auxiliary=g.auxiliaryState?g.auxiliaryState():std::vector<u8>{};saves::put32(bytes,(u32)auxiliary.size());bytes.insert(bytes.end(),auxiliary.begin(),auxiliary.end());saves::put32(bytes,checksum(bytes));return bytes;
    }
    std::vector<u8> bess(){commitRegisters();const auto n=a->state_size(ctx);if(n<16||n>2*1024*1024)return {};std::vector<u8> result(n);a->save_state(ctx,result.data());
        std::size_t end=n-16,at=saves::le32(result,n-8);for(unsigned i=0;i<128&&at+8<=n-8;++i){const auto length=saves::le32(result,at+4);if(length>n-8-at-8)return {};if(!std::memcmp(result.data()+at,"END ",4)){end=at;break;}at+=8+length;}
        const auto exact=state();std::vector<u8> block;saves::block(block,"GSTU",exact);result.insert(result.begin()+end,block.begin(),block.end());return result;}
    bool restore(std::span<const u8> bytes){if(bytes.size()>6*1024*1024||bytes.size()<16)return false;
        if(bytes.size()>=8&&!std::memcmp(bytes.data(),"GBSACC1",7)){
            constexpr std::size_t header=64,pixels=160*144*4;
            if(bytes.size()<header+pixels+8||saves::le32(bytes,8)!=cartridgeHash||saves::le32(bytes,12)!=modelNumber(g)||checksum(bytes.first(bytes.size()-4))!=saves::le32(bytes,bytes.size()-4))return false;
            const auto rate=saves::le32(bytes,16),palette=saves::le32(bytes,20),debt=saves::le32(bytes,24),size=saves::le32(bytes,44);
            if((rate!=44100&&rate!=48000&&rate!=96000)||palette>3||debt>8224||(u64)header+size+pixels+8>bytes.size())return false;
            const auto auxAt=header+size+pixels;const auto auxSize=saves::le32(bytes,auxAt);if((u64)auxAt+4+auxSize+4!=bytes.size())return false;
            for(int i=0;i<4;++i)if(bytes[52+i]>1||bytes[56+i]>100)return false;
            const auto oldAux=g.auxiliaryState?g.auxiliaryState():std::vector<u8>{};
            if(g.auxiliaryRestore&&!g.auxiliaryRestore(bytes.subspan(auxAt+4,auxSize))){g.auxiliaryRestore(oldAux);return false;}
            const auto backup=bess();if(!a->load_state(ctx,bytes.data()+header,size)){a->load_state(ctx,backup.data(),backup.size());if(g.auxiliaryRestore)g.auxiliaryRestore(oldAux);return false;}
            const auto inputs=bytes[48];g.mem.right=inputs&1;g.mem.left=inputs&2;g.mem.up=inputs&4;g.mem.down=inputs&8;g.mem.a=inputs&16;g.mem.b=inputs&32;g.mem.select=inputs&64;g.mem.start=inputs&128;
            for(int i=0;i<4;++i){g.multiplayerInput[i]=bytes[48+i];if(bytes[52+i]>1||bytes[56+i]>100){a->load_state(ctx,backup.data(),backup.size());return false;}g.apu.mutedChannels[i]=bytes[52+i]!=0;g.apu.channelVolume[i]=bytes[56+i];}
            g.mem.motion.tiltX=(std::int16_t)saves::le16(bytes,60);g.mem.motion.tiltY=(std::int16_t)saves::le16(bytes,62);g.apu.setSampleRate((int)rate);g.currentPalette=g.ppu.palette=(int)palette;g.frameDebt=(int)debt;
            auto number=[&](std::size_t at){u64 n=0;for(int i=0;i<8;++i)n|=(u64)bytes[at+i]<<(i*8);return n;};g.totalCycles=number(28);g.ppu.frameNumber=number(36);
            refresh();for(std::size_t i=0;i<g.ppu.pixels.size();++i)g.ppu.pixels[i]=saves::le32(bytes,header+size+i*4);g.ppu.drawing=g.ppu.pixels;
        }else if(!std::memcmp(bytes.data(),"GBSTATE",7)){
            auto old=std::make_unique<GameBoy>();saves::configureClone(*old,g);if(!old->deserializeState(bytes))return false;const auto portable=saves::exportBess(*old);
            const auto backup=bess();if(!a->load_state(ctx,portable.data(),portable.size())){a->load_state(ctx,backup.data(),backup.size());return false;}g.totalCycles=old->totalCycles;g.frameDebt=old->frameDebt;g.ppu.frameNumber=old->ppu.frameNumber;refresh();
        }else if(bytes.size()>=4&&!std::memcmp(bytes.data()+bytes.size()-4,"BESS",4)){
            // Check cartridge identity and model before SameBoy's transactional loader.
            std::size_t at=saves::le32(bytes,bytes.size()-8);if(at>bytes.size()-8)return false;bool core=false,info=false;std::span<const u8> exact;
            for(unsigned blocks=0;at+8<=bytes.size()-8&&blocks<128;++blocks){const auto tag=bytes.subspan(at,4);const auto n=saves::le32(bytes,at+4);at+=8;if(n>bytes.size()-8-at)return false;
                if(!std::memcmp(tag.data(),"INFO",4)){if(n!=18||!std::equal(bytes.begin()+at,bytes.begin()+at+16,g.mem.rom.begin()+0x134)||bytes[at+16]!=g.mem.rom[0x14E]||bytes[at+17]!=g.mem.rom[0x14F])return false;info=true;}
                if(!std::memcmp(tag.data(),"GSTU",4)){if(!exact.empty())return false;exact=bytes.subspan(at,n);}
                if(!std::memcmp(tag.data(),"CORE",4)){if(n<0xD0)return false;const auto c=bytes[at+4];if((c=='C')!=g.mem.isCGBHardware())return false;core=true;}at+=n;
                if(!std::memcmp(tag.data(),"END ",4))break;
            }if(!core||!info)return false;if(!exact.empty())return restore(exact);const auto backup=bess();const auto oldAux=g.auxiliaryState?g.auxiliaryState():std::vector<u8>{};if(g.auxiliaryRestore&&!g.auxiliaryRestore({}))return false;if(!a->load_state(ctx,bytes.data(),bytes.size())){a->load_state(ctx,backup.data(),backup.size());if(g.auxiliaryRestore)g.auxiliaryRestore(oldAux);return false;}g.frameDebt=0;refresh();
        }else return false;g.apu.clearQueue();return true;
    }
    void printer(bool on){a->printer(ctx,on?1:0);}
    bool inspect(GBSInspect& view){a->inspect(ctx,&view);return true;}
    void testRAM(){a->test_ram(ctx);std::size_t n=0;a->memory(ctx,2,&n);g.mem.eram.resize(n);refresh();}
};
inline bool enable(GameBoy& g){if(g.backendOwner)return true;auto* apiTable=api();if(!apiTable)return false;
    auto e=std::make_shared<Engine>(g,apiTable);if(!e->valid())return false;g.backendOwner=e;auto* p=e.get();
    g.mem.extendedControllers=true;g.backendReset=[p](){return p->reset();};g.backendRefresh=[p](){p->refresh();};g.cpu.externalStep=[p](){return p->step();};
    g.mem.externalRead=[p](u16 addr){return p->read(addr);};g.mem.externalWrite=[p](u16 addr,u8 value){p->write(addr,value);};
    g.mem.externalSerialPulse=[p](bool value){p->pulse(value);};g.mem.externalSerialOut=[p](){return p->outgoing();};g.mem.externalBatterySave=[p](){return p->saveBattery();};g.mem.externalROMBank=[p](bool upper){return p->bank(upper);};
    g.mem.externalPoke=[p](unsigned access,std::size_t offset,u8 value){p->poke(access,offset,value);};
    g.backendState=[p](){return p->state();};g.backendBess=[p](){return p->bess();};g.backendRestore=[p](std::span<const u8> bytes){return p->restore(bytes);};g.backendCommitRegisters=[p](){p->commitRegisters();};g.backendPrinter=[p](bool on){p->printer(on);};g.backendInspect=[p](GBSInspect& view){return p->inspect(view);};return true;
}
} // namespace accurate
