#pragma once
// Included by windows_main.cpp. These checks exercise the production window
// procedure on a desktop, while replacing physical XInput/keyboard device reads
// with deterministic signals. They do not certify real devices or audible audio.
namespace windows_qa {
static XINPUT_VIBRATION lastVibration{};
static unsigned vibrationCalls=0;
static DWORD WINAPI disconnectedPad(DWORD,XINPUT_STATE*){return ERROR_DEVICE_NOT_CONNECTED;}
static DWORD WINAPI captureVibration(DWORD,XINPUT_VIBRATION* value){lastVibration=*value;++vibrationCalls;return ERROR_SUCCESS;}
static SHORT WINAPI heldTiltKeys(int key){return key==VK_NUMPAD4||key==VK_NUMPAD8?(SHORT)0x8000:0;}
}
static bool runUiSelfTest(const std::filesystem::path& reportPath){
    std::vector<std::pair<std::string,bool>> checks;
    auto check=[&](const char* name,bool passed){checks.emplace_back(name,passed);};
    const auto savedReadGamepad=readGamepad;const auto savedSendRumble=sendRumble;const auto savedReadAsyncKey=readAsyncKey;
    readGamepad=windows_qa::disconnectedPad;sendRumble=windows_qa::captureVibration;
    readAsyncKey=windows_qa::heldTiltKeys;pollInput(true);
    ShowWindow(hwnd,SW_SHOW);SetForegroundWindow(hwnd);SetFocus(hwnd);paused=false;focusPaused=false;
    check("visible main window obtains foreground focus",IsWindowVisible(hwnd)&&GetForegroundWindow()==hwnd&&GetFocus()==hwnd);

    SendMessageW(hwnd,WM_KEYDOWN,'Z',0);
    check("keyboard press reaches the emulated A button",keyboard['Z']&&(desiredJoypad&0x10)&&(InputMovie::joypad(gb.mem)&0x10));
    SendMessageW(hwnd,WM_KEYUP,'Z',0);
    check("keyboard release clears the emulated A button",!keyboard['Z']&&!(desiredJoypad&0x10)&&!(InputMovie::joypad(gb.mem)&0x10));
    SendMessageW(hwnd,WM_KEYDOWN,'P',0);
    const bool initiallyPaused=paused;
    SendMessageW(hwnd,WM_KEYDOWN,'P',(LPARAM)(1ULL<<30));
    check("repeated pause key does not toggle twice",initiallyPaused&&paused);
    SendMessageW(hwnd,WM_KEYUP,'P',0);SendMessageW(hwnd,WM_KEYDOWN,'P',0);SendMessageW(hwnd,WM_KEYUP,'P',0);
    check("next distinct pause key resumes",!paused);

    virtualCase=true;InvalidateRect(hwnd,nullptr,FALSE);UpdateWindow(hwnd);
    const auto aButton=virtualButtons[4];const LPARAM point=MAKELPARAM((aButton.left+aButton.right)/2,(aButton.top+aButton.bottom)/2);
    SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,point);
    check("virtual A button captures mouse and presses A",aButton.right>aButton.left&&GetCapture()==hwnd&&(virtualInput&0x10)&&(desiredJoypad&0x10));
    SendMessageW(hwnd,WM_LBUTTONUP,0,point);
    check("mouse release clears virtual A and capture",GetCapture()!=hwnd&&!virtualInput&&!(desiredJoypad&0x10));

    // Moving capture to another window in the same thread used to release that
    // window's new capture from WM_CAPTURECHANGED.
    HWND child=CreateWindowExW(0,L"STATIC",L"QA capture target",WS_CHILD,0,0,1,1,hwnd,nullptr,appInstance,nullptr);
    SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,point);
    if(child)SetCapture(child);
    check("capture transfer releases virtual input and preserves new owner",child&&GetCapture()==child&&!virtualInput&&!(desiredJoypad&0x10));
    ReleaseCapture();if(child)DestroyWindow(child);

    pollInput(true);
    check("foreground numpad tilt signal reaches cartridge",gb.mem.motion.tiltX==112&&gb.mem.motion.tiltY==112);
    SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessageW(hwnd,WM_KEYDOWN,'Z',0);
    turbo=heldTurbo=heldRewind=true;
    HWND other=CreateWindowExW(0,L"STATIC",L"GameBoy Studio QA focus target",WS_OVERLAPPEDWINDOW|WS_VISIBLE,10,10,240,120,nullptr,nullptr,appInstance,nullptr);
    if(other){SetForegroundWindow(other);SetFocus(other);}
    pollInput(true);
    check("focus transfer reaches a second top-level window",other&&GetForegroundWindow()==other&&GetFocus()==other);
    check("focus loss releases keyboard, virtual buttons and capture",std::none_of(keyboard.begin(),keyboard.end(),[](bool down){return down;})&&!virtualInput&&!desiredJoypad&&GetCapture()!=hwnd);
    check("focus loss clears held turbo and rewind",!turbo&&!heldTurbo&&!heldRewind);
    check("focus loss pauses a running game",paused&&focusPaused);
    check("background numpad tilt cannot change cartridge input",gb.mem.motion.tiltX==0&&gb.mem.motion.tiltY==0);

    // Hardcore can keep emulation running when focus is lost. Rumble must stay
    // off in that state even though paused is false.
    paused=false;gb.mem.rumbleActive=true;windows_qa::vibrationCalls=0;frameCompleted();
    check("background execution keeps rumble silent",windows_qa::vibrationCalls>0&&!windows_qa::lastVibration.wLeftMotorSpeed&&!windows_qa::lastVibration.wRightMotorSpeed);
    paused=true;focusPaused=true;SetForegroundWindow(hwnd);SetFocus(hwnd);
    check("focus return resumes a game paused by focus loss",GetForegroundWindow()==hwnd&&!paused&&!focusPaused);
    windows_qa::vibrationCalls=0;frameCompleted();
    check("foreground cartridge rumble reaches XInput callback",windows_qa::vibrationCalls>0&&windows_qa::lastVibration.wLeftMotorSpeed==30000&&windows_qa::lastVibration.wRightMotorSpeed==30000);
    gb.mem.rumbleActive=false;if(other)DestroyWindow(other);

    SendMessageW(hwnd,WM_COMMAND,IDM_SPEED+6,0);turbo=heldTurbo=true;
    SendMessageW(hwnd,WM_COMMAND,IDM_SPEED+2,0);
    check("normal-speed menu clears turbo and selects 1x",speedIndex==2&&!turbo&&!heldTurbo);
    SendMessageW(hwnd,WM_COMMAND,IDM_SLOT+9,0);
    check("quick-save slot menu selects slot nine",stateSlot==9);
    SendMessageW(hwnd,WM_COMMAND,IDM_FULLSCREEN,0);const bool entered=fullscreen;
    SendMessageW(hwnd,WM_KEYDOWN,VK_ESCAPE,0);SendMessageW(hwnd,WM_KEYUP,VK_ESCAPE,0);
    check("fullscreen command and Escape restore window",entered&&!fullscreen);

    readGamepad=savedReadGamepad;sendRumble=savedSendRumble;readAsyncKey=savedReadAsyncKey;
    const bool passed=std::all_of(checks.begin(),checks.end(),[](const auto& item){return item.second;});
    const bool wine=GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"wine_get_version")!=nullptr;
    std::ostringstream report;report<<"{\n  \"schema_version\": 1,\n  \"status\": \""<<(passed?"PASS":"FAIL")
        <<"\",\n  \"runtime\": \""<<(wine?"wine":"windows")<<"\",\n  \"native_manual_validation\": false,\n  \"checks\": [";
    for(std::size_t i=0;i<checks.size();++i){if(i)report<<',';report<<"\n    {\"name\": "<<studio::jsonString(checks[i].first)<<", \"status\": \""<<(checks[i].second?"PASS":"FAIL")<<"\"}";}
    report<<"\n  ],\n  \"scope\": \"Visible Win32 window messages, focus/capture transitions, menus and fullscreen. XInput and asynchronous tilt-key reads are deterministic test doubles. Physical controllers, GPU output, audible sound and human usability are outside this automated check.\"\n}\n";
    return Memory::writeAtomic(reportPath,report.str())&&passed;
}
