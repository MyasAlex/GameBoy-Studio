#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <xinput.h>
#include <string>
#include "gameboy.hpp"
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"winmm.lib")

static GameBoy gb;
static HWND hwnd=nullptr;
static HMENU appMenu=nullptr;
static bool running=true,paused=false,turbo=false,sound=true,fullscreen=false;
static bool focusPaused=false;
static WINDOWPLACEMENT placement{};
static std::array<bool,256> keyboard{};
using XInputGetStateFn=DWORD (WINAPI*)(DWORD,XINPUT_STATE*);
static XInputGetStateFn readGamepad=nullptr;
static HMODULE gamepadLibrary=nullptr;
static std::wstring notification;
static ULONGLONG messageUntil=0,lastBatterySave=0;
static std::chrono::steady_clock::time_point previous=std::chrono::steady_clock::now();
static double accumulator=0;
static int renderScale=4;

static constexpr UINT IDM_OPEN=100,IDM_RESET=101,IDM_PAUSE=102,IDM_SAVE=103,IDM_LOAD=104,
    IDM_SCREEN=105,IDM_SOUND=106,IDM_EXIT=107,IDM_FULLSCREEN=108,IDM_PAL0=200,IDM_SCALE2=300,IDM_TURBO=400;

struct AudioDevice {
    HWAVEOUT handle=nullptr;
    static constexpr int N=3,SAMPLES=512;
    std::array<std::array<std::int16_t,SAMPLES*2>,N> blocks{};
    std::array<WAVEHDR,N> headers{};
    bool open(){
        WAVEFORMATEX w{};w.wFormatTag=WAVE_FORMAT_PCM;w.nChannels=2;
        w.nSamplesPerSec=APU::rate;w.wBitsPerSample=16;w.nBlockAlign=4;
        w.nAvgBytesPerSec=w.nSamplesPerSec*w.nBlockAlign;
        if(waveOutOpen(&handle,WAVE_MAPPER,&w,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){handle=nullptr;return false;}
        for(int i=0;i<N;++i){
            headers[i].lpData=(LPSTR)blocks[i].data();
            headers[i].dwBufferLength=(DWORD)(blocks[i].size()*sizeof(std::int16_t));
            waveOutPrepareHeader(handle,&headers[i],sizeof(WAVEHDR));
            headers[i].dwFlags|=WHDR_DONE;
        }
        return true;
    }
    void pump(){
        if(!handle)return;
        const bool play=sound&&!paused&&!turbo&&!gb.mem.rom.empty();
        if(!play)gb.apu.clearQueue();
        for(int i=0;i<N;++i){
            if(headers[i].dwFlags&WHDR_DONE){
                if(play&&gb.apu.available()<SAMPLES)break;
                if(play)gb.apu.samples(blocks[i].data(),SAMPLES);
                else blocks[i].fill(0);
                waveOutWrite(handle,&headers[i],sizeof(WAVEHDR));
            }
        }
    }
    void flush(){if(handle)waveOutReset(handle);gb.apu.clearQueue();}
    void close(){
        if(!handle)return;
        waveOutReset(handle);
        for(auto& h:headers)waveOutUnprepareHeader(handle,&h,sizeof(WAVEHDR));
        waveOutClose(handle);handle=nullptr;
    }
} audio;

static void alert(const wchar_t* s){notification=s;messageUntil=GetTickCount64()+2300;}
static void updateTitle(){
    std::wstring title=L"Game Boy Studio 0.6 | DMG";
    if(!gb.mem.rom.empty()){
        title+=L" | ";
        auto stem=gb.mem.romPath.stem().wstring();title+=stem;
        if(gb.mem.cgbROM)title+=L" (CGB-compatible ROM: DMG mode only)";
    }else title+=L" | File > Open ROM...";
    if(paused)title+=L" | PAUSED";
    if(turbo)title+=L" | TURBO";
    if(messageUntil>GetTickCount64()){title+=L" | ";title+=notification;}
    SetWindowTextW(hwnd,title.c_str());
}
static bool openROM(const std::filesystem::path& p){
    if(!gb.load(p)){
        const std::wstring error(gb.mem.loadError.begin(),gb.mem.loadError.end());
        MessageBoxW(hwnd,error.c_str(),L"ROM error",MB_OK|MB_ICONERROR);
        return false;
    }
    audio.flush();paused=false;accumulator=0;previous=std::chrono::steady_clock::now();
    updateTitle();InvalidateRect(hwnd,nullptr,FALSE);return true;
}
static void showOpenDialog(){
    wchar_t name[32768]{};
    OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=hwnd;
    ofn.lpstrFile=name;ofn.nMaxFile=32768;
    ofn.lpstrFilter=L"Game Boy ROM (*.gb;*.gbc)\0*.gb;*.gbc\0All files (*.*)\0*.*\0";
    ofn.nFilterIndex=1;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    if(GetOpenFileNameW(&ofn))openROM(std::filesystem::path(name));
}
static bool screenshot(){
    if(gb.mem.rom.empty())return false;
    auto target=gb.mem.romPath.parent_path()/(gb.mem.romPath.stem().wstring()+L"_screen.bmp");
    std::ofstream f(target,std::ios::binary);if(!f)return false;
    BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};
    file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(info);
    file.bfSize=file.bfOffBits+(DWORD)gb.ppu.pixels.size()*4;
    info.biSize=sizeof(info);info.biWidth=GB_W;info.biHeight=-GB_H;info.biPlanes=1;
    info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=(DWORD)gb.ppu.pixels.size()*4;
    f.write((const char*)&file,sizeof(file));f.write((const char*)&info,sizeof(info));
    f.write((const char*)gb.ppu.pixels.data(),(std::streamsize)gb.ppu.pixels.size()*4);
    return (bool)f;
}
static void pollInput(){
    static XINPUT_STATE input{};static bool connected=false;
    static ULONGLONG lastPoll=0;
    const auto now=GetTickCount64();
    if(readGamepad&&now-lastPoll>=(connected?8:250)){
        lastPoll=now;input={};connected=false;
        for(DWORD i=0;i<4;++i)if(readGamepad(i,&input)==ERROR_SUCCESS){connected=true;break;}
    }
    const WORD buttons=connected?input.Gamepad.wButtons:0;
    const u8 before=gb.mem.joypadRead();
    const bool focused=GetForegroundWindow()==hwnd;
    auto pressed=[&](int key,WORD button){return keyboard[key]||(focused&&(buttons&button));};
    gb.mem.right=pressed(VK_RIGHT,XINPUT_GAMEPAD_DPAD_RIGHT)||(focused&&connected&&input.Gamepad.sThumbLX>12000);
    gb.mem.left=pressed(VK_LEFT,XINPUT_GAMEPAD_DPAD_LEFT)||(focused&&connected&&input.Gamepad.sThumbLX< -12000);
    gb.mem.up=pressed(VK_UP,XINPUT_GAMEPAD_DPAD_UP)||(focused&&connected&&input.Gamepad.sThumbLY>12000);
    gb.mem.down=pressed(VK_DOWN,XINPUT_GAMEPAD_DPAD_DOWN)||(focused&&connected&&input.Gamepad.sThumbLY< -12000);
    gb.mem.a=pressed('Z',XINPUT_GAMEPAD_A);gb.mem.b=pressed('X',XINPUT_GAMEPAD_B);
    gb.mem.select=pressed(VK_SHIFT,XINPUT_GAMEPAD_BACK);gb.mem.start=pressed(VK_RETURN,XINPUT_GAMEPAD_START);
    gb.mem.joyChanged(before);
}
static void toggleFullscreen(){
    if(!fullscreen){
        placement.length=sizeof(placement);GetWindowPlacement(hwnd,&placement);
        MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
        if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor)){
            SetWindowLongPtrW(hwnd,GWL_STYLE,WS_POPUP|WS_VISIBLE);
            SetMenu(hwnd,nullptr);
            SetWindowPos(hwnd,HWND_TOP,monitor.rcMonitor.left,monitor.rcMonitor.top,
                monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,SWP_FRAMECHANGED);
            fullscreen=true;
        }
    }else{
        SetWindowLongPtrW(hwnd,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_VISIBLE);
        SetMenu(hwnd,appMenu);SetWindowPlacement(hwnd,&placement);
        SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
        fullscreen=false;
    }
}
static void handleKey(WPARAM key,bool down){
    if(key<keyboard.size())keyboard[(std::size_t)key]=down;
    pollInput();
    if(key==VK_TAB){turbo=down;return;}
    if(!down)return;
    switch(key){
        case VK_F1:gb.setPalette(gb.currentPalette+1);alert(L"Palette changed");break;
        case VK_F5:alert(gb.saveState()?L"State saved":L"State save failed");break;
        case VK_F9:alert(gb.loadState()?L"State loaded":L"State load failed");audio.flush();break;
        case VK_F12:alert(screenshot()?L"Screenshot saved":L"Screenshot failed");break;
        case 'P':paused=!paused;audio.flush();break;
        case VK_F11:toggleFullscreen();break;
        case VK_ESCAPE:if(fullscreen)toggleFullscreen();break;
        default:break;
    }
    updateTitle();
}
static HMENU createMenus(){
    HMENU main=CreateMenu(), file=CreatePopupMenu(), settings=CreatePopupMenu(), palettes=CreatePopupMenu();
    AppendMenuW(file,MF_STRING,IDM_OPEN,L"Open ROM...\tCtrl+O");
    AppendMenuW(file,MF_STRING,IDM_RESET,L"Reset\tCtrl+R");
    AppendMenuW(file,MF_STRING,IDM_SAVE,L"Save state\tF5");
    AppendMenuW(file,MF_STRING,IDM_LOAD,L"Load state\tF9");
    AppendMenuW(file,MF_STRING,IDM_SCREEN,L"Screenshot (BMP)\tF12");
    AppendMenuW(file,MF_SEPARATOR,0,nullptr);
    AppendMenuW(file,MF_STRING,IDM_EXIT,L"Exit");
    AppendMenuW(settings,MF_STRING,IDM_PAUSE,L"Pause / Resume\tP");
    AppendMenuW(settings,MF_STRING,IDM_TURBO,L"Turbo while held\tTab");
    AppendMenuW(settings,MF_STRING|MF_CHECKED,IDM_SOUND,L"Sound on/off");
    AppendMenuW(settings,MF_STRING,IDM_FULLSCREEN,L"Fullscreen\tF11");
    for(int i=0;i<4;++i){
        static const wchar_t* names[]={L"Classic green",L"Grey",L"Blue",L"Amber"};
        AppendMenuW(palettes,MF_STRING,IDM_PAL0+i,names[i]);
    }
    AppendMenuW(settings,MF_POPUP,(UINT_PTR)palettes,L"Palette (F1)");
    for(int i=2;i<=6;i+=2){std::wstring caption=std::to_wstring(i)+L"x";AppendMenuW(settings,MF_STRING,IDM_SCALE2+i,caption.c_str());}
    AppendMenuW(main,MF_POPUP,(UINT_PTR)file,L"File");
    AppendMenuW(main,MF_POPUP,(UINT_PTR)settings,L"Options");
    return main;
}
struct BackBuffer {
    HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ original=nullptr;int width=0,height=0;
    void clear(){if(dc){SelectObject(dc,original);DeleteObject(bitmap);DeleteDC(dc);}dc=nullptr;bitmap=nullptr;original=nullptr;width=height=0;}
    bool resize(HDC target,int w,int h){
        if(dc&&w==width&&h==height)return true;
        clear();dc=CreateCompatibleDC(target);if(!dc)return false;
        bitmap=CreateCompatibleBitmap(target,w,h);if(!bitmap){DeleteDC(dc);dc=nullptr;return false;}
        original=SelectObject(dc,bitmap);width=w;height=h;return true;
    }
    ~BackBuffer(){clear();}
} backBuffer;
static void render(HWND w, HDC target){
    RECT rect{};GetClientRect(w,&rect);
    const int width=rect.right-rect.left,height=rect.bottom-rect.top;
    if(width<=0||height<=0)return;
    if(!backBuffer.resize(target,width,height))return;
    HDC dc=backBuffer.dc;
    const int scale=std::min(width/GB_W,height/GB_H);
    const int destW=(scale>=1?scale*GB_W:width),destH=(scale>=1?scale*GB_H:height);
    const int x=(width-destW)/2,y=(height-destH)/2;
    HBRUSH black=(HBRUSH)GetStockObject(BLACK_BRUSH);FillRect(dc,&rect,black);
    BITMAPINFO bmi{};bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth=GB_W;bmi.bmiHeader.biHeight=-GB_H;bmi.bmiHeader.biPlanes=1;
    bmi.bmiHeader.biBitCount=32;bmi.bmiHeader.biCompression=BI_RGB;
    SetStretchBltMode(dc,COLORONCOLOR);
    StretchDIBits(dc,x,y,destW,destH,0,0,GB_W,GB_H,gb.ppu.pixels.data(),&bmi,DIB_RGB_COLORS,SRCCOPY);
    BitBlt(target,0,0,width,height,dc,0,0,SRCCOPY);
}
static void tickApp(){
    pollInput();
    auto now=std::chrono::steady_clock::now();
    double dt=std::chrono::duration<double>(now-previous).count();previous=now;
    if(!paused&&!gb.mem.rom.empty()){
        const double frameTime=70224.0/4194304.0;
        accumulator=std::min(0.20,accumulator+std::min(dt,0.15)*(turbo?5.0:1.0));
        int frames=0;
        while(accumulator>=frameTime && frames<(turbo?12:4)){
            gb.runFrame();accumulator-=frameTime;++frames;
        }
        if(frames){InvalidateRect(hwnd,nullptr,FALSE);UpdateWindow(hwnd);}
    }else accumulator=0;
    audio.pump();
    if(GetTickCount64()-lastBatterySave>30000){gb.mem.saveBattery();lastBatterySave=GetTickCount64();}
    if(messageUntil && GetTickCount64()>messageUntil){messageUntil=0;updateTitle();}
}
static LRESULT CALLBACK wndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp){
    switch(msg){
        case WM_CREATE:{hwnd=w;DragAcceptFiles(w,TRUE);SetTimer(w,1,1,nullptr);return 0;}
        case WM_TIMER:tickApp();return 0;
        case WM_PAINT:{PAINTSTRUCT ps{};HDC h=BeginPaint(w,&ps);render(w,h);EndPaint(w,&ps);return 0;}
        case WM_ERASEBKGND:return 1;
        case WM_SIZE:InvalidateRect(w,nullptr,FALSE);return 0;
        case WM_KEYDOWN:case WM_SYSKEYDOWN:
            if(wp==VK_CONTROL)return 0;
            if(GetKeyState(VK_CONTROL)<0 && wp=='O'){showOpenDialog();return 0;}
            if(GetKeyState(VK_CONTROL)<0 && wp=='R'){gb.reset();audio.flush();return 0;}
            if(!(lp&(1L<<30)) || wp==VK_TAB)handleKey(wp,true);
            return 0;
        case WM_KEYUP:case WM_SYSKEYUP:handleKey(wp,false);return 0;
        case WM_KILLFOCUS:{keyboard.fill(false);pollInput();turbo=false;
            focusPaused=!paused;paused=true;audio.flush();updateTitle();return 0;}
        case WM_SETFOCUS:if(focusPaused){paused=false;focusPaused=false;previous=std::chrono::steady_clock::now();updateTitle();}return 0;
        case WM_DROPFILES:{HDROP files=(HDROP)wp;wchar_t path[32768]{};
            if(DragQueryFileW(files,0,path,32768))openROM(path);DragFinish(files);return 0;}
        case WM_COMMAND:{
            const UINT id=LOWORD(wp);
            if(id==IDM_OPEN)showOpenDialog();
            else if(id==IDM_RESET){gb.reset();audio.flush();}
            else if(id==IDM_PAUSE){paused=!paused;audio.flush();}
            else if(id==IDM_SAVE)alert(gb.saveState()?L"State saved":L"State save failed");
            else if(id==IDM_LOAD){alert(gb.loadState()?L"State loaded":L"State load failed");audio.flush();}
            else if(id==IDM_SCREEN)alert(screenshot()?L"Screenshot saved":L"Screenshot failed");
            else if(id==IDM_SOUND){sound=!sound;audio.flush();CheckMenuItem(appMenu,IDM_SOUND,MF_BYCOMMAND|(sound?MF_CHECKED:MF_UNCHECKED));}
            else if(id==IDM_FULLSCREEN)toggleFullscreen();
            else if(id==IDM_TURBO)turbo=!turbo;
            else if(id>=IDM_PAL0&&id<IDM_PAL0+4)gb.setPalette(id-IDM_PAL0);
            else if(id==IDM_SCALE2+2||id==IDM_SCALE2+4||id==IDM_SCALE2+6){renderScale=id-IDM_SCALE2;RECT r{0,0,GB_W*renderScale,GB_H*renderScale};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,TRUE);SetWindowPos(w,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER);}
            else if(id==IDM_EXIT)DestroyWindow(w);
            updateTitle();InvalidateRect(w,nullptr,FALSE);return 0;
        }
        case WM_CLOSE:DestroyWindow(w);return 0;
        case WM_DESTROY:running=false;KillTimer(w,1);gb.mem.saveBattery();audio.close();PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    SetProcessDPIAware();
    timeBeginPeriod(1);
    gamepadLibrary=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!gamepadLibrary)gamepadLibrary=LoadLibraryExW(L"xinput9_1_0.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(gamepadLibrary)readGamepad=reinterpret_cast<XInputGetStateFn>(GetProcAddress(gamepadLibrary,"XInputGetState"));
    WNDCLASSW cls{};cls.lpfnWndProc=wndProc;cls.hInstance=instance;
    cls.lpszClassName=L"GBStudioBetaWindow";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    if(!RegisterClassW(&cls)){timeEndPeriod(1);return 1;}
    RECT r{0,0,GB_W*renderScale,GB_H*renderScale};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,TRUE);
    appMenu=createMenus();
    hwnd=CreateWindowExW(0,cls.lpszClassName,L"Game Boy Studio",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,appMenu,instance,nullptr);
    if(!hwnd){timeEndPeriod(1);return 1;}
    audio.open();ShowWindow(hwnd,show);UpdateWindow(hwnd);
    int count=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);
    if(args && count>=2)openROM(args[1]);
    else {
        wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
        const auto demo=std::filesystem::path(executable).parent_path()/L"demo.gb";
        if(std::filesystem::exists(demo))openROM(demo);
    }
    if(args)LocalFree(args);
    updateTitle();
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    timeEndPeriod(1);
    if(gamepadLibrary)FreeLibrary(gamepadLibrary);
    if(appMenu)DestroyMenu(appMenu);
    return (int)msg.wParam;
}
