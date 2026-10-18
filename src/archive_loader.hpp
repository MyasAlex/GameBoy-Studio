#pragma once
#include "portable_state.hpp"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "../third_party/miniz/miniz.h"
#include <cctype>

namespace archives {
struct Result {std::vector<u8> rom;std::string member,error;explicit operator bool()const{return !rom.empty()&&error.empty();}};
inline bool singleBankSupported(std::span<const u8> rom){return rom.size()!=16384||rom[0x147]==0x00||rom[0x147]==0x08||rom[0x147]==0x09;}
inline bool romName(std::string_view path){const auto at=path.find_last_of('.');if(at==std::string_view::npos)return false;
    std::string ext(path.substr(at));for(char& c:ext)c=(char)std::tolower((unsigned char)c);return ext==".gb"||ext==".gbc";}
inline Result read(const std::filesystem::path& path){
    const auto input=Memory::loadBytes(path,32*1024*1024);if(input.empty())return {{},{},"File is empty, unreadable, or exceeds 32 MiB."};
    if(input.size()>=2&&input[0]=='P'&&input[1]=='K'){
        mz_zip_archive zip{};if(!mz_zip_reader_init_mem(&zip,input.data(),input.size(),0))return {{},{},"Invalid ZIP archive."};
        struct Close {mz_zip_archive& zip;~Close(){mz_zip_reader_end(&zip);}} close{zip};
        const auto count=mz_zip_reader_get_num_files(&zip);if(count>4096)return {{},{},"ZIP contains too many files."};
        int chosen=-1;mz_zip_archive_file_stat stat{};
        for(mz_uint i=0;i<count;++i){mz_zip_archive_file_stat current{};if(!mz_zip_reader_file_stat(&zip,i,&current))return {{},{},"Invalid ZIP directory."};
            if(current.m_is_directory||!romName(current.m_filename))continue;
            if(chosen>=0)return {{},{},"ZIP has multiple games. Extract the game you want to play."};
            if(current.m_uncomp_size<16384||current.m_uncomp_size>8*1024*1024||current.m_is_encrypted||!current.m_is_supported)return {{},{},"ZIP game is encrypted, unsupported, or exceeds 8 MiB."};
            chosen=(int)i;stat=current;}
        if(chosen<0)return {{},{},"No .gb/.gbc game found in ZIP."};Result result;result.rom.resize((std::size_t)stat.m_uncomp_size);result.member=stat.m_filename;
        if(!mz_zip_reader_extract_to_mem(&zip,(mz_uint)chosen,result.rom.data(),result.rom.size(),0))return {{},{},"ZIP data/CRC validation failed."};if(!singleBankSupported(result.rom))return {{},{},"Banked ROM must contain at least 32 KiB."};return result;
    }
    if(input.size()>=2&&input[0]==0x1F&&input[1]==0x8B){
        if(input.size()<18||input[2]!=8||(input[3]&0xE0))return {{},{},"Invalid GZIP header."};std::size_t at=10;const u8 flags=input[3];
        if(flags&4){if(at+2>input.size()-8)return {{},{},"Invalid GZIP extra field."};const auto length=(unsigned)(input[at]|(input[at+1]<<8));at+=2;if(length>input.size()-8-at)return {{},{},"Truncated GZIP header."};at+=length;}
        for(u8 flag:{u8(8),u8(16)})if(flags&flag){while(at<input.size()-8&&input[at])++at;if(at>=input.size()-8)return {{},{},"Unterminated GZIP name/comment."};++at;}
        if(flags&2){if(at+2>input.size()-8)return {{},{},"Missing GZIP header CRC."};const auto crc=(u16)mz_crc32(0,input.data(),at);
            if(crc!=(u16)(input[at]|(input[at+1]<<8)))return {{},{},"GZIP header CRC mismatch."};at+=2;}
        const u32 size=saves::le32(input,input.size()-4);if(size<16384||size>8*1024*1024||size%16384)return {{},{},"GZIP output length is not a supported ROM."};
        Result result;result.rom.resize(size);tinfl_decompressor decoder;tinfl_init(&decoder);std::size_t compressed=input.size()-8-at,produced=size;
        const auto status=tinfl_decompress(&decoder,input.data()+at,&compressed,result.rom.data(),result.rom.data(),&produced,TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
        if(status!=TINFL_STATUS_DONE||produced!=size||compressed!=input.size()-8-at||mz_crc32(0,result.rom.data(),size)!=saves::le32(input,input.size()-8))return {{},{},"GZIP data/CRC validation failed."};if(!singleBankSupported(result.rom))return {{},{},"Banked ROM must contain at least 32 KiB."};return result;
    }
    return {input,{}, {}};
}
} // namespace archives
