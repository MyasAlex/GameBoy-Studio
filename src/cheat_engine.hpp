#pragma once
#include "gameboy.hpp"
#include "cartridge_tools.hpp"
#include <optional>

namespace cheats {
struct Entry {std::string code,description;bool enabled=true;std::optional<cartridge::GameGeniePatch> genie;std::optional<cartridge::GameSharkPatch> shark;};
class Engine {
    Memory& memory;bool active=true;
public:
    std::vector<Entry> entries;
    explicit Engine(Memory& m):memory(m){memory.romReadFilter=[this](u16 address,u8 original){
        if(!active)return original;for(const auto& e:entries)if(e.enabled&&e.genie&&e.genie->address==address&&(!e.genie->compare||*e.genie->compare==original))return e.genie->value;return original;};}
    ~Engine(){memory.romReadFilter={};}
    Engine(const Engine&)=delete;Engine& operator=(const Engine&)=delete;
    void setEnabled(bool enabled){active=enabled;}
    bool enabled()const{return active;}
    bool add(std::string code,std::string description={}){
        if(entries.size()>=256)return false;Entry e{code,description,true,{},{}};
        // Eight unhyphenated hex digits mean GameShark / Game Boy Action Replay type 01.
        auto digits=cartridge::detail::digits(code,false);
        if(digits&&digits->size()==8)e.shark=cartridge::parseGameShark(code);else e.genie=cartridge::parseGameGenie(code);
        if(!e.genie&&!e.shark)return false;if(e.shark){const auto address=e.shark->address;const auto type=e.shark->type;
            if(type==1&&(address<0xA000||(address>=0xFE00&&address<0xFF80)||address==0xFFFF))return false;
            if(type>=0x80&&type<=0x8F&&(address<0xA000||address>=0xC000))return false;
            if(type>=0x90&&(address<0xD000||address>=0xE000))return false;}
        entries.push_back(std::move(e));return true;
    }
    void applyRAM(){if(!active)return;for(const auto& e:entries)if(e.enabled&&e.shark){const auto& code=*e.shark;
        if(code.type==1)memory.write(code.address,code.value);
        else {const unsigned access=code.type<0x90?2:1;const std::size_t bank=code.type&15,offset=access==2?bank*8192+(code.address&8191):std::max<std::size_t>(1,bank)*4096+(code.address&4095);
            if(access==2&&offset<memory.eram.size()){memory.eram[offset]=code.value;if(memory.externalPoke)memory.externalPoke(access,offset,code.value);}
            if(access==1&&memory.isCGBHardware()&&offset<memory.wram.size()){memory.wram[offset]=code.value;if(memory.externalPoke)memory.externalPoke(access,offset,code.value);}}}}
    bool load(const std::filesystem::path& file){const auto data=Memory::loadBytes(file,256*1024);if(data.empty())return false;
        const auto old=entries;entries.clear();std::istringstream text(std::string(data.begin(),data.end()));std::string line;
        while(std::getline(text,line)){if(line.empty()||line[0]=='#')continue;const bool disabled=line[0]=='!';if(disabled)line.erase(line.begin());
            const auto pos=line.find('|');if(!add(line.substr(0,pos),pos==std::string::npos?"":line.substr(pos+1))){entries=old;return false;}entries.back().enabled=!disabled;}
        return true;}
    bool save(const std::filesystem::path& file)const{std::ostringstream out;out<<"# GameBoy Studio cheats: [!]code|description\n";
        for(const auto& e:entries)out<<(e.enabled?"":"!")<<e.code<<'|'<<e.description<<'\n';return Memory::writeAtomic(file,out.str(),true);}
};
enum class Compare { Equal,NotEqual,Increased,Decreased,Changed,Unchanged };
class Search {
    std::vector<u16> candidates;std::array<u8,65536> previous{};
public:
    void begin(const Memory& m){candidates.clear();for(unsigned a=0xC000;a<0xE000;++a){candidates.push_back((u16)a);previous[a]=m.read((u16)a);}
        for(unsigned a=0xA000;a<0xC000;++a)if(m.ramEnabled&&!m.eram.empty()){candidates.push_back((u16)a);previous[a]=m.read((u16)a);}}
    void filter(const Memory& m,Compare comparison,std::optional<u8> value={}){
        candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](u16 a){const u8 now=m.read(a),old=value?*value:previous[a];bool keep=false;
            switch(comparison){case Compare::Equal:keep=now==old;break;case Compare::NotEqual:keep=now!=old;break;case Compare::Increased:keep=now>old;break;
            case Compare::Decreased:keep=now<old;break;case Compare::Changed:keep=now!=old;break;case Compare::Unchanged:keep=now==old;break;}
            previous[a]=now;return !keep;}),candidates.end());}
    const std::vector<u16>& results()const{return candidates;}
};
} // namespace cheats
