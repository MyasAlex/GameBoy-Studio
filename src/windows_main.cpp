#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "network_tools.hpp"
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <xinput.h>
#include <windowsx.h>
#include <string>
#include "gameboy.hpp"
#include "session.hpp"
#include "studio_tools.hpp"
#include "cartridge_tools.hpp"
#include "host_audio.hpp"
#include "cheat_engine.hpp"
#include "archive_loader.hpp"
#include "lua_tools.hpp"
#include "library_tools.hpp"
#include "d3d_renderer.hpp"
#include "webcam.hpp"
#include "update_check.hpp"
#include "accuracy_core.hpp"
#include "achievement_tools.hpp"
#include "image_loader.hpp"
#include "pixel_filter_host.hpp"
#include "discord_presence.hpp"
#include "input_profiles.hpp"
#include "frame_pacing.hpp"
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"winmm.lib")

static GameBoy gb;
static RewindBuffer rewindBuffer;
static InputMovie movie;
static studio::Debugger debugger(gb);
static HostAudio audio;
static achievements::Client achievementClient(gb);
static GameBoy gbSecond;
static devices::LocalLink link;
static devices::Printer printer;
static networking::Cable network;
static cheats::Engine cheatEngine(gb.mem);
static cheats::Search ramSearch;
static scripting::Plugins lua(gb);
static collection::Library library;
static collection::Timeline timeline;
static media::AviWriter video;
static GpuRenderer gpu;
static media::PixelFilterHost pixelFilters;
static presence::Discord discordPresence;
static Webcam webcam;
static HWND inspectorWindow=nullptr,linkWindow=nullptr;
static int inspectorMode=0,scaleFilter=0,colorCorrection=0,eqPreset=0,captureHotkey=-1;
static bool gpuEnabled=false,virtualCase=false,heldTurbo=false,heldRewind=false;
static std::array<int,15> hotkeys={'O'|256,'R'|256,'P',VK_F5,VK_F9,VK_F6,VK_F12,VK_F11,'D'|256,VK_TAB,VK_BACK,'S'|256|512,'O'|256|512,VK_F1,VK_F7};
using XInputSetStateFn=DWORD (WINAPI*)(DWORD,XINPUT_VIBRATION*);
static XInputSetStateFn sendRumble=nullptr;
static DWORD primaryPadIndex=0;
static std::array<controls::GamepadProfile,2> padProfiles;
static std::array<RECT,8> virtualButtons{};static u8 virtualInput=0;
static HWND hwnd=nullptr,studioWindow=nullptr,studioText=nullptr,studioAddress=nullptr;
static HINSTANCE appInstance=nullptr;
static HMENU appMenu=nullptr;
static bool paused=false,turbo=false,sound=true,fullscreen=false,focusPaused=false;
static bool russian=false,integerScaling=true,ghosting=false,resumeSession=true,debuggerRunning=false;
static bool debuggerSkipBreakpoint=false;
static bool lightTheme=false,compareDisplay=false;
static bool highResolutionTimer=false,selfTest=false,uiSelfTest=false;
static std::filesystem::path uiTestReport;
static bool timingTest=false,timingAudio=false,timingAudioOpened=false,timingFinished=false;
static double timingDuration=5.0;
static std::filesystem::path timingReport;
static std::chrono::steady_clock::time_point timingStart;
static u64 timingStartCycles=0,timingStartFrames=0;
static int timingMaxBatch=0;
static int selfTestExitCode=0;
static WINDOWPLACEMENT placement{};
static std::array<bool,256> keyboard{};
static constexpr std::array<int,8> defaultKeys={VK_RIGHT,VK_LEFT,VK_UP,VK_DOWN,'Z','X',VK_SHIFT,VK_RETURN};
static std::array<int,8> inputKeys=defaultKeys;
static u8 desiredJoypad=0;
static int captureKey=-1;
using XInputGetStateFn=DWORD (WINAPI*)(DWORD,XINPUT_STATE*);
static XInputGetStateFn readGamepad=nullptr;
using AsyncKeyStateFn=SHORT (WINAPI*)(int);
static AsyncKeyStateFn readAsyncKey=GetAsyncKeyState;
static HMODULE gamepadLibrary=nullptr;
static std::wstring notification;
static ULONGLONG messageUntil=0,lastBatterySave=0,lastRewind=0,lastStatus=0;
static std::chrono::steady_clock::time_point previous=std::chrono::steady_clock::now();
static host_timing::Pacer framePacer;
static double measuredFPS=0;
static u64 framesMeasured=0,totalFrames=0;
static int renderScale=4,paletteIndex=0,displayFilter=0,ghostWeight=40,speedIndex=2,volume=100,stateSlot=1;
static unsigned rewindSeconds=30,audioLatency=40;
static HardwareModel selectedModel=HardwareModel::Auto;
static std::filesystem::path configPath;
static std::array<std::filesystem::path,6> recentROMs{};
static std::filesystem::path dmgBoot,cgbBoot;
static RECT restoredWindow{CW_USEDEFAULT,CW_USEDEFAULT,0,0};
static bool restoredMaximized=false,displayReady=false;
static std::array<u32,GB_W*GB_H> displayPixels{},previousDisplay{};
static std::array<u32,4> customPalette={0xFFF8E7,0xBAAE8A,0x6D674F,0x24241D};
static constexpr std::array<double,7> speeds={0.25,0.5,1.0,2.0,4.0,8.0,16.0};
static constexpr std::array<const wchar_t*,7> speedNames={L"0.25x",L"0.5x",L"1x",L"2x",L"4x",L"8x",L"16x"};
static constexpr std::array<std::array<u32,4>,7> displayPalettes={{
    {0x9BBC0F,0x8BAC0F,0x306230,0x0F380F},{0xE8E8D8,0xB0B0A0,0x68685C,0x202020},
    {0xB1F0C2,0x72B99E,0x34766A,0x123A39},{0xFFFFFF,0xAAAAAA,0x555555,0x000000},
    {0xF5E6C5,0xBC9C6C,0x765536,0x302218},{0xFFCC66,0xCE913B,0x805020,0x291808},
    {0xE0E8FF,0x9CAAD9,0x566491,0x202B50}
}};
static constexpr int statusHeight=26;
static constexpr UINT IDM_OPEN=100,IDM_RESET=101,IDM_PAUSE=102,IDM_SAVE=103,IDM_LOAD=104,
    IDM_SCREEN=105,IDM_SOUND=106,IDM_EXIT=107,IDM_FULLSCREEN=108,IDM_SAVE_AS=109,IDM_LOAD_FROM=110,
    IDM_FRAME=111,IDM_REWIND_ONCE=112,IDM_RESUME=113,IDM_RECORD=114,IDM_MOVIE_SAVE=115,IDM_PLAY=116,IDM_MOVIE_STOP=117,
    IDM_PAL0=200,IDM_CUSTOM_COLOR=220,IDM_SCALE=300,IDM_TURBO=400,IDM_SPEED=410,IDM_MODEL=500,
    IDM_FILTER=520,IDM_GHOST=530,IDM_GHOST_AMOUNT=531,IDM_INTEGER=540,IDM_REWIND=550,
    IDM_LANGUAGE=560,IDM_HELP=570,IDM_ABOUT=571,IDM_STUDIO=580,IDM_REPORT=581,IDM_CONFIG=582,
    IDM_RECENT=600,IDM_SLOT=620,IDM_BOOT_DMG=640,IDM_BOOT_CGB=641,IDM_BOOT_OFF=642,
    IDM_BIND=650,IDM_BIND_RESET=658,IDM_VOLUME=670,IDM_CHANNEL=680,IDM_LATENCY=690,IDM_WAV=695,IDM_WAV_STOP=696,IDM_THEME=697,IDM_COMPARE=698,IDM_PATCH=699;
static constexpr UINT IDC_STEP=1001,IDC_RUN=1002,IDC_REFRESH=1003,IDC_EXPORT=1004,IDC_BREAK=1005,IDC_CLEAR=1006,IDC_WATCH=1007;

