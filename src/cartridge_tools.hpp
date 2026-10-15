#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Independent, transactional patch/code parsing. No input files or live memory
// are modified. BPS1 follows byuu's public-domain specification:
// https://github.com/blakesmith/rombp/blob/master/docs/bps_spec.md
// IPS framing/EOF extension is described by the original Flips implementation:
// https://github.com/Alcaro/Flips/blob/master/libips.cpp
namespace cartridge {
using Byte=std::uint8_t;
inline constexpr std::size_t maxROMBytes=8*1024*1024;
inline constexpr std::size_t maxPatchBytes=32*1024*1024;
inline constexpr std::size_t maxIPSWrites=16*maxROMBytes;
struct PatchResult {bool success=false;std::vector<Byte> rom;std::string error;};
inline std::uint32_t crc32(std::span<const Byte> bytes){
    static constexpr auto table=[](){
        std::array<std::uint32_t,256> result{};
        for(unsigned i=0;i<256;++i){std::uint32_t crc=i;
            for(int bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xEDB88320u:0u);
            result[i]=crc;
        }
        return result;
    }();
    std::uint32_t crc=0xFFFFFFFFu;for(Byte b:bytes)crc=table[(crc^b)&255]^(crc>>8);return ~crc;
}
namespace detail {
inline PatchResult fail(std::string message){return {false,{},std::move(message)};}
inline bool marker(std::span<const Byte> bytes,std::string_view text){
    return bytes.size()>=text.size()&&std::equal(text.begin(),text.end(),bytes.begin());
}
inline std::size_t big(std::span<const Byte> bytes,std::size_t at,unsigned count){
    std::size_t value=0;for(unsigned i=0;i<count;++i)value=(value<<8)|bytes[at+i];return value;
}
inline std::uint32_t little32(std::span<const Byte> bytes,std::size_t at){
    return (std::uint32_t)bytes[at]|((std::uint32_t)bytes[at+1]<<8)|
        ((std::uint32_t)bytes[at+2]<<16)|((std::uint32_t)bytes[at+3]<<24);
}
inline PatchResult ips(std::span<const Byte> source,std::span<const Byte> patch){
    std::vector<Byte> target(source.begin(),source.end());std::size_t at=5,writes=0;
    while(true){
        if(patch.size()-at<3)return fail("IPS is missing its EOF marker.");
        const auto offset=big(patch,at,3);at+=3;
        if(offset==0x454F46){
            const auto remaining=patch.size()-at;
            if(remaining==3){const auto size=big(patch,at,3);
                if(size>maxROMBytes)return fail("IPS output exceeds the 8 MiB ROM limit.");
                target.resize(size,0);
            }else if(remaining)return fail("IPS contains invalid trailing data.");
            return {true,std::move(target),{}};
        }
        if(patch.size()-at<2)return fail("IPS record length is truncated.");
        auto length=big(patch,at,2);at+=2;bool rle=length==0;Byte repeated=0;
        if(rle){
            if(patch.size()-at<3)return fail("IPS RLE record is truncated.");
            length=big(patch,at,2);at+=2;repeated=patch[at++];
            if(!length)return fail("IPS RLE length cannot be zero.");
        }else if(length>patch.size()-at)return fail("IPS literal record is truncated.");
        if(offset>maxROMBytes||length>maxROMBytes-offset)return fail("IPS output exceeds the 8 MiB ROM limit.");
        // Overlapping IPS records can repeatedly rewrite the same output.
        // Bound expanded work as well as allocation for untrusted patches.
        if(length>maxIPSWrites-writes)return fail("IPS records exceed the 128 MiB processing limit.");writes+=length;
        if(target.size()<offset+length)target.resize(offset+length,0);
        if(rle)std::fill(target.begin()+offset,target.begin()+offset+length,repeated);
        else {std::copy_n(patch.begin()+at,length,target.begin()+offset);at+=length;}
    }
}
inline bool number(std::span<const Byte> patch,std::size_t& at,std::size_t end,std::uint64_t& value){
    value=0;std::uint64_t shift=1;constexpr auto maximum=std::numeric_limits<std::uint64_t>::max();
    while(at<end){
        const Byte byte=patch[at++];const auto digit=(std::uint64_t)(byte&0x7F);
        if(digit>(maximum-value)/shift)return false;value+=digit*shift;
        if(byte&0x80)return true;
        if(shift>(maximum>>7))return false;shift<<=7;
        if(value>maximum-shift)return false;value+=shift;
    }
    return false;
}
inline bool relative(std::size_t& cursor,std::uint64_t encoded,std::size_t limit){
    const auto distance=encoded>>1;
    if(cursor>limit)return false;
    if(encoded&1){if(distance>cursor)return false;cursor-=(std::size_t)distance;}
    else {if(distance>limit-cursor)return false;cursor+=(std::size_t)distance;}
    return true;
}
inline PatchResult bps(std::span<const Byte> source,std::span<const Byte> patch){
    if(patch.size()<19)return fail("BPS header or checksum footer is truncated.");
    const auto end=patch.size()-12;
    if(crc32(patch.first(patch.size()-4))!=little32(patch,patch.size()-4))return fail("BPS patch CRC32 does not match.");
    if(crc32(source)!=little32(patch,end))return fail("BPS source ROM CRC32 does not match.");
    std::size_t at=4;std::uint64_t sourceSize=0,targetSize=0,metadataSize=0;
    if(!number(patch,at,end,sourceSize)||!number(patch,at,end,targetSize)||!number(patch,at,end,metadataSize))
        return fail("BPS contains a truncated or overflowing header integer.");
    if(sourceSize!=source.size())return fail("BPS source ROM size does not match.");
    if(targetSize>maxROMBytes)return fail("BPS output exceeds the 8 MiB ROM limit.");
    if(metadataSize>end-at)return fail("BPS metadata is truncated.");at+=(std::size_t)metadataSize;
    std::vector<Byte> target;target.reserve((std::size_t)targetSize);
    std::size_t sourceCursor=0,targetCursor=0;
    while(target.size()<targetSize){
        std::uint64_t action=0;
        if(!number(patch,at,end,action))return fail("BPS action integer is truncated or overflowing.");
        const auto length=(action>>2)+1;
        if(length>targetSize-target.size())return fail("BPS action writes past the target ROM.");
        switch(action&3){
            case 0:{const auto offset=target.size();
                if(offset>source.size()||length>source.size()-offset)return fail("BPS SourceRead exceeds the source ROM.");
                target.insert(target.end(),source.begin()+offset,source.begin()+offset+(std::size_t)length);break;
            }
            case 1:
                if(length>end-at)return fail("BPS TargetRead literal data is truncated.");
                target.insert(target.end(),patch.begin()+at,patch.begin()+at+(std::size_t)length);at+=(std::size_t)length;break;
            case 2:{std::uint64_t offset=0;
                if(!number(patch,at,end,offset)||!relative(sourceCursor,offset,source.size())||length>source.size()-sourceCursor)
                    return fail("BPS SourceCopy points outside the source ROM.");
                target.insert(target.end(),source.begin()+sourceCursor,source.begin()+sourceCursor+(std::size_t)length);
                sourceCursor+=(std::size_t)length;break;
            }
            case 3:{std::uint64_t offset=0;
                if(!number(patch,at,end,offset)||!relative(targetCursor,offset,target.size())||targetCursor>=target.size())
                    return fail("BPS TargetCopy points to unavailable target data.");
                // Read and append one byte at a time: overlapping copies are
                // intentional and implement repeating runs in the BPS format.
                for(std::uint64_t i=0;i<length;++i){const auto byte=target[targetCursor++];target.push_back(byte);}break;
            }
        }
    }
    if(at!=end)return fail("BPS contains extra actions or trailing data.");
    if(crc32(target)!=little32(patch,end+4))return fail("BPS target ROM CRC32 does not match.");
    return {true,std::move(target),{}};
}
inline int nibble(char c){
    if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;
}
inline std::optional<std::vector<unsigned>> digits(std::string_view code,bool hyphens){
    if(code.size()>64)return {};
    std::vector<unsigned> result;
    for(char c:code){
        if(c==' '||c=='\t'||c=='\r'||c=='\n'||(hyphens&&c=='-'))continue;
        const auto value=nibble(c);if(value<0)return {};result.push_back((unsigned)value);
    }
    return result;
}
} // namespace detail
inline PatchResult applyPatch(std::span<const Byte> rom,std::span<const Byte> patch){
    if(rom.size()>maxROMBytes)return detail::fail("Source ROM exceeds the 8 MiB ROM limit.");
    if(patch.size()>maxPatchBytes)return detail::fail("Patch exceeds the 32 MiB input limit.");
    if(detail::marker(patch,"PATCH"))return detail::ips(rom,patch);
    if(detail::marker(patch,"BPS1"))return detail::bps(rom,patch);
    return detail::fail("Unrecognized patch format. Expected IPS or BPS1.");
}

struct GameGeniePatch {std::uint16_t address=0;Byte value=0;std::optional<Byte> compare;};
struct GameSharkPatch {std::uint16_t address=0;Byte value=0;Byte type=1;};
// Game Boy decoding equations independently follow SameBoy's parser:
// https://github.com/LIJI32/SameBoy/blob/master/Core/cheats.c
inline std::optional<GameGeniePatch> parseGameGenie(std::string_view code){
    const auto parsed=detail::digits(code,true);
    if(!parsed||(parsed->size()!=6&&parsed->size()!=9))return {};
    const auto& d=*parsed;
    const auto address=(std::uint16_t)(((d[5]^15)<<12)|(d[2]<<8)|(d[3]<<4)|d[4]);
    if(address>=0x8000)return {}; // The physical Game Genie substitutes ROM reads.
    GameGeniePatch result{address,(Byte)((d[0]<<4)|d[1]),{}};
    if(d.size()==9){const Byte encoded=(Byte)((d[6]<<4)|d[8]);result.compare=(Byte)(((encoded>>2)|(encoded<<6))^0xBA);}
    return result;
}
// Type 01 only; bank-changing and other GameShark/Action Replay types are
// deliberately rejected. The returned data does not itself alter memory.
// https://docs.pyboy.dk/api/gameshark.html
inline std::optional<GameSharkPatch> parseGameShark(std::string_view code){
    const auto parsed=detail::digits(code,false);
    if(!parsed||parsed->size()!=8)return {};
    const auto& d=*parsed;const Byte type=(Byte)((d[0]<<4)|d[1]);if(type!=1&&(type<0x80||type>0x8F)&&(type<0x90||type>0x97))return {};
    return GameSharkPatch{(std::uint16_t)((d[6]<<12)|(d[7]<<8)|(d[4]<<4)|d[5]),(Byte)((d[2]<<4)|d[3]),type};
}
} // namespace cartridge
