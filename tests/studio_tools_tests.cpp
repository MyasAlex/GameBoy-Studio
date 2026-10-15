#include "../src/studio_tools.hpp"
#include <cstdlib>
#define CHECK(expr) do { if(!(expr)){std::cerr<<"CHECK FAILED: "<<#expr<<" at "<<__LINE__<<'\n';std::abort();} } while(false)

static void testOpcodes() {
    // Independent length card: rows 0x0_, 0x1_, ... from the SM83 opcode table.
    constexpr const char* lengths[]={
        "1311112131111121","2311112121111121","2311112121111121","2311112121111121",
        "1111111111111111","1111111111111111","1111111111111111","1111111111111111",
        "1111111111111111","1111111111111111","1111111111111111","1111111111111111",
        "1133312111323321","1131312111313121","2111112121311121","2111112121311121"};
    const std::set<unsigned> illegal={0xD3,0xDB,0xDD,0xE3,0xE4,0xEB,0xEC,0xED,0xF4,0xFC,0xFD};
    Memory memory;memory.wram[1]=0x34;memory.wram[2]=0x12;
    unsigned valid=0;
    for(unsigned opcode=0;opcode<256;++opcode){
        memory.wram[0]=(u8)opcode;auto instruction=studio::decode(memory,0xC000);
        if(instruction.size!=lengths[opcode>>4][opcode&15]-'0')std::cerr<<"Length mismatch "<<studio::hex(opcode)<<" decoded="<<(unsigned)instruction.size<<" reference="<<lengths[opcode>>4][opcode&15]<<'\n';
        CHECK(instruction.size==lengths[opcode>>4][opcode&15]-'0');
        CHECK(instruction.valid==!illegal.contains(opcode));
        CHECK(!instruction.text.empty());if(instruction.valid)++valid;
    }
    CHECK(valid==245);
    const std::map<unsigned,std::string> golden={
        {0x01,"LD BC, $1234"},{0x08,"LD [$1234], SP"},{0x10,"STOP $34"},
        {0x18,"JR $C036"},{0x20,"JR NZ, $C036"},{0x22,"LD [HL+], A"},{0x3A,"LD A, [HL-]"},
        {0x76,"HALT"},{0x8E,"ADC A, [HL]"},{0xC4,"CALL NZ, $1234"},{0xD9,"RETI"},
        {0xE0,"LDH [$FF34], A"},{0xE2,"LDH [$FF00+C], A"},{0xEA,"LD [$1234], A"},
        {0xF0,"LDH A, [$FF34]"},{0xF8,"LD HL, SP+52"},{0xFF,"RST $38"}};
    for(const auto& [op,text]:golden){memory.wram[0]=(u8)op;CHECK(studio::decode(memory,0xC000).text==text);}
    memory.wram[0]=0xCB;
    for(unsigned op=0;op<256;++op){memory.wram[1]=(u8)op;const auto i=studio::decode(memory,0xC000);CHECK(i.valid&&i.size==2&&i.bytes[1]==op);}
    const std::map<unsigned,std::string> cbGolden={{0,"RLC B"},{0x0E,"RRC [HL]"},{0x1F,"RR A"},{0x26,"SLA [HL]"},
        {0x2D,"SRA L"},{0x36,"SWAP [HL]"},{0x3F,"SRL A"},{0x46,"BIT 0, [HL]"},{0x7F,"BIT 7, A"},
        {0x80,"RES 0, B"},{0xBE,"RES 7, [HL]"},{0xC0,"SET 0, B"},{0xFF,"SET 7, A"}};
    for(const auto& [op,text]:cbGolden){memory.wram[1]=(u8)op;CHECK(studio::decode(memory,0xC000).text==text);}
    memory.wram[0]=0xE8;memory.wram[1]=0xFE;CHECK(studio::decode(memory,0xC000).text=="ADD SP, -2");
    memory.wram[0]=0x18;memory.wram[1]=0xFC;CHECK(studio::decode(memory,0xC000).text=="JR $BFFE");
    // Fetching at FFFF wraps operands into cartridge ROM; memory dumps must not wrap.
    memory.rom.assign(32768,0);memory.ie=0x01;memory.rom[0]=0xCD;memory.rom[1]=0xAB;
    CHECK(studio::decode(memory,0xFFFF).text=="LD BC, $ABCD");
}
static void testDebugger() {
    GameBoy gb;gb.mem.io[0x40]=0;gb.cpu.PC=0xC000;gb.cpu.SP=0xDFFF;
    // LD A,42; INC A; LD [C100],A; JR -2.
    const u8 program[]={0x3E,0x2A,0x3C,0xEA,0x00,0xC1,0x18,0xFE};
    std::copy(std::begin(program),std::end(program),gb.mem.wram.begin());
    studio::Debugger debug(gb);debug.addBreakpoint(0xC002);
    auto run=debug.run(100);CHECK(run.reason==studio::StopReason::Breakpoint&&run.steps==1&&gb.cpu.A==42);
    CHECK(debug.run(100).steps==0);
    debug.setRunTo(0xC003);run=debug.run(100,0,true);
    CHECK(run.reason==studio::StopReason::RunTo&&run.steps==1&&gb.cpu.A==43&&gb.cpu.PC==0xC003);
    debug.addWatchpoint(0xC100);run=debug.run(100);
    CHECK(run.reason==studio::StopReason::Watchpoint&&run.steps==1&&run.watchAddress==0xC100&&gb.mem.wram[0x100]==43);
    debug.removeWatchpoint(0xC100);debug.addBreakpoint(0xC006,[](const GameBoy& g){return g.cpu.A==99;});
    run=debug.run(3);CHECK(run.reason==studio::StopReason::Budget&&run.steps==3);
    gb.cpu.A=99;CHECK(debug.run(100).reason==studio::StopReason::Breakpoint);
    debug.clearBreakpoints();debug.setTraceCapacity(2);debug.run(10);CHECK(debug.trace().size()==2);
    CHECK(debug.trace().back().before.PC==0xC006);CHECK(debug.trace().back().cpuCycles==12);
    CHECK(studio::formatTrace(debug.trace().back()).find("CPU_T=12")!=std::string::npos);
    debug.clearTrace();debug.setTraceCapacity(0);debug.run(2);CHECK(debug.trace().empty());
    debug.setRunTo(0xC006);CHECK(debug.run(1).reason==studio::StopReason::RunTo);
    debug.clearRunTo();gb.cpu.locked=true;CHECK(debug.run(1).reason==studio::StopReason::CpuLocked);
    gb.cpu.locked=false;gb.cpu.halted=true;gb.mem.ie=0;gb.mem.io[0x0F]=0;
    const auto idle=debug.step();CHECK(idle.kind==studio::TraceKind::Idle&&idle.before.PC==idle.after.PC);
    gb.cpu.halted=false;gb.cpu.IME=true;gb.mem.ie=1;gb.mem.io[0x0F]=1;
    const auto irq=debug.step();CHECK(irq.kind==studio::TraceKind::Interrupt&&irq.after.PC==0x40&&irq.cpuCycles==20);
    gb.cpu.IME=false;gb.mem.ie=0;gb.mem.io[0x0F]=0;gb.cpu.PC=0xC000;gb.cpu.haltBug=true;
    const auto bug=debug.step();CHECK(bug.instruction.bytes[1]==0x3E&&gb.cpu.A==0x3E&&gb.cpu.PC==0xC001);
    debug.clearRunTo();run=debug.run(100,4);CHECK(run.cycles>=4&&run.steps<100);
}
static void fixChecksums(std::vector<u8>& rom) {
    u8 header=0;for(unsigned n=0x134;n<=0x14C;++n)header=(u8)(header-rom[n]-1);rom[0x14D]=header;
    u16 sum=0;for(std::size_t n=0;n<rom.size();++n)if(n!=0x14E&&n!=0x14F)sum=(u16)(sum+rom[n]);
    rom[0x14E]=(u8)(sum>>8);rom[0x14F]=(u8)sum;
}
static void testReports() {
    const std::string check="123456789";CHECK(studio::crc32(std::span((const u8*)check.data(),check.size()))==0xCBF43926);
    std::vector<u8> rom(32768);const std::string title="STUDIO\"TEST";std::copy(title.begin(),title.end(),rom.begin()+0x134);
    fixChecksums(rom);auto r=studio::headerReport(rom);
    CHECK(r.headerPresent&&r.headerChecksumValid&&r.globalChecksumValid&&r.controllerSupported);
    CHECK(r.declaredRomBytes==rom.size());CHECK(!r.logoValid);
    auto json=studio::romReport(rom,"unicode-\xD0\x98.gb");CHECK(json.find("STUDIO\\\"TEST")!=std::string::npos);
    rom[0x134]^=1;r=studio::headerReport(rom);CHECK(!r.headerChecksumValid&&!r.globalChecksumValid);
    rom[0x147]=0x22;fixChecksums(rom);r=studio::headerReport(rom);CHECK(r.controllerSupported&&r.controllerRamBytes==256);
    CHECK(studio::romReport(rom).find("MBC7+SENSOR")!=std::string::npos);
    rom[0x147]=5;rom[0x149]=0;r=studio::headerReport(rom);CHECK(r.declaredRamBytes==0&&r.controllerRamBytes==512);
    rom[0x148]=0x08;CHECK(studio::headerReport(rom).declaredRomBytes==8388608);
    rom[0x148]=0x52;CHECK(studio::headerReport(rom).declaredRomBytes==72*16384);
    CHECK(!studio::headerReport(std::span<const u8>{}).headerPresent);
    CHECK(!studio::headerReport(std::span<const u8>(rom).first(0x14F)).headerPresent);
    GameBoy gb;gb.mem.rom=rom;CHECK(studio::romReport(gb).find("\"runtime\"")!=std::string::npos);
    const auto cycles=gb.totalCycles;const auto div=gb.mem.divCounter;
    studio::disassemble(gb.mem,0x100,20);studio::memoryDump(gb.mem,0xFF00,256);studio::romReport(gb);
    CHECK(gb.totalCycles==cycles&&gb.mem.divCounter==div);
}
static void testBoundsAndArguments() {
    Memory memory;memory.ie=0x5A;memory.hram[0x7E]=0x41;
    CHECK(studio::memoryBytes(memory,0xFFFE,100000).size()==2);
    CHECK(studio::memoryBytes(memory,0xFFFF,100000)==std::vector<u8>{0x5A});
    CHECK(studio::memoryBytes(memory,0,100000).size()==65536);
    CHECK(studio::memoryDump(memory,0xFFFF,2).find("$0000")==std::string::npos);
    u64 value=77;
    CHECK(studio::parseUnsigned("0xFFFF",value,65535,16)&&value==65535);
    CHECK(studio::parseUnsigned("$ff",value,65535,16)&&value==255);
    for(auto invalid:{"", "-1", "+1", "1x", " 1", "1 ", "18446744073709551616"})CHECK(!studio::parseUnsigned(invalid,value,100000));
    CHECK(!studio::parseUnsigned("65536",value,65535));CHECK(!studio::parseUnsigned("0x",value,65535,16));
    CHECK(studio::jsonString("\x01\n\\\"")=="\"\\u0001\\n\\\\\\\"\"");
}
int main(){testOpcodes();testDebugger();testReports();testBoundsAndArguments();std::cout<<"Studio tools tests passed\n";}
