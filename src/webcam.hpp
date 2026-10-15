#pragma once
#ifdef _WIN32
#include <windows.h>
#include <vfw.h>
#include <mutex>
#include <array>
#ifdef _MSC_VER
#pragma comment(lib,"vfw32.lib")
#endif
// VfW sources are opt-in. The callback only publishes a grayscale frame; it
// never accesses CPU/PPU memory from the device callback thread.
class Webcam {
    HWND capture=nullptr;std::mutex mutex;std::array<u8,128*112> grayscale{};
    int width=0,height=0;bool topDown=false;std::string error;
    static LRESULT CALLBACK frame(HWND window,LPVIDEOHDR header){auto* camera=(Webcam*)capGetUserData(window);if(!camera||!header||!header->lpData)return FALSE;
        const int stride=(camera->width*3+3)&~3;if(stride<=0||header->dwBytesUsed<(DWORD)(stride*camera->height))return FALSE;
        std::array<u8,128*112> result{};for(int y=0;y<112;++y)for(int x=0;x<128;++x){int sy=y*camera->height/112;if(!camera->topDown)sy=camera->height-1-sy;
            const auto* pixel=(const unsigned char*)header->lpData+sy*stride+(x*camera->width/128)*3;result[y*128+x]=(u8)((77*pixel[2]+150*pixel[1]+29*pixel[0])>>8);}
        std::lock_guard lock(camera->mutex);camera->grayscale=result;return TRUE;}
public:
    Webcam(){grayscale.fill(128);}~Webcam(){close();}
    bool open(HWND owner){close();capture=capCreateCaptureWindowW(L"GameBoy Camera",WS_CHILD,0,0,1,1,owner,0);if(!capture){error="Could not create webcam capture window.";return false;}
        if(!capDriverConnect(capture,0)){error="No VfW-compatible webcam driver found.";close();return false;}
        BITMAPINFOHEADER format{};format.biSize=sizeof(format);format.biWidth=640;format.biHeight=480;format.biPlanes=1;format.biBitCount=24;format.biCompression=BI_RGB;format.biSizeImage=640*480*3;
        if(!capSetVideoFormat(capture,&format,sizeof(format))){error="Webcam cannot provide RGB24 video.";close();return false;}
        BITMAPINFOHEADER actual{};if(!capGetVideoFormat(capture,&actual,sizeof(actual))||actual.biBitCount!=24||actual.biCompression!=BI_RGB||actual.biWidth<=0||actual.biWidth>4096||actual.biHeight==0||std::abs(actual.biHeight)>4096){error="Unsupported webcam pixel format.";close();return false;}
        width=actual.biWidth;topDown=actual.biHeight<0;height=std::abs(actual.biHeight);capSetUserData(capture,(LONG_PTR)this);capSetCallbackOnFrame(capture,frame);error.clear();return true;}
    void close(){if(capture){capSetCallbackOnFrame(capture,nullptr);capSetUserData(capture,0);capDriverDisconnect(capture);DestroyWindow(capture);capture=nullptr;}}
    std::array<u8,128*112> image(){if(capture)capGrabFrameNoStop(capture);std::lock_guard lock(mutex);return grayscale;}
    void setImage(std::array<u8,128*112> pixels){close();std::lock_guard lock(mutex);grayscale=pixels;}
    const std::string& lastError()const{return error;}
};
#endif