static const wchar_t* tr(const wchar_t* en,const wchar_t* ru){return russian?ru:en;}
static std::wstring wide(std::string_view s){
    if(s.empty())return {};
    const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);
    std::wstring result(n,L' ');MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),result.data(),n);return result;
}
static void updateTitle();
static void updateTimer();
static void saveConfig();
static void refreshStudio();
static HMENU createMenus();
static void command(UINT id);
static void extraLoadConfig();static void extraSaveConfig();static void extraCartridgeLoaded(const std::filesystem::path& source);static void frameCompleted();static bool extraCommand(UINT id);static bool extraKey(WPARAM key,bool down);static void configureEQ();static std::string issueReport();
static void alert(const wchar_t* s){notification=s;messageUntil=GetTickCount64()+3000;updateTitle();}
static void resetDisplay(){displayReady=false;gpu.resetHistory();}
static void prepareDisplay(){
    const auto& sourcePalette=PPU::palettes[gb.ppu.palette&3];
    const auto& targetPalette=paletteIndex==7?customPalette:displayPalettes[(std::size_t)paletteIndex];
    for(std::size_t i=0;i<displayPixels.size();++i){
        u32 color=gb.ppu.pixels[i]&0xFFFFFF;
        if(!gb.mem.cgbMode&&!gb.mem.dmgCompatibility)for(int shade=0;shade<4;++shade)if(color==(sourcePalette[shade]&0xFFFFFF)){color=targetPalette[shade];break;}
        const bool onGPU=gpuEnabled&&!compareDisplay&&!selfTest&&gb.accessoryPixels.empty();
        if(ghosting&&displayReady&&!onGPU){
            const u32 old=previousDisplay[i];u32 blended=0;
            for(int shift=0;shift<24;shift+=8)blended|=(((color>>shift&255)*(100-ghostWeight)+(old>>shift&255)*ghostWeight)/100)<<shift;
            color=blended;
        }
        displayPixels[i]=onGPU?color:media::correctColor(color,colorCorrection);
    }
    previousDisplay=displayPixels;displayReady=true;
}
static std::wstring keyName(int key){
    wchar_t name[64]{};UINT scan=MapVirtualKeyW((UINT)key,MAPVK_VK_TO_VSC);
    if(key>=VK_PRIOR&&key<=VK_DOWN)scan|=0x100;
    if(GetKeyNameTextW((LONG)(scan<<16),name,64))return name;return L"VK "+std::to_wstring(key);
}
static bool writableFolder(const std::filesystem::path& folder){
    const auto test=folder/(L".gbstudio-write-"+std::to_wstring(GetCurrentProcessId()));
    HANDLE file=CreateFileW(test.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;CloseHandle(file);return true;
}
static int setting(const wchar_t* key,int fallback){return GetPrivateProfileIntW(L"Studio",key,fallback,configPath.c_str());}
static std::wstring settingText(const wchar_t* key,const wchar_t* fallback=L""){
    wchar_t value[32768]{};GetPrivateProfileStringW(L"Studio",key,fallback,value,32768,configPath.c_str());return value;
}
static void loadConfig(){
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);const auto folder=std::filesystem::path(executable).parent_path();
    configPath=folder/L"GameBoy-Studio.ini";
    if(!writableFolder(folder)){
        wchar_t local[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
        auto fallback=local[0]?std::filesystem::path(local):std::filesystem::temp_directory_path();
        fallback/=L"GameBoy-Studio";std::error_code ec;std::filesystem::create_directories(fallback,ec);configPath=fallback/L"config.ini";
    }
    russian=setting(L"Russian",PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_RUSSIAN)!=0;
    sound=setting(L"Sound",1)!=0;integerScaling=setting(L"IntegerScaling",1)!=0;ghosting=setting(L"Ghosting",0)!=0;resumeSession=setting(L"ResumeSession",1)!=0;
    lightTheme=setting(L"LightTheme",0)!=0;compareDisplay=setting(L"CompareDisplay",0)!=0;
    renderScale=std::clamp(setting(L"Scale",4),1,8);paletteIndex=std::clamp(setting(L"Palette",0),0,7);
    displayFilter=std::clamp(setting(L"Filter",0),0,2);ghostWeight=std::clamp(setting(L"GhostWeight",40),10,80);
    speedIndex=std::clamp(setting(L"SpeedIndex",2),0,6);volume=std::clamp(setting(L"Volume",100),0,100);
    audioLatency=(unsigned)std::clamp(setting(L"AudioLatency",40),20,80);
    stateSlot=std::clamp(setting(L"StateSlot",1),1,9);selectedModel=(HardwareModel)std::clamp(setting(L"Model",0),0,14);
    rewindSeconds=(unsigned)std::clamp(setting(L"RewindSeconds",30),0,300);rewindBuffer.configure(rewindSeconds);
    for(int i=0;i<6;++i)recentROMs[i]=settingText((L"Recent"+std::to_wstring(i)).c_str());
    for(int i=0;i<8;++i)inputKeys[i]=std::clamp(setting((L"Key"+std::to_wstring(i)).c_str(),defaultKeys[i]),1,255);
    for(int i=0;i<4;++i){customPalette[i]=(u32)setting((L"Color"+std::to_wstring(i)).c_str(),(int)customPalette[i])&0xFFFFFF;
        gb.apu.setChannelMuted(i,setting((L"Mute"+std::to_wstring(i)).c_str(),0)!=0);}
    dmgBoot=settingText(L"DMGBootROM");cgbBoot=settingText(L"CGBBootROM");
    if(!dmgBoot.empty()&&!gb.loadBootROM(dmgBoot))dmgBoot.clear();if(!cgbBoot.empty()&&!gb.loadBootROM(cgbBoot))cgbBoot.clear();
    restoredWindow.left=setting(L"WindowX",CW_USEDEFAULT);restoredWindow.top=setting(L"WindowY",CW_USEDEFAULT);
    restoredWindow.right=setting(L"WindowWidth",0);restoredWindow.bottom=setting(L"WindowHeight",0);restoredMaximized=setting(L"Maximized",0)!=0;
    if(restoredWindow.left!=CW_USEDEFAULT){
        RECT r={restoredWindow.left,restoredWindow.top,restoredWindow.left+std::max(240L,restoredWindow.right),restoredWindow.top+std::max(260L,restoredWindow.bottom)};
        if(!MonitorFromRect(&r,MONITOR_DEFAULTTONULL))restoredWindow.left=restoredWindow.top=CW_USEDEFAULT;
    }
}
static void saveConfig(){
    extraSaveConfig();
    if(configPath.empty()||selfTest)return;
    WINDOWPLACEMENT pos{};pos.length=sizeof(pos);if(fullscreen)pos=placement;else GetWindowPlacement(hwnd,&pos);
    std::wostringstream out;out<<L"; GameBoy Studio 1.0 preview. UTF-16 INI; values are documented in README.\r\n[Studio]\r\n";
    auto value=[&](const wchar_t* key,auto data){out<<key<<L'='<<data<<L"\r\n";};
    value(L"Russian",russian);value(L"Sound",sound);value(L"IntegerScaling",integerScaling);value(L"Ghosting",ghosting);value(L"ResumeSession",resumeSession);
    value(L"LightTheme",lightTheme);value(L"CompareDisplay",compareDisplay);
    value(L"Scale",renderScale);value(L"Palette",paletteIndex);value(L"Filter",displayFilter);value(L"GhostWeight",ghostWeight);value(L"SpeedIndex",speedIndex);
    value(L"Volume",volume);value(L"AudioLatency",audioLatency);value(L"StateSlot",stateSlot);value(L"Model",(int)selectedModel);value(L"RewindSeconds",rewindSeconds);
    value(L"WindowX",pos.rcNormalPosition.left);value(L"WindowY",pos.rcNormalPosition.top);value(L"WindowWidth",pos.rcNormalPosition.right-pos.rcNormalPosition.left);
    value(L"WindowHeight",pos.rcNormalPosition.bottom-pos.rcNormalPosition.top);value(L"Maximized",pos.showCmd==SW_SHOWMAXIMIZED);
    value(L"DMGBootROM",dmgBoot.wstring());value(L"CGBBootROM",cgbBoot.wstring());
    for(int i=0;i<6;++i)value((L"Recent"+std::to_wstring(i)).c_str(),recentROMs[i].wstring());
    for(int i=0;i<8;++i)value((L"Key"+std::to_wstring(i)).c_str(),inputKeys[i]);
    for(int i=0;i<4;++i){value((L"Color"+std::to_wstring(i)).c_str(),customPalette[i]);value((L"Mute"+std::to_wstring(i)).c_str(),gb.apu.mutedChannels[i]);}
    const auto data=out.str();auto temporary=configPath;temporary+=L".tmp";std::ofstream f(temporary,std::ios::binary|std::ios::trunc);
    if(!f)return;const unsigned char bom[]={0xFF,0xFE};f.write((const char*)bom,2);f.write((const char*)data.data(),(std::streamsize)data.size()*sizeof(wchar_t));f.close();
    if(f&&MoveFileExW(temporary.c_str(),configPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,configPath.c_str());
}
static std::filesystem::path slotPath(){
    const auto suffix=stateSlot==1?std::wstring(L".state"):L".slot"+std::to_wstring(stateSlot)+L".state";
    return gb.mem.romPath.parent_path()/(gb.mem.romPath.filename().wstring()+suffix);
}
static std::filesystem::path sessionPath(){
    auto folder=configPath.parent_path()/L"Sessions";std::error_code ec;std::filesystem::create_directories(folder,ec);
    return folder/(wide(studio::hex(gb.romHash(),8).substr(1))+L".state");
}
static void saveSession(){if(resumeSession&&!selfTest&&!gb.mem.rom.empty())saves::save(gb,sessionPath());}
static void resetPacing(){framePacer.reset();previous=std::chrono::steady_clock::now();framesMeasured=0;measuredFPS=0;lastStatus=GetTickCount64();}
static void setPaused(bool value){if(value&&!paused&&!achievementClient.canPause()){alert(L"Hardcore pause cooldown: allow more frames to run");return;}if(value&&sendRumble){XINPUT_VIBRATION silent{};sendRumble(primaryPadIndex,&silent);}paused=value;focusPaused=false;resetPacing();audio.flush(gb.apu);updateTimer();updateTitle();}
static void updateTimer(){
    if(!hwnd)return;const bool active=(!paused&&!gb.mem.rom.empty()&&!IsIconic(hwnd))||heldRewind;SetTimer(hwnd,1,active?8:150,nullptr);
    if(active&&!highResolutionTimer){timeBeginPeriod(1);highResolutionTimer=true;}else if(!active&&highResolutionTimer){timeEndPeriod(1);highResolutionTimer=false;}
}
static void rebuildMenus(){HMENU old=appMenu;appMenu=createMenus();if(!fullscreen)SetMenu(hwnd,appMenu);DrawMenuBar(hwnd);if(old)DestroyMenu(old);updateTitle();}
static void updateTitle(){
    if(!hwnd)return;std::wstring title=L"GameBoy Studio 1.0 | ";title+=gb.mem.dmgCompatibility?L"CGB / DMG compatibility":gb.mem.cgbMode?L"Game Boy Color":L"Game Boy";
    title+=gb.mem.rom.empty()?tr(L" | Open ROM: Ctrl+O",L" | Открыть ROM: Ctrl+O"):L" | "+gb.mem.romPath.stem().wstring();
    if(paused)title+=tr(L" | PAUSED",L" | ПАУЗА");else if(!gb.mem.rom.empty())title+=L" | "+std::wstring(turbo?L"8x":speedNames[speedIndex])+L" | "+std::to_wstring((int)std::lround(measuredFPS))+L" FPS";
    if(turbo)title+=L" | TURBO";SetWindowTextW(hwnd,title.c_str());
    if(appMenu){
        auto checked=[&](UINT id,bool on){CheckMenuItem(appMenu,id,MF_BYCOMMAND|(on?MF_CHECKED:MF_UNCHECKED));};
        checked(IDM_PAUSE,paused);checked(IDM_SOUND,sound);checked(IDM_GHOST,ghosting);checked(IDM_INTEGER,integerScaling);checked(IDM_RESUME,resumeSession);
        checked(IDM_THEME,lightTheme);checked(IDM_COMPARE,compareDisplay);
        CheckMenuRadioItem(appMenu,IDM_MODEL,IDM_MODEL+2,IDM_MODEL+(UINT)selectedModel,MF_BYCOMMAND);
        CheckMenuRadioItem(appMenu,IDM_PAL0,IDM_PAL0+7,IDM_PAL0+paletteIndex,MF_BYCOMMAND);
        CheckMenuRadioItem(appMenu,IDM_SPEED,IDM_SPEED+6,IDM_SPEED+speedIndex,MF_BYCOMMAND);
        CheckMenuRadioItem(appMenu,IDM_FILTER,IDM_FILTER+2,IDM_FILTER+displayFilter,MF_BYCOMMAND);
        CheckMenuRadioItem(appMenu,IDM_SLOT+1,IDM_SLOT+9,IDM_SLOT+stateSlot,MF_BYCOMMAND);
        CheckMenuRadioItem(appMenu,IDM_LANGUAGE,IDM_LANGUAGE+1,IDM_LANGUAGE+(russian?1:0),MF_BYCOMMAND);
        for(int i=0;i<8;++i)EnableMenuItem(appMenu,IDM_PAL0+i,MF_BYCOMMAND|(gb.mem.cgbMode?MF_GRAYED:MF_ENABLED));
        for(int i=0;i<4;++i){checked(IDM_CHANNEL+i,!gb.apu.mutedChannels[i]);
            EnableMenuItem(appMenu,IDM_CHANNEL+i,MF_BYCOMMAND|(movie.mode()==InputMovie::Mode::Idle?MF_ENABLED:MF_GRAYED));}
        const bool hasROM=!gb.mem.rom.empty();for(UINT id:{IDM_RESET,IDM_SAVE,IDM_LOAD,IDM_SAVE_AS,IDM_LOAD_FROM,IDM_SCREEN,IDM_FRAME,IDM_RECORD,IDM_PLAY,IDM_REPORT,IDM_WAV,IDM_PATCH})
            EnableMenuItem(appMenu,id,MF_BYCOMMAND|(hasROM?MF_ENABLED:MF_GRAYED));
        EnableMenuItem(appMenu,IDM_MOVIE_SAVE,MF_BYCOMMAND|(movie.frames()>0?MF_ENABLED:MF_GRAYED));
        EnableMenuItem(appMenu,IDM_MOVIE_STOP,MF_BYCOMMAND|(movie.mode()!=InputMovie::Mode::Idle?MF_ENABLED:MF_GRAYED));
        EnableMenuItem(appMenu,IDM_WAV_STOP,MF_BYCOMMAND|(audio.recording()?MF_ENABLED:MF_GRAYED));
    }
    InvalidateRect(hwnd,nullptr,FALSE);
}
static void stopMovie(){if(movie.mode()!=InputMovie::Mode::Idle){movie.stop();alert(tr(L"Movie stopped",L"Запись / воспроизведение остановлено"));}}
static bool openROM(const std::filesystem::path& path,bool restore=false){
    auto loadedPath=path;
    const auto extension=path.extension().wstring();
    if(extension==L".zip"||extension==L".ZIP"||extension==L".gz"||extension==L".GZ"){
        const auto content=archives::read(path);if(!content){MessageBoxW(hwnd,wide(content.error).c_str(),L"ROM archive",MB_OK|MB_ICONERROR);return false;}
        auto folder=configPath.parent_path()/L"ArchiveCache";std::error_code ec;std::filesystem::create_directories(folder,ec);
        loadedPath=folder/(wide(studio::hex(saves::hash(content.rom),8).substr(1))+(content.rom.size()>0x143&&(content.rom[0x143]&0x80)?L".gbc":L".gb"));
        if(ec||!Memory::writeAtomic(loadedPath,{(const char*)content.rom.data(),content.rom.size()})){alert(L"Could not create ROM archive cache");return false;}
    }
    saveSession();if(!gb.load(loadedPath,selectedModel)){if(!selfTest)MessageBoxW(hwnd,wide(gb.mem.loadError).c_str(),tr(L"ROM error",L"Ошибка ROM"),MB_OK|MB_ICONERROR);return false;}
    extraCartridgeLoaded(path);movie.clear();debugger.clearTrace();debugger.clearBreakpoints();debugger.clearWatchpoints();debugger.clearRunTo();debuggerRunning=false;
    rewindBuffer.clear();audio.flush(gb.apu);resetDisplay();paused=false;focusPaused=false;resetPacing();measuredFPS=0;framesMeasured=0;
    previous=std::chrono::steady_clock::now();lastStatus=GetTickCount64();lastBatterySave=lastStatus;
    if(restore&&resumeSession&&std::filesystem::exists(sessionPath())&&saves::load(gb,sessionPath())){if(audio.rate()!=(unsigned)gb.apu.sampleRate)audio.open(audioLatency,(unsigned)gb.apu.sampleRate);configureEQ();alert(tr(L"Previous session restored",L"Предыдущая сессия восстановлена"));}
    rewindBuffer.capture(gb);prepareDisplay();std::error_code ec;const auto full=std::filesystem::absolute(path,ec);const auto used=ec?path:full;
    std::array<std::filesystem::path,6> list{};list[0]=used;int at=1;for(const auto& old:recentROMs)if(!old.empty()&&old!=used&&at<6)list[at++]=old;
    recentROMs=list;resetPacing();updateTimer();rebuildMenus();saveConfig();refreshStudio();return true;
}
static bool sameFile(const std::filesystem::path& a,const std::filesystem::path& b){
    if(a.empty()||b.empty())return false;std::error_code ec;if(std::filesystem::equivalent(a,b,ec)&&!ec)return true;
    const auto left=std::filesystem::weakly_canonical(a,ec).wstring();if(ec)return false;
    const auto right=std::filesystem::weakly_canonical(b,ec).wstring();if(ec)return false;
    return CompareStringOrdinal(left.c_str(),(int)left.size(),right.c_str(),(int)right.size(),TRUE)==CSTR_EQUAL;
}
static bool chooseFile(std::filesystem::path& path,const wchar_t* filter,const wchar_t* extension,bool save,const wchar_t* title){
    wchar_t name[32768]{};const auto initial=path.wstring();wcsncpy_s(name,initial.c_str(),_TRUNCATE);
    OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=hwnd;ofn.lpstrFile=name;ofn.nMaxFile=32768;
    ofn.lpstrFilter=filter;ofn.lpstrDefExt=extension;ofn.lpstrTitle=title;ofn.nFilterIndex=1;
    ofn.Flags=OFN_PATHMUSTEXIST|OFN_EXPLORER|OFN_NOCHANGEDIR|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    const BOOL ok=save?GetSaveFileNameW(&ofn):GetOpenFileNameW(&ofn);if(!ok)return false;path=name;
    if(save&&(sameFile(path,gb.mem.romPath)||sameFile(path,dmgBoot)||sameFile(path,cgbBoot))){
        MessageBoxW(hwnd,tr(L"Choose a different filename. Output cannot replace the loaded cartridge or boot ROM.",L"Выберите другое имя файла. Результат не может заменить загруженный картридж или загрузочную ROM."),
            tr(L"Source file protected",L"Исходный файл защищён"),MB_OK|MB_ICONERROR);return false;}
    return true;
}
static void showOpenDialog(){std::filesystem::path path;
    if(chooseFile(path,L"Game Boy ROM / ZIP / GZIP (*.gb;*.gbc;*.zip;*.gz)\0*.gb;*.gbc;*.zip;*.gz\0All files (*.*)\0*.*\0",L"gb",false,tr(L"Open ROM",L"Открыть ROM")))openROM(path);}
static void applyROMPatch(){
    if(gb.mem.rom.empty())return;std::filesystem::path patch;
    if(!chooseFile(patch,L"IPS / BPS patch (*.ips;*.bps)\0*.ips;*.bps\0",L"ips",false,tr(L"Apply patch to a separate ROM copy",L"Применить патч к отдельной копии ROM")))return;
    std::error_code ec;const auto bytes=std::filesystem::file_size(patch,ec);
    if(ec||bytes>32*1024*1024){alert(tr(L"Patch unreadable or exceeds 32 MiB",L"Патч недоступен или больше 32 МиБ"));return;}
    const auto input=Memory::loadBytes(patch);const auto result=cartridge::applyPatch(gb.mem.rom,input);
    if(!result.success){MessageBoxW(hwnd,wide(result.error).c_str(),tr(L"Patch rejected",L"Патч отклонён"),MB_OK|MB_ICONERROR);return;}
    auto folder=configPath.parent_path()/L"Sessions"/L"Patched";std::filesystem::create_directories(folder,ec);
    if(ec){alert(tr(L"Could not create patched ROM folder",L"Не удалось создать папку копий ROM"));return;}
    const auto extension=result.rom.size()>0x143&&(result.rom[0x143]&0x80)?L".gbc":L".gb";
    const auto name=gb.mem.romPath.stem().wstring()+L"-patched-"+wide(studio::hex(cartridge::crc32(result.rom),8).substr(1));
    auto output=folder/(name+extension);
    if(std::filesystem::exists(output,ec)&&Memory::loadBytes(output)!=result.rom)output=folder/(name+L"-"+std::to_wstring(GetTickCount64())+extension);
    if(!std::filesystem::exists(output,ec)&&!Memory::writeAtomic(output,std::string_view((const char*)result.rom.data(),result.rom.size()))){
        alert(tr(L"Could not save patched ROM copy",L"Не удалось сохранить копию ROM с патчем"));return;}
    if(openROM(output))alert(tr(L"Patched copy loaded; original ROM preserved",L"Загружена копия с патчем; исходная ROM сохранена"));
}
static void stateAction(bool save,bool named=false){
    if(link.connected()){alert(tr(L"Disconnect link before single-console states",L"Отключите Link перед состояниями одной консоли"));return;}
    if(gb.mem.rom.empty())return;auto path=slotPath();
    if(!save&&!named&&stateSlot==1){std::error_code ec;if(!std::filesystem::exists(path,ec)){auto legacy=gb.mem.romPath;legacy.replace_extension(L".state");if(std::filesystem::exists(legacy,ec))path=legacy;}}
    if(named&&!chooseFile(path,L"GameBoy Studio state (*.state)\0*.state\0All files (*.*)\0*.*\0",L"state",save,
        save?tr(L"Save named state",L"Сохранить состояние с именем"):tr(L"Load state",L"Загрузить состояние")))return;
    if(!save)stopMovie();const bool ok=save?saves::save(gb,path):saves::load(gb,path);
    alert(ok?(save?tr(L"State saved",L"Состояние сохранено"):tr(L"State loaded",L"Состояние загружено")):
        (save?tr(L"State save failed",L"Ошибка сохранения состояния"):tr(L"State rejected: wrong ROM, corrupt or incompatible file",L"Состояние отклонено: другая ROM, повреждённый или несовместимый файл")));
    if(ok&&!save){if(audio.rate()!=(unsigned)gb.apu.sampleRate)audio.open(audioLatency,(unsigned)gb.apu.sampleRate);configureEQ();audio.setVolume(volume);audio.flush(gb.apu);rewindBuffer.clear();rewindBuffer.capture(gb);resetDisplay();prepareDisplay();debugger.clearTrace();refreshStudio();}
}
static bool screenshot(){
    if(gb.mem.rom.empty())return false;SYSTEMTIME now{};GetLocalTime(&now);wchar_t stamp[64]{};
    swprintf_s(stamp,L"_%04u%02u%02u_%02u%02u%02u_%03u.bmp",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds);
    auto path=gb.mem.romPath.parent_path()/(gb.mem.romPath.stem().wstring()+stamp);std::ofstream f(path,std::ios::binary);if(!f)return false;
    BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+(DWORD)gb.ppu.pixels.size()*4;
    info.biSize=sizeof(info);info.biWidth=GB_W;info.biHeight=-GB_H;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=(DWORD)gb.ppu.pixels.size()*4;
    f.write((const char*)&file,sizeof(file));f.write((const char*)&info,sizeof(info));f.write((const char*)gb.ppu.pixels.data(),(std::streamsize)gb.ppu.pixels.size()*4);return (bool)f;
}
static void pollInput(bool forceGamepadPoll=false){
    if(movie.mode()==InputMovie::Mode::Playback)return;
    static XINPUT_STATE input{};static bool connected=false;static ULONGLONG lastPoll=0;const auto now=GetTickCount64();
    if(readGamepad&&(forceGamepadPoll||now-lastPoll>=(connected?8:250))){lastPoll=now;input={};connected=false;for(DWORD i=0;i<4;++i)if((padProfiles[0].device<0||(int)i==padProfiles[0].device)&&readGamepad(i,&input)==ERROR_SUCCESS){connected=true;primaryPadIndex=i;break;}}
    const u32 buttons=connected?(u32)input.Gamepad.wButtons|(input.Gamepad.bLeftTrigger>127?0x10000:0)|(input.Gamepad.bRightTrigger>127?0x20000:0):0;const bool focused=GetForegroundWindow()==hwnd;
    auto pressed=[&](int action,WORD){return focused&&(keyboard[(std::size_t)inputKeys[action]]||(buttons&padProfiles[0].buttons[(std::size_t)action])||(virtualInput&(1<<action)));};
    const std::array<bool,8> inputMask={
        pressed(0,XINPUT_GAMEPAD_DPAD_RIGHT)||(focused&&connected&&input.Gamepad.sThumbLX>padProfiles[0].deadzone),
        pressed(1,XINPUT_GAMEPAD_DPAD_LEFT)||(focused&&connected&&input.Gamepad.sThumbLX< -padProfiles[0].deadzone),
        pressed(2,XINPUT_GAMEPAD_DPAD_UP)||(focused&&connected&&input.Gamepad.sThumbLY>padProfiles[0].deadzone),
        pressed(3,XINPUT_GAMEPAD_DPAD_DOWN)||(focused&&connected&&input.Gamepad.sThumbLY< -padProfiles[0].deadzone),
        pressed(4,XINPUT_GAMEPAD_A),pressed(5,XINPUT_GAMEPAD_B),pressed(6,XINPUT_GAMEPAD_BACK),pressed(7,XINPUT_GAMEPAD_START)};
    desiredJoypad=0;for(int i=0;i<8;++i)if(inputMask[i])desiredJoypad|=(u8)(1u<<i);
    if(link.connected()){
        static constexpr int keys2[]={'D','A','W','S','G','H','T','Y'};u8 second=0;for(int i=0;i<8;++i)if(focused&&(readAsyncKey(keys2[i])&0x8000))second|=(u8)(1u<<i);
        if(readGamepad){XINPUT_STATE pad{};for(DWORD i=0;i<4;++i)if((padProfiles[1].device<0?(i!=primaryPadIndex||( !connected)):(int)i==padProfiles[1].device)&&readGamepad(i,&pad)==ERROR_SUCCESS){
            const u32 buttons=(u32)pad.Gamepad.wButtons|(pad.Gamepad.bLeftTrigger>127?0x10000:0)|(pad.Gamepad.bRightTrigger>127?0x20000:0);for(int bit=0;bit<8;++bit)if(focused&&(buttons&padProfiles[1].buttons[bit]))second|=(u8)(1<<bit);break;}}
        InputMovie::setJoypad(gbSecond.mem,second);
    }
    gb.mem.motion.tiltX=focused?(std::int16_t)((readAsyncKey(VK_NUMPAD4)&0x8000?112:0)-(readAsyncKey(VK_NUMPAD6)&0x8000?112:0)):0;
    gb.mem.motion.tiltY=focused?(std::int16_t)((readAsyncKey(VK_NUMPAD8)&0x8000?112:0)-(readAsyncKey(VK_NUMPAD2)&0x8000?112:0)):0;
    if(connected&&focused&&padProfiles[0].tiltStick){gb.mem.motion.tiltX+=(std::int16_t)(input.Gamepad.sThumbRX*112/32768);gb.mem.motion.tiltY+=(std::int16_t)(input.Gamepad.sThumbRY*112/32768);}

    // Movies sample input once per frame. Updating JOYP/IF between samples
    // would create an unrecorded IRQ for a quick press-and-release transition.
    if(movie.mode()!=InputMovie::Mode::Recording)InputMovie::setJoypad(gb.mem,desiredJoypad);
}
static void toggleFullscreen(){
    if(!fullscreen){placement.length=sizeof(placement);GetWindowPlacement(hwnd,&placement);MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
        if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor)){
            SetWindowLongPtrW(hwnd,GWL_STYLE,WS_POPUP|WS_VISIBLE);SetMenu(hwnd,nullptr);
            SetWindowPos(hwnd,HWND_TOP,monitor.rcMonitor.left,monitor.rcMonitor.top,monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,SWP_FRAMECHANGED);fullscreen=true;}
    }else{SetWindowLongPtrW(hwnd,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_VISIBLE);SetMenu(hwnd,appMenu);SetWindowPlacement(hwnd,&placement);
        SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);fullscreen=false;}
}
static void resetGame(){achievementClient.reset();stopMovie();gb.reset();if(link.connected()){gbSecond.reset();link.synchronize();}audio.flush(gb.apu);rewindBuffer.clear();rewindBuffer.capture(gb);debugger.clearTrace();debuggerRunning=false;
    resetDisplay();prepareDisplay();resetPacing();refreshStudio();}
