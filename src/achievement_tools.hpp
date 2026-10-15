#pragma once
#include "gameboy.hpp"
#include "../third_party/rcheevos/include/rc_client.h"
#include "../third_party/rcheevos/include/rc_consoles.h"
#include <deque>
#include <mutex>
#include <optional>
#include <atomic>
#include <condition_variable>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace achievements {
struct Request {std::string url,post,contentType,userAgent;};
struct Response {std::string body;int status=RC_API_SERVER_RESPONSE_CLIENT_ERROR;};
using Transport=std::function<Response(const Request&)>;
#ifdef _WIN32
inline Response https(const Request& input){
    auto wide=[](std::string_view value){if(value.empty())return std::wstring{};const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),(int)value.size(),nullptr,0);std::wstring result((std::size_t)n,0);if(n)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),(int)value.size(),result.data(),n);return result;};
    struct Handle {HINTERNET h;~Handle(){if(h)WinHttpCloseHandle(h);}operator HINTERNET()const{return h;}};
    const auto url=wide(input.url);URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=(DWORD)-1;
    if(!WinHttpCrackUrl(url.c_str(),(DWORD)url.size(),0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS)return {};
    const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);if(host!=L"retroachievements.org"&&host!=L"retroachievements.org:443")return {};
    std::wstring path(parts.lpszUrlPath,parts.dwUrlPathLength);if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
    Handle session{WinHttpOpen(wide(input.userAgent.empty()?"GameBoyStudio/1.0.0 (Windows)":input.userAgent).c_str(),WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.h)return {};WinHttpSetTimeouts(session,3000,3000,5000,5000);
    Handle connection{WinHttpConnect(session,host.c_str(),parts.nPort,0)};if(!connection.h)return {};
    Handle request{WinHttpOpenRequest(connection,input.post.empty()?L"GET":L"POST",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!request.h)return {};DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    const auto header=wide("Content-Type: "+(input.contentType.empty()?"application/x-www-form-urlencoded":input.contentType)+"\r\n");
    if(!WinHttpSendRequest(request,header.c_str(),(DWORD)header.size(),input.post.empty()?WINHTTP_NO_REQUEST_DATA:(void*)input.post.data(),(DWORD)input.post.size(),(DWORD)input.post.size(),0)||!WinHttpReceiveResponse(request,nullptr))return {};
    DWORD status=0,size=sizeof(status);if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))return {};
    Response output;output.status=(int)status;std::array<char,8192> buffer{};DWORD n=0;
    do{if(!WinHttpReadData(request,buffer.data(),(DWORD)buffer.size(),&n)||output.body.size()+n>4*1024*1024)return {};output.body.append(buffer.data(),n);}while(n);return output;
}
#endif
class Client {
    struct Job {Request request;rc_client_server_callback_t callback;void* data;};
    struct Completed {Response response;rc_client_server_callback_t callback;void* data;};
    GameBoy& g;rc_client_t* client=nullptr;Transport transport;std::mutex mutex;std::deque<Job> jobs;std::deque<Completed> completed;
    std::condition_variable condition;std::jthread worker;bool stopping=false,resetPending=false,gamePending=false;
    std::deque<std::string> notices;std::string userName;
    static Client& self(const rc_client_t* c){return *static_cast<Client*>(rc_client_get_userdata(c));}
    static uint32_t RC_CCONV memory(uint32_t address,uint8_t* buffer,uint32_t count,rc_client_t* client){auto& g=self(client).g;uint32_t n=0;
        for(;n<count;++n){const u64 at=(u64)address+n;if(at>0x33FFF)break;
            if(at>=0x16000){const auto offset=(std::size_t)(at-0x16000+8192);if(offset>=g.mem.eram.size())break;buffer[n]=g.mem.eram[offset];}
            else if(at>=0x10000){if(!g.mem.isCGBHardware())break;buffer[n]=g.mem.wram[(std::size_t)(at-0x10000+8192)];}
            else if(at>=0xA000&&at<=0xBFFF){const auto offset=(std::size_t)(at-0xA000);if(offset>=g.mem.eram.size())break;buffer[n]=g.mem.eram[offset];}
            else if(at>=0xE000&&at<=0xFDFF)buffer[n]=g.mem.wram[(std::size_t)(at-0xE000)];
            else if(at>=0x8000&&at<=0x9FFF)buffer[n]=g.mem.vram[(std::size_t)(at-0x8000)];
            else if(at>=0xFE00&&at<=0xFE9F)buffer[n]=g.mem.oam[(std::size_t)(at-0xFE00)];
            else if(at>=0xC000&&at<=0xDFFF)buffer[n]=g.mem.wram[(std::size_t)(at-0xC000)];
            else buffer[n]=g.mem.read((u16)at);
        }return n;
    }
    static void RC_CCONV server(const rc_api_request_t* request,rc_client_server_callback_t callback,void* data,rc_client_t* client){auto& owner=self(client);
        Job job{{request->url?request->url:"",request->post_data?request->post_data:"",request->content_type?request->content_type:"","GameBoyStudio/1.0.0 (Windows) "},callback,data};char agent[128]{};rc_client_get_user_agent_clause(client,agent,sizeof(agent));job.request.userAgent+=agent;
        bool rejected=false;{std::lock_guard lock(owner.mutex);if(owner.jobs.size()>=64||owner.stopping)rejected=true;else owner.jobs.push_back(std::move(job));}owner.condition.notify_one();
        if(rejected){const rc_api_server_response_t failed{"",0,RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR};callback(&failed,data);}
    }
    static void RC_CCONV loaded(int result,const char* error,rc_client_t* c,void*){auto& owner=self(c);owner.gamePending=false;
        if(result!=RC_OK)owner.notices.push_back(std::string("RetroAchievements: ")+(error?error:"game loading failed"));
        else {const auto* game=rc_client_get_game_info(c);owner.notices.push_back(std::string("RetroAchievements: ")+(game?game->title:"no achievement set"));if(!rc_client_has_achievements(c))rc_client_set_hardcore_enabled(c,0);}
    }
    static void RC_CCONV loggedIn(int result,const char* error,rc_client_t* c,void*){auto& owner=self(c);if(result!=RC_OK)owner.notices.push_back(std::string("RetroAchievements login: ")+(error?error:"failed"));else {owner.notices.push_back("RetroAchievements signed in");owner.loadGame();}}
    static void RC_CCONV event(const rc_client_event_t* event,rc_client_t* c){auto& owner=self(c);
        switch(event->type){case RC_CLIENT_EVENT_RESET:owner.resetPending=true;break;
        case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:if(event->achievement)owner.notices.push_back(std::string("Achievement: ")+event->achievement->title+" (+"+std::to_string(event->achievement->points)+")");break;
        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:if(event->achievement)owner.notices.push_back(std::string(event->achievement->title)+" "+event->achievement->measured_progress);break;
        case RC_CLIENT_EVENT_GAME_COMPLETED:owner.notices.push_back("Achievement set completed");break;
        case RC_CLIENT_EVENT_DISCONNECTED:owner.notices.push_back("RetroAchievements offline; unlocks queued for retry");break;
        case RC_CLIENT_EVENT_RECONNECTED:owner.notices.push_back("RetroAchievements reconnected");break;
        case RC_CLIENT_EVENT_SERVER_ERROR:if(event->server_error)owner.notices.push_back(std::string("RetroAchievements: ")+event->server_error->error_message);break;
        default:break;}while(owner.notices.size()>32)owner.notices.pop_front();
    }
public:
    explicit Client(GameBoy& game,Transport http={}):g(game),transport(std::move(http)){
#ifdef _WIN32
        if(!transport)transport=https;
#endif
        client=rc_client_create(memory,server);rc_client_set_userdata(client,this);rc_client_set_event_handler(client,event);rc_client_set_allow_background_memory_reads(client,0);
        worker=std::jthread([this](std::stop_token){for(;;){Job job;{std::unique_lock lock(mutex);condition.wait(lock,[this](){return stopping||!jobs.empty();});if(stopping)return;job=std::move(jobs.front());jobs.pop_front();}
            Response response;try{if(transport)response=transport(job.request);}catch(...){response={};}
            {std::lock_guard lock(mutex);completed.push_back({std::move(response),job.callback,job.data});}}
        });
    }
    ~Client(){{std::lock_guard lock(mutex);stopping=true;}condition.notify_all();if(worker.joinable())worker.join();
        std::deque<Job> pending;std::deque<Completed> done;{std::lock_guard lock(mutex);pending.swap(jobs);done.swap(completed);}const rc_api_server_response_t failure{"",0,RC_API_SERVER_RESPONSE_CLIENT_ERROR};
        for(auto& item:pending)item.callback(&failure,item.data);for(auto& item:done)item.callback(&failure,item.data);rc_client_destroy(client);}
    Client(const Client&)=delete;Client& operator=(const Client&)=delete;
    void login(std::string user,std::string token){userName=std::move(user);rc_client_begin_login_with_token(client,userName.c_str(),token.c_str(),loggedIn,nullptr);std::fill(token.begin(),token.end(),'\0');}
    void logout(){rc_client_unload_game(client);rc_client_logout(client);userName.clear();rc_client_set_hardcore_enabled(client,0);}
    bool signedIn()const{return rc_client_get_user_info(client)!=nullptr;}
    void loadGame(){rc_client_unload_game(client);if(!signedIn()||g.mem.rom.empty())return;gamePending=true;if(rc_client_get_hardcore_enabled(client))resetPending=true;
        rc_client_begin_identify_and_load_game(client,g.mem.cgbROM?RC_CONSOLE_GAMEBOY_COLOR:RC_CONSOLE_GAMEBOY,nullptr,g.mem.rom.data(),g.mem.rom.size(),loaded,nullptr);}
    void poll(){std::deque<Completed> ready;{std::lock_guard lock(mutex);ready.swap(completed);}for(auto& item:ready){const rc_api_server_response_t response{item.response.body.data(),item.response.body.size(),item.response.status};item.callback(&response,item.data);}rc_client_idle(client);}
    void frame(){if(g.backendRefresh)g.backendRefresh();rc_client_do_frame(client);}
    bool hardcore()const{return signedIn()&&(gamePending||rc_client_get_game_info(client))&&rc_client_get_hardcore_enabled(client)!=0;}
    void setHardcore(bool on){rc_client_set_hardcore_enabled(client,on?1:0);}
    bool takeReset(){const bool result=resetPending;resetPending=false;return result;}
    void reset(){rc_client_reset(client);}
    bool canPause(){return !hardcore()||rc_client_can_pause(client,nullptr)!=0;}
    std::optional<std::string> notice(){if(notices.empty())return {};auto value=std::move(notices.front());notices.pop_front();return value;}
    std::vector<std::string> rows(){std::vector<std::string> result;auto* list=rc_client_create_achievement_list(client,RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED,RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
        if(list){for(unsigned b=0;b<list->num_buckets;++b)for(unsigned n=0;n<list->buckets[b].num_achievements;++n){const auto* item=list->buckets[b].achievements[n];result.push_back(std::string(item->state==RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED?"✓ ":"○ ")+item->title+" | "+item->description+" | "+item->measured_progress+" | "+std::to_string(item->points));}rc_client_destroy_achievement_list(list);}return result;}
    std::vector<u8> progress(){if(!rc_client_get_game_info(client))return {};const auto n=rc_client_progress_size(client);if(n>2*1024*1024)return {};std::vector<u8> bytes(n);if(rc_client_serialize_progress_sized(client,bytes.data(),n)!=RC_OK)return {};return bytes;}
    bool restore(std::span<const u8> bytes){if(!rc_client_get_game_info(client))return true;return rc_client_deserialize_progress_sized(client,bytes.empty()?nullptr:bytes.data(),bytes.size())==RC_OK;}
};
} // namespace achievements
