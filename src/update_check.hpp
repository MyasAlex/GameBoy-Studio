#pragma once
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <array>
#include <string>
#include <charconv>
#pragma comment(lib,"winhttp.lib")
namespace updates {
struct Result {std::string tag,error;bool newer=false;};
struct Internet {HINTERNET handle=nullptr;explicit Internet(HINTERNET value):handle(value){}~Internet(){if(handle)WinHttpCloseHandle(handle);}operator HINTERNET()const{return handle;}};
inline std::array<unsigned,3> version(std::string_view text){if(!text.empty()&&(text.front()=='v'||text.front()=='V'))text.remove_prefix(1);std::array<unsigned,3> values{};
    for(auto& value:values){auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);if(error!=std::errc{})break;text.remove_prefix((std::size_t)(end-text.data()));if(text.empty()||text.front()!='.')break;text.remove_prefix(1);}return values;}
inline Result check(){
    Internet session(WinHttpOpen(L"GameBoy-Studio/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session)return {{},"Could not start an update check.",false};WinHttpSetTimeouts(session,2000,2000,2000,2000);
    Internet connection(WinHttpConnect(session,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0));
    Internet request(WinHttpOpenRequest(connection,L"GET",L"/repos/MyasAlex/GameBoy-Studio/releases/latest",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request||!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr))return {{},"Update server is unavailable.",false};
    DWORD status=0,length=sizeof(status);WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX);
    if(status!=200)return {{},"No public release is available, or the update server rejected the request.",false};
    std::string body;std::array<char,4096> buffer{};DWORD count=0;
    do{if(!WinHttpReadData(request,buffer.data(),(DWORD)buffer.size(),&count))return {{},"Update response could not be read.",false};if(body.size()+count>32768)return {{},"Update response exceeds the size limit.",false};body.append(buffer.data(),count);}while(count);
    auto at=body.find("\"tag_name\"");if(at==std::string::npos)return {{},"Release response has no version tag.",false};at=body.find(':',at+10);if(at==std::string::npos)return {{},"Invalid release response.",false};at=body.find('"',at+1);if(at==std::string::npos)return {{},"Invalid version tag.",false};const auto end=body.find('"',at+1);
    if(end==std::string::npos||end-at>80)return {{},"Invalid version tag length.",false};const auto tag=body.substr(at+1,end-at-1);return {tag,{},version(tag)>version("1.0")};
}
} // namespace updates
#endif