static bool rewindOnce(){if(achievementClient.hardcore()){alert(L"Disable Hardcore to use rewind");return false;}
    if(link.connected()){alert(tr(L"Single-console rewind is disabled during Link",L"Перемотка одной консоли отключена при Link"));return false;}
    if(movie.mode()!=InputMovie::Mode::Idle){alert(tr(L"Stop the movie before rewinding",L"Остановите запись / воспроизведение перед перемоткой"));return false;}
    if(!rewindBuffer.rewind(gb))return false;audio.flush(gb.apu);resetDisplay();prepareDisplay();resetPacing();debugger.clearTrace();refreshStudio();InvalidateRect(hwnd,nullptr,FALSE);return true;
}
static void singleFrame(){if(achievementClient.hardcore()){alert(L"Disable Hardcore to advance frames");return;}if(gb.mem.rom.empty())return;setPaused(true);pollInput();
    if(movie.mode()==InputMovie::Mode::Recording)InputMovie::setJoypad(gb.mem,desiredJoypad);if(!movie.beforeFrame(gb))return;
    if(link.connected())link.runFrame();else gb.runFrame();frameCompleted();++totalFrames;rewindBuffer.capture(gb);prepareDisplay();audio.pump(gb.apu,false);audio.flush(gb.apu);refreshStudio();InvalidateRect(hwnd,nullptr,FALSE);}
