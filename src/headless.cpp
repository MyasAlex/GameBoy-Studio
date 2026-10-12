#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#ifdef _MSC_VER
#pragma comment(lib,"shell32.lib")
#endif
#endif
#include "studio_tools.hpp"

namespace {
struct Options {
    std::filesystem::path rom,output,report,trace,dump,boot;
    u64 frames=120,traceLimit=100000,instructions=0,dumpCount=0,disassemblyCount=0;
    u16 dumpStart=0,disassemblyStart=0;
    bool test=false,mooneye=false,grey=false,reportOnly=false,help=false;
    std::vector<u16> breakpoints;
    std::optional<u16> runTo;
    HardwareModel model=HardwareModel::Auto;
};
constexpr const char* usage=
    "Usage: gb_headless ROM.gb|ROM.gbc [frames] [options]\n"
    "  --frames N               Frame budget, 1..100000 (default 120)\n"
    "  --model auto|dmg|cgb      Select hardware model\n"
    "  --boot-rom file.bin       Use an external DMG/CGB boot image\n"
    "  --test | --mooneye        Detect hardware-test result\n"
    "  --output frame.ppm       Export the original 160x144 framebuffer\n"
    "  --grey                   Use the monochrome palette\n"
    "  --report report.json     Export ROM checksums and runtime diagnostics\n"
    "  --report-only            Inspect ROM header without running (JSON to stdout unless --report)\n"
    "  --trace trace.txt         Export instruction/register trace, including idle/interrupt steps\n"
    "  --trace-limit N           Maximum trace lines, 1..1000000 (default 100000)\n"
    "  --instructions N          Additional CPU-step budget, 1..100000000\n"
    "  --breakpoint HEX          Stop before executing this PC (repeatable)\n"
    "  --run-to HEX              Stop upon reaching this PC\n"
    "  --dump HEX COUNT file     Export mapped memory bytes; never wraps past FFFF\n"
    "  --disassemble HEX COUNT   Print up to 4096 decoded instructions\n"
    "  --help                   Show this help\n"
    "Addresses are hexadecimal (C000, 0xC000 or $C000); counts are decimal.\n";
std::filesystem::path utf8Path(std::string_view value) {
    return std::filesystem::path(std::u8string(value.begin(),value.end()));
}
std::vector<std::string> commandArguments(int argc,char** argv) {
#ifdef _WIN32
    // Narrow CRT argv uses the system code page. Read the original UTF-16 command line.
    int count=0;wchar_t** wide=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!wide)throw std::runtime_error("Could not read Windows command line.");
    std::vector<std::string> result;
    try {
        result.reserve((std::size_t)count);
        for(int n=0;n<count;++n){
            const int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[n],-1,nullptr,0,nullptr,nullptr);
            if(!bytes)throw std::runtime_error("Invalid Unicode command-line argument.");
            std::string argument((std::size_t)bytes,'\0');
            if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[n],-1,argument.data(),bytes,nullptr,nullptr))throw std::runtime_error("Could not convert command-line argument.");
            argument.pop_back();result.push_back(std::move(argument));
        }
    } catch(...) {LocalFree(wide);throw;}
    LocalFree(wide);(void)argc;(void)argv;return result;
#else
    std::vector<std::string> result;for(int n=0;n<argc;++n)result.emplace_back(argv[n]);return result;
