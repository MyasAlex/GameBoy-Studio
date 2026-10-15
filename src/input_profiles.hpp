#pragma once
#include "gameboy.hpp"
namespace controls {
struct GamepadProfile {int device=-1,deadzone=12000;bool tiltStick=true;std::array<u32,8> buttons={8,4,1,2,0x1000,0x2000,0x20,0x10};};
inline bool save(const std::filesystem::path& path,const std::array<GamepadProfile,2>& profiles){std::ostringstream out;out<<"GBPAD1\n";for(const auto& p:profiles){out<<p.device<<' '<<p.deadzone<<' '<<p.tiltStick;for(auto button:p.buttons)out<<' '<<button;out<<'\n';}return Memory::writeAtomic(path,out.str(),true);}
inline bool load(const std::filesystem::path& path,std::array<GamepadProfile,2>& profiles){const auto bytes=Memory::loadBytes(path,4096);std::istringstream in(std::string(bytes.begin(),bytes.end()));std::string magic;std::array<GamepadProfile,2> values{};if(!(in>>magic)||magic!="GBPAD1")return false;
    for(auto& p:values){unsigned tilt=0;if(!(in>>p.device>>p.deadzone>>tilt)||p.device< -1||p.device>3||p.deadzone<1000||p.deadzone>30000||tilt>1)return false;p.tiltStick=tilt!=0;for(auto& button:p.buttons)if(!(in>>button)||button>0x3FFFF)return false;}profiles=values;return true;}
} // namespace controls