static void exportReport(){
    if(gb.mem.rom.empty())return;auto path=gb.mem.romPath;path.replace_extension(L".diagnostic.json");
    if(!chooseFile(path,L"JSON report (*.json)\0*.json\0All files (*.*)\0*.*\0",L"json",true,tr(L"Export ROM diagnostics",L"Экспорт диагностики ROM")))return;
    alert(Memory::writeAtomic(path,issueReport())?tr(L"Report exported",L"Отчёт экспортирован"):tr(L"Report export failed",L"Ошибка экспорта отчёта"));
}
static void showHelp(){
    MessageBoxW(hwnd,tr(
        L"GAME CONTROLS\nArrows: D-pad; Z: A; X: B; Shift: Select; Enter: Start.\nXInput: D-pad / left stick, A / B, Back / Start.\nKeys can be remapped under Options > Controls.\n\nHOTKEYS\nCtrl+O: open ROM; Ctrl+R: reset; P: pause.\nF1: next DMG display palette; F5 / F9: quick save / load.\nCtrl+Shift+S / O: named save / load; 1-9: select quick slot.\nF6: one frame; Backspace (hold): rewind; Tab (hold): 8x turbo.\nCtrl+0: normal speed (1x), cancel turbo.\nF11: fullscreen; Esc: exit fullscreen; F12: raw BMP screenshot.\nCtrl+D: Studio debugger.\n\nDISPLAY & AUDIO\nDisplay effects never modify PPU pixels; BMP captures the raw frame.\nAudio plays at 1x; other speeds, pause and rewind are muted.\nMovies record frame inputs plus the initial state and require the same ROM.\n\nSETTINGS\nGameBoy-Studio.ini beside the executable; falls back to\n%LOCALAPPDATA%\\GameBoy-Studio\\config.ini if needed.\nNo telemetry is used. Link connects two local emulated consoles.",
        L"УПРАВЛЕНИЕ ИГРОЙ\nСтрелки: крестовина; Z: A; X: B; Shift: Select; Enter: Start.\nXInput: крестовина / левый стик, A / B, Back / Start.\nКлавиши меняются в Параметры > Управление.\n\nГОРЯЧИЕ КЛАВИШИ\nCtrl+O: открыть ROM; Ctrl+R: сброс; P: пауза.\nF1: следующая палитра DMG; F5 / F9: сохранить / загрузить.\nCtrl+Shift+S / O: сохранение / загрузка с именем; 1-9: слот.\nF6: один кадр; удерживать Backspace: перемотка; Tab: турбо 8x.\nCtrl+0: обычная скорость (1x), выключить турбо.\nF11: полный экран; Esc: выйти; F12: исходный кадр BMP.\nCtrl+D: отладчик Studio.\n\nЭКРАН И ЗВУК\nЭффекты не меняют пиксели PPU; BMP сохраняет исходный кадр.\nЗвук работает на 1x; другие скорости, пауза и перемотка без звука.\nФайлы записей хранят ввод и начальное состояние; нужна та же ROM.\n\nНАСТРОЙКИ\nGameBoy-Studio.ini рядом с программой; резервный путь:\n%LOCALAPPDATA%\\GameBoy-Studio\\config.ini.\nТелеметрия отсутствует. Link связывает две локальные консоли."),tr(L"Controls and help",L"Управление и справка"),MB_OK|MB_ICONINFORMATION);
}
static void refreshStudio(){
    if(!studioText)return;std::ostringstream out;out<<"GAMEBOY STUDIO 0.9\r\n"<<studio::formatRegisters(studio::capture(gb))<<"\r\n\r\nDISASSEMBLY (mapped memory at PC)\r\n";
    out<<studio::disassemble(gb.mem,gb.cpu.PC,14)<<"\r\nWORK RAM $C000\r\n"<<studio::memoryDump(gb.mem,0xC000,128)<<"\r\nRECENT CPU TRACE (Studio execution only)\r\n";
    const auto& trace=debugger.trace();for(std::size_t i=trace.size()>20?trace.size()-20:0;i<trace.size();++i)out<<studio::formatTrace(trace[i])<<"\r\n";
    out<<"\r\nROM DIAGNOSTICS (header inspection, not a compatibility result)\r\n"<<studio::romReport(gb);
    if(const auto hit=debugger.busHit())out<<"\r\nBUS "<<(hit->ppu?"PPU ":"CPU/DMA ")<<(hit->write?"WRITE ":"READ ")<<studio::hex(hit->address,4)<<" = "<<studio::hex(hit->value)<<(hit->blocked?" (blocked)":"")<<"\r\n";
    out<<"\r\nCPU PROFILE\r\n"<<debugger.profileReport()<<"\r\nLUA LOG\r\n"<<lua.log();
    std::string data=out.str(),crlf;crlf.reserve(data.size());for(std::size_t i=0;i<data.size();++i){if(data[i]=='\n'&&(i==0||data[i-1]!='\r'))crlf+='\r';crlf+=data[i];}
    SetWindowTextW(studioText,wide(crlf).c_str());
}
static bool addressFromStudio(u16& address){
    wchar_t text[64]{};GetWindowTextW(studioAddress,text,64);wchar_t* end=nullptr;const auto value=wcstoul(text,&end,16);while(end&&*end==L' ')++end;
    if(end==text||!end||*end||value>0xFFFF){alert(tr(L"Enter an address in hex, e.g. C000",L"Введите адрес в hex, например C000"));return false;}address=(u16)value;return true;
}
static LRESULT CALLBACK studioProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
        case WM_CREATE:{
            auto button=[&](const wchar_t* name,int x,int width,UINT id){CreateWindowW(L"BUTTON",name,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,x,8,width,28,w,(HMENU)(UINT_PTR)id,appInstance,nullptr);};
            button(tr(L"CPU step",L"Шаг CPU"),8,88,IDC_STEP);button(tr(L"Run / pause",L"Пуск / пауза"),102,104,IDC_RUN);
            button(tr(L"Refresh",L"Обновить"),212,86,IDC_REFRESH);button(tr(L"Export JSON",L"Экспорт JSON"),304,108,IDC_EXPORT);
            studioAddress=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"0100",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,420,8,64,28,w,nullptr,appInstance,nullptr);
            button(tr(L"Break",L"Стоп PC"),490,82,IDC_BREAK);button(tr(L"Watch",L"Стоп RAM"),578,90,IDC_WATCH);button(tr(L"Clear stops",L"Снять стопы"),674,104,IDC_CLEAR);
            studioText=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,8,44,770,520,w,nullptr,appInstance,nullptr);
            SendMessageW(studioText,EM_SETLIMITTEXT,1024*1024,0);SendMessageW(studioText,WM_SETFONT,(WPARAM)GetStockObject(ANSI_FIXED_FONT),TRUE);refreshStudio();return 0;
        }
        case WM_SIZE:{RECT r{};GetClientRect(w,&r);if(studioText)MoveWindow(studioText,8,44,std::max(80L,r.right-16),std::max(40L,r.bottom-52),TRUE);return 0;}
        case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE&&(HWND)lp!=hwnd){debuggerRunning=false;setPaused(true);}return 0;
        case WM_COMMAND:{
            if(LOWORD(wp)==IDC_STEP){stopMovie();setPaused(true);debuggerRunning=false;debugger.step();audio.pump(gb.apu,false);resetDisplay();prepareDisplay();refreshStudio();}
            else if(LOWORD(wp)==IDC_RUN){stopMovie();debuggerRunning=!debuggerRunning;
                if(debuggerRunning){debuggerSkipBreakpoint=true;setPaused(false);}else setPaused(true);}
            else if(LOWORD(wp)==IDC_REFRESH)refreshStudio();else if(LOWORD(wp)==IDC_EXPORT)exportReport();
            else if(LOWORD(wp)==IDC_CLEAR){debugger.clearBreakpoints();debugger.clearWatchpoints();debugger.clearBusWatchpoints();debugger.clearBankBreakpoints();debugger.clearRunTo();alert(tr(L"Breakpoints and watches cleared",L"Точки остановки и наблюдения сняты"));}
            else if(LOWORD(wp)==IDC_BREAK||LOWORD(wp)==IDC_WATCH){u16 addr=0;if(addressFromStudio(addr)){
                if(LOWORD(wp)==IDC_BREAK)debugger.addBreakpoint(addr);else debugger.addBusWatchpoint(addr);
                alert(LOWORD(wp)==IDC_BREAK?tr(L"PC breakpoint added",L"Точка остановки PC добавлена"):tr(L"CPU bus watchpoint added",L"Остановка на обращении CPU добавлена"));}}
            return 0;
        }
        case WM_CLOSE:DestroyWindow(w);return 0;
        case WM_DESTROY:studioWindow=studioText=studioAddress=nullptr;debuggerRunning=false;return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
static void showStudio(){if(link.connected()){alert(L"Disconnect Link before CPU debugging");return;}setPaused(true);if(studioWindow){ShowWindow(studioWindow,SW_SHOW);SetForegroundWindow(studioWindow);refreshStudio();return;}
    studioWindow=CreateWindowExW(0,L"GBStudioInspector",tr(L"Studio — CPU / memory / ROM",L"Studio — CPU / память / ROM"),WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,810,650,hwnd,nullptr,appInstance,nullptr);}
static void handleKey(WPARAM key,bool down){
    if(key>=keyboard.size())return;
    if(down&&captureKey>=0){
        if(key==VK_ESCAPE){captureKey=-1;alert(tr(L"Key remapping cancelled",L"Изменение клавиши отменено"));return;}
        if(key==VK_CONTROL||key==VK_MENU||std::any_of(hotkeys.begin(),hotkeys.end(),[&](int binding){return binding==(int)key;})||(key>='1'&&key<='9')){
            alert(tr(L"This key is reserved for emulator commands",L"Эта клавиша зарезервирована для команд эмулятора"));return;}
        const int old=inputKeys[captureKey];for(int i=0;i<8;++i)if(i!=captureKey&&inputKeys[i]==(int)key)inputKeys[i]=old;
        inputKeys[captureKey]=(int)key;captureKey=-1;keyboard.fill(false);pollInput();rebuildMenus();saveConfig();alert(tr(L"Control remapped",L"Клавиша изменена"));return;
    }
    keyboard[(std::size_t)key]=down;pollInput();if(!down)return;
    if(key==VK_ESCAPE&&fullscreen)toggleFullscreen();
    if(key>='1'&&key<='9')stateSlot=(int)(key-'0');
    updateTitle();saveConfig();
}
#include "studio_features_ui.hpp"

