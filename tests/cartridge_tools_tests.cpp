#include "../src/cartridge_tools.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(expr) do { if(!(expr)) { std::cerr<<"CHECK FAILED: "<<#expr<<" at "<<__FILE__<<":"<<__LINE__<<"\n"; std::abort(); } } while(false)
using cartridge::Byte;
using Bytes=std::vector<Byte>;
static Bytes text(std::string_view value){return {value.begin(),value.end()};}
// Independent bitwise reference CRC, checked against the conventional vector.
static std::uint32_t referenceCRC(std::span<const Byte> bytes){
    std::uint32_t value=0xFFFFFFFFu;
    for(Byte b:bytes){value^=b;for(int bit=0;bit<8;++bit)value=(value>>1)^((value&1)?0xEDB88320u:0u);}
    return ~value;
}
static void little(Bytes& bytes,std::uint32_t value){for(int i=0;i<4;++i)bytes.push_back((Byte)(value>>(i*8)));}
static void number(Bytes& bytes,std::uint64_t value){
    while(true){const Byte digit=(Byte)(value&127);value>>=7;
        if(!value){bytes.push_back(digit|0x80);return;}bytes.push_back(digit);--value;}
}
static Bytes bps(std::span<const Byte> source,std::span<const Byte> target,Bytes actions,std::string_view metadata={}){
    auto bytes=text("BPS1");number(bytes,source.size());number(bytes,target.size());number(bytes,metadata.size());
    bytes.insert(bytes.end(),metadata.begin(),metadata.end());bytes.insert(bytes.end(),actions.begin(),actions.end());
    little(bytes,referenceCRC(source));little(bytes,referenceCRC(target));little(bytes,referenceCRC(bytes));return bytes;
}
static void patchChecksum(Bytes& bytes){
    const auto crc=referenceCRC(std::span<const Byte>(bytes).first(bytes.size()-4));
    for(int i=0;i<4;++i)bytes[bytes.size()-4+i]=(Byte)(crc>>(8*i));
}
static void checkFailure(const Bytes& source,const Bytes& patch){
    const auto before=source;const auto result=cartridge::applyPatch(source,patch);
    CHECK(!result.success&&result.rom.empty()&&!result.error.empty()&&source==before);
}
static void testIPS(){
    const auto source=text("abcdefgh");auto patch=text("PATCH");
    const Bytes records={0,0,1,0,2,'X','Y', 0,0,6,0,0,0,4,'Z', 'E','O','F'};
    patch.insert(patch.end(),records.begin(),records.end());
    auto result=cartridge::applyPatch(source,patch);
    CHECK(result.success&&result.error.empty()&&result.rom==text("aXYdefZZZZ"));
    CHECK(source==text("abcdefgh"));
    patch.insert(patch.end(),{0,0,7});result=cartridge::applyPatch(source,patch);
    CHECK(result.success&&result.rom==text("aXYdefZ")); // Optional final truncate.
    auto expand=text("PATCHEOF");expand.insert(expand.end(),{0,0,10});result=cartridge::applyPatch(source,expand);
    CHECK(result.success&&result.rom.size()==10&&result.rom[8]==0&&result.rom[9]==0);
    auto unchanged=cartridge::applyPatch(source,text("PATCHEOF"));CHECK(unchanged.success&&unchanged.rom==source);
    checkFailure(source,text("PATCH"));checkFailure(source,text("PATCHEOFx"));
    checkFailure(source,Bytes{'P','A','T','C','H',0,0,0,0,2,1}); // Truncated literal.
    checkFailure(source,Bytes{'P','A','T','C','H',0,0,0,0,0,0,0,1,'E','O','F'}); // Empty RLE.
    checkFailure(source,Bytes{'P','A','T','C','H',0x80,0,0,0,1,1,'E','O','F'}); // 8 MiB plus one byte.
    checkFailure(source,Bytes{'P','A','T','C','H','E','O','F',0x80,0,1});
    // Record-order overlaps are valid: later writes replace earlier writes.
    result=cartridge::applyPatch(source,Bytes{'P','A','T','C','H',0,0,1,0,3,'1','2','3',0,0,2,0,1,'X','E','O','F'});
    CHECK(result.success&&result.rom==text("a1X3efgh"));
    result=cartridge::applyPatch(source,Bytes{'P','A','T','C','H',0x7F,0xFF,0xFF,0,1,0x5A,'E','O','F'});
    CHECK(result.success&&result.rom.size()==cartridge::maxROMBytes&&result.rom.back()==0x5A);
    const Bytes oversizedSource(cartridge::maxROMBytes+1,0);
    checkFailure(oversizedSource,text("PATCHEOF"));
    // A small malicious patch cannot request unbounded repeated RLE writes.
    auto excessive=text("PATCH");
    for(std::size_t i=0;i<=cartridge::maxIPSWrites/65535;++i)
        excessive.insert(excessive.end(),{0,0,0,0,0,0xFF,0xFF,1});
    const auto eof=text("EOF");excessive.insert(excessive.end(),eof.begin(),eof.end());checkFailure(source,excessive);
}
static void testBPS(){
    const auto source=text("abc"),target=text("axc");
    const auto golden=bps(source,target,{0x80,0x81,'x',0x80},"synthetic fixture");
    auto result=cartridge::applyPatch(source,golden);CHECK(result.success&&result.rom==target);
    auto bad=golden;bad[4]^=1;checkFailure(source,bad); // Patch CRC protects any command/header byte.
    auto wrongSource=source;wrongSource[0]='z';checkFailure(wrongSource,golden);
    bad=golden;bad[bad.size()-8]^=1;patchChecksum(bad);checkFailure(source,bad); // Target CRC checked independently.
    bad=golden;bad[4]=0x84;patchChecksum(bad);checkFailure(source,bad); // Matching CRC but wrong source length.
    checkFailure(source,text("BPS1"));
    // All four actions, backwards SourceCopy and backwards TargetCopy cursors.
    const auto dictionary=text("ABCDEFGH");const auto copied=text("ABZEFGHEFABZEFGHEFABA");
    const Bytes actions={0x84,0x81,'Z',0x8E,0x88,0x86,0x89,0xA3,0x80,0x87,0x93,0x83,0x85};
    result=cartridge::applyPatch(dictionary,bps(dictionary,copied,actions));
    CHECK(result.success&&result.rom==copied);
    // An overlapping TargetCopy can produce a run much longer than the seed.
    const Bytes seed={'Q'};const Bytes repeated(4096,'Q');Bytes overlap={0x81,'Q'};
    number(overlap,((repeated.size()-2)<<2)|3);number(overlap,0);
    result=cartridge::applyPatch(seed,bps(seed,repeated,overlap));CHECK(result.success&&result.rom==repeated);
    checkFailure(source,bps(source,text("x"),{0x83,0x80})); // TargetCopy before anything was written.
    checkFailure(source,bps(source,text("x"),{0x82,0x88})); // SourceCopy beyond the source.
    checkFailure(source,bps(source,text("x"),{0x82,0x83})); // SourceCopy before the source.
    checkFailure(source,bps(source,text("x"),{0x84})); // Writes two bytes into a one-byte target.
    checkFailure(source,bps(source,text("x"),{0x81})); // Truncated TargetRead.
    checkFailure(source,bps(source,text("a"),{0x80,0x80})); // Extra action.
    auto overflow=text("BPS1");overflow.insert(overflow.end(),20,0x7F);
    little(overflow,referenceCRC(source));little(overflow,0);little(overflow,referenceCRC(overflow));checkFailure(source,overflow);
    auto oversized=text("BPS1");number(oversized,source.size());number(oversized,cartridge::maxROMBytes+1);number(oversized,0);
    little(oversized,referenceCRC(source));little(oversized,0);little(oversized,referenceCRC(oversized));checkFailure(source,oversized);
    // Empty targets and metadata are permitted by the BPS wire format.
    result=cartridge::applyPatch(source,bps(source,{},{}));CHECK(result.success&&result.rom.empty());
    // Every truncated prefix of a known valid patch fails safely.
    for(std::size_t i=0;i<golden.size();++i)checkFailure(source,Bytes(golden.begin(),golden.begin()+i));
}
static void testCodes(){
    auto genie=cartridge::parseGameGenie("00A-17B-C49");
    CHECK(genie&&genie->address==0x4A17&&genie->value==0&&genie->compare==0xC8);
    genie=cartridge::parseGameGenie(" 422-34e ");CHECK(genie&&genie->address==0x1234&&genie->value==0x42&&!genie->compare);
    genie=cartridge::parseGameGenie("42234EC49");CHECK(genie&&genie->address==0x1234&&genie->compare==0xC8);
    CHECK(!cartridge::parseGameGenie("422-34G"));CHECK(!cartridge::parseGameGenie("422-341")); // Outside ROM.
    CHECK(!cartridge::parseGameGenie("42234EC4"));CHECK(!cartridge::parseGameGenie("42234EC499"));
    const auto shark=cartridge::parseGameShark("01ff56c0");CHECK(shark&&shark->address==0xC056&&shark->value==0xFF);
    const auto banked=cartridge::parseGameShark("91FF56D0");CHECK(banked&&banked->type==0x91&&banked->address==0xD056);CHECK(!cartridge::parseGameShark("98FF56D0"));CHECK(!cartridge::parseGameShark("00FF56C0"));
    CHECK(!cartridge::parseGameShark("01FF56CG"));CHECK(!cartridge::parseGameShark("01FF56C"));
    CHECK(!cartridge::parseGameShark("01FF56C01"));CHECK(!cartridge::parseGameShark("01-FF56C0"));
}
int main(){
    CHECK(referenceCRC(text("123456789"))==0xCBF43926u);
    CHECK(cartridge::crc32(text("123456789"))==0xCBF43926u&&cartridge::crc32({})==0);
    testIPS();testBPS();testCodes();std::cout<<"ALL CARTRIDGE TOOL TESTS PASSED\n";
}