#endif
}
Options parse(const std::vector<std::string>& arguments) {
    Options o;
    if(arguments.size()<2){o.help=true;return o;}
    if(arguments[1]=="--help"||arguments[1]=="-h"){o.help=true;return o;}
    o.rom=utf8Path(arguments[1]);bool gotFrames=false;
    auto number=[](const std::string& value,u64 maximum,int base=10){
        u64 parsed=0;if(!studio::parseUnsigned(value,parsed,maximum,base))throw std::runtime_error("Invalid numeric argument: "+value);return parsed;
    };
    auto positive=[&](const std::string& value,u64 maximum){const u64 n=number(value,maximum);if(!n)throw std::runtime_error("Count must be greater than zero: "+value);return n;};
    for(std::size_t n=2;n<arguments.size();++n) {
        const auto& arg=arguments[n];
        auto next=[&]() -> const std::string& {
            if(n+1>=arguments.size()||arguments[n+1].rfind("--",0)==0)throw std::runtime_error("Missing value for "+arg);
            return arguments[++n];
        };
        if(arg=="--help"||arg=="-h")o.help=true;
        else if(arg=="--test")o.test=true;
        else if(arg=="--mooneye")o.mooneye=true;
        else if(arg=="--grey")o.grey=true;
        else if(arg=="--report-only")o.reportOnly=true;
        else if(arg=="--boot-rom")o.boot=utf8Path(next());
        else if(arg=="--output")o.output=utf8Path(next());
        else if(arg=="--report")o.report=utf8Path(next());
        else if(arg=="--trace")o.trace=utf8Path(next());
        else if(arg=="--trace-limit")o.traceLimit=positive(next(),1000000);
        else if(arg=="--instructions")o.instructions=positive(next(),100000000);
        else if(arg=="--breakpoint")o.breakpoints.push_back((u16)number(next(),65535,16));
        else if(arg=="--run-to")o.runTo=(u16)number(next(),65535,16);
        else if(arg=="--dump"){o.dumpStart=(u16)number(next(),65535,16);o.dumpCount=positive(next(),65536);o.dump=utf8Path(next());}
        else if(arg=="--disassemble"){o.disassemblyStart=(u16)number(next(),65535,16);o.disassemblyCount=positive(next(),4096);}
        else if(arg=="--model"){
            const auto& name=next();
            if(name=="auto")o.model=HardwareModel::Auto;else if(name=="dmg")o.model=HardwareModel::DMG;
            else if(name=="cgb")o.model=HardwareModel::CGB;else throw std::runtime_error("Unknown hardware model: "+name);
        } else if(arg=="--frames"){
            if(gotFrames)throw std::runtime_error("Frame count was specified twice.");o.frames=positive(next(),100000);gotFrames=true;
        } else if(!arg.empty()&&arg.front()!='-'){
            if(gotFrames)throw std::runtime_error("Unexpected argument: "+arg);o.frames=positive(arg,100000);gotFrames=true;
        } else throw std::runtime_error("Unknown option: "+arg);
    }
    if(o.test&&o.mooneye)throw std::runtime_error("Choose either --test or --mooneye.");
    if(o.reportOnly&&(!o.output.empty()||!o.trace.empty()||!o.dump.empty()||o.disassemblyCount||o.instructions||!o.breakpoints.empty()||o.runTo||o.test||o.mooneye))
        throw std::runtime_error("--report-only cannot be combined with execution or memory export options.");
    // Prevent accidental ROM corruption or two exports overwriting each other.
    std::vector<std::filesystem::path> used={o.rom};
    if(!o.boot.empty())used.push_back(o.boot);
    for(const auto& path:{o.output,o.report,o.trace,o.dump})if(!path.empty()){
        for(const auto& other:used){
            std::error_code error;
            const bool equivalent=std::filesystem::equivalent(path,other,error);
            const auto a=std::filesystem::weakly_canonical(path),b=std::filesystem::weakly_canonical(other);
#ifdef _WIN32
            const bool same=CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_EQUAL;
#else
            const bool same=a==b;
#endif
            if(equivalent||same)
                throw std::runtime_error("Input and output files must have distinct paths.");
        }
        used.push_back(path);
    }
    return o;
}
void writeText(const std::filesystem::path& path,std::string_view value) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    if(!file||!file.write(value.data(),(std::streamsize)value.size()))throw std::runtime_error("Could not write "+studio::pathUtf8(path));
}
int execute(const Options& o) {
    if(o.help){std::cout<<usage;return 0;}
    if(o.reportOnly){
        std::error_code error;const auto size=std::filesystem::file_size(o.rom,error);
        if(error||size>8*1024*1024)throw std::runtime_error("Could not inspect ROM (maximum supported size is 8 MiB).");
        const auto bytes=Memory::loadBytes(o.rom);
        if(bytes.size()!=size)throw std::runtime_error("Could not read the complete ROM.");
        const auto report=studio::romReport(bytes,studio::pathUtf8(o.rom.filename()));
        if(o.report.empty())std::cout<<report;else writeText(o.report,report);return 0;
    }
    GameBoy gb;
    if(!o.boot.empty()&&!gb.loadBootROM(o.boot))throw std::runtime_error("Could not configure Boot ROM: "+gb.mem.loadError);
    if(!gb.load(o.rom,o.model))throw std::runtime_error("Could not load ROM: "+gb.mem.loadError);
    if(o.test&&gb.mem.eram.empty()&&gb.mem.mbc==1)gb.mem.eram.assign(8192,0xFF);
    if(o.grey)gb.setPalette(1);
    studio::Debugger debugger(gb);debugger.setTraceCapacity(o.trace.empty()?0:1);
    for(u16 address:o.breakpoints)debugger.addBreakpoint(address);
    if(o.runTo)debugger.setRunTo(*o.runTo);
    std::ofstream trace;
    if(!o.trace.empty()){trace.open(o.trace,std::ios::binary|std::ios::trunc);if(!trace)throw std::runtime_error("Could not open trace output.");}
    const bool stepping=o.mooneye||o.instructions||!o.trace.empty()||!o.breakpoints.empty()||o.runTo.has_value();
    bool passed=false,failed=false,stopped=false;
    u64 completed=0,steps=0,traced=0;std::size_t printed=0;
    studio::StopReason reason=studio::StopReason::Budget;
    for(u64 frame=0;frame<o.frames&&!stopped;++frame){
        if(stepping){
            const u64 deadline=gb.totalCycles+GB_FRAME;
            while(gb.totalCycles<deadline){
                if(o.instructions&&steps>=o.instructions){stopped=true;break;}
                if(o.mooneye&&gb.mem.cpuRead(gb.cpu.PC)==0x40){
                    const auto& c=gb.cpu;
                    passed=c.B==3&&c.C==5&&c.D==8&&c.E==13&&c.H==21&&c.L==34;
                    failed=c.B==0x42&&c.C==0x42&&c.D==0x42&&c.E==0x42&&c.H==0x42&&c.L==0x42;
                    if(passed||failed){stopped=true;break;}
                }
                const auto result=debugger.run(1);steps+=result.steps;
                if(result.steps&&trace.is_open()&&traced<o.traceLimit){trace<<studio::formatTrace(debugger.trace().back())<<'\n';++traced;}
                if(result.reason!=studio::StopReason::Budget){reason=result.reason;stopped=true;break;}
            }
        } else gb.runFrame();
        completed=gb.totalCycles/GB_FRAME;
        if(gb.mem.serialOutput.size()>printed){std::cout<<gb.mem.serialOutput.substr(printed)<<std::flush;printed=gb.mem.serialOutput.size();}
        if(o.test){
            const auto& serial=gb.mem.serialOutput;
            passed=serial.find("Passed")!=std::string::npos;failed=serial.find("Failed")!=std::string::npos||serial.find("FAILED")!=std::string::npos;
            if(gb.mem.eram.size()>=4&&gb.mem.eram[1]==0xDE&&gb.mem.eram[2]==0xB0&&gb.mem.eram[3]==0x61&&gb.mem.eram[0]<0x80){
                passed=gb.mem.eram[0]==0;failed=!passed;
                for(std::size_t n=4;n<gb.mem.eram.size()&&gb.mem.eram[n];++n)std::cout<<(char)gb.mem.eram[n];std::cout<<'\n';
            }
            if(passed||failed)stopped=true;
        }
    }
    if(trace.is_open()){trace.flush();if(!trace)throw std::runtime_error("Could not write complete trace.");}
    std::cout<<"\nframes="<<completed<<" PC="<<studio::hex(gb.cpu.PC,4)<<" LY="<<(unsigned)gb.mem.io[0x44]
             <<" ROM="<<gb.mem.gameTitle<<" MODEL="<<(gb.mem.cgbMode?"CGB":"DMG")<<" SPEED="<<(gb.mem.doubleSpeed?2:1);
    if(stepping)std::cout<<" steps="<<steps<<" stop="<<studio::stopReasonName(reason);
    if(!o.trace.empty())std::cout<<" traced="<<traced<<(traced==o.traceLimit?" (trace limit reached)":"");std::cout<<'\n';
    if(o.disassemblyCount)std::cout<<studio::disassemble(gb.mem,o.disassemblyStart,(std::size_t)o.disassemblyCount);
    if(!o.dump.empty()){
        const auto bytes=studio::memoryBytes(gb.mem,o.dumpStart,(std::size_t)o.dumpCount);
        writeText(o.dump,std::string_view((const char*)bytes.data(),bytes.size()));
    }
    if(!o.report.empty())writeText(o.report,studio::romReport(gb));
    if(!o.output.empty()){
        std::ofstream file(o.output,std::ios::binary|std::ios::trunc);if(!file)throw std::runtime_error("Could not write screenshot.");
        file<<"P6\n160 144\n255\n";for(u32 px:gb.ppu.pixels){const char rgb[3]={(char)(px>>16),(char)(px>>8),(char)px};file.write(rgb,3);}
        if(!file)throw std::runtime_error("Could not write complete screenshot.");
    }
    if(!o.test&&!o.mooneye)gb.mem.saveBattery();
    if(o.test||o.mooneye){std::cout<<(passed?"RESULT: PASS\n":failed?"RESULT: FAIL\n":"RESULT: TIMEOUT\n");return passed?0:1;}
    return 0;
}
} // namespace
int main(int argc,char** argv) {
    try{return execute(parse(commandArguments(argc,argv)));}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