static HMENU createMenus(){
    HMENU main=CreateMenu(),file=CreatePopupMenu(),time=CreatePopupMenu(),video=CreatePopupMenu(),settings=CreatePopupMenu(),help=CreatePopupMenu();
    auto add=[](HMENU menu,UINT id,const wchar_t* caption){AppendMenuW(menu,MF_STRING,id,caption);};
    auto popup=[](HMENU parent,HMENU child,const wchar_t* caption){AppendMenuW(parent,MF_POPUP,(UINT_PTR)child,caption);};
    auto sep=[](HMENU menu){AppendMenuW(menu,MF_SEPARATOR,0,nullptr);};
    add(file,IDM_OPEN,tr(L"Open ROM...\tCtrl+O",L"Открыть ROM...\tCtrl+O"));HMENU recent=CreatePopupMenu();
    for(int i=0;i<6;++i)if(!recentROMs[i].empty())add(recent,IDM_RECENT+i,recentROMs[i].filename().c_str());
    if(recentROMs[0].empty())AppendMenuW(recent,MF_STRING|MF_GRAYED,0,tr(L"No recent ROMs",L"Нет недавних ROM"));
    popup(file,recent,tr(L"Recent games",L"Недавние игры"));add(file,IDM_PATCH,tr(L"Apply IPS / BPS patch to a copy...",L"Применить IPS / BPS к копии ROM..."));
    add(file,IDM_RESET,tr(L"Reset\tCtrl+R",L"Сброс\tCtrl+R"));sep(file);
    add(file,IDM_SAVE,tr(L"Quick save\tF5",L"Быстрое сохранение\tF5"));add(file,IDM_LOAD,tr(L"Quick load\tF9",L"Быстрая загрузка\tF9"));HMENU slots=CreatePopupMenu();
    for(int i=1;i<=9;++i){const auto caption=std::to_wstring(i);add(slots,IDM_SLOT+i,caption.c_str());}popup(file,slots,tr(L"Quick slot (keys 1-9)",L"Быстрый слот (клавиши 1-9)"));
    add(file,IDM_SAVE_AS,tr(L"Save state as...\tCtrl+Shift+S",L"Сохранить состояние как...\tCtrl+Shift+S"));add(file,IDM_LOAD_FROM,tr(L"Load state from...\tCtrl+Shift+O",L"Загрузить состояние...\tCtrl+Shift+O"));
    add(file,IDM_RESUME,tr(L"Autosave / restore last session",L"Автосохранение / восстановление сессии"));sep(file);
    add(file,IDM_SCREEN,tr(L"Screenshot (raw BMP)\tF12",L"Снимок экрана (исходный BMP)\tF12"));add(file,IDM_EXIT,tr(L"Exit",L"Выход"));
    add(time,IDM_PAUSE,tr(L"Pause / resume\tP",L"Пауза / продолжить\tP"));add(time,IDM_FRAME,tr(L"Advance one frame\tF6",L"Один кадр\tF6"));HMENU speed=CreatePopupMenu();
    for(int i=0;i<7;++i)add(speed,IDM_SPEED+i,i==2?tr(L"Normal speed (1x)\tCtrl+0",L"Обычная скорость (1x)\tCtrl+0"):speedNames[i]);popup(time,speed,tr(L"Speed (audio at 1x)",L"Скорость (звук на 1x)"));add(time,IDM_TURBO,tr(L"Turbo 8x while held\tTab",L"Турбо 8x при удержании\tTab"));
    HMENU depth=CreatePopupMenu();static const unsigned depths[]={0,30,120,300};
    for(int i=0;i<4;++i){static const wchar_t* en[]={L"Off",L"30 seconds",L"2 minutes",L"5 minutes"};static const wchar_t* ru[]={L"Выкл.",L"30 секунд",L"2 минуты",L"5 минут"};
        AppendMenuW(depth,MF_STRING|(rewindSeconds==depths[i]?MF_CHECKED:0),IDM_REWIND+i,tr(en[i],ru[i]));}
    popup(time,depth,tr(L"Rewind depth (64 MiB cap)",L"Глубина перемотки (до 64 МиБ)"));add(time,IDM_REWIND_ONCE,tr(L"Rewind one snapshot\tBackspace",L"На снимок назад\tBackspace"));sep(time);
    add(time,IDM_RECORD,tr(L"Start input recording",L"Начать запись ввода"));add(time,IDM_MOVIE_SAVE,tr(L"Stop / save recording...",L"Остановить / сохранить запись..."));
    add(time,IDM_PLAY,tr(L"Play input recording...",L"Воспроизвести запись ввода..."));add(time,IDM_MOVIE_STOP,tr(L"Stop recording / playback",L"Остановить запись / воспроизведение"));
    HMENU palettes=CreatePopupMenu();static const wchar_t* enPal[]={L"Classic DMG green",L"Game Boy Pocket",L"Game Boy Light",L"Black and white",L"Sepia",L"Amber LCD",L"Blue",L"Custom"};
    static const wchar_t* ruPal[]={L"Классический зелёный DMG",L"Game Boy Pocket",L"Game Boy Light",L"Чёрно-белая",L"Сепия",L"Янтарный LCD",L"Синяя",L"Своя"};
    for(int i=0;i<8;++i)add(palettes,IDM_PAL0+i,tr(enPal[i],ruPal[i]));HMENU colors=CreatePopupMenu();
    for(int i=0;i<4;++i){auto label=tr(L"Shade ",L"Оттенок ")+std::to_wstring(i)+L"...";add(colors,IDM_CUSTOM_COLOR+i,label.c_str());}
    popup(palettes,colors,tr(L"Edit custom colors",L"Изменить свои цвета"));popup(video,palettes,tr(L"DMG display palette\tF1",L"Палитра экрана DMG\tF1"));
    HMENU filter=CreatePopupMenu();add(filter,IDM_FILTER,tr(L"None",L"Нет"));add(filter,IDM_FILTER+1,tr(L"LCD pixel grid",L"LCD: сетка пикселей"));add(filter,IDM_FILTER+2,tr(L"CRT scanlines",L"CRT: строки развёртки"));
    popup(video,filter,tr(L"Display effect",L"Эффект экрана"));add(video,IDM_GHOST,tr(L"LCD ghosting",L"LCD: остаточное изображение"));HMENU response=CreatePopupMenu();
    for(int i=0;i<3;++i){static const wchar_t* en[]={L"Fast (20% persistence)",L"Medium (40%)",L"Slow (60%)"};static const wchar_t* ru[]={L"Быстро (20% остатка)",L"Средне (40%)",L"Медленно (60%)"};
        AppendMenuW(response,MF_STRING|(ghostWeight==(i+1)*20?MF_CHECKED:0),IDM_GHOST_AMOUNT+i,tr(en[i],ru[i]));}
    popup(video,response,tr(L"LCD response",L"Отклик LCD"));add(video,IDM_COMPARE,tr(L"Compare raw / processed side by side",L"Сравнить исходный кадр / эффекты рядом"));
    add(video,IDM_THEME,tr(L"Light window theme",L"Светлая тема окна"));add(video,IDM_INTEGER,tr(L"Integer scaling",L"Целочисленный масштаб"));HMENU scales=CreatePopupMenu();
    for(int i=1;i<=8;++i){const auto caption=std::to_wstring(i)+L"x";add(scales,IDM_SCALE+i,caption.c_str());}popup(video,scales,tr(L"Window scale",L"Масштаб окна"));add(video,IDM_FULLSCREEN,tr(L"Fullscreen\tF11",L"Полный экран\tF11"));
    add(settings,IDM_SOUND,tr(L"Sound enabled",L"Звук включён"));HMENU volumes=CreatePopupMenu();for(int i=0;i<5;++i){const auto caption=std::to_wstring(i*25)+L"%";
        AppendMenuW(volumes,MF_STRING|(volume==i*25?MF_CHECKED:0),IDM_VOLUME+i,caption.c_str());}popup(settings,volumes,tr(L"Master volume",L"Общая громкость"));
    HMENU channels=CreatePopupMenu();static const wchar_t* channelNames[]={L"1: Pulse + sweep",L"2: Pulse",L"3: Wave",L"4: Noise"};
    static const wchar_t* channelRu[]={L"1: Импульс + sweep",L"2: Импульс",L"3: Волна",L"4: Шум"};
    for(int i=0;i<4;++i)add(channels,IDM_CHANNEL+i,tr(channelNames[i],channelRu[i]));popup(settings,channels,tr(L"Audio channels",L"Звуковые каналы"));
    HMENU latency=CreatePopupMenu();for(int i=0;i<3;++i){const unsigned ms=20u<<i;const auto name=std::to_wstring(ms)+L" ms";AppendMenuW(latency,MF_STRING|(audioLatency==ms?MF_CHECKED:0),IDM_LATENCY+i,name.c_str());}
    popup(settings,latency,tr(L"Audio latency",L"Задержка звука"));add(settings,IDM_WAV,tr(L"Start WAV recording...",L"Начать запись WAV..."));add(settings,IDM_WAV_STOP,tr(L"Stop WAV recording",L"Остановить запись WAV"));
    HMENU models=CreatePopupMenu();add(models,IDM_MODEL,tr(L"Auto (from cartridge)",L"Авто (по картриджу)"));add(models,IDM_MODEL+1,L"Game Boy");add(models,IDM_MODEL+2,L"Game Boy Color");
    popup(settings,models,tr(L"Hardware model (restarts game)",L"Модель (перезапуск игры)"));HMENU boot=CreatePopupMenu();
    add(boot,IDM_BOOT_DMG,tr(L"Select DMG boot ROM...",L"Выбрать загрузочную ROM DMG..."));add(boot,IDM_BOOT_CGB,tr(L"Select CGB boot ROM...",L"Выбрать загрузочную ROM CGB..."));
    add(boot,IDM_BOOT_OFF,tr(L"Disable external boot ROMs",L"Отключить внешние загрузочные ROM"));popup(settings,boot,tr(L"External boot ROM (restarts game)",L"Внешняя загрузочная ROM (перезапуск)"));
    HMENU controls=CreatePopupMenu();static const wchar_t* actions[]={L"Right",L"Left",L"Up",L"Down",L"A",L"B",L"Select",L"Start"};
    static const wchar_t* actionRu[]={L"Вправо",L"Влево",L"Вверх",L"Вниз",L"A",L"B",L"Select",L"Start"};
    for(int i=0;i<8;++i){const auto caption=std::wstring(tr(actions[i],actionRu[i]))+L" : "+keyName(inputKeys[i])+L"...";add(controls,IDM_BIND+i,caption.c_str());}
    add(controls,IDM_BIND_RESET,tr(L"Restore default keys",L"Восстановить клавиши"));popup(settings,controls,tr(L"Controls",L"Управление"));HMENU languages=CreatePopupMenu();
    add(languages,IDM_LANGUAGE,L"English");add(languages,IDM_LANGUAGE+1,L"Русский");popup(settings,languages,tr(L"Language",L"Язык"));
    add(help,IDM_STUDIO,tr(L"Studio debugger...\tCtrl+D",L"Отладчик Studio...\tCtrl+D"));add(help,IDM_REPORT,tr(L"Export ROM diagnostics...",L"Экспорт диагностики ROM..."));
    add(help,IDM_HELP,tr(L"Controls / hotkeys",L"Управление / горячие клавиши"));add(help,IDM_CONFIG,tr(L"Show configuration file",L"Показать файл настроек"));add(help,IDM_ABOUT,tr(L"About",L"О программе"));
    extendMenus(main,file,time,video,settings,help);popup(main,file,tr(L"File",L"Файл"));popup(main,time,tr(L"Time",L"Время"));popup(main,video,tr(L"Display",L"Экран"));popup(main,settings,tr(L"Options",L"Параметры"));popup(main,help,tr(L"Studio / Help",L"Studio / Справка"));return main;
}
struct BackBuffer {
    HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ original=nullptr;int width=0,height=0;
    void clear(){if(dc){SelectObject(dc,original);DeleteObject(bitmap);DeleteDC(dc);}dc=nullptr;bitmap=nullptr;original=nullptr;width=height=0;}
    bool resize(HDC target,int w,int h){if(dc&&w==width&&h==height)return true;clear();dc=CreateCompatibleDC(target);if(!dc)return false;bitmap=CreateCompatibleBitmap(target,w,h);
        if(!bitmap){DeleteDC(dc);dc=nullptr;return false;}original=SelectObject(dc,bitmap);width=w;height=h;return true;}
    ~BackBuffer(){clear();}
} backBuffer;
static bool render(HWND w,HDC target,int forcedW=0,int forcedH=0){
    RECT rect{};GetClientRect(w,&rect);if(forcedW>0&&forcedH>0)rect={0,0,forcedW,forcedH};
    const int width=rect.right,height=rect.bottom;if(width<=0||height<=0||!backBuffer.resize(target,width,height))return false;
    HDC dc=backBuffer.dc;HBRUSH dark=CreateSolidBrush(lightTheme?RGB(237,241,245):RGB(14,18,23));FillRect(dc,&rect,dark);DeleteObject(dark);
    const bool border=!compareDisplay&&!gb.accessoryPixels.empty();const int contentW=border?(int)gb.accessoryWidth:GB_W,contentH=border?(int)gb.accessoryHeight:GB_H;
    const int availableHeight=std::max(1,height-statusHeight-(virtualCase?100:0)),sourceWidth=compareDisplay?GB_W*2:contentW,scale=std::min(width/sourceWidth,availableHeight/contentH);
    const double fit=std::min((double)width/sourceWidth,(double)availableHeight/contentH),factor=integerScaling&&scale>=1?(double)scale:fit;
    const int destW=std::max(1,(int)(contentW*factor)),destH=std::max(1,(int)(contentH*factor)),totalWidth=destW*(compareDisplay?2:1),x=(width-totalWidth)/2,y=(availableHeight-destH)/2;
    if(!gb.mem.rom.empty()){
        if(!displayReady)prepareDisplay();BITMAPINFO bmi{};bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bmi.bmiHeader.biWidth=GB_W;bmi.bmiHeader.biHeight=-GB_H;
        bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;bmi.bmiHeader.biCompression=BI_RGB;SetStretchBltMode(dc,COLORONCOLOR);
        if(compareDisplay)StretchDIBits(dc,x,y,destW,destH,0,0,GB_W,GB_H,gb.ppu.pixels.data(),&bmi,DIB_RGB_COLORS,SRCCOPY);
        const int processedX=x+(compareDisplay?destW:0);
        if(!compareDisplay&&!gb.accessoryPixels.empty()){
            bmi.bmiHeader.biWidth=(LONG)gb.accessoryWidth;bmi.bmiHeader.biHeight=-(LONG)gb.accessoryHeight;
            StretchDIBits(dc,processedX,y,destW,destH,0,0,(int)gb.accessoryWidth,(int)gb.accessoryHeight,gb.accessoryPixels.data(),&bmi,DIB_RGB_COLORS,SRCCOPY);
        }else
        if(!gpuEnabled||compareDisplay||forcedW){std::vector<u32> scaled;const u32* processed=displayPixels.data();int sourceW=GB_W,sourceH=GB_H;
        if(scaleFilter==4){scaled=pixelFilters.scale(displayPixels,GB_W,GB_H,3);if(!scaled.empty()){processed=scaled.data();sourceW*=3;sourceH*=3;}else {scaleFilter=0;alert(wide(pixelFilters.lastError()).c_str());}}else if(scaleFilter==3){scaled=media::hq2x(displayPixels,GB_W,GB_H);processed=scaled.data();sourceW*=2;sourceH*=2;}else if(scaleFilter){const int multiplier=scaleFilter+1;scaled=media::scale(displayPixels,GB_W,GB_H,multiplier);processed=scaled.data();sourceW*=multiplier;sourceH*=multiplier;}
        bmi.bmiHeader.biWidth=sourceW;bmi.bmiHeader.biHeight=-sourceH;
        StretchDIBits(dc,processedX,y,destW,destH,0,0,sourceW,sourceH,processed,&bmi,DIB_RGB_COLORS,SRCCOPY);
        if(displayFilter&&factor>=3){const HPEN pen=CreatePen(PS_SOLID,1,displayFilter==1?RGB(34,46,29):RGB(12,12,18));const auto old=SelectObject(dc,pen);
            for(int row=1;row<GB_H;++row){const int yy=y+(int)(row*factor)-1;MoveToEx(dc,processedX,yy,nullptr);LineTo(dc,processedX+destW,yy);}
            if(displayFilter==1)for(int col=1;col<GB_W;++col){const int xx=processedX+(int)(col*factor)-1;MoveToEx(dc,xx,y,nullptr);LineTo(dc,xx,y+destH);}
            SelectObject(dc,old);DeleteObject(pen);}}
    }else{
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(153,188,15));HFONT font=CreateFontW(28,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        auto old=SelectObject(dc,font);RECT welcome{12,std::max(10,availableHeight/2-70),width-12,availableHeight};DrawTextW(dc,L"GAMEBOY STUDIO",-1,&welcome,DT_CENTER|DT_TOP|DT_SINGLELINE);
        SelectObject(dc,old);DeleteObject(font);SetTextColor(dc,lightTheme?RGB(52,65,78):RGB(195,206,209));welcome.top+=44;
        DrawTextW(dc,tr(L"Open a .gb / .gbc ROM with Ctrl+O\nor drag it into this window.\n\nStudio / Help contains controls and diagnostics.",
            L"Откройте .gb / .gbc ROM через Ctrl+O\nили перетащите её в это окно.\n\nУправление и диагностика — в Studio / Справка."),-1,&welcome,DT_CENTER|DT_TOP);
    }
    RECT status{0,std::max(0,height-statusHeight),width,height};HBRUSH bar=CreateSolidBrush(lightTheme?RGB(214,222,231):RGB(29,35,42));FillRect(dc,&status,bar);DeleteObject(bar);std::wstring text;
    if(messageUntil>GetTickCount64())text=notification;
    else if(!gb.mem.rom.empty()){
        text=(paused?tr(L"Paused",L"Пауза"):std::to_wstring((int)std::lround(measuredFPS))+L" FPS")+std::wstring(L"  |  ");text+=turbo?L"8x":speedNames[speedIndex];
        if(!paused&&gb.clockRate&&measuredFPS>0)text+=L" ("+std::to_wstring((int)std::lround(measuredFPS*GB_FRAME*100/gb.clockRate))+L"%)";
        text+=tr(L"  |  Slot ",L"  |  Слот ")+std::to_wstring(stateSlot);text+=tr(L"  |  Rewind ",L"  |  Назад ")+std::to_wstring((int)rewindBuffer.availableSeconds())+L"s";
        if(movie.mode()==InputMovie::Mode::Recording)text+=L"  |  REC "+std::to_wstring(movie.frames());else if(movie.mode()==InputMovie::Mode::Playback)text+=L"  |  PLAY "+std::to_wstring(movie.position())+L"/"+std::to_wstring(movie.frames());
        if(gb.mem.bootROMMapped)text+=L"  |  BOOT";if(audio.recording())text+=L"  |  WAV";if(compareDisplay)text+=tr(L"  |  Raw / display",L"  |  Исходный / эффекты");
    }else text=tr(L"DMG + CGB  |  Offline  |  Ctrl+O: Open ROM",L"DMG + CGB  |  Без сети  |  Ctrl+O: открыть ROM");
    status.left=10;status.right-=8;SetBkMode(dc,TRANSPARENT);SetTextColor(dc,lightTheme?RGB(32,45,58):RGB(209,220,225));DrawTextW(dc,text.c_str(),-1,&status,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if(virtualCase){HPEN pen=CreatePen(PS_SOLID,8,RGB(123,130,149));auto old=SelectObject(dc,pen);auto brush=SelectObject(dc,GetStockObject(NULL_BRUSH));RoundRect(dc,std::max(4,x-12),std::max(4,y-12),std::min(width-4,x+totalWidth+12),std::min(availableHeight-4,y+destH+12),18,18);SelectObject(dc,brush);SelectObject(dc,old);DeleteObject(pen);}
    virtualButtons.fill(RECT{});if(virtualCase){const int base=availableHeight+8,center=std::max(72,width/4),r=22;virtualButtons[2]={center-r,base,center+r,base+30};virtualButtons[3]={center-r,base+54,center+r,base+84};virtualButtons[1]={center-r-36,base+28,center-r+4,base+56};virtualButtons[0]={center+r-4,base+28,center+r+36,base+56};
        virtualButtons[4]={width-68,base+12,width-20,base+60};virtualButtons[5]={width-126,base+34,width-78,base+82};virtualButtons[6]={width/2-62,base+60,width/2-8,base+82};virtualButtons[7]={width/2+2,base+60,width/2+56,base+82};
        static const wchar_t* labels[]={L"→",L"←",L"↑",L"↓",L"A",L"B",L"Select",L"Start"};for(unsigned i=0;i<8;++i){const auto& box=virtualButtons[i];HBRUSH brush=CreateSolidBrush(virtualInput&(1<<i)?RGB(150,54,86):RGB(76,82,96));FillRect(dc,&box,brush);DeleteObject(brush);SetTextColor(dc,RGB(255,255,255));RECT text=box;DrawTextW(dc,labels[i],-1,&text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}}
    const bool result=BitBlt(target,0,0,width,height,dc,0,0,SRCCOPY)!=FALSE;
    if(gpuEnabled&&!compareDisplay&&!forcedW&&!gb.mem.rom.empty()&&gb.accessoryPixels.empty()){
        std::vector<u32> scaled;std::span<const u32> source=displayPixels;unsigned sw=GB_W,sh=GB_H;
        if(scaleFilter==4){scaled=pixelFilters.scale(displayPixels,GB_W,GB_H,3);if(!scaled.empty()){source=scaled;sw*=3;sh*=3;}else scaleFilter=0;}
        const RECT gameRect{x,y,x+destW,y+destH};if(!gpu.draw(w,gameRect,source,sw,sh,displayFilter,scaleFilter,colorCorrection,ghosting?ghostWeight/100.0f:0.0f,gb.ppu.frameNumber)){gpuEnabled=false;prepareDisplay();gpu.hide();CheckMenuItem(appMenu,EX_GPU,MF_BYCOMMAND|MF_UNCHECKED);alert(wide(gpu.lastError()).c_str());InvalidateRect(w,nullptr,FALSE);}
    }else gpu.hide();return result;
}
static bool renderSelfTest(){
    const auto raw=gb.ppu.pixels;HDC screen=GetDC(nullptr);if(!screen)return false;
    HDC target=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,2560,1440);ReleaseDC(nullptr,screen);
    if(!target||!bitmap){if(target)DeleteDC(target);if(bitmap)DeleteObject(bitmap);return false;}
    const auto original=SelectObject(target,bitmap);bool ok=true;
    static const std::array<SIZE,4> sizes={SIZE{80,120},SIZE{320,314},SIZE{1280,900},SIZE{2560,1440}};
    for(int effect=0;effect<3;++effect)for(int theme=0;theme<2;++theme)for(int compare=0;compare<2;++compare){
        displayFilter=effect;lightTheme=theme!=0;compareDisplay=compare!=0;ghosting=compare!=0;paletteIndex=(effect+theme)%7;
        resetDisplay();prepareDisplay();prepareDisplay();
        for(const auto size:sizes)ok=render(hwnd,target,size.cx,size.cy)&&ok;
        ok=gb.ppu.pixels==raw&&ok;
    }
    SelectObject(target,original);DeleteObject(bitmap);DeleteDC(target);return ok;
}
static void finishTimingTest(){
    if(!timingTest||timingFinished)return;
    const double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-timingStart).count();
    if(wall<timingDuration)return;timingFinished=true;
    const u64 cycles=gb.totalCycles-timingStartCycles,frames=gb.ppu.frameNumber-timingStartFrames;
    const double emulated=gb.clockRate?(double)cycles/gb.clockRate:0,ratio=emulated/wall;
    const bool passed=std::isfinite(ratio)&&ratio>=0.97&&ratio<=1.03&&(!timingAudio||timingAudioOpened);
    std::ostringstream report;report<<std::setprecision(12)<<"{\n  \"status\": \""<<(passed?"PASS":"FAIL")<<"\",\n  \"wall_seconds\": "<<wall
        <<",\n  \"emulated_seconds\": "<<emulated<<",\n  \"speed_ratio\": "<<ratio<<",\n  \"clock_rate\": "<<gb.clockRate
        <<",\n  \"base_cycles\": "<<cycles<<",\n  \"ppu_frames\": "<<frames<<",\n  \"ppu_fps\": "<<frames/wall
        <<",\n  \"cpu_double_speed\": "<<(gb.mem.doubleSpeed?"true":"false")<<",\n  \"speed_setting\": "<<speeds[speedIndex]
        <<",\n  \"turbo\": "<<(turbo?"true":"false")<<",\n  \"maximum_frame_batch\": "<<timingMaxBatch
        <<",\n  \"sound_requested\": "<<(timingAudio?"true":"false")<<",\n  \"audio_opened\": "<<(timingAudioOpened?"true":"false")
        <<",\n  \"audio_backend\": "<<studio::jsonString(audio.backendName())
        <<",\n  \"scope\": \"Actual hidden Win32 timer loop and hardware backend at 1x. GPU rendering and audible quality are outside this check.\"\n}\n";
    const bool written=Memory::writeAtomic(timingReport,report.str());selfTestExitCode=passed&&written?0:4;PostMessageW(hwnd,WM_CLOSE,0,0);
}
static void tickApp(){
    static bool ticking=false;if(ticking)return;
    ticking=true;struct TickGuard{bool& active;~TickGuard(){active=false;}} guard{ticking};
    discordPresence.update(gb,paused);achievementClient.poll();if(achievementClient.takeReset()){network.close();link.disconnect();printer.detach();if(linkWindow)DestroyWindow(linkWindow);closeEditingPanels();lua.stop();cheatEngine.setEnabled(false);stopMovie();debuggerRunning=false;rewindBuffer.clear();heldRewind=false;speedIndex=2;gb.mem.saveBattery();gb.reset();achievementClient.reset();audio.flush(gb.apu);}
    if(const auto notice=achievementClient.notice())alert(wide(*notice).c_str());
    if(network.active()){if(!network.poll()){cheatEngine.setEnabled(!network.active());alert(wide(network.status()).c_str());if(!network.active())setPaused(true);}if(network.connected()&&audio.rate()!=(unsigned)gb.apu.sampleRate){audio.open(audioLatency,(unsigned)gb.apu.sampleRate);configureEQ();audio.setVolume(volume);}if(network.consumeRollback()){audio.flush(gb.apu);resetDisplay();}}
    pollInput();const auto now=std::chrono::steady_clock::now();double dt=std::chrono::duration<double>(now-previous).count();previous=now;const auto ticks=GetTickCount64();
    if(achievementClient.hardcore())heldRewind=false;
    if(heldRewind&&!gb.mem.rom.empty()&&ticks-lastRewind>=75){rewindOnce();lastRewind=ticks;}
    else if(!paused&&(!IsIconic(hwnd)||achievementClient.hardcore())&&!gb.mem.rom.empty()&&!heldRewind){
        const double frameTime=(double)GB_FRAME/gb.clockRate,rate=turbo?8.0:speeds[speedIndex];framePacer.addElapsed(dt,rate,frameTime);
        int frames=0;const int budget=std::max(4,(int)(rate*4));
        while(framePacer.due()&&frames<budget){
            if(movie.mode()==InputMovie::Mode::Recording)InputMovie::setJoypad(gb.mem,desiredJoypad);
            if(!movie.beforeFrame(gb)){setPaused(true);alert(tr(L"Input playback / recording ended",L"Воспроизведение / запись ввода завершено"));break;}
            if(debuggerRunning){const auto result=debugger.run(100000,GB_FRAME,debuggerSkipBreakpoint);debuggerSkipBreakpoint=false;
                if(result.reason!=studio::StopReason::Budget){debuggerRunning=false;setPaused(true);resetDisplay();prepareDisplay();alert(wide(studio::stopReasonName(result.reason)).c_str());refreshStudio();break;}}
            else if(network.active()){if(!network.advance(network.isHost()?desiredJoypad:InputMovie::joypad(gbSecond.mem)))break;}else if(link.connected())link.runFrame();else gb.runFrame();framePacer.consume();++frames;++totalFrames;++framesMeasured;frameCompleted();if(!link.connected())rewindBuffer.capture(gb);prepareDisplay();
            audio.pump(gb.apu,sound&&!paused&&!turbo&&speedIndex==2&&!heldRewind);
        }
        timingMaxBatch=std::max(timingMaxBatch,frames);
        if(frames)InvalidateRect(hwnd,nullptr,FALSE);if(selfTest&&!timingTest&&totalFrames>=12){selfTestExitCode=renderSelfTest()?0:3;PostMessageW(hwnd,WM_CLOSE,0,0);}
    }else framePacer.reset();
    audio.pump(gb.apu,sound&&!paused&&!turbo&&speedIndex==2&&!heldRewind&&!gb.mem.rom.empty());
    if(ticks-lastBatterySave>30000){if(!selfTest){gb.mem.saveBattery();if(link.connected())gbSecond.mem.saveBattery();library.save();}lastBatterySave=ticks;}
    if(ticks-lastStatus>=1000){measuredFPS=(double)framesMeasured*1000/(double)(ticks-lastStatus);framesMeasured=0;lastStatus=ticks;updateTitle();}
    if(messageUntil&&ticks>messageUntil){messageUntil=0;updateTitle();}
    finishTimingTest();
}
static void command(UINT id){
    if(achievementClient.hardcore()&&(id==IDM_LOAD||id==IDM_LOAD_FROM||id==IDM_PLAY||id==IDM_FRAME||id==IDM_STUDIO||(id>=IDM_SPEED&&id<IDM_SPEED+2))){alert(L"Disable Hardcore to use this feature");return;}
    if(extraCommand(id))return;
    if(id==IDM_OPEN)showOpenDialog();else if(id==IDM_PATCH)applyROMPatch();else if(id>=IDM_RECENT&&id<IDM_RECENT+6)openROM(recentROMs[id-IDM_RECENT]);else if(id==IDM_RESET)resetGame();
    else if(id==IDM_PAUSE){debuggerRunning=false;setPaused(!paused);}else if(id==IDM_FRAME)singleFrame();
    else if(id==IDM_SAVE||id==IDM_LOAD||id==IDM_SAVE_AS||id==IDM_LOAD_FROM)stateAction(id==IDM_SAVE||id==IDM_SAVE_AS,id==IDM_SAVE_AS||id==IDM_LOAD_FROM);
    else if(id==IDM_SCREEN)alert(screenshot()?tr(L"Raw screenshot saved beside ROM",L"Исходный кадр сохранён рядом с ROM"):tr(L"Screenshot failed",L"Ошибка снимка экрана"));
    else if(id==IDM_SOUND){sound=!sound;audio.flush(gb.apu);}else if(id==IDM_FULLSCREEN)toggleFullscreen();else if(id==IDM_TURBO)alert(tr(L"Hold Tab for 8x turbo",L"Удерживайте Tab для турбо 8x"));
    else if(id==IDM_INTEGER)integerScaling=!integerScaling;else if(id==IDM_THEME)lightTheme=!lightTheme;else if(id==IDM_COMPARE)compareDisplay=!compareDisplay;
    else if(id==IDM_GHOST){ghosting=!ghosting;resetDisplay();prepareDisplay();}
    else if(id>=IDM_GHOST_AMOUNT&&id<IDM_GHOST_AMOUNT+3){ghostWeight=(id-IDM_GHOST_AMOUNT+1)*20;resetDisplay();prepareDisplay();rebuildMenus();}
    else if(id>=IDM_FILTER&&id<=IDM_FILTER+2)displayFilter=id-IDM_FILTER;else if(id>=IDM_SPEED&&id<=IDM_SPEED+6){speedIndex=id-IDM_SPEED;turbo=heldTurbo=false;audio.flush(gb.apu);resetPacing();}
    else if(id>=IDM_MODEL&&id<=IDM_MODEL+2){const auto model=(HardwareModel)(id-IDM_MODEL);
        if(gb.setModel(model)){selectedModel=model;stopMovie();audio.flush(gb.apu);rewindBuffer.clear();rewindBuffer.capture(gb);resetDisplay();prepareDisplay();}
        else MessageBoxW(hwnd,wide(gb.mem.loadError).c_str(),tr(L"Hardware model",L"Модель консоли"),MB_OK|MB_ICONERROR);}
    else if(id>=IDM_PAL0&&id<IDM_PAL0+8){paletteIndex=id-IDM_PAL0;resetDisplay();prepareDisplay();}
    else if(id>=IDM_CUSTOM_COLOR&&id<IDM_CUSTOM_COLOR+4){const int shade=id-IDM_CUSTOM_COLOR;static COLORREF colors[16]{};CHOOSECOLORW cc{};cc.lStructSize=sizeof(cc);cc.hwndOwner=hwnd;
        const auto c=customPalette[shade];cc.rgbResult=RGB((c>>16)&255,(c>>8)&255,c&255);cc.lpCustColors=colors;cc.Flags=CC_FULLOPEN|CC_RGBINIT;
        if(ChooseColorW(&cc)){customPalette[shade]=(GetRValue(cc.rgbResult)<<16)|(GetGValue(cc.rgbResult)<<8)|GetBValue(cc.rgbResult);paletteIndex=7;resetDisplay();prepareDisplay();}}
    else if(id>=IDM_SCALE+1&&id<=IDM_SCALE+8){if(fullscreen)toggleFullscreen();renderScale=id-IDM_SCALE;RECT r{0,0,GB_W*renderScale,GB_H*renderScale+statusHeight};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,TRUE);
        SetWindowPos(hwnd,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER);}
    else if(id>=IDM_SLOT+1&&id<=IDM_SLOT+9)stateSlot=id-IDM_SLOT;
    else if(id>=IDM_REWIND&&id<IDM_REWIND+4){static const unsigned values[]={0,30,120,300};rewindSeconds=values[id-IDM_REWIND];rewindBuffer.configure(rewindSeconds);rewindBuffer.capture(gb);rebuildMenus();}
    else if(id==IDM_REWIND_ONCE){if(!rewindOnce())alert(tr(L"No earlier rewind snapshot",L"Нет более раннего снимка"));}
    else if(id==IDM_RESUME)resumeSession=!resumeSession;else if(id==IDM_RECORD){lua.stop();debuggerRunning=false;rewindBuffer.clear();if(movie.startRecording(gb))alert(tr(L"Recording frame inputs",L"Запись ввода по кадрам"));}
    else if(id==IDM_MOVIE_SAVE){movie.stop();auto path=gb.mem.romPath;path.replace_extension(L".gbmovie");
        if(chooseFile(path,L"GameBoy Studio input movie (*.gbmovie)\0*.gbmovie\0",L"gbmovie",true,tr(L"Save input recording",L"Сохранить запись ввода")))
            alert(movie.save(path)?tr(L"Input recording saved",L"Запись ввода сохранена"):tr(L"Recording save failed",L"Ошибка сохранения записи"));}
    else if(id==IDM_PLAY){lua.stop();std::filesystem::path path;
        if(chooseFile(path,L"GameBoy Studio input movie (*.gbmovie)\0*.gbmovie\0",L"gbmovie",false,tr(L"Play input recording",L"Воспроизвести запись ввода"))){
            if(movie.startPlayback(gb,path)){debuggerRunning=false;audio.flush(gb.apu);rewindBuffer.clear();resetDisplay();prepareDisplay();setPaused(false);alert(tr(L"Playing frame inputs",L"Воспроизведение ввода по кадрам"));}
            else alert(tr(L"Recording rejected: wrong ROM or invalid file",L"Запись отклонена: другая ROM или неверный файл"));}}
    else if(id==IDM_MOVIE_STOP)stopMovie();
    else if(id==IDM_BOOT_DMG||id==IDM_BOOT_CGB){std::filesystem::path path;
        if(chooseFile(path,L"Boot ROM (*.bin;*.rom)\0*.bin;*.rom\0All files (*.*)\0*.*\0",L"bin",false,tr(L"Select your external boot ROM",L"Выберите свою загрузочную ROM"))){
            std::error_code ec;const auto bytes=std::filesystem::file_size(path,ec);const bool expected=!ec&&(id==IDM_BOOT_DMG?bytes==256:bytes==2304||bytes==2048);
            if(expected&&gb.loadBootROM(path)){if(id==IDM_BOOT_DMG)dmgBoot=path;else cgbBoot=path;resetGame();alert(tr(L"External boot ROM configured",L"Внешняя загрузочная ROM настроена"));}
            else alert(tr(L"DMG needs 256 bytes; CGB needs 2304 or packed 2048 bytes",L"DMG: 256 байт; CGB: 2304 или упакованные 2048 байт"));}}
    else if(id==IDM_BOOT_OFF){gb.clearBootROM();dmgBoot.clear();cgbBoot.clear();resetGame();}
    else if(id>=IDM_BIND&&id<IDM_BIND+8){captureKey=id-IDM_BIND;alert(tr(L"Press the new control key; Esc cancels",L"Нажмите новую клавишу; Esc отменяет"));}
    else if(id==IDM_BIND_RESET){inputKeys=defaultKeys;keyboard.fill(false);pollInput();rebuildMenus();}
    else if(id>=IDM_VOLUME&&id<IDM_VOLUME+5){volume=(id-IDM_VOLUME)*25;audio.setVolume(volume);rebuildMenus();}
    else if(id>=IDM_CHANNEL&&id<IDM_CHANNEL+4&&movie.mode()==InputMovie::Mode::Idle){const int channel=id-IDM_CHANNEL;gb.apu.setChannelMuted(channel,!gb.apu.mutedChannels[channel]);audio.flush(gb.apu);}
    else if(id>=IDM_LATENCY&&id<IDM_LATENCY+3){audioLatency=20u<<(id-IDM_LATENCY);if(!audio.open(audioLatency,(unsigned)gb.apu.sampleRate))alert(tr(L"Audio device unavailable",L"Звуковое устройство недоступно"));audio.setVolume(volume);rebuildMenus();}
    else if(id==IDM_WAV){SYSTEMTIME t{};GetLocalTime(&t);wchar_t suffix[64]{};swprintf_s(suffix,L"_%04u%02u%02u_%02u%02u%02u.wav",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
        auto path=gb.mem.romPath.parent_path()/(gb.mem.romPath.stem().wstring()+suffix);
        if(chooseFile(path,L"Wave audio (*.wav)\0*.wav\0",L"wav",true,tr(L"Record WAV to a new file",L"Запись WAV в новый файл")))
            alert(audio.startRecording(path)?tr(L"WAV recording started",L"Запись WAV начата"):wide(audio.lastError()).c_str());}
    else if(id==IDM_WAV_STOP)alert(audio.stopRecording()?tr(L"WAV recording saved",L"Запись WAV сохранена"):wide(audio.lastError()).c_str());
    else if(id==IDM_LANGUAGE||id==IDM_LANGUAGE+1){russian=id==IDM_LANGUAGE+1;rebuildMenus();}else if(id==IDM_STUDIO)showStudio();else if(id==IDM_REPORT)exportReport();else if(id==IDM_HELP)showHelp();
    else if(id==IDM_CONFIG){saveConfig();MessageBoxW(hwnd,configPath.c_str(),tr(L"Configuration file",L"Файл настроек"),MB_OK|MB_ICONINFORMATION);}
    else if(id==IDM_ABOUT)MessageBoxW(hwnd,tr(L"GameBoy Studio 1.0 preview\nDMG / MGB / CGB / SGB emulator for Windows x64.\nSameBoy accuracy core; GameBoy Studio tools.\nDirect3D effects, linked consoles, BESS, TAS and Studio tools.\n\nNo commercial games or proprietary boot ROMs are included.\nSee README and ROADMAP for verified features and limitations.",
        L"GameBoy Studio 1.0 preview\nСобственный эмулятор SM83 / DMG / CGB для Windows x64.\nЭффекты экрана, состояния, перемотка, запись ввода и инструменты Studio.\n\nКоммерческие игры и фирменные загрузочные ROM не включены.\nПодтверждённые функции и ограничения — в README и ROADMAP."),L"GameBoy Studio",MB_OK|MB_ICONINFORMATION);
    else if(id==IDM_EXIT){saveConfig();DestroyWindow(hwnd);return;}updateTitle();saveConfig();refreshStudio();
}
static LRESULT CALLBACK wndProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
        case WM_CREATE:hwnd=w;DragAcceptFiles(w,TRUE);updateTimer();return 0;case WM_TIMER:tickApp();return 0;
        case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);render(w,dc);EndPaint(w,&ps);return 0;}case WM_ERASEBKGND:return 1;
        case WM_GETMINMAXINFO:{auto info=(MINMAXINFO*)lp;info->ptMinTrackSize={240,260};return 0;}
        case WM_LBUTTONDOWN:{if(virtualCase){POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};for(unsigned i=0;i<8;++i)if(PtInRect(&virtualButtons[i],point)){virtualInput|=(u8)(1<<i);SetCapture(w);pollInput();InvalidateRect(w,nullptr,FALSE);break;}}return 0;}
        case WM_LBUTTONUP:virtualInput=0;if(GetCapture()==w)ReleaseCapture();pollInput();InvalidateRect(w,nullptr,FALSE);return 0;
        // Capture may already belong to another window in this thread. Releasing
        // it here would cancel that window's drag or other mouse interaction.
        case WM_CAPTURECHANGED:virtualInput=0;pollInput();InvalidateRect(w,nullptr,FALSE);return 0;
        case WM_SIZE:if(wp==SIZE_MINIMIZED)audio.flush(gb.apu);updateTimer();InvalidateRect(w,nullptr,FALSE);return 0;
        case WM_SYSKEYDOWN:if(wp!=VK_F11&&captureKey<0)return DefWindowProcW(w,msg,wp,lp);[[fallthrough]];
        case WM_KEYDOWN:{
            if(captureKey>=0){if(!(lp&(1LL<<30)))handleKey(wp,true);return 0;}
            if(!(lp&(1LL<<30))&&extraKey(wp,true))return 0;
            if(!(lp&(1LL<<30))||wp==VK_TAB)handleKey(wp,true);return 0;
        }
        case WM_KEYUP:extraKey(wp,false);handleKey(wp,false);return 0;case WM_SYSKEYUP:if(wp==VK_F11){handleKey(wp,false);return 0;}return DefWindowProcW(w,msg,wp,lp);
        case WM_KILLFOCUS:{if(selfTest&&!uiSelfTest)return 0;keyboard.fill(false);virtualInput=0;if(GetCapture()==w)ReleaseCapture();pollInput();turbo=heldTurbo=heldRewind=false;captureKey=captureHotkey=-1;
            if(sendRumble){XINPUT_VIBRATION silent{};sendRumble(primaryPadIndex,&silent);}if(debuggerRunning&&studioWindow&&(HWND)wp==studioWindow)return 0;
            if(achievementClient.canPause()){focusPaused=!paused;paused=true;}resetPacing();audio.flush(gb.apu);updateTimer();updateTitle();return 0;}
        case WM_SETFOCUS:if(focusPaused){paused=false;focusPaused=false;resetPacing();updateTimer();updateTitle();}return 0;
        case WM_DROPFILES:{HDROP files=(HDROP)wp;wchar_t path[32768]{};if(DragQueryFileW(files,0,path,32768))openROM(path);DragFinish(files);return 0;}
        case WM_COMMAND:command(LOWORD(wp));return 0;case WM_CLOSE:saveConfig();DestroyWindow(w);return 0;
        case WM_DESTROY:KillTimer(w,1);saveSession();if(!selfTest)gb.mem.saveBattery();audio.close();if(highResolutionTimer){timeEndPeriod(1);highResolutionTimer=false;}
            network.close();link.disconnect();printer.detach();lua.stop();video.close();gpu.close();pixelFilters.close();webcam.close();if(sendRumble){XINPUT_VIBRATION silent{};sendRumble(primaryPadIndex,&silent);}if(comparisonWindow)DestroyWindow(comparisonWindow);if(tasWindow)DestroyWindow(tasWindow);if(timelineWindow)DestroyWindow(timelineWindow);if(inspectorWindow)DestroyWindow(inspectorWindow);if(linkWindow)DestroyWindow(linkWindow);
            if(studioWindow)DestroyWindow(studioWindow);PostQuitMessage(selfTestExitCode);return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
