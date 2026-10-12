#pragma once
#include "gameboy.hpp"
#include <charconv>
#include <deque>
#include <map>
#include <optional>
#include <set>

// Reusable Studio APIs. Inspection never advances the emulated clock.
// Opcode/header definitions follow https://gbdev.io/pandocs/ .
namespace studio {
inline std::string hex(u64 value, unsigned width=2) {
    std::ostringstream out;
    out << '$' << std::uppercase << std::hex << std::setfill('0') << std::setw(width) << value;
    return out.str();
}
struct Instruction {
    u16 address=0;
    std::array<u8,3> bytes{};
    u8 size=1;
    bool valid=true;
    std::string text;
};

template<class Reader> inline Instruction decodeWithReader(Reader read, u16 address) {
    Instruction result; result.address=address;
    // Read exactly the instruction bytes, including the 16-bit address wrap.
    result.bytes[0]=read(address);
    const unsigned op=result.bytes[0], y=(op>>3)&7, z=op&7, p=y>>1, q=y&1;
    static constexpr const char* r8[]={"B","C","D","E","H","L","[HL]","A"};
    static constexpr const char* r16[]={"BC","DE","HL","SP"};
    static constexpr const char* stack[]={"BC","DE","HL","AF"};
    static constexpr const char* mem[]={"[BC]","[DE]","[HL+]","[HL-]"};
    static constexpr const char* cond[]={"NZ","Z","NC","C"};
    static constexpr const char* alu[]={"ADD A, ","ADC A, ","SUB A, ","SBC A, ","AND A, ","XOR A, ","OR A, ","CP A, "};
    auto imm8=[&]() { result.size=2;result.bytes[1]=read((u16)(address+1));return hex(result.bytes[1]); };
    auto imm16=[&]() { result.size=3;result.bytes[1]=read((u16)(address+1));result.bytes[2]=read((u16)(address+2));return hex((u16)(result.bytes[1]|(result.bytes[2]<<8)),4); };
    auto relative=[&]() {imm8();return hex((u16)(address+2+(s8)result.bytes[1]),4);};
    auto signed8=[&]() {imm8();const int v=(s8)result.bytes[1];return (v>=0?"+":"")+std::to_string(v);};
    const auto join=[](std::string_view first, std::string_view second){return std::string(first)+std::string(second);};
    if(op<0x40) {
        switch(z) {
            case 0:
                if(y==0)result.text="NOP";
                else if(y==1)result.text="LD ["+imm16()+"], SP";
                else if(y==2){imm8();result.text="STOP "+hex(result.bytes[1]);}
                else if(y==3)result.text="JR "+relative();
                else result.text=join("JR ",cond[y-4])+", "+relative();
                break;
            case 1:result.text=q?join("ADD HL, ",r16[p]):join("LD ",r16[p])+", "+imm16();break;
            case 2:result.text=q?join("LD A, ",mem[p]):join("LD ",mem[p])+", A";break;
            case 3:result.text=join(q?"DEC ":"INC ",r16[p]);break;
            case 4:result.text=join("INC ",r8[y]);break;
            case 5:result.text=join("DEC ",r8[y]);break;
            case 6:result.text=join("LD ",r8[y])+", "+imm8();break;
            case 7:{static constexpr const char* misc[]={"RLCA","RRCA","RLA","RRA","DAA","CPL","SCF","CCF"};result.text=misc[y];break;}
        }
    } else if(op<0x80)result.text=op==0x76?"HALT":join("LD ",r8[y])+", "+r8[z];
    else if(op<0xC0)result.text=join(alu[y],r8[z]);
    else {
        switch(z) {
            case 0:
                if(y<4)result.text=join("RET ",cond[y]);
                else if(y==4)result.text="LDH ["+hex(0xFF00+([&](){imm8();return result.bytes[1];})(),4)+"], A";
                else if(y==5)result.text="ADD SP, "+signed8();
                else if(y==6)result.text="LDH A, ["+hex(0xFF00+([&](){imm8();return result.bytes[1];})(),4)+"]";
                else result.text="LD HL, SP"+signed8();
                break;
            case 1:
                if(!q)result.text=join("POP ",stack[p]);
                else {static constexpr const char* misc[]={"RET","RETI","JP HL","LD SP, HL"};result.text=misc[p];}
                break;
            case 2:
                if(y<4)result.text=join("JP ",cond[y])+", "+imm16();
                else if(y==4)result.text="LDH [$FF00+C], A";
                else if(y==5)result.text="LD ["+imm16()+"], A";
                else if(y==6)result.text="LDH A, [$FF00+C]";
                else result.text="LD A, ["+imm16()+"]";
                break;
            case 3:
                if(y==0)result.text="JP "+imm16();
                else if(y==1) {
                    imm8();const unsigned cb=result.bytes[1], cy=(cb>>3)&7, cz=cb&7;
                    static constexpr const char* rot[]={"RLC ","RRC ","RL ","RR ","SLA ","SRA ","SWAP ","SRL "};
                    if(cb<0x40)result.text=join(rot[cy],r8[cz]);
                    else result.text=std::string(cb<0x80?"BIT ":cb<0xC0?"RES ":"SET ")+std::to_string(cy)+", "+r8[cz];
                } else if(y==6)result.text="DI";
                else if(y==7)result.text="EI";
                else result.valid=false;
                break;
            case 4:if(y<4)result.text=join("CALL ",cond[y])+", "+imm16();else result.valid=false;break;
            case 5:if(!q)result.text=join("PUSH ",stack[p]);else if(p==0)result.text="CALL "+imm16();else result.valid=false;break;
            case 6:result.text=std::string(alu[y])+imm8();break;
            case 7:result.text="RST "+hex(y*8,2);break;
        }
    }
    if(!result.valid)result.text="ILLEGAL "+hex(op);
    return result;
}
inline Instruction decode(const Memory& memory,u16 address) {
    return decodeWithReader([&](u16 addr){return memory.read(addr);},address);
}
inline std::string formatInstruction(const Instruction& i) {
    std::ostringstream out;out<<hex(i.address,4)<<"  ";
    for(unsigned n=0;n<3;++n)out<<(n<i.size?hex(i.bytes[n]).substr(1):"  ")<<' ';
    out<<" "<<i.text;return out.str();
}
inline std::string disassemble(const Memory& memory,u16 address,std::size_t count=16) {
    count=std::min<std::size_t>(count,4096);std::ostringstream out;
    for(std::size_t n=0;n<count;++n){auto i=decode(memory,address);out<<formatInstruction(i)<<'\n';address=(u16)(address+i.size);}
    return out.str();
}

struct Registers {
    u8 A=0,F=0,B=0,C=0,D=0,E=0,H=0,L=0,IE=0,IF=0;
    u16 SP=0,PC=0;
    bool IME=false,halted=false,stopped=false,locked=false,haltBug=false;
    u64 cycles=0;
    u16 romBank=0;
};
inline Registers capture(const GameBoy& gb) {
    const auto& c=gb.cpu;
    return {c.A,c.F,c.B,c.C,c.D,c.E,c.H,c.L,gb.mem.ie,gb.mem.io[0x0F],c.SP,c.PC,
            c.IME,c.halted,c.stopped,c.locked,c.haltBug,gb.totalCycles,(u16)gb.mem.bankN()};
}
inline std::string formatRegisters(const Registers& r) {
    std::ostringstream out;
    out<<"AF="<<hex((u16)(r.A<<8|r.F),4)<<" BC="<<hex((u16)(r.B<<8|r.C),4)
       <<" DE="<<hex((u16)(r.D<<8|r.E),4)<<" HL="<<hex((u16)(r.H<<8|r.L),4)
       <<" SP="<<hex(r.SP,4)<<" PC="<<hex(r.PC,4)
       <<" FLAGS="<<(r.F&0x80?'Z':'-')<<(r.F&0x40?'N':'-')<<(r.F&0x20?'H':'-')<<(r.F&0x10?'C':'-')
       <<" IME="<<r.IME<<" IE="<<hex(r.IE)<<" IF="<<hex(r.IF)
       <<" HALT="<<r.halted<<" STOP="<<r.stopped<<" LOCK="<<r.locked
       <<" BANK="<<r.romBank<<" BASE_CYCLES="<<r.cycles;return out.str();
}
enum class TraceKind { Instruction,Interrupt,Idle,Locked };
struct TraceEntry {
    Registers before,after;
    Instruction instruction;
    TraceKind kind=TraceKind::Instruction;
    int cpuCycles=0;
};
inline std::string formatTrace(const TraceEntry& t) {
    const char* event=t.kind==TraceKind::Interrupt?"INTERRUPT":t.kind==TraceKind::Idle?"IDLE":t.kind==TraceKind::Locked?"CPU LOCKED":"";
    std::ostringstream out;
    out<<t.before.cycles<<" "<<formatRegisters(t.before)<<" | ";
    if(t.kind==TraceKind::Instruction)out<<formatInstruction(t.instruction);else out<<event;
    out<<" | CPU_T="<<t.cpuCycles<<" BASE_T="<<(t.after.cycles-t.before.cycles)
       <<" NEXT_PC="<<hex(t.after.PC,4);
    if(t.before.haltBug)out<<" HALT_BUG";
    return out.str();
}
enum class StopReason { Budget,Breakpoint,RunTo,Watchpoint,CpuLocked };
inline const char* stopReasonName(StopReason reason) {
    switch(reason){case StopReason::Breakpoint:return "breakpoint";case StopReason::RunTo:return "run-to";
        case StopReason::Watchpoint:return "memory value changed";case StopReason::CpuLocked:return "CPU locked";default:return "budget";}
}
struct RunResult { StopReason reason=StopReason::Budget;u64 steps=0,cycles=0;std::optional<u16> watchAddress; };
class Debugger {
    GameBoy& gb;
    std::map<u16,std::function<bool(const GameBoy&)>> breakpoints;
    std::set<u16> watchpoints;
    std::optional<u16> runTo;
    std::deque<TraceEntry> history;
    std::size_t capacity=256;
public:
    explicit Debugger(GameBoy& machine):gb(machine) {}
    void addBreakpoint(u16 address,std::function<bool(const GameBoy&)> condition={}){breakpoints[address]=std::move(condition);}
    void removeBreakpoint(u16 address){breakpoints.erase(address);}
    bool hasBreakpoint(u16 address)const{return breakpoints.contains(address);}
    void clearBreakpoints(){breakpoints.clear();}
    void setRunTo(u16 address){runTo=address;}
    void clearRunTo(){runTo.reset();}
    void addWatchpoint(u16 address){watchpoints.insert(address);}
    void removeWatchpoint(u16 address){watchpoints.erase(address);}
    void clearWatchpoints(){watchpoints.clear();}
    bool hasWatchpoint(u16 address)const{return watchpoints.contains(address);}
    const std::deque<TraceEntry>& trace()const{return history;}
    void clearTrace(){history.clear();}
    void setTraceCapacity(std::size_t value){capacity=std::min<std::size_t>(value,65536);while(history.size()>capacity)history.pop_front();}
    TraceEntry step() {
        TraceEntry t;t.before=capture(gb);
        const auto& cpu=gb.cpu;const bool pending=(gb.mem.ie&gb.mem.io[0x0F]&0x1F)!=0;
        if(cpu.locked)t.kind=TraceKind::Locked;
        else if(cpu.stopped&&!(gb.mem.io[0x0F]&0x10))t.kind=TraceKind::Idle;
        else if(cpu.IME&&pending)t.kind=TraceKind::Interrupt;
        else if(cpu.halted&&!pending)t.kind=TraceKind::Idle;
        t.instruction=decodeWithReader([&](u16 addr){
            // HALT bug suppresses the first PC increment, duplicating the opcode as operand.
            if(cpu.haltBug&&addr!=cpu.PC)addr=(u16)(addr-1);
            return gb.mem.cpuRead(addr);
        },cpu.PC);
        t.cpuCycles=gb.cpu.step();t.after=capture(gb);
        if(capacity){if(history.size()==capacity)history.pop_front();history.push_back(t);}
        return t;
    }
    RunResult run(u64 maxInstructions,u64 maxBaseCycles=0,bool skipCurrentBreakpoint=false) {
        RunResult result;const u64 start=gb.totalCycles;
        while(result.steps<maxInstructions&&(!maxBaseCycles||gb.totalCycles-start<maxBaseCycles)) {
            if(gb.cpu.locked){result.reason=StopReason::CpuLocked;break;}
            if(runTo&&gb.cpu.PC==*runTo){runTo.reset();result.reason=StopReason::RunTo;break;}
            auto point=breakpoints.find(gb.cpu.PC);
            if(!(skipCurrentBreakpoint&&result.steps==0)&&point!=breakpoints.end()&&(!point->second||point->second(gb))) {
                result.reason=StopReason::Breakpoint;break;
            }
            std::vector<std::pair<u16,u8>> values;values.reserve(watchpoints.size());
            for(u16 address:watchpoints)values.emplace_back(address,gb.mem.read(address));
            step();++result.steps;
            for(const auto& [address,value]:values)if(gb.mem.read(address)!=value){
                result.reason=StopReason::Watchpoint;result.watchAddress=address;break;
            }
            if(result.reason==StopReason::Watchpoint)break;
        }
        // A target reached on the final allowed step still reports the actual stop.
        if(result.reason==StopReason::Budget&&runTo&&gb.cpu.PC==*runTo){runTo.reset();result.reason=StopReason::RunTo;}
        if(result.reason==StopReason::Budget&&gb.cpu.locked)result.reason=StopReason::CpuLocked;
        result.cycles=gb.totalCycles-start;return result;
    }
};

inline std::vector<u8> memoryBytes(const Memory& memory,u16 start,std::size_t count) {
    count=std::min<std::size_t>(count,65536u-(unsigned)start);
    std::vector<u8> bytes;bytes.reserve(count);
    for(std::size_t n=0;n<count;++n)bytes.push_back(memory.read((u16)(start+n)));
    return bytes;
}
inline std::string memoryDump(const Memory& memory,u16 start,std::size_t count=256) {
    const auto bytes=memoryBytes(memory,start,count);std::ostringstream out;
    for(std::size_t offset=0;offset<bytes.size();offset+=16){
        out<<hex(start+offset,4)<<"  ";
        for(std::size_t n=0;n<16;++n)out<<(offset+n<bytes.size()?hex(bytes[offset+n]).substr(1):"  ")<<' ';
        out<<" ";for(std::size_t n=0;n<16&&offset+n<bytes.size();++n){const u8 c=bytes[offset+n];out<<(char)(c>=32&&c<127?c:'.');}
        out<<'\n';
    }
    return out.str();
}
inline bool parseUnsigned(std::string_view text,u64& value,u64 maximum,int base=10) {
    if(text.empty())return false;
    if(base==16){if(text.front()=='$')text.remove_prefix(1);else if(text.size()>2&&text.substr(0,2)=="0x")text.remove_prefix(2);}
    if(text.empty())return false;
    u64 parsed=0;const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),parsed,base);
    if(error!=std::errc{}||end!=text.data()+text.size()||parsed>maximum)return false;
    value=parsed;return true;
}
inline std::string jsonString(std::string_view value) {
    std::ostringstream out;out<<'"';
    for(unsigned char ch:value) {
        if(ch=='"')out<<"\\\"";else if(ch=='\\')out<<"\\\\";
        else if(ch=='\n')out<<"\\n";else if(ch=='\r')out<<"\\r";else if(ch=='\t')out<<"\\t";
        else if(ch<32)out<<"\\u00"<<hex(ch).substr(1);else out<<(char)ch;
    }
    out<<'"';return out.str();
}
inline std::string pathUtf8(const std::filesystem::path& path){const auto s=path.u8string();return std::string(s.begin(),s.end());}
inline u32 crc32(std::span<const u8> bytes) {
    u32 value=0xFFFFFFFFu;
    for(u8 b:bytes){value^=b;for(unsigned n=0;n<8;++n)value=(value>>1)^((value&1)?0xEDB88320u:0u);}
    return value^0xFFFFFFFFu;
}
inline const char* cartridgeName(u8 type) {
    switch(type){
        case 0:return "ROM ONLY";case 1:return "MBC1";case 2:return "MBC1+RAM";case 3:return "MBC1+RAM+BATTERY";
        case 5:return "MBC2";case 6:return "MBC2+BATTERY";case 8:return "ROM+RAM";case 9:return "ROM+RAM+BATTERY";
        case 0x0B:return "MMM01";case 0x0C:return "MMM01+RAM";case 0x0D:return "MMM01+RAM+BATTERY";
        case 0x0F:return "MBC3+TIMER+BATTERY";case 0x10:return "MBC3+TIMER+RAM+BATTERY";
        case 0x11:return "MBC3";case 0x12:return "MBC3+RAM";case 0x13:return "MBC3+RAM+BATTERY";
        case 0x19:return "MBC5";case 0x1A:return "MBC5+RAM";case 0x1B:return "MBC5+RAM+BATTERY";
        case 0x1C:return "MBC5+RUMBLE";case 0x1D:return "MBC5+RUMBLE+RAM";case 0x1E:return "MBC5+RUMBLE+RAM+BATTERY";
        case 0x20:return "MBC6";case 0x22:return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
        case 0xFC:return "POCKET CAMERA";case 0xFD:return "BANDAI TAMA5";case 0xFE:return "HuC3";case 0xFF:return "HuC1+RAM+BATTERY";
        default:return "UNKNOWN";
    }
}
inline bool supportedCartridge(u8 type) {
    switch(type){case 0:case 1:case 2:case 3:case 5:case 6:case 8:case 9:
        case 0x0F:case 0x10:case 0x11:case 0x12:case 0x13:
        case 0x19:case 0x1A:case 0x1B:case 0x1C:case 0x1D:case 0x1E:return true;default:return false;}
}
struct RomReport {
    bool headerPresent=false,headerChecksumValid=false,globalChecksumValid=false,logoValid=false,controllerSupported=false;
    std::size_t bytes=0,declaredRomBytes=0,declaredRamBytes=0,controllerRamBytes=0;
    u8 cartridgeType=0,cgbFlag=0,romSizeCode=0,ramSizeCode=0,sgbFlag=0,destinationCode=0,version=0;
    u8 headerChecksumStored=0,headerChecksumCalculated=0;
    u16 globalChecksumStored=0,globalChecksumCalculated=0;
    u32 crc=0;
    std::string title;
    std::vector<std::string> warnings;
};
inline RomReport headerReport(std::span<const u8> rom) {
    RomReport report;report.bytes=rom.size();report.crc=crc32(rom);
    if(rom.size()<0x150){report.warnings.push_back("ROM is shorter than the cartridge header.");return report;}
    report.headerPresent=true;report.cartridgeType=rom[0x147];report.cgbFlag=rom[0x143];
    report.romSizeCode=rom[0x148];report.ramSizeCode=rom[0x149];report.sgbFlag=rom[0x146];report.destinationCode=rom[0x14A];report.version=rom[0x14C];
    report.controllerSupported=supportedCartridge(report.cartridgeType);
    const unsigned titleEnd=(report.cgbFlag&0x80)?0x13F:0x144;
    for(unsigned n=0x134;n<titleEnd&&rom[n];++n){const u8 ch=rom[n];report.title+=(char)(ch>=32&&ch<127?ch:'?');}
    for(unsigned n=0x134;n<=0x14C;++n)report.headerChecksumCalculated=(u8)(report.headerChecksumCalculated-rom[n]-1);
    report.headerChecksumStored=rom[0x14D];report.headerChecksumValid=report.headerChecksumStored==report.headerChecksumCalculated;
    for(std::size_t n=0;n<rom.size();++n)if(n!=0x14E&&n!=0x14F)report.globalChecksumCalculated=(u16)(report.globalChecksumCalculated+rom[n]);
    report.globalChecksumStored=(u16)(rom[0x14E]<<8|rom[0x14F]);report.globalChecksumValid=report.globalChecksumStored==report.globalChecksumCalculated;
    static constexpr u8 logo[]={0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,0x03,0x73,0x00,0x83,0x00,0x0C,0x00,0x0D,
        0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E,0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99,
        0xBB,0xBB,0x67,0x63,0x6E,0x0E,0xEC,0xCC,0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E};
    report.logoValid=std::equal(std::begin(logo),std::end(logo),rom.begin()+0x104);
    if(report.romSizeCode<=8)report.declaredRomBytes=(std::size_t)32768<<report.romSizeCode;
    else if(report.romSizeCode>=0x52&&report.romSizeCode<=0x54){static constexpr unsigned banks[]={72,80,96};report.declaredRomBytes=(std::size_t)banks[report.romSizeCode-0x52]*16384;}
    static constexpr std::size_t ramBytes[]={0,2048,8192,32768,131072,65536};
    if(report.ramSizeCode<std::size(ramBytes))report.declaredRamBytes=ramBytes[report.ramSizeCode];
    report.controllerRamBytes=report.declaredRamBytes;
    if(report.cartridgeType==5||report.cartridgeType==6)report.controllerRamBytes=512; // MBC2 nibble storage ignores the header RAM code.
    if(!report.controllerSupported)report.warnings.push_back("Cartridge controller is unsupported by this core.");
    if(!report.headerChecksumValid)report.warnings.push_back("Header checksum mismatch.");
    if(!report.globalChecksumValid)report.warnings.push_back("Global checksum mismatch; the console does not enforce this checksum.");
    if(!report.logoValid)report.warnings.push_back("Nintendo logo differs from the boot-header pattern.");
    if(!report.declaredRomBytes)report.warnings.push_back("Unknown ROM size code.");
    else if(report.declaredRomBytes!=rom.size())report.warnings.push_back("Actual ROM length differs from the declared ROM size.");
    if(report.ramSizeCode>=std::size(ramBytes))report.warnings.push_back("Unknown RAM size code.");
    if(rom.size()<32768||rom.size()>8*1024*1024||rom.size()%16384)report.warnings.push_back("Core requires complete 16 KiB banks, from 32 KiB to 8 MiB.");
    if(report.sgbFlag==3)report.warnings.push_back("SGB enhancement commands and borders are not implemented.");
    return report;
}
inline std::string romReport(std::span<const u8> rom,std::string_view filename={}) {
    const auto r=headerReport(rom);std::ostringstream out;
    out<<"{\n  \"schema\": \"gameboy-studio-diagnostic-v1\",\n  \"file\": "<<jsonString(filename)
       <<",\n  \"bytes\": "<<r.bytes<<",\n  \"crc32\": "<<jsonString(hex(r.crc,8).substr(1))
       <<",\n  \"header_present\": "<<(r.headerPresent?"true":"false");
    if(r.headerPresent) {
        out<<",\n  \"title\": "<<jsonString(r.title)<<",\n  \"cartridge\": {\"type\": "<<(unsigned)r.cartridgeType
           <<", \"name\": "<<jsonString(cartridgeName(r.cartridgeType))<<", \"controller_supported\": "<<(r.controllerSupported?"true":"false")<<"}"
           <<",\n  \"cgb_flag\": "<<(unsigned)r.cgbFlag<<",\n  \"sgb_flag\": "<<(unsigned)r.sgbFlag
           <<",\n  \"destination_code\": "<<(unsigned)r.destinationCode<<",\n  \"version\": "<<(unsigned)r.version
           <<",\n  \"declared_rom_bytes\": "<<r.declaredRomBytes<<",\n  \"declared_ram_bytes\": "<<r.declaredRamBytes
           <<",\n  \"controller_ram_storage_bytes\": "<<r.controllerRamBytes
           <<",\n  \"logo_valid\": "<<(r.logoValid?"true":"false")
           <<",\n  \"header_checksum\": {\"stored\": "<<(unsigned)r.headerChecksumStored<<", \"calculated\": "<<(unsigned)r.headerChecksumCalculated<<", \"valid\": "<<(r.headerChecksumValid?"true":"false")<<"}"
           <<",\n  \"global_checksum\": {\"stored\": "<<r.globalChecksumStored<<", \"calculated\": "<<r.globalChecksumCalculated<<", \"valid\": "<<(r.globalChecksumValid?"true":"false")<<"}";
    }
    out<<",\n  \"warnings\": [";for(std::size_t n=0;n<r.warnings.size();++n){if(n)out<<", ";out<<jsonString(r.warnings[n]);}
    out<<"],\n  \"compatibility_note\": \"Header checks describe the cartridge, not verified game compatibility.\"\n}\n";
    return out.str();
}
inline std::string romReport(const GameBoy& gb) {
    auto report=romReport(gb.mem.rom,pathUtf8(gb.mem.romPath.filename()));
    const auto end=report.rfind('}');std::ostringstream state;
    state<<",\n  \"runtime\": {\"model\": \""<<(gb.mem.cgbMode?"CGB":"DMG")<<"\", \"speed\": "<<(gb.mem.doubleSpeed?2:1)
         <<", \"base_cycles\": "<<gb.totalCycles<<", \"registers\": "<<jsonString(formatRegisters(capture(gb)))<<"}\n";
    report.insert(end,state.str());return report;
}
} // namespace studio
