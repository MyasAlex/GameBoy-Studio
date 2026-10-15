#pragma once
#ifdef _WIN32
#include <windows.h>
#include "studio_tools.hpp"
namespace presence {
// Local Discord IPC protocol. The user supplies their application client ID.
class Discord {
    HANDLE pipe=INVALID_HANDLE_VALUE;std::string clientId;ULONGLONG last=0;u32 nonce=0;std::vector<u8> incoming;
    bool packet(u32 opcode,std::string_view json){if(pipe==INVALID_HANDLE_VALUE||json.size()>65536)return false;const u32 header[]={opcode,(u32)json.size()};DWORD n=0;
        if(!WriteFile(pipe,header,sizeof(header),&n,nullptr)||n!=sizeof(header)||!WriteFile(pipe,json.data(),(DWORD)json.size(),&n,nullptr)||n!=json.size()){disconnect();return false;}return true;}
    void connect(){if(pipe!=INVALID_HANDLE_VALUE||clientId.empty())return;for(int i=0;i<10;++i){const auto path=L"\\\\.\\pipe\\discord-ipc-"+std::to_wstring(i);pipe=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(pipe!=INVALID_HANDLE_VALUE)break;}
        if(pipe!=INVALID_HANDLE_VALUE)packet(0,"{\"v\":1,\"client_id\":"+studio::jsonString(clientId)+"}");}
    void receive(){if(pipe==INVALID_HANDLE_VALUE)return;DWORD available=0;std::array<u8,4096> input{};
        while(PeekNamedPipe(pipe,nullptr,0,nullptr,&available,nullptr)&&available){DWORD n=0;if(!ReadFile(pipe,input.data(),std::min<DWORD>(available,(DWORD)input.size()),&n,nullptr)||!n){disconnect();return;}if(incoming.size()+n>131088){disconnect();return;}incoming.insert(incoming.end(),input.begin(),input.begin()+n);}
        while(incoming.size()>=8){const auto read32=[&](unsigned at){return (u32)incoming[at]|((u32)incoming[at+1]<<8)|((u32)incoming[at+2]<<16)|((u32)incoming[at+3]<<24);};const auto opcode=read32(0),size=read32(4);if(opcode>4||size>65536){disconnect();return;}if(incoming.size()<8+size)break;
            if(opcode==2){disconnect();return;}if(opcode==3&&!packet(4,std::string_view((const char*)incoming.data()+8,size)))return;incoming.erase(incoming.begin(),incoming.begin()+8+size);}
    }
public:
    ~Discord(){disconnect();}void disconnect(){if(pipe!=INVALID_HANDLE_VALUE)CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;incoming.clear();}
    bool configure(std::string id){if(id.size()<8||id.size()>24||!std::all_of(id.begin(),id.end(),[](char c){return c>='0'&&c<='9';}))return false;disconnect();clientId=std::move(id);last=0;return true;}
    void disable(){disconnect();clientId.clear();}bool enabled()const{return !clientId.empty();}const std::string& applicationId()const{return clientId;}
    void update(const GameBoy& g,bool paused){receive();const auto now=GetTickCount64();if(now-last<15000||clientId.empty())return;last=now;connect();if(pipe==INVALID_HANDLE_VALUE)return;
        std::ostringstream out;out<<"{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":"<<GetCurrentProcessId()<<",\"activity\":{\"details\":"<<studio::jsonString(g.mem.rom.empty()?"GameBoy Studio":g.mem.gameTitle)<<",\"state\":"<<studio::jsonString(paused?"Paused":g.mem.cgbMode?"Game Boy Color":"Game Boy")<<"}},\"nonce\":"<<studio::jsonString(std::to_string(++nonce))<<"}";packet(1,out.str());
    }
};
} // namespace presence
#endif