#include "windows_qa.hpp"
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    if(!accurate::enable(gb)||!accurate::enable(gbSecond)){MessageBoxW(nullptr,L"gb_accuracy.dll is missing or incompatible. Restore it from the distribution.",L"GameBoy Studio",MB_OK|MB_ICONERROR);return 1;}
    gb.auxiliaryState=[](){return achievementClient.progress();};gb.auxiliaryRestore=[](std::span<const u8> data){return achievementClient.restore(data);};
    audio.capture=[](std::span<const host_audio::StereoFrame> data){if(video.recording()&&!video.appendAudio(data)){video.close();alert(L"Video audio capture failed");}};
    appInstance=instance;gb.mem.cameraImage=[](){return webcam.image();};SetProcessDPIAware();int count=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);std::filesystem::path requested;
    if(args)for(int i=1;i<count;++i){
        if(wcscmp(args[i],L"--self-test")==0)selfTest=true;
        else if(wcscmp(args[i],L"--ui-self-test")==0){if(i+1>=count){LocalFree(args);return 2;}uiSelfTest=selfTest=true;uiTestReport=args[++i];}
        else if(wcscmp(args[i],L"--timing-test")==0){if(i+1>=count){LocalFree(args);return 2;}timingTest=selfTest=true;timingReport=args[++i];}
        else if(wcscmp(args[i],L"--timing-audio")==0)timingAudio=true;
        else if(wcscmp(args[i],L"--timing-seconds")==0){if(i+1>=count){LocalFree(args);return 2;}wchar_t* end=nullptr;timingDuration=std::wcstod(args[++i],&end);if(!end||*end||!std::isfinite(timingDuration)||timingDuration<2||timingDuration>30){LocalFree(args);return 2;}}
        else if(requested.empty())requested=args[i];
    }if(args)LocalFree(args);
    if((timingTest&&uiSelfTest)||(timingTest&&(!requested.empty()&&sameFile(timingReport,requested)))||(uiSelfTest&&(!requested.empty()&&sameFile(uiTestReport,requested))))return 2;
    if(selfTest){resumeSession=false;speedIndex=2;sound=timingTest&&timingAudio;rewindBuffer.configure(0);wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);configPath=std::filesystem::path(executable).parent_path()/L".self-test.ini";}
    else {loadConfig();extraLoadConfig();}gamepadLibrary=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!gamepadLibrary)gamepadLibrary=LoadLibraryExW(L"xinput9_1_0.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(gamepadLibrary){readGamepad=reinterpret_cast<XInputGetStateFn>(GetProcAddress(gamepadLibrary,"XInputGetState"));sendRumble=reinterpret_cast<XInputSetStateFn>(GetProcAddress(gamepadLibrary,"XInputSetState"));}
    WNDCLASSW cls{};cls.lpfnWndProc=wndProc;cls.hInstance=instance;cls.lpszClassName=L"GBStudioWindow09";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    if(!RegisterClassW(&cls))return 1;WNDCLASSW inspector=cls;inspector.lpfnWndProc=studioProc;inspector.lpszClassName=L"GBStudioInspector";inspector.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);RegisterClassW(&inspector);
    RECT r{0,0,GB_W*renderScale,GB_H*renderScale+statusHeight};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,TRUE);appMenu=createMenus();
    const int width=restoredWindow.right>=240?restoredWindow.right:r.right-r.left,height=restoredWindow.bottom>=260?restoredWindow.bottom:r.bottom-r.top;
    hwnd=CreateWindowExW(0,cls.lpszClassName,L"GameBoy Studio 1.0 preview",WS_OVERLAPPEDWINDOW,restoredWindow.left,restoredWindow.top,width,height,nullptr,appMenu,instance,nullptr);if(!hwnd)return 1;
    if(!selfTest||(timingTest&&timingAudio)){timingAudioOpened=audio.open(audioLatency,(unsigned)gb.apu.sampleRate);if(!timingAudioOpened&&!selfTest)alert(tr(L"Audio device unavailable",L"Звуковое устройство недоступно"));}audio.setVolume(volume);
    ShowWindow(hwnd,selfTest&&!uiSelfTest?SW_HIDE:(restoredMaximized?SW_SHOWMAXIMIZED:show));UpdateWindow(hwnd);bool loaded=false;
    if(!requested.empty())loaded=openROM(requested);else if(!selfTest&&!recentROMs[0].empty()&&std::filesystem::exists(recentROMs[0]))loaded=openROM(recentROMs[0],true);
    if(!loaded&&requested.empty()){
        wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);const auto folder=std::filesystem::path(executable).parent_path();
        if(std::filesystem::exists(folder/L"demo.gbc"))loaded=openROM(folder/L"demo.gbc");else if(std::filesystem::exists(folder/L"demo.gb"))loaded=openROM(folder/L"demo.gb");}
    if(selfTest&&!loaded){DestroyWindow(hwnd);if(gamepadLibrary)FreeLibrary(gamepadLibrary);if(appMenu)DestroyMenu(appMenu);return 2;}
    if(uiSelfTest){selfTestExitCode=sameFile(uiTestReport,gb.mem.romPath)?2:(runUiSelfTest(uiTestReport)?0:5);SendMessageW(hwnd,WM_CLOSE,0,0);}
    if(timingTest){timingStartCycles=gb.totalCycles;timingStartFrames=gb.ppu.frameNumber;timingStart=std::chrono::steady_clock::now();resetPacing();}
    updateTitle();updateTimer();lastStatus=GetTickCount64();MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    if(gamepadLibrary)FreeLibrary(gamepadLibrary);if(appMenu)DestroyMenu(appMenu);return (int)msg.wParam;
}
