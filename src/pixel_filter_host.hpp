#pragma once
#ifdef _WIN32
#include <windows.h>
#include "gameboy.hpp"
// xBRZ is a separately licensed, general image filter process. The Studio
// frontend exchanges images over a bounded binary pipe protocol.
namespace media {
class PixelFilterHost {
    HANDLE process=nullptr,thread=nullptr,input=nullptr,output=nullptr;std::string error;
    bool start(){if(process)return true;wchar_t path[32768]{};const auto n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768)return false;
        const auto executable=std::filesystem::path(path).parent_path()/L"gb_pixel_filters.exe";SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE childInput=nullptr,childOutput=nullptr;
        if(!CreatePipe(&childInput,&input,&sa,65536)||!CreatePipe(&output,&childOutput,&sa,65536)){if(childInput)CloseHandle(childInput);if(childOutput)CloseHandle(childOutput);close();return false;}
        SetHandleInformation(input,HANDLE_FLAG_INHERIT,0);SetHandleInformation(output,HANDLE_FLAG_INHERIT,0);STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=childInput;si.hStdOutput=childOutput;si.hStdError=childOutput;
        PROCESS_INFORMATION pi{};const BOOL ok=CreateProcessW(executable.c_str(),nullptr,nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,executable.parent_path().c_str(),&si,&pi);
        CloseHandle(childInput);CloseHandle(childOutput);if(!ok){close();error="gb_pixel_filters.exe is missing or could not start.";return false;}process=pi.hProcess;thread=pi.hThread;return true;
    }
    bool send(std::span<const u8> bytes){std::size_t at=0;while(at<bytes.size()){DWORD n=0;if(!WriteFile(input,bytes.data()+at,(DWORD)std::min<std::size_t>(65536,bytes.size()-at),&n,nullptr)||!n)return false;at+=n;}return true;}
    bool receive(std::span<u8> bytes){std::size_t at=0;const auto deadline=GetTickCount64()+3000;while(at<bytes.size()){DWORD available=0;if(!PeekNamedPipe(output,nullptr,0,nullptr,&available,nullptr))return false;
            if(!available){if(GetTickCount64()>deadline||WaitForSingleObject(process,0)==WAIT_OBJECT_0)return false;Sleep(1);continue;}
            DWORD n=0;if(!ReadFile(output,bytes.data()+at,(DWORD)std::min<std::size_t>(bytes.size()-at,available),&n,nullptr)||!n)return false;at+=n;}return true;}
public:
    ~PixelFilterHost(){close();}
    void close(){if(input)CloseHandle(input);if(output)CloseHandle(output);input=output=nullptr;if(process){if(WaitForSingleObject(process,500)==WAIT_TIMEOUT)TerminateProcess(process,0);CloseHandle(process);}if(thread)CloseHandle(thread);process=thread=nullptr;}
    std::vector<u32> scale(std::span<const u32> pixels,unsigned width,unsigned height,unsigned factor){if(pixels.size()!=(std::size_t)width*height||!width||!height||width>1024||height>1024||factor<2||factor>6||!start())return {};
        const u32 header[4]={0x46504247,width,height,factor};if(!send({(const u8*)header,sizeof(header)})||!send({(const u8*)pixels.data(),pixels.size()*4})){error="Pixel filter input failed.";close();return {};}
        std::array<u32,2> reply{};if(!receive({(u8*)reply.data(),sizeof(reply)})||reply[0]!=0x46504247||reply[1]!=(u32)(pixels.size()*factor*factor)){error="Pixel filter response failed.";close();return {};}
        std::vector<u32> result(reply[1]);if(!receive({(u8*)result.data(),result.size()*4})){error="Pixel filter stopped during a frame.";close();return {};}return result;
    }
    const std::string& lastError()const{return error;}
};
} // namespace media
#endif
