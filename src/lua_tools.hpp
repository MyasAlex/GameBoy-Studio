#pragma once
#include "session.hpp"
extern "C" {
#include "../third_party/lua-src/lua.h"
#include "../third_party/lua-src/lauxlib.h"
#include "../third_party/lua-src/lualib.h"
}
namespace scripting {
class Lua {
    GameBoy& gb;lua_State* state=nullptr;std::size_t allocated=0;bool running=false;std::string error,output;
    static void* allocate(void* user,void* ptr,std::size_t oldSize,std::size_t newSize){auto& self=*(Lua*)user;if(!ptr)oldSize=0;
        if(!newSize){self.allocated-=oldSize;std::free(ptr);return nullptr;}
        if(newSize>oldSize&&newSize-oldSize>8*1024*1024-self.allocated)return nullptr;
        void* value=std::realloc(ptr,newSize);if(value)self.allocated=self.allocated-oldSize+newSize;return value;}
    static Lua& self(lua_State* s){return **(Lua**)lua_getextraspace(s);}
    static void limit(lua_State* s,lua_Debug*){luaL_error(s,"Script instruction budget exceeded.");}
    static int read(lua_State* s){const auto address=luaL_checkinteger(s,1);if(address<0||address>65535)return luaL_error(s,"Address out of range.");lua_pushinteger(s,self(s).gb.mem.read((u16)address));return 1;}
    static int write(lua_State* s){const auto address=luaL_checkinteger(s,1),value=luaL_checkinteger(s,2);
        if(address<0x8000||address>65535||value<0||value>255)return luaL_error(s,"Write needs a RAM/I/O address and byte value.");self(s).gb.mem.write((u16)address,(u8)value);return 0;}
    static int joypad(lua_State* s){const auto value=luaL_checkinteger(s,1);if(value<0||value>255)return luaL_error(s,"Joypad must be a byte.");InputMovie::setJoypad(self(s).gb.mem,(u8)value);return 0;}
    static int frame(lua_State* s){lua_pushinteger(s,(lua_Integer)self(s).gb.ppu.frameNumber);return 1;}
    static int registers(lua_State* s){auto& cpu=self(s).gb.cpu;lua_createtable(s,0,6);for(auto [name,value]:std::array<std::pair<const char*,u16>,6>{{{"AF",cpu.AF()},{"BC",cpu.BC()},{"DE",cpu.DE()},{"HL",cpu.HL()},{"PC",cpu.PC},{"SP",cpu.SP}}}){lua_pushinteger(s,value);lua_setfield(s,-2,name);}return 1;}
    static int print(lua_State* s){auto& text=self(s).output;for(int i=1;i<=lua_gettop(s);++i){std::size_t n=0;const char* value=luaL_tolstring(s,i,&n);
        if(text.size()+n+2<65536){if(i>1)text+='\t';text.append(value,n);}lua_pop(s,1);}if(text.size()<65536)text+='\n';return 0;}
    bool call(int arguments=0){lua_sethook(state,limit,LUA_MASKCOUNT,100000);const int code=lua_pcall(state,arguments,0,0);lua_sethook(state,nullptr,0,0);
        if(code!=LUA_OK){const char* message=lua_tostring(state,-1);error=message?message:"Lua error.";lua_pop(state,1);running=false;return false;}return true;}
public:
    explicit Lua(GameBoy& machine):gb(machine){}~Lua(){stop();}Lua(const Lua&)=delete;Lua& operator=(const Lua&)=delete;
    bool load(const std::filesystem::path& path){stop();const auto bytes=Memory::loadBytes(path,1024*1024);if(bytes.empty()){error="Empty/oversized Lua script.";return false;}
        state=lua_newstate(allocate,this);if(!state){error="Lua allocation failed.";return false;}*(Lua**)lua_getextraspace(state)=this;
        // Base without filesystem/eval, plus table/string/math/utf8. No os, io,
        // package, debug, native modules, network, or arbitrary process API.
        for(auto [name,open]:std::array<std::pair<const char*,lua_CFunction>,5>{{{"_G",luaopen_base},{LUA_TABLIBNAME,luaopen_table},{LUA_STRLIBNAME,luaopen_string},{LUA_MATHLIBNAME,luaopen_math},{LUA_UTF8LIBNAME,luaopen_utf8}}}){luaL_requiref(state,name,open,1);lua_pop(state,1);}
        for(const char* name:{"dofile","loadfile","load","collectgarbage","pcall","xpcall","setmetatable"}){lua_pushnil(state);lua_setglobal(state,name);}lua_pushcfunction(state,print);lua_setglobal(state,"print");
        lua_createtable(state,0,5);for(auto [name,fn]:std::array<std::pair<const char*,lua_CFunction>,5>{{{"read",read},{"write",write},{"joypad",joypad},{"frame",frame},{"registers",registers}}}){lua_pushcfunction(state,fn);lua_setfield(state,-2,name);}lua_setglobal(state,"gb");
        if(luaL_loadbuffer(state,(const char*)bytes.data(),bytes.size(),"script")!=LUA_OK){error=lua_tostring(state,-1);lua_pop(state,1);return false;}
        error.clear();output.clear();running=true;return call();}
    bool onFrame(){if(!state||!running)return false;lua_getglobal(state,"on_frame");if(lua_isnil(state,-1)){lua_pop(state,1);return true;}
        if(!lua_isfunction(state,-1)){lua_pop(state,1);error="on_frame must be a function.";running=false;return false;}return call();}
    void stop(){if(state)lua_close(state);state=nullptr;allocated=0;running=false;}
    bool active()const{return running;}
    const std::string& lastError()const{return error;}const std::string& log()const{return output;}
};
// Each plugin has its own bounded Lua VM. A failure disables only that plugin.
class Plugins {
    struct Entry {std::filesystem::path path;std::unique_ptr<Lua> script;};
    GameBoy& gb;std::vector<Entry> entries;std::string error,output;
public:
    explicit Plugins(GameBoy& machine):gb(machine){}
    bool toggle(const std::filesystem::path& path){
        for(auto it=entries.begin();it!=entries.end();++it)if(it->path==path){entries.erase(it);return true;}
        if(entries.size()>=8){error="At most eight plugins can run together.";return false;}
        auto script=std::make_unique<Lua>(gb);if(!script->load(path)){error=script->lastError();return false;}
        entries.push_back({path,std::move(script)});return true;
    }
    bool contains(const std::filesystem::path& path)const {return std::any_of(entries.begin(),entries.end(),[&](const Entry& e){return e.path==path&&e.script->active();});}
    bool load(const std::filesystem::path& path){stop();return toggle(path);}
    bool onFrame(){bool ok=true;error.clear();for(auto& entry:entries)if(entry.script->active()&&!entry.script->onFrame()){error+=entry.path.filename().string()+": "+entry.script->lastError()+"\n";ok=false;}return ok;}
    void stop(){entries.clear();}
    bool active()const {return std::any_of(entries.begin(),entries.end(),[](const Entry& e){return e.script->active();});}
    const std::string& lastError()const{return error;}
    const std::string& log(){output.clear();for(const auto& entry:entries){if(output.size()>65536)break;output+=entry.path.filename().string()+":\n"+entry.script->log();}return output;}
};
} // namespace scripting
