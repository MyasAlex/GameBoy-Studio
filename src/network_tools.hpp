#pragma once
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif
#include "link_devices.hpp"
#include <map>
#include <memory>
namespace networking {
#ifdef _WIN32
using Socket=SOCKET;static constexpr Socket invalid=INVALID_SOCKET;
inline void release(Socket s){if(s!=invalid)closesocket(s);}inline int lastError(){return WSAGetLastError();}
inline bool wouldBlock(int e){return e==WSAEWOULDBLOCK||e==WSAEINPROGRESS;}
inline void nonblock(Socket s){u_long mode=1;ioctlsocket(s,FIONBIO,&mode);int enabled=1;setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(const char*)&enabled,sizeof(enabled));}
#else
using Socket=int;static constexpr Socket invalid=-1;
inline void release(Socket s){if(s!=invalid)::close(s);}inline int lastError(){return errno;}
inline bool wouldBlock(int e){return e==EWOULDBLOCK||e==EAGAIN||e==EINPROGRESS;}
inline void nonblock(Socket s){fcntl(s,F_SETFL,fcntl(s,F_GETFL,0)|O_NONBLOCK);int enabled=1;setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(const char*)&enabled,sizeof(enabled));}
#endif
inline void put64(std::vector<u8>& out,u64 value){for(int i=0;i<8;++i)out.push_back((u8)(value>>(8*i)));}
inline u64 get64(std::span<const u8> input){u64 value=0;for(int i=0;i<8;++i)value|=(u64)input[i]<<(8*i);return value;}
// Nonblocking TCP for framed handshakes, state synchronization and input-only
// play. Both PCs emulate both consoles; host owns player 1, guest owns player 2.
// Late inputs are corrected by replaying a bounded two-console rollback buffer.
class Cable {
    Socket listener=invalid,socket=invalid;bool server=false,connecting=false,helloSent=false,ready=false;
    GameBoy* first=nullptr,*second=nullptr;devices::LocalLink* link=nullptr;
    std::unique_ptr<devices::Rollback> rollback;
    std::vector<u8> incoming,outgoing;std::size_t sent=0;std::map<u64,u8> remote;
    u64 frame=0,receivedNext=0;std::int64_t confirmed=-1;u8 predicted=0;unsigned lead=3;bool corrected=false;
    std::string error,state="Off";
    static constexpr std::size_t maxPacket=16*1024*1024,maxBuffer=32*1024*1024;
    GameBoy* retryFirst=nullptr,*retrySecond=nullptr;devices::LocalLink* retryLink=nullptr;std::string retryAddress;unsigned retryPort=0,retryLead=3;bool retryEnabled=false,retryServer=false;
    std::chrono::steady_clock::time_point retryAt{},lastReceived=std::chrono::steady_clock::now(),lastHeartbeat=lastReceived;
    bool fail(std::string message,bool reconnect=false){error=std::move(message);const bool again=reconnect&&retryEnabled;close(false);retryEnabled=again;retryAt=std::chrono::steady_clock::now()+std::chrono::seconds(2);state=(again?"Reconnecting: ":"Disconnected: ")+error;return false;}
    void queue(u8 type,std::span<const u8> payload){if(outgoing.size()+payload.size()+5>maxBuffer){fail("Network send buffer limit exceeded.");return;}
        saves::put32(outgoing,(u32)payload.size()+1);outgoing.push_back(type);outgoing.insert(outgoing.end(),payload.begin(),payload.end());}
    void hello(){if(helloSent)return;std::vector<u8> payload;saves::put32(payload,10);saves::put32(payload,first->romHash());saves::put32(payload,second->romHash());
        payload.push_back(server?1:0);payload.push_back((u8)lead);queue(1,payload);helloSent=true;state="Synchronizing ROMs / states";}
    void begin(){link->connect(*first,*second);rollback=std::make_unique<devices::Rollback>(*first,*second,*link,12);ready=true;frame=0;receivedNext=0;confirmed=-1;predicted=0;remote.clear();state="Connected: input rollback";}
    bool packet(u8 type,std::span<const u8> payload){
        if(type==1){if(payload.size()!=14||saves::le32(payload,0)!=10||saves::le32(payload,4)!=first->romHash()||saves::le32(payload,8)!=second->romHash()||payload[12]==(server?1:0)||payload[13]>6)return fail("Peer protocol/ROMs/role do not match.");
            lead=std::min<unsigned>(lead,payload[13]);if(server){const auto a=saves::exportBess(*first),b=saves::exportBess(*second);if(a.empty()||b.empty())return fail("Could not capture network synchronization states.");
                std::vector<u8> states;saves::put32(states,(u32)a.size());saves::put32(states,(u32)b.size());states.insert(states.end(),a.begin(),a.end());states.insert(states.end(),b.begin(),b.end());queue(2,states);}return true;}
        if(type==2){if(server||ready||payload.size()<8)return fail("Unexpected network states.");const auto a=saves::le32(payload,0),b=saves::le32(payload,4);
            if(a>6*1024*1024||b>6*1024*1024||a+b+8!=payload.size())return fail("Invalid network state lengths.");const auto oldA=first->serializeState(),oldB=second->serializeState();std::string reason;
            if(!saves::importBess(*first,payload.subspan(8,a),&reason)||!saves::importBess(*second,payload.subspan(8+a,b),&reason)){
                first->deserializeState(oldA);second->deserializeState(oldB);return fail("Network state rejected: "+reason);}queue(3,{});begin();return true;}
        if(type==3){if(!server||ready||!payload.empty())return fail("Unexpected network acknowledgement.");begin();return true;}
        if(type==4){if(!ready||payload.size()!=9)return fail("Invalid network input packet.");const u64 number=get64(payload);
            if(number!=receivedNext++||number>frame+12||number+12<frame)return fail("Peer input exceeded rollback window.");
            if(number<frame){if(!rollback->correct(number,payload[8],server))return fail("Late input could not be restored.");corrected=true;}
            else remote[number]=payload[8];if((std::int64_t)number>confirmed){confirmed=(std::int64_t)number;predicted=payload[8];}return true;}
        if(type==5){if(!ready||payload.size()!=8)return fail("Invalid heartbeat.");return true;}
        return fail("Unknown network packet type.");
    }
public:
    Cable(){
#ifdef _WIN32
        WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)error="Winsock initialization failed.";
#endif
    }
    ~Cable(){close();
#ifdef _WIN32
        WSACleanup();
#endif
    }
    Cable(const Cable&)=delete;Cable& operator=(const Cable&)=delete;
    void close(bool clearError=true){release(socket);release(listener);socket=listener=invalid;ready=connecting=helloSent=false;rollback.reset();incoming.clear();outgoing.clear();sent=0;remote.clear();sentFrames.clear();corrected=false;
        if(link&&first&&second)link->disconnect();first=second=nullptr;link=nullptr;state="Off";if(clearError){error.clear();retryEnabled=false;retryFirst=retrySecond=nullptr;retryLink=nullptr;}}
    bool host(GameBoy& a,GameBoy& b,devices::LocalLink& cable,unsigned port=8765,unsigned prediction=3){close();if(port<1||port>65535||a.mem.rom.empty()||b.mem.rom.empty())return false;
        if(a.mem.mbc==7||b.mem.mbc==7||a.mem.mbc==252||b.mem.mbc==252)return fail("Live Camera/tilt inputs are not synchronized by this network protocol.");
        retryFirst=&a;retrySecond=&b;retryLink=&cable;retryPort=port;retryLead=prediction;retryServer=true;retryEnabled=true;lastReceived=lastHeartbeat=std::chrono::steady_clock::now();
        server=true;first=&a;second=&b;link=&cable;lead=std::min(prediction,6u);listener=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(listener==invalid)return fail("Could not create server socket.");
        int reuse=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_ANY);address.sin_port=htons((u16)port);
        if(bind(listener,(sockaddr*)&address,sizeof(address))||listen(listener,1))return fail("Could not listen on the selected TCP port.");nonblock(listener);state="Waiting for peer";return true;}
    bool join(GameBoy& a,GameBoy& b,devices::LocalLink& cable,const std::string& ipv4,unsigned port=8765,unsigned prediction=3){close();if(a.mem.mbc==7||b.mem.mbc==7||a.mem.mbc==252||b.mem.mbc==252)return fail("Live Camera/tilt inputs are not synchronized by this network protocol.");retryFirst=&a;retrySecond=&b;retryLink=&cable;retryPort=port;retryLead=prediction;retryAddress=ipv4;retryServer=false;retryEnabled=true;lastReceived=lastHeartbeat=std::chrono::steady_clock::now();server=false;first=&a;second=&b;link=&cable;lead=std::min(prediction,6u);
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons((u16)port);if(port<1||port>65535||inet_pton(AF_INET,ipv4.c_str(),&address.sin_addr)!=1)return fail("Enter an IPv4 address and TCP port.");
        socket=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(socket==invalid)return fail("Could not create client socket.");nonblock(socket);connecting=true;
        const int result=connect(socket,(sockaddr*)&address,sizeof(address));if(result&&!wouldBlock(lastError()))return fail("Could not connect to host.",true);if(!result){connecting=false;hello();}state="Connecting";return true;}
    bool poll(){
        const auto now=std::chrono::steady_clock::now();
        if(retryEnabled&&socket==invalid&&listener==invalid){if(now<retryAt)return true;auto* a=retryFirst;auto* b=retrySecond;auto* cable=retryLink;const auto ip=retryAddress;const auto port=retryPort,prediction=retryLead;const bool hostMode=retryServer;
            if(!a||!b||!cable)return false;return hostMode?host(*a,*b,*cable,port,prediction):join(*a,*b,*cable,ip,port,prediction);}
        if(socket!=invalid&&now-lastReceived>std::chrono::seconds(ready?30:10))return fail("Connection timed out",true);
        if(ready&&now-lastHeartbeat>=std::chrono::seconds(1)){std::vector<u8> heartbeat;put64(heartbeat,frame);queue(5,heartbeat);lastHeartbeat=now;}
        if(listener!=invalid&&socket==invalid){socket=accept(listener,nullptr,nullptr);if(socket==invalid){if(wouldBlock(lastError()))return true;return fail("Network accept failed.",true);}nonblock(socket);lastReceived=lastHeartbeat=now;hello();}
        if(socket==invalid)return listener!=invalid;
        if(connecting){fd_set writes;FD_ZERO(&writes);FD_SET(socket,&writes);timeval timeout{};if(select((int)socket+1,nullptr,&writes,nullptr,&timeout)<=0)return true;
            int result=0;
#ifdef _WIN32
            int length=sizeof(result);
#else
            socklen_t length=sizeof(result);
#endif
            if(getsockopt(socket,SOL_SOCKET,SO_ERROR,(char*)&result,&length)||result)return fail("Peer connection failed.",true);connecting=false;hello();}
        while(sent<outgoing.size()){
#ifdef _WIN32
            const int flags=0;
#else
            const int flags=MSG_NOSIGNAL;
#endif
            const int result=(int)send(socket,(const char*)outgoing.data()+sent,(int)std::min<std::size_t>(outgoing.size()-sent,65536),flags);
            if(result<0){if(wouldBlock(lastError()))break;return fail("Network send failed.",true);}if(!result)break;sent+=(std::size_t)result;
        }if(sent==outgoing.size()){outgoing.clear();sent=0;}
        std::array<u8,65536> buffer{};for(int attempt=0;attempt<4;++attempt){const int result=(int)recv(socket,(char*)buffer.data(),(int)buffer.size(),0);
            if(result<0){if(wouldBlock(lastError()))break;return fail("Network receive failed.",true);}if(!result)return fail("Peer disconnected; waiting to resynchronize.",true);
            lastReceived=now;
            if(incoming.size()+(std::size_t)result>maxBuffer)return fail("Network receive buffer limit exceeded.");incoming.insert(incoming.end(),buffer.begin(),buffer.begin()+result);}
        std::size_t offset=0;while(incoming.size()-offset>=4){const u32 length=saves::le32(incoming,offset);if(length<1||length>maxPacket)return fail("Network packet length limit exceeded.");if(length>incoming.size()-offset-4)break;
            // packet() can close/reset all buffers on failure; return immediately.
            const u8 type=incoming[offset+4];const std::vector<u8> payload(incoming.begin()+offset+5,incoming.begin()+offset+4+length);if(!packet(type,payload))return false;offset+=4+length;}
        if(offset)incoming.erase(incoming.begin(),incoming.begin()+offset);return true;
    }
    bool advance(u8 local){if(!ready||!rollback)return false;
        if(lead==0&&!remote.contains(frame)){// Send input once while waiting in lockstep mode.
            if(!sentFrames.contains(frame)){std::vector<u8> p;put64(p,frame);p.push_back(local);queue(4,p);sentFrames[frame]=local;}return false;}
        if(lead&&(std::int64_t)frame-confirmed>(std::int64_t)lead)return false;
        if(!sentFrames.contains(frame)){std::vector<u8> p;put64(p,frame);p.push_back(local);queue(4,p);sentFrames[frame]=local;}
        local=sentFrames[frame];sentFrames.erase(frame);const auto found=remote.find(frame);const u8 other=found!=remote.end()?found->second:predicted;
        if(found!=remote.end())remote.erase(found);rollback->advance(server?local:other,server?other:local);++frame;return true;}
    bool active()const{return listener!=invalid||socket!=invalid||retryEnabled;}bool connected()const{return ready;}bool isHost()const{return server;}
    bool consumeRollback(){const bool value=corrected;corrected=false;return value;}
    const std::string& status()const{return state;}const std::string& lastErrorText()const{return error;}u64 frameNumber()const{return frame;}
private:
    std::map<u64,u8> sentFrames;
};
} // namespace networking
