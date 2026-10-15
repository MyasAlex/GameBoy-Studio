#pragma once
// Included by windows_main.cpp after its basic controls/dialog helpers.
static constexpr UINT EX_PPU_WATCH=994,EX_PAD_EDIT=990,EX_PAD_SAVE=991,EX_PAD_LOAD=992,EX_COMPARE=985,EX_TAS_GRID=986,EX_TIMELINE_GRAPH=987,EX_HOMEBREW=988,EX_COMPAT=989,EX_DISCORD=944,EX_PAIR_SAVE=980,EX_PAIR_LOAD=981,EX_RA_LOGIN=940,EX_RA_LOGOUT=941,EX_RA_LIST=942,EX_RA_HARDCORE=943,EX_HARDWARE=960,EX_UPDATES=920,EX_PLUGIN=921,EX_CHEATS=800,EX_CHEAT_ADD=801,EX_CHEAT_TOGGLE=802,EX_CHEAT_CLEAR=803,EX_SEARCH=804,EX_SEARCH_CHANGED=805,EX_SEARCH_EQUAL=806,EX_FREEZE=807,
    EX_MEMORY=810,EX_WATCH_READ=811,EX_WATCH_WRITE=812,EX_INSPECT=813,EX_PROFILE=814,EX_LUA=815,EX_LUA_STOP=816,
    EX_CAMERA_OPEN=827,EX_CAMERA_OFF=828,EX_CAMERA_IMAGE=829,EX_NET_HOST=824,EX_NET_JOIN=825,EX_NET_OFF=826,EX_LINK=820,EX_DISCONNECT=821,EX_PRINTER=822,EX_PRINT_SAVE=823,
    EX_XBRZ=855,EX_GPU=830,EX_SCALE=831,EX_COLOR=835,EX_RATE=840,EX_EQ=844,EX_CHVOL=850,EX_FLAC=875,EX_VIDEO=876,
    EX_CHECKPOINT=880,EX_TIMELINE=881,EX_BESS_SAVE=882,EX_BESS_LOAD=883,EX_LIBRARY=884,EX_FAVORITE=885,EX_NOTE=886,EX_COVER=887,EX_PRESET_SAVE=888,EX_PRESET_LOAD=889,
    EX_TAS=890,EX_TAS_PLAY=891,EX_VIRTUAL=892,EX_HOTKEY=900;
static constexpr std::array<UINT,15> hotCommands={IDM_OPEN,IDM_RESET,IDM_PAUSE,IDM_SAVE,IDM_LOAD,IDM_FRAME,IDM_SCREEN,IDM_FULLSCREEN,IDM_STUDIO,0,0,IDM_SAVE_AS,IDM_LOAD_FROM,IDM_PAL0,IDM_MOVIE_STOP};
static const std::array<const wchar_t*,15> hotNames={L"Open ROM",L"Reset",L"Pause",L"Save",L"Load",L"One frame",L"Screenshot",L"Fullscreen",L"Studio",L"Hold turbo",L"Hold rewind",L"Named save",L"Named load",L"Next palette",L"Stop movie"};
static std::string utf8(std::wstring_view text){const auto n=WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),nullptr,0,nullptr,nullptr);std::string out(n,' ');WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),out.data(),n,nullptr,nullptr);return out;}
struct TextPrompt {std::wstring title,label,value;HWND edit=nullptr;bool accepted=false,done=false,secret=false;};
static LRESULT CALLBACK textPromptProc(HWND w,UINT message,WPARAM wp,LPARAM lp){auto* data=(TextPrompt*)GetWindowLongPtrW(w,GWLP_USERDATA);
    if(message==WM_CREATE){data=(TextPrompt*)((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)data);
        CreateWindowW(L"STATIC",data->label.c_str(),WS_CHILD|WS_VISIBLE,12,12,570,44,w,nullptr,appInstance,nullptr);
        data->edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",data->value.c_str(),WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|(data->secret?ES_PASSWORD:0),12,62,570,28,w,(HMENU)10,appInstance,nullptr);SendMessageW(data->edit,EM_SETLIMITTEXT,4096,0);
        CreateWindowW(L"BUTTON",L"OK",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,392,106,90,30,w,(HMENU)IDOK,appInstance,nullptr);
        CreateWindowW(L"BUTTON",tr(L"Cancel",L"Отмена"),WS_CHILD|WS_VISIBLE,492,106,90,30,w,(HMENU)IDCANCEL,appInstance,nullptr);SetFocus(data->edit);return 0;}
    if(!data)return DefWindowProcW(w,message,wp,lp);
    if(message==WM_COMMAND&&(LOWORD(wp)==IDOK||LOWORD(wp)==IDCANCEL)){if(LOWORD(wp)==IDOK){std::wstring value(4097,L'\0');value.resize(GetWindowTextW(data->edit,value.data(),4097));data->value=value;data->accepted=true;}DestroyWindow(w);return 0;}
    if(message==WM_CLOSE){DestroyWindow(w);return 0;}if(message==WM_DESTROY){data->done=true;return 0;}return DefWindowProcW(w,message,wp,lp);}
static std::optional<std::wstring> prompt(const wchar_t* title,const wchar_t* label,std::wstring value=L"",bool secret=false){
    const bool wasPaused=paused;setPaused(true);TextPrompt data{title,label,value};data.secret=secret;WNDCLASSW cls{};cls.lpfnWndProc=textPromptProc;cls.hInstance=appInstance;cls.lpszClassName=L"GBSTextPrompt09";cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
    EnableWindow(hwnd,FALSE);HWND w=CreateWindowExW(WS_EX_DLGMODALFRAME,cls.lpszClassName,title,WS_CAPTION|WS_SYSMENU|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,612,186,hwnd,nullptr,appInstance,&data);
    if(!w){EnableWindow(hwnd,TRUE);return {};}
    MSG msg{};while(!data.done&&GetMessageW(&msg,nullptr,0,0)>0){if(msg.message==WM_KEYDOWN&&(msg.wParam==VK_RETURN||msg.wParam==VK_ESCAPE)){SendMessageW(w,WM_COMMAND,msg.wParam==VK_RETURN?IDOK:IDCANCEL,0);continue;}if(!IsDialogMessageW(w,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
    EnableWindow(hwnd,TRUE);SetForegroundWindow(hwnd);if(!wasPaused)setPaused(false);return data.accepted?std::optional(data.value):std::nullopt;
}
struct ListPrompt {std::vector<std::wstring> rows;std::vector<std::filesystem::path> covers;std::vector<std::size_t> visible;HWND list=nullptr,search=nullptr,image=nullptr;HBITMAP bitmap=nullptr;bool done=false;std::optional<std::size_t> chosen;};
static void updateListPrompt(ListPrompt& data){wchar_t query[512]{};GetWindowTextW(data.search,query,512);CharLowerBuffW(query,(DWORD)wcslen(query));SendMessageW(data.list,LB_RESETCONTENT,0,0);data.visible.clear();
    for(std::size_t i=0;i<data.rows.size();++i){auto lower=data.rows[i];CharLowerBuffW(lower.data(),(DWORD)lower.size());if(lower.find(query)!=std::wstring::npos){data.visible.push_back(i);SendMessageW(data.list,LB_ADDSTRING,0,(LPARAM)data.rows[i].c_str());}}}
static LRESULT CALLBACK listPromptProc(HWND w,UINT message,WPARAM wp,LPARAM lp){auto* data=(ListPrompt*)GetWindowLongPtrW(w,GWLP_USERDATA);
    if(message==WM_CREATE){data=(ListPrompt*)((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)data);
        data->search=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,12,12,626,26,w,(HMENU)11,appInstance,nullptr);
        data->list=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL|LBS_NOTIFY,12,48,450,300,w,(HMENU)12,appInstance,nullptr);
        SendMessageW(data->list,LB_SETHORIZONTALEXTENT,1500,0);data->image=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_BITMAP,478,60,160,144,w,nullptr,appInstance,nullptr);
        CreateWindowW(L"STATIC",tr(L"Search above. Select a row to see its thumbnail. Double-click or Open.",L"Поиск сверху. Выберите строку для миниатюры. Двойной щелчок или Открыть."),WS_CHILD|WS_VISIBLE,478,216,160,96,w,nullptr,appInstance,nullptr);
        CreateWindowW(L"BUTTON",tr(L"Open",L"Открыть"),WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,448,366,90,30,w,(HMENU)IDOK,appInstance,nullptr);
        CreateWindowW(L"BUTTON",tr(L"Cancel",L"Отмена"),WS_CHILD|WS_VISIBLE,548,366,90,30,w,(HMENU)IDCANCEL,appInstance,nullptr);updateListPrompt(*data);return 0;}
    if(!data)return DefWindowProcW(w,message,wp,lp);
    if(message==WM_COMMAND){if(LOWORD(wp)==11&&HIWORD(wp)==EN_CHANGE){updateListPrompt(*data);return 0;}
        const auto selected=SendMessageW(data->list,LB_GETCURSEL,0,0);const bool valid=selected>=0&&(std::size_t)selected<data->visible.size();
        if(LOWORD(wp)==12&&HIWORD(wp)==LBN_SELCHANGE&&valid){if(data->bitmap){SendMessageW(data->image,STM_SETIMAGE,IMAGE_BITMAP,0);DeleteObject(data->bitmap);data->bitmap=nullptr;}
            const auto at=data->visible[(std::size_t)selected];if(at<data->covers.size()&&!data->covers[at].empty()){data->bitmap=imaging::thumbnail(data->covers[at],160,144);SendMessageW(data->image,STM_SETIMAGE,IMAGE_BITMAP,(LPARAM)data->bitmap);}return 0;}
        if(LOWORD(wp)==IDOK||(LOWORD(wp)==12&&HIWORD(wp)==LBN_DBLCLK)){if(valid){data->chosen=data->visible[(std::size_t)selected];DestroyWindow(w);}return 0;}
        if(LOWORD(wp)==IDCANCEL){DestroyWindow(w);return 0;}}
    if(message==WM_CLOSE){DestroyWindow(w);return 0;}if(message==WM_DESTROY){if(data->bitmap)DeleteObject(data->bitmap);data->done=true;return 0;}return DefWindowProcW(w,message,wp,lp);}
static std::optional<std::size_t> chooseRow(const wchar_t* title,std::vector<std::wstring> rows,std::vector<std::filesystem::path> covers={}){
    const bool wasPaused=paused;setPaused(true);ListPrompt data;data.rows=std::move(rows);data.covers=std::move(covers);WNDCLASSW cls{};cls.lpfnWndProc=listPromptProc;cls.hInstance=appInstance;cls.lpszClassName=L"GBSListPrompt09";cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
    EnableWindow(hwnd,FALSE);HWND w=CreateWindowExW(WS_EX_DLGMODALFRAME,cls.lpszClassName,title,WS_CAPTION|WS_SYSMENU|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,666,448,hwnd,nullptr,appInstance,&data);
    if(w){MSG msg{};while(!data.done&&GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(w,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}}
    EnableWindow(hwnd,TRUE);SetForegroundWindow(hwnd);if(!wasPaused)setPaused(false);return data.chosen;
}
static std::filesystem::path gameFolder(){return configPath.parent_path()/L"Games"/wide(studio::hex(gb.romHash(),8).substr(1));}
static void extraLoadConfig(){
    gpuEnabled=setting(L"GPU",0)!=0;scaleFilter=std::clamp(setting(L"ScaleFilter",0),0,4);colorCorrection=std::clamp(setting(L"ColorCorrection",0),0,2);virtualCase=setting(L"VirtualCase",0)!=0;
    int rate=setting(L"SampleRate",44100);if(!gb.apu.setSampleRate(rate))gb.apu.setSampleRate(44100);
    eqPreset=std::clamp(setting(L"EQ",0),0,3);for(int i=0;i<4;++i)gb.apu.setChannelVolume(i,setting((L"ChannelVolume"+std::to_wstring(i)).c_str(),100));
    for(std::size_t i=0;i<hotkeys.size();++i)hotkeys[i]=std::clamp(setting((L"Hotkey"+std::to_wstring(i)).c_str(),hotkeys[i]),0,2047);
    wchar_t presenceId[64]{};GetPrivateProfileStringW(L"Studio",L"DiscordApplicationID",L"",presenceId,64,configPath.c_str());if(presenceId[0])discordPresence.configure(utf8(presenceId));
    controls::load(configPath.parent_path()/L"gamepads.profile",padProfiles);library.open(configPath.parent_path()/L"library.tsv");
}
static void extraSaveConfig(){if(selfTest)return;auto put=[&](const wchar_t* name,int value){WritePrivateProfileStringW(L"Studio",name,std::to_wstring(value).c_str(),configPath.c_str());};
    WritePrivateProfileStringW(L"Studio",L"DiscordApplicationID",wide(discordPresence.applicationId()).c_str(),configPath.c_str());
    put(L"GPU",gpuEnabled);put(L"ScaleFilter",scaleFilter);put(L"ColorCorrection",colorCorrection);put(L"VirtualCase",virtualCase);put(L"SampleRate",gb.apu.sampleRate);put(L"EQ",eqPreset);
    for(int i=0;i<4;++i)put((L"ChannelVolume"+std::to_wstring(i)).c_str(),gb.apu.channelVolume[i]);for(std::size_t i=0;i<hotkeys.size();++i)put((L"Hotkey"+std::to_wstring(i)).c_str(),hotkeys[i]);controls::save(configPath.parent_path()/L"gamepads.profile",padProfiles);library.save();
    if(!gb.mem.rom.empty()){const auto file=gameFolder()/L"display.ini";auto gamePut=[&](const wchar_t* key,int value){WritePrivateProfileStringW(L"Display",key,std::to_wstring(value).c_str(),file.c_str());};
        gamePut(L"GPU",gpuEnabled);gamePut(L"Scaler",scaleFilter);gamePut(L"Color",colorCorrection);gamePut(L"Palette",paletteIndex);gamePut(L"Effect",displayFilter);gamePut(L"Ghost",ghosting);gamePut(L"GhostWeight",ghostWeight);gamePut(L"Case",virtualCase);
        for(unsigned i=0;i<4;++i)gamePut((L"Custom"+std::to_wstring(i)).c_str(),customPalette[i]&0xFFFFFF);}

}
static void configureEQ(){static constexpr int presets[4][3]={{0,0,0},{6,0,-2},{-2,2,5},{-6,3,-4}};audio.setEqualizer(presets[eqPreset][0],presets[eqPreset][1],presets[eqPreset][2]);}
static void closeEditingPanels();
static void extraCartridgeLoaded(const std::filesystem::path& source){
    closeEditingPanels();if(linkWindow)DestroyWindow(linkWindow);achievementClient.loadGame();network.close();link.disconnect();printer.detach();if(!gbSecond.mem.rom.empty())gbSecond.mem.saveBattery();lua.stop();video.close();cheatEngine.setEnabled(true);cheatEngine.entries.clear();
    debugger.clearBusWatchpoints();debugger.clearBankBreakpoints();debugger.resetProfile();timeline.open(gameFolder()/L"Checkpoints");
    if(!selfTest){library.add(gb.romHash(),source,gb.mem.gameTitle.empty()?utf8(source.stem().wstring()):gb.mem.gameTitle);const auto cheats=gameFolder()/L"cheats.txt";if(std::filesystem::exists(cheats))cheatEngine.load(cheats);}
    if(!selfTest){const auto file=gameFolder()/L"display.ini";auto gameGet=[&](const wchar_t* key,int fallback){return (int)GetPrivateProfileIntW(L"Display",key,fallback,file.c_str());};
        gpuEnabled=gameGet(L"GPU",gpuEnabled)!=0;scaleFilter=std::clamp(gameGet(L"Scaler",scaleFilter),0,4);colorCorrection=std::clamp(gameGet(L"Color",colorCorrection),0,2);paletteIndex=std::clamp(gameGet(L"Palette",paletteIndex),0,7);displayFilter=std::clamp(gameGet(L"Effect",displayFilter),0,2);ghosting=gameGet(L"Ghost",ghosting)!=0;ghostWeight=std::clamp(gameGet(L"GhostWeight",ghostWeight),10,80);virtualCase=gameGet(L"Case",virtualCase)!=0;
        for(unsigned i=0;i<4;++i)customPalette[i]=0xFF000000u|((u32)gameGet((L"Custom"+std::to_wstring(i)).c_str(),customPalette[i]&0xFFFFFF)&0xFFFFFF);}
    if(audio.rate()!=(unsigned)gb.apu.sampleRate&&!selfTest)audio.open(audioLatency,(unsigned)gb.apu.sampleRate);configureEQ();
}
static void frameCompleted(){if(!network.active())cheatEngine.applyRAM();if(lua.active()&&!lua.onFrame())alert(wide(lua.lastError()).c_str());
    if(video.recording()&&!video.append(gb.ppu.pixels)){video.close();alert(tr(L"Video recording stopped: output error or size limit",L"Запись видео остановлена: ошибка или предел размера"));}
    if(achievementClient.signedIn()&&!link.connected())achievementClient.frame();
    if(!selfTest)library.played(gb.romHash(),GB_FRAME);if(inspectorWindow)InvalidateRect(inspectorWindow,nullptr,FALSE);
    if(link.connected()){gbSecond.apu.clearQueue();if(linkWindow)InvalidateRect(linkWindow,nullptr,FALSE);}
    if(sendRumble){XINPUT_VIBRATION vibration{};if(!paused&&gb.mem.rumbleActive)vibration.wLeftMotorSpeed=vibration.wRightMotorSpeed=30000;sendRumble(primaryPadIndex,&vibration);}
}
static bool extraKey(WPARAM key,bool down){
    if(!down){if((hotkeys[9]&255)==(int)key&&(heldTurbo||turbo)){heldTurbo=false;turbo=false;resetPacing();audio.flush(gb.apu);}if((hotkeys[10]&255)==(int)key)heldRewind=false;return false;}
    if(key==VK_CONTROL||key==VK_SHIFT||key==VK_MENU)return false;
    const int value=(int)key|(GetKeyState(VK_CONTROL)<0?256:0)|(GetKeyState(VK_SHIFT)<0?512:0)|(GetKeyState(VK_MENU)<0?1024:0);
    if(captureHotkey>=0){if(key!=VK_ESCAPE){const int previous=hotkeys[captureHotkey];for(std::size_t i=0;i<hotkeys.size();++i)if((int)i!=captureHotkey&&hotkeys[i]==value)hotkeys[i]=previous;hotkeys[captureHotkey]=value;}
        captureHotkey=-1;saveConfig();rebuildMenus();alert(tr(L"Hotkey updated",L"Горячая клавиша изменена"));return true;}
    for(std::size_t i=0;i<hotkeys.size();++i)if(value==hotkeys[i]){if(i==9){heldTurbo=turbo=true;resetPacing();audio.flush(gb.apu);}else if(i==10){heldRewind=true;lastRewind=0;}
        else if(i==13){paletteIndex=(paletteIndex+1)%8;resetDisplay();prepareDisplay();}else command(hotCommands[i]);return true;}
    if(value==('0'|256)){command(IDM_SPEED+2);return true;}return false;
}
static LRESULT CALLBACK inspectorProc(HWND w,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_CREATE){auto button=[&](const wchar_t* name,int x,int id){CreateWindowW(L"BUTTON",name,WS_CHILD|WS_VISIBLE,8+x*102,8,98,26,w,(HMENU)(UINT_PTR)id,appInstance,nullptr);};
        button(L"Tiles bank 0",0,1);button(L"Tiles bank 1",1,2);button(L"OAM sprites",2,3);button(L"APU scope",3,4);button(L"BG map",4,5);button(L"Palettes",5,6);button(L"PPU FIFO",6,7);return 0;}
    if(message==WM_COMMAND){inspectorMode=std::clamp((int)LOWORD(wp)-1,0,6);InvalidateRect(w,nullptr,FALSE);return 0;}
    if(message==WM_LBUTTONDOWN&&inspectorMode<2&&!gb.mem.rom.empty()){
        const int x=LOWORD(lp)-8,y=HIWORD(lp)-48;if(x>=0&&x<512&&y>=0&&y<192){const int tile=(y/16)*32+x/16,px=(x%16)/2,py=(y%16)/2;
            const std::size_t at=(std::size_t)inspectorMode*8192+tile*16+py*2;const unsigned bit=7-px;const unsigned raw=((gb.mem.vram[at]>>bit)&1)|(((gb.mem.vram[at+1]>>bit)&1)<<1),next=(raw+1)&3;
            gb.mem.pokeVRAM((unsigned)inspectorMode,at%8192,(u8)((gb.mem.vram[at]&~(1<<bit))|((next&1)<<bit)));gb.mem.pokeVRAM((unsigned)inspectorMode,(at+1)%8192,(u8)((gb.mem.vram[at+1]&~(1<<bit))|(((next>>1)&1)<<bit)));InvalidateRect(w,nullptr,FALSE);}return 0;}
    if(message==WM_LBUTTONDOWN&&inspectorMode>=2&&inspectorMode!=3&&!gb.mem.rom.empty()){
        const int x=LOWORD(lp)-8,y=HIWORD(lp)-48;
        if(inspectorMode==2&&x>=0&&x<512&&y>=0&&y<256){const int index=(y/64)*16+x/32;if(index<40){const auto value=prompt(L"OAM sprite",L"Hex Y X tile flags (four bytes)",wide(studio::hex(gb.mem.oam[index*4]).substr(1)+" "+studio::hex(gb.mem.oam[index*4+1]).substr(1)+" "+studio::hex(gb.mem.oam[index*4+2]).substr(1)+" "+studio::hex(gb.mem.oam[index*4+3]).substr(1)));
            if(value){std::istringstream in(utf8(*value));std::array<unsigned,4> bytes{};if(in>>std::hex>>bytes[0]>>bytes[1]>>bytes[2]>>bytes[3]&&std::all_of(bytes.begin(),bytes.end(),[](unsigned v){return v<=255;}))for(int n=0;n<4;++n)gb.mem.pokeOAM((std::size_t)index*4+n,(u8)bytes[n]);}}}
        else if(inspectorMode==4&&x>=0&&x<512&&y>=0&&y<512){const auto at=(std::size_t)((gb.mem.io[0x40]&8)?0x1C00:0x1800)+(y/16)*32+x/16;const auto value=prompt(L"Background map cell",L"Hex tile number and CGB attributes",wide(studio::hex(gb.mem.vram[at]).substr(1)+" "+studio::hex(gb.mem.vram[8192+at]).substr(1)));if(value){unsigned tile=0,attr=0;std::istringstream in(utf8(*value));if(in>>std::hex>>tile>>attr&&tile<256&&attr<256){gb.mem.pokeVRAM(0,at,(u8)tile);if(gb.mem.cgbMode)gb.mem.pokeVRAM(1,at,(u8)attr);}}}
        else if(inspectorMode==5&&x>=0&&x<640&&y>=0&&y<96){const int palette=x/80,color=(x%80)/20;const bool obj=y>=48;auto& data=obj?gb.mem.objPalette:gb.mem.bgPalette;const u32 rgb=gb.ppu.color(data,palette,color);
            static COLORREF custom[16]{};CHOOSECOLORW choose{};choose.lStructSize=sizeof(choose);choose.hwndOwner=w;choose.rgbResult=RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255);choose.lpCustColors=custom;choose.Flags=CC_FULLOPEN|CC_RGBINIT;
            if(ChooseColorW(&choose)){const u16 raw=(u16)((GetRValue(choose.rgbResult)>>3)|((GetGValue(choose.rgbResult)>>3)<<5)|((GetBValue(choose.rgbResult)>>3)<<10));const auto at=(std::size_t)palette*8+color*2;gb.mem.pokePalette(obj,at,(u8)raw);gb.mem.pokePalette(obj,at+1,(u8)(raw>>8));}}
        InvalidateRect(w,nullptr,FALSE);return 0;}
    if(message==WM_PAINT){PAINTSTRUCT paint{};HDC dc=BeginPaint(w,&paint);RECT rect{};GetClientRect(w,&rect);FillRect(dc,&rect,(HBRUSH)(COLOR_WINDOW+1));
        if(inspectorMode==3){const COLORREF colors[4]={RGB(210,55,60),RGB(50,140,70),RGB(50,95,210),RGB(170,80,190)};
            for(int c=0;c<4;++c){HPEN pen=CreatePen(PS_SOLID,1,colors[c]);auto old=SelectObject(dc,pen);for(int n=0;n<512;++n){const auto at=(gb.apu.scopePosition+n)%512;const int y=70+c*85-gb.apu.scope[c][at]/512;
                if(n)LineTo(dc,8+n,y);else MoveToEx(dc,8,y,nullptr);}SelectObject(dc,old);DeleteObject(pen);}GBSInspect detail{};if(gb.backendInspect&&gb.backendInspect(detail)){for(unsigned c=0;c<4;++c){const std::wstring line=L"CH"+std::to_wstring(c+1)+L" "+(detail.active[c]?L"ON":L"OFF")+L"  PCM="+std::to_wstring(detail.sample[c])+L"  volume="+std::to_wstring(detail.volume[c])+L"  length="+std::to_wstring(detail.length[c])+L"  period="+std::to_wstring(detail.period[c])+L"  timer="+std::to_wstring(detail.countdown[c]);TextOutW(dc,8,440+(int)c*28,line.c_str(),(int)line.size());}}
            std::wstring label=std::to_wstring(gb.apu.sampleRate)+L" Hz | "+wide(audio.backendName())+L" | queued "+std::to_wstring((int)audio.queuedMilliseconds())+L" ms";TextOutW(dc,8,400,label.c_str(),(int)label.size());
        }else if(inspectorMode==6){GBSInspect detail{};if(gb.backendInspect&&gb.backendInspect(detail)){
            static const wchar_t* states[]={L"Tile T1",L"Tile T2",L"Plane 0 T1",L"Plane 0 T2",L"Plane 1 T1",L"Plane 1 T2",L"Push"};
            std::wstring line=L"Mode "+std::to_wstring(detail.mode)+L" | fetcher "+(detail.fetcher<7?std::wstring(states[detail.fetcher]):std::to_wstring(detail.fetcher))+L" | x="+std::to_wstring(detail.x)+L" | window y="+std::to_wstring(detail.window_y);TextOutW(dc,8,52,line.c_str(),(int)line.size());
            line=L"Tile "+wide(studio::hex(detail.tile))+L" | attributes "+wide(studio::hex(detail.attributes))+L" | selected sprites "+std::to_wstring(detail.selected_objects);TextOutW(dc,8,82,line.c_str(),(int)line.size());
            for(unsigned row=0;row<2;++row){const auto size=row?detail.obj_size:detail.bg_size;line=(row?L"OBJ FIFO: ":L"BG FIFO: ")+std::to_wstring(size)+L" pixels";TextOutW(dc,8,124+(int)row*112,line.c_str(),(int)line.size());
                for(unsigned i=0;i<16;++i){const auto value=(row?detail.obj_pixels[i]:detail.bg_pixels[i])&3;const u32 rgb=PPU::palettes[1][value];RECT box{8+(int)i*38,152+(int)row*112,42+(int)i*38,196+(int)row*112};HBRUSH brush=CreateSolidBrush(i<size?RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255):RGB(225,225,225));FillRect(dc,&box,brush);DeleteObject(brush);FrameRect(dc,&box,(HBRUSH)GetStockObject(GRAY_BRUSH));const auto label=std::to_wstring(i);TextOutW(dc,box.left,box.bottom+4,label.c_str(),(int)label.size());}}
            const wchar_t* hint=L"Pause / step in Studio to inspect instruction boundaries. Enable PPU bus watches to capture VRAM/OAM reads.";TextOutW(dc,8,386,hint,(int)wcslen(hint));}
        }else if(inspectorMode==5){for(int obj=0;obj<2;++obj)for(int palette=0;palette<8;++palette)for(int color=0;color<4;++color){const u32 rgb=gb.ppu.color(obj?gb.mem.objPalette:gb.mem.bgPalette,palette,color);RECT swatch{8+palette*80+color*20,48+obj*48,8+palette*80+(color+1)*20,80+obj*48};HBRUSH brush=CreateSolidBrush(RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255));FillRect(dc,&swatch,brush);DeleteObject(brush);}const wchar_t* label=L"BG palettes above, OBJ below. Click a color to edit RGB15.";TextOutW(dc,8,164,label,(int)wcslen(label));
        }else{const int width=inspectorMode==4?256:256,height=inspectorMode==2?128:inspectorMode==4?256:96;std::vector<u32> pixels((std::size_t)width*height,0xFFFFFFFF);
            if(inspectorMode<2){for(int tile=0;tile<384;++tile)for(int y=0;y<8;++y)for(int x=0;x<8;++x){const auto at=inspectorMode*8192+tile*16+y*2;const unsigned raw=((gb.mem.vram[at]>>(7-x))&1)|(((gb.mem.vram[at+1]>>(7-x))&1)<<1);
                pixels[(tile/32*8+y)*width+tile%32*8+x]=PPU::palettes[1][raw];}}
            else if(inspectorMode==2){for(int i=0;i<40;++i)for(int y=0;y<((gb.mem.io[0x40]&4)?16:8);++y)for(int x=0;x<8;++x){const u8 attr=gb.mem.oam[i*4+3];const int sy=(attr&0x40)?((gb.mem.io[0x40]&4)?15:7)-y:y,sx=(attr&0x20)?x:7-x;
                const unsigned bank=gb.mem.cgbMode?(attr>>3)&1:0;const int at=(gb.mem.oam[i*4+2]&((gb.mem.io[0x40]&4)?0xFE:0xFF))*16+sy*2;const unsigned raw=((gb.mem.vramByte(bank,(u16)at)>>sx)&1)|(((gb.mem.vramByte(bank,(u16)(at+1))>>sx)&1)<<1);
                pixels[(i/16*32+y)*width+i%16*16+x]=gb.mem.cgbMode?gb.ppu.color(gb.mem.objPalette,attr&7,raw):PPU::palettes[1][raw];}}
            else for(int y=0;y<256;++y)for(int x=0;x<256;++x){const auto value=gb.ppu.backgroundAt(false,x,y);pixels[y*256+x]=gb.mem.cgbMode?gb.ppu.color(gb.mem.bgPalette,value.attr&7,value.raw):PPU::palettes[1][value.raw];}
            BITMAPINFO info{};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
            StretchDIBits(dc,8,48,width*2,height*2,0,0,width,height,pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);
            const wchar_t* label=tr(L"Tiles: click a pixel to cycle its color while paused. Palette/OAM is live.",L"Тайлы: щелчок меняет цвет пикселя на паузе. Палитра/OAM обновляются.");TextOutW(dc,8,inspectorMode==4?566:inspectorMode<2?256:448,label,(int)wcslen(label));}
        EndPaint(w,&paint);return 0;}
    if(message==WM_CLOSE){DestroyWindow(w);return 0;}if(message==WM_DESTROY){inspectorWindow=nullptr;return 0;}return DefWindowProcW(w,message,wp,lp);
}
static void showInspector(){setPaused(true);if(inspectorWindow){SetForegroundWindow(inspectorWindow);return;}WNDCLASSW cls{};cls.lpfnWndProc=inspectorProc;cls.hInstance=appInstance;cls.lpszClassName=L"GBSPPUAPU09";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
    inspectorWindow=CreateWindowExW(0,cls.lpszClassName,tr(L"PPU / APU inspector",L"Инспектор PPU / APU"),WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,820,700,hwnd,nullptr,appInstance,nullptr);}
static LRESULT CALLBACK linkWindowProc(HWND w,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_PAINT){PAINTSTRUCT paint{};HDC dc=BeginPaint(w,&paint);RECT rect{};GetClientRect(w,&rect);BITMAPINFO info{};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=160;info.bmiHeader.biHeight=-144;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        StretchDIBits(dc,0,0,rect.right,rect.bottom,0,0,160,144,gbSecond.ppu.pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);EndPaint(w,&paint);return 0;}
    if(message==WM_CLOSE){network.close();cheatEngine.setEnabled(true);link.disconnect();setPaused(true);gbSecond.mem.saveBattery();DestroyWindow(w);return 0;}if(message==WM_DESTROY){linkWindow=nullptr;return 0;}return DefWindowProcW(w,message,wp,lp);}
static bool loadSecondCartridge(const std::filesystem::path& source){
    const auto data=Memory::loadBytes(source,8*1024*1024);if(data.size()<32768){gbSecond.mem.loadError="Invalid second ROM.";return false;}
    const auto folder=configPath.parent_path()/L"Player2"/wide(studio::hex(saves::hash(data),8).substr(1));std::error_code ec;std::filesystem::create_directories(folder,ec);
    const auto copy=folder/(data[0x143]&0x80?L"game.gbc":L"game.gb");if(ec||!Memory::writeAtomic(copy,{(const char*)data.data(),data.size()})){gbSecond.mem.loadError="Could not create player 2 cartridge storage.";return false;}
    for(const auto* extension:{L".sav",L".rtc"}){auto inputSave=source;inputSave.replace_extension(extension);auto outputSave=copy;outputSave.replace_extension(extension);
        if(std::filesystem::exists(inputSave)&&!std::filesystem::exists(outputSave))std::filesystem::copy_file(inputSave,outputSave,std::filesystem::copy_options::none,ec);}
    auto nativeIn=source;nativeIn+=L".sbbattery";auto nativeOut=copy;nativeOut+=L".sbbattery";if(std::filesystem::exists(nativeIn)&&!std::filesystem::exists(nativeOut))std::filesystem::copy_file(nativeIn,nativeOut,std::filesystem::copy_options::none,ec);
    return gbSecond.load(copy,selectedModel);
}
static void showSecond(){if(linkWindow)DestroyWindow(linkWindow);WNDCLASSW cls{};cls.lpfnWndProc=linkWindowProc;cls.hInstance=appInstance;cls.lpszClassName=L"GBSSecond09";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
    linkWindow=CreateWindowExW(WS_EX_NOACTIVATE,cls.lpszClassName,tr(L"Link player 2: WASD, G/H, T/Y",L"Link игрок 2: WASD, G/H, T/Y"),WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,500,480,hwnd,nullptr,appInstance,nullptr);}
static std::string issueReport(){auto report=studio::romReport(gb);const auto end=report.rfind('}');std::ostringstream host;
    host<<",\n  \"host\": {\"audio_backend\": "<<studio::jsonString(audio.backendName())<<", \"sample_rate\": "<<gb.apu.sampleRate<<", \"audio_underruns\": "<<audio.underflows()<<", \"audio_overflows\": "<<audio.overflows()<<", \"gpu_enabled\": "<<(gpuEnabled?"true":"false")<<", \"gpu_software\": "<<(gpu.isSoftware()?"true":"false")<<", \"network\": "<<studio::jsonString(network.status())<<", \"cheats\": "<<cheatEngine.entries.size()<<", \"lua_active\": "<<(lua.active()?"true":"false")<<"}\n";
    host<<",\n  \"host_timing\": {\"selected_speed\": "<<speeds[speedIndex]<<", \"turbo\": "<<(turbo?"true":"false")<<", \"paused\": "<<(paused?"true":"false")<<", \"measured_frame_calls_per_second\": "<<measuredFPS<<", \"base_clock_hz\": "<<gb.clockRate<<"}\n";
    report.insert(end,host.str());return report;}
#include "studio_panels_ui.hpp"
static bool extraCommand(UINT id){
    if(achievementClient.hardcore()&&(id==EX_COMPARE||id==EX_TAS_GRID||id==EX_TIMELINE_GRAPH||id==EX_PAIR_LOAD||id==EX_PPU_WATCH||id==EX_MEMORY||id==EX_WATCH_READ||id==EX_WATCH_WRITE||id==EX_INSPECT||id==EX_PROFILE||id==EX_LUA||id==EX_PLUGIN||id==EX_BESS_LOAD||id==EX_TIMELINE||id==EX_TAS||id==EX_TAS_PLAY||id==EX_CHEATS||id==EX_CHEAT_ADD||id==EX_CHEAT_TOGGLE||id==EX_CHEAT_CLEAR||id==EX_SEARCH||id==EX_SEARCH_CHANGED||id==EX_SEARCH_EQUAL||id==EX_FREEZE||id==EX_LINK||id==EX_NET_HOST||id==EX_NET_JOIN)){alert(L"Disable Hardcore to use this feature");return true;}

    if(network.active()&&(id==EX_PPU_WATCH||id==EX_MEMORY||id==EX_PLUGIN||id==EX_CHEAT_ADD||id==EX_CHEATS||id==EX_CHEAT_CLEAR||id==EX_FREEZE||id==EX_RATE)){alert(L"Editing and cheats are disabled during a network session");return true;}
    if((link.connected()||network.active())&&(id==EX_BESS_LOAD||id==EX_BESS_SAVE||id==EX_CHECKPOINT||id==EX_TIMELINE||id==EX_TAS||id==EX_TAS_PLAY||id==EX_PROFILE||id==EX_LUA)){alert(L"Disconnect Link / network before single-console Studio operations");return true;}
    if(id==EX_PAD_EDIT){const auto value=prompt(L"Gamepad profile",L"Player (1/2), action (0=R 1=L 2=U 3=D 4=A 5=B 6=Select 7=Start), hex button mask. XInput A=1000 B=2000 X=4000 Y=8000, triggers=10000/20000. Example: 1 4 1000");if(value){unsigned player=0,action=0,mask=0;std::istringstream in(utf8(*value));if(in>>player>>action>>std::hex>>mask&&player>=1&&player<=2&&action<8&&mask<=0x3FFFF)padProfiles[player-1].buttons[action]=mask;}}
    else if(id==EX_PAD_SAVE||id==EX_PAD_LOAD){auto path=configPath.parent_path()/L"gamepads.profile";if(chooseFile(path,L"Gamepad profile (*.profile)\0*.profile\0",L"profile",id==EX_PAD_SAVE,L"Gamepad profiles")){const bool ok=id==EX_PAD_SAVE?controls::save(path,padProfiles):controls::load(path,padProfiles);alert(ok?L"Gamepad profile saved / loaded":L"Invalid gamepad profile");}}
    else if(id==EX_COMPARE)showComparison();else if(id==EX_TAS_GRID)showTasEditor();else if(id==EX_TIMELINE_GRAPH)showTimelineGraph();else if(id==EX_HOMEBREW)showHomebrew();else if(id==EX_COMPAT)showCompatibility();
    else if(id==EX_RA_LOGIN){setPaused(true);const auto user=prompt(L"RetroAchievements",L"Username");if(user){const auto token=prompt(L"RetroAchievements token",L"Account token from a previous emulator login. It stays in memory until logout.",L"",true);if(token)achievementClient.login(utf8(*user),utf8(*token));}}
    else if(id==EX_DISCORD){if(discordPresence.enabled())discordPresence.disable();else {const auto id=prompt(L"Discord Rich Presence",L"Discord application client ID (create an app in the Discord Developer Portal). No login token is needed.");if(id&&!discordPresence.configure(utf8(*id)))alert(L"Invalid numeric Discord client ID");}}
    else if(id==EX_RA_LOGOUT){achievementClient.logout();cheatEngine.setEnabled(true);}
    else if(id==EX_RA_LIST){auto data=achievementClient.rows();std::vector<std::wstring> rows;for(const auto& row:data)rows.push_back(wide(row));chooseRow(L"RetroAchievements",rows);}
    else if(id==EX_RA_HARDCORE){const bool on=!achievementClient.hardcore();if(on){closeEditingPanels();lua.stop();cheatEngine.setEnabled(false);movie.clear();debuggerRunning=false;heldRewind=false;rewindBuffer.clear();speedIndex=2;}else cheatEngine.setEnabled(true);achievementClient.setHardcore(on);}
    else if(id>=EX_HARDWARE&&id<=EX_HARDWARE+14){
        const auto model=(HardwareModel)(id-EX_HARDWARE);network.close();link.disconnect();printer.detach();gb.mem.saveBattery();
        if(gb.setModel(model)){selectedModel=model;stopMovie();rewindBuffer.clear();audio.flush(gb.apu);resetDisplay();prepareDisplay();}else alert(wide(gb.mem.loadError).c_str());
    }
    else if(id==EX_UPDATES){setPaused(true);const auto result=updates::check();
        if(!result.error.empty())MessageBoxW(hwnd,wide(result.error).c_str(),L"Update check",MB_OK|MB_ICONINFORMATION);
        else {const auto text=tr(L"Latest published tag: ",L"Последний опубликованный тег: ")+wide(result.tag)+tr(L"\nOpen the release page?",L"\nОткрыть страницу релиза?");
            if(MessageBoxW(hwnd,text.c_str(),L"GameBoy Studio updates",MB_YESNO|MB_ICONINFORMATION)==IDYES)ShellExecuteW(hwnd,L"open",L"https://github.com/MyasAlex/GameBoy-Studio/releases/latest",nullptr,nullptr,SW_SHOWNORMAL);}}
    else if(id==EX_PLUGIN){const auto folder=configPath.parent_path()/L"Plugins";std::error_code ec;std::filesystem::create_directories(folder,ec);std::vector<std::filesystem::path> paths;std::vector<std::wstring> rows;
        if(!ec)for(const auto& entry:std::filesystem::directory_iterator(folder,ec)){if(entry.is_regular_file()&&entry.path().extension()==L".lua"){paths.push_back(entry.path());rows.push_back((lua.contains(entry.path())?L"[ON] ":L"[OFF] ")+entry.path().filename().wstring());if(rows.size()==256)break;}}
        if(rows.empty())MessageBoxW(hwnd,(tr(L"Put .lua scripts in: ",L"Поместите .lua скрипты в: ")+folder.wstring()).c_str(),L"Lua plugins",MB_OK);
        else {const auto row=chooseRow(L"Toggle Lua plugin (maximum 8)",rows);if(row&&!lua.toggle(paths[*row]))alert(wide(lua.lastError()).c_str());}}
    else if(id==EX_XBRZ){scaleFilter=4;}
    else if(id==EX_GPU){gpuEnabled=!gpuEnabled;if(!gpuEnabled)gpu.hide();}
    else if(id>=EX_SCALE&&id<EX_SCALE+4){scaleFilter=(int)(id-EX_SCALE);}
    else if(id>=EX_COLOR&&id<EX_COLOR+3){colorCorrection=(int)(id-EX_COLOR);resetDisplay();prepareDisplay();}
    else if(id>=EX_RATE&&id<EX_RATE+3){static constexpr int rates[]={44100,48000,96000};video.close();audio.stopRecording();gb.apu.setSampleRate(rates[id-EX_RATE]);audio.open(audioLatency,(unsigned)gb.apu.sampleRate);audio.setVolume(volume);configureEQ();}
    else if(id>=EX_EQ&&id<EX_EQ+4){eqPreset=(int)(id-EX_EQ);configureEQ();}
    else if(id>=EX_CHVOL&&id<EX_CHVOL+20){gb.apu.setChannelVolume((int)(id-EX_CHVOL)/5,(int)((id-EX_CHVOL)%5)*25);}
    else if(id==EX_VIRTUAL)virtualCase=!virtualCase;
    else if(id==EX_FLAC){auto path=gb.mem.romPath;path.replace_extension(L".flac");if(chooseFile(path,L"FLAC (*.flac)\0*.flac\0",L"flac",true,L"Record lossless FLAC"))alert(audio.startRecording(path)?tr(L"FLAC recording started",L"Запись FLAC начата"):wide(audio.lastError()).c_str());}
    else if(id==EX_VIDEO){if(video.recording()){video.close();alert(tr(L"AVI saved",L"AVI сохранено"));}else {auto path=gb.mem.romPath;path.replace_extension(L".avi");if(chooseFile(path,L"AVI video (*.avi)\0*.avi\0",L"avi",true,L"Record Motion JPEG + PCM AVI video"))alert(video.open(path,(unsigned)gb.apu.sampleRate,gb.clockRate,imaging::jpeg)?tr(L"AVI recording started",L"Запись AVI начата"):tr(L"Choose a new AVI filename",L"Выберите новое имя AVI"));}}
    else if(id==EX_BESS_SAVE||id==EX_BESS_LOAD){auto path=gb.mem.romPath;path.replace_extension(L".bess");if(chooseFile(path,L"BESS state (*.bess;*.state)\0*.bess;*.state\0",L"bess",id==EX_BESS_SAVE,L"Portable BESS save state")){
        std::string error;const bool ok=id==EX_BESS_SAVE?saves::save(gb,path):saves::load(gb,path,&error);if(ok){audio.open(audioLatency,(unsigned)gb.apu.sampleRate);configureEQ();audio.setVolume(volume);resetDisplay();prepareDisplay();rewindBuffer.clear();}alert(ok?tr(L"BESS saved / loaded",L"BESS сохранено / загружено"):wide(error.empty()?"BESS operation failed":error).c_str());}}
    else if(id==EX_CHECKPOINT){const auto comment=prompt(L"Checkpoint",tr(L"Comment. Restoring an older checkpoint creates a new branch on the next save.",L"Комментарий. После загрузки старой точки новое сохранение создаст ветку."));if(comment)alert(timeline.capture(gb,utf8(*comment))?tr(L"Checkpoint and thumbnail saved",L"Точка и миниатюра сохранены"):tr(L"Checkpoint failed",L"Ошибка сохранения точки"));}
    else if(id==EX_TIMELINE){std::vector<std::wstring> rows;std::vector<std::filesystem::path> covers;for(const auto& point:timeline.points){rows.push_back(wide(point.id+" <- "+point.parent+" | "+point.comment+" | cycles="+std::to_string(point.cycles)));covers.push_back(timeline.thumbnail(covers.size()));}
        const auto row=chooseRow(tr(L"Timeline / branches",L"История / ветки"),rows,covers);if(row){if(timeline.restore(gb,*row)){audio.open(audioLatency,(unsigned)gb.apu.sampleRate);configureEQ();setPaused(true);rewindBuffer.clear();resetDisplay();prepareDisplay();}else alert(tr(L"Checkpoint rejected",L"Точка отклонена"));}}
    else if(id==EX_LIBRARY){std::vector<std::wstring> rows;std::vector<std::filesystem::path> covers;for(const auto& game:library.games){rows.push_back((game.favorite?L"★ ":L"")+wide(game.title)+L" | "+std::to_wstring(game.cycles/GB_CLOCK/60)+L" min | "+wide(game.note+" | "+game.path));covers.push_back(std::filesystem::u8path(game.cover));}
        const auto row=chooseRow(tr(L"Game library",L"Библиотека игр"),rows,covers);if(row)openROM(std::filesystem::u8path(library.games[*row].path));}
    else if(id==EX_FAVORITE||id==EX_NOTE||id==EX_COVER){for(auto& game:library.games)if(game.hash==gb.romHash()){
        if(id==EX_FAVORITE)game.favorite=!game.favorite;else if(id==EX_NOTE){const auto note=prompt(L"Game note",L"Note",wide(game.note));if(note)game.note=utf8(*note);}
        else {std::filesystem::path path;if(chooseFile(path,L"Image (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0",L"png",false,L"Select cover image"))game.cover=collection::Library::studioPath(path);}library.save();break;}}
    else if(id==EX_PRESET_SAVE||id==EX_PRESET_LOAD){const auto path=gameFolder()/L"display.preset";if(id==EX_PRESET_SAVE){std::error_code ec;std::filesystem::create_directories(gameFolder(),ec);std::ostringstream out;out<<paletteIndex<<' '<<displayFilter<<' '<<gpuEnabled<<' '<<scaleFilter<<' '<<colorCorrection<<' '<<ghosting<<' '<<ghostWeight;Memory::writeAtomic(path,out.str(),true);}
        else {const auto bytes=Memory::loadBytes(path,256);std::istringstream in(std::string(bytes.begin(),bytes.end()));int pal=0,filter=0,gpuValue=0,scaleValue=0,color=0,ghost=0,weight=0;if(in>>pal>>filter>>gpuValue>>scaleValue>>color>>ghost>>weight){paletteIndex=std::clamp(pal,0,7);displayFilter=std::clamp(filter,0,2);gpuEnabled=gpuValue!=0;scaleFilter=std::clamp(scaleValue,0,4);colorCorrection=std::clamp(color,0,2);ghosting=ghost!=0;ghostWeight=std::clamp(weight,10,80);resetDisplay();prepareDisplay();}}}
    else if(id==EX_CHEAT_ADD){const auto code=prompt(L"Game Genie / GameShark / Action Replay",tr(L"Game Genie 6/9 digits, or GameShark / AR types 01, 80-8F (SRAM bank), 90-97 (CGB RAM bank).",L"Game Genie 6/9 цифр или GameShark / AR типы 01, 80-8F (банк SRAM), 90-97 (банк CGB RAM)."));if(code){const bool ok=cheatEngine.add(utf8(*code));alert(ok?tr(L"Cheat active",L"Чит включён"):tr(L"Invalid or unsupported code",L"Неверный или неподдерживаемый код"));}}
    else if(id==EX_CHEATS||id==EX_CHEAT_TOGGLE){std::vector<std::wstring> rows;for(const auto& cheat:cheatEngine.entries)rows.push_back((cheat.enabled?L"ON ":L"OFF ")+wide(cheat.code+" | "+cheat.description));const auto row=chooseRow(L"Cheat manager: select to toggle",rows);if(row)cheatEngine.entries[*row].enabled=!cheatEngine.entries[*row].enabled;}
    else if(id==EX_CHEAT_CLEAR)cheatEngine.entries.clear();
    else if(id==EX_SEARCH){ramSearch.begin(gb.mem);alert(tr(L"RAM search initialized; narrow using changed/equal",L"Поиск RAM начат; уточняйте изменённые / равные"));}
    else if(id==EX_SEARCH_CHANGED){ramSearch.filter(gb.mem,cheats::Compare::Changed);alert((std::to_wstring(ramSearch.results().size())+L" candidates").c_str());}
    else if(id==EX_SEARCH_EQUAL){const auto value=prompt(L"RAM value search",L"Hex byte value (00-FF)");if(value){u64 v=0;if(studio::parseUnsigned(utf8(*value),v,255,16))ramSearch.filter(gb.mem,cheats::Compare::Equal,(u8)v);alert((std::to_wstring(ramSearch.results().size())+L" candidates").c_str());}}
    else if(id==EX_FREEZE){std::vector<std::wstring> rows;const auto& results=ramSearch.results();for(std::size_t i=0;i<std::min<std::size_t>(results.size(),2048);++i)rows.push_back(wide(studio::hex(results[i],4)+" = "+studio::hex(gb.mem.read(results[i]))));
        const auto row=chooseRow(L"Select RAM address to freeze",rows);if(row){const u16 address=results[*row];const auto code="01"+studio::hex(gb.mem.read(address)).substr(1)+studio::hex(address&255).substr(1)+studio::hex(address>>8).substr(1);cheatEngine.add(code,"RAM freeze");}}
    else if(id==EX_MEMORY){const auto value=prompt(L"Memory editor",L"Hex address and hex byte, e.g. C123 63");if(value){std::istringstream in(utf8(*value));unsigned address=0,byte=0;if(in>>std::hex>>address>>byte&&address>=0x8000&&address<=65535&&byte<=255){gb.mem.write((u16)address,(u8)byte);refreshStudio();}else alert(L"Invalid RAM/I/O address or byte");}}
    else if(id==EX_PPU_WATCH){debugger.setPPUWatches(!debugger.ppuWatches());}
    else if(id==EX_WATCH_READ||id==EX_WATCH_WRITE){const auto value=prompt(L"Bus watchpoint",L"Hex address");if(value){u64 address=0;if(studio::parseUnsigned(utf8(*value),address,65535,16))debugger.addBusWatchpoint((u16)address,id==EX_WATCH_READ?studio::BusAccess::Read:studio::BusAccess::Write);}}
    else if(id==EX_INSPECT)showInspector();else if(id==EX_PROFILE){debugger.setProfiling(!debugger.isProfiling());if(debugger.isProfiling()){debugger.resetProfile();showStudio();debuggerRunning=true;debuggerSkipBreakpoint=true;setPaused(false);}}
    else if(id==EX_LUA){std::filesystem::path path;if(chooseFile(path,L"Lua script (*.lua)\0*.lua\0",L"lua",false,L"Run bounded Lua script"))alert(lua.load(path)?tr(L"Lua script active",L"Скрипт Lua запущен"):wide(lua.lastError()).c_str());}
    else if(id==EX_LUA_STOP)lua.stop();
    else if(id==EX_PAIR_SAVE||id==EX_PAIR_LOAD){if(network.active()||!link.connected()){alert(L"Connect two local ROMs and disconnect network first");return true;}setPaused(true);auto path=gameFolder()/L"link.gbpair";
        if(chooseFile(path,L"Linked consoles (*.gbpair)\0*.gbpair\0",L"gbpair",id==EX_PAIR_SAVE,L"State of both linked consoles")){bool ok=false;if(id==EX_PAIR_SAVE){const auto bytes=devices::savePair(gb,gbSecond,link);ok=!bytes.empty()&&Memory::writeAtomic(path,{(const char*)bytes.data(),bytes.size()},true);}else ok=devices::loadPair(gb,gbSecond,link,Memory::loadBytes(path,12*1024*1024));alert(ok?L"Linked state saved / restored":L"Linked state failed validation");audio.flush(gb.apu);prepareDisplay();}}
    else if(id==EX_NET_HOST||id==EX_NET_JOIN){
        if(!link.connected()){alert(L"First connect two local ROMs using Link, then host/join on both PCs.");return true;}
        const auto address=prompt(L"Network Link",id==EX_NET_HOST?L"TCP port (8765 by default). Both PCs must load the same two ROMs.":L"Host IPv4 and TCP port, e.g. 192.168.1.5 8765",id==EX_NET_HOST?L"8765":L"127.0.0.1 8765");
        if(address){std::istringstream in(utf8(*address));unsigned port=8765;std::string ip;bool valid=false;
            if(id==EX_NET_HOST){if(in>>port)valid=network.host(gb,gbSecond,link,port);}else if(in>>ip>>port)valid=network.join(gb,gbSecond,link,ip,port);
            movie.clear();lua.stop();debuggerRunning=false;cheatEngine.setEnabled(!valid);audio.flush(gb.apu);if(valid)setPaused(false);alert(wide(valid?network.status():network.lastErrorText()).c_str());}}
    else if(id==EX_NET_OFF){network.close();cheatEngine.setEnabled(true);if(!gbSecond.mem.rom.empty())gbSecond.mem.saveBattery();setPaused(true);}
    else if(id==EX_LINK){if(gb.mem.rom.empty()){alert(L"Open player 1 ROM first");return true;}std::filesystem::path path;if(chooseFile(path,L"Game Boy ROM (*.gb;*.gbc)\0*.gb;*.gbc\0",L"gb",false,L"Second player's ROM")){
        network.close();printer.detach();link.disconnect();gbSecond.mem.saveBattery();if(loadSecondCartridge(path)){gb.reset();gbSecond.reset();rewindBuffer.clear();movie.clear();lua.stop();debuggerRunning=false;link.connect(gb,gbSecond);showSecond();setPaused(false);}else alert(wide(gbSecond.mem.loadError).c_str());}}
    else if(id==EX_DISCONNECT){network.close();cheatEngine.setEnabled(true);link.disconnect();gbSecond.mem.saveBattery();if(linkWindow)DestroyWindow(linkWindow);}
    else if(id==EX_CAMERA_OPEN){alert(webcam.open(hwnd)?tr(L"Webcam input enabled for Game Boy Camera",L"Веб-камера включена для Game Boy Camera"):wide(webcam.lastError()).c_str());}
    else if(id==EX_CAMERA_OFF){webcam.close();alert(tr(L"Webcam disconnected",L"Веб-камера отключена"));}
    else if(id==EX_CAMERA_IMAGE){std::filesystem::path path;if(chooseFile(path,L"Camera BMP (*.bmp)\0*.bmp\0",L"bmp",false,L"Game Boy Camera: use BMP image")){
        HBITMAP bitmap=(HBITMAP)LoadImageW(nullptr,path.c_str(),IMAGE_BITMAP,128,112,LR_LOADFROMFILE|LR_CREATEDIBSECTION);if(bitmap){BITMAPINFO info{};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=128;info.bmiHeader.biHeight=-112;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
            std::array<u32,128*112> pixels{};HDC dc=GetDC(hwnd);const int read=GetDIBits(dc,bitmap,0,112,pixels.data(),&info,DIB_RGB_COLORS);ReleaseDC(hwnd,dc);DeleteObject(bitmap);
            if(read==112){std::array<u8,128*112> gray{};for(std::size_t i=0;i<gray.size();++i)gray[i]=(u8)((77*((pixels[i]>>16)&255)+150*((pixels[i]>>8)&255)+29*(pixels[i]&255))>>8);webcam.setImage(gray);alert(L"Camera bitmap input ready");}else alert(L"Camera bitmap could not be decoded");}}}
    else if(id==EX_PRINTER){network.close();link.disconnect();printer.attach(gb);alert(tr(L"Game Boy Printer attached",L"Game Boy Printer подключён"));}
    else if(id==EX_PRINT_SAVE){std::filesystem::path path=gameFolder()/L"printer.bmp";if(chooseFile(path,L"Printer BMP (*.bmp)\0*.bmp\0",L"bmp",true,L"Save last printed page"))alert(printer.saveLast(path)?L"Printer page saved":L"No printable page yet");}
    else if(id==EX_TAS){movie.stop();const auto value=prompt(L"TAS editor",tr(L"First frame, last frame, hex input byte. Bits: R L U D A B Select Start. Example: 0 59 10",L"Первый кадр, последний, hex-байт ввода. Биты: R L U D A B Select Start. Пример: 0 59 10"));if(value){std::istringstream in(utf8(*value));std::size_t first=0,last=0;unsigned mask=0;const bool valid=(bool)(in>>first>>last>>std::hex>>mask);alert(valid&&mask<=255&&movie.edit(first,last,(u8)mask)?L"TAS input range updated":L"Record/load an input movie, stop it, then edit a valid frame range");}}
    else if(id==EX_TAS_PLAY){if(movie.playCurrent(gb)){audio.flush(gb.apu);rewindBuffer.clear();setPaused(false);}else alert(L"No matching input movie is loaded");}
    else if(id>=EX_HOTKEY&&id<EX_HOTKEY+hotkeys.size()){captureHotkey=(int)(id-EX_HOTKEY);alert(tr(L"Press a new hotkey (Ctrl/Shift/Alt supported); Esc cancels",L"Нажмите сочетание (Ctrl/Shift/Alt поддерживаются); Esc отменяет"));}
    else return false;
    if(!gb.mem.rom.empty()){std::error_code ec;std::filesystem::create_directories(gameFolder(),ec);cheatEngine.save(gameFolder()/L"cheats.txt");}
    rebuildMenus();saveConfig();refreshStudio();InvalidateRect(hwnd,nullptr,FALSE);return true;
}
static void extendMenus(HMENU main,HMENU file,HMENU time,HMENU videoMenu,HMENU settings,HMENU help){
    auto item=[](HMENU menu,UINT id,const wchar_t* label,bool checked=false){AppendMenuW(menu,MF_STRING|(checked?MF_CHECKED:0),id,label);};auto sub=[](HMENU menu,HMENU child,const wchar_t* label){AppendMenuW(menu,MF_POPUP,(UINT_PTR)child,label);};
    item(help,EX_DISCORD,L"Discord Rich Presence (application ID)...",discordPresence.enabled());
    item(help,EX_COMPARE,L"Hardware comparison (four consoles)");item(help,EX_COMPAT,L"Compatibility database");item(file,EX_HOMEBREW,L"Homebrew catalog / import");
    HMENU achievementsMenu=CreatePopupMenu();item(achievementsMenu,EX_RA_LOGIN,L"RetroAchievements: sign in with token...");item(achievementsMenu,EX_RA_LOGOUT,L"Sign out");item(achievementsMenu,EX_RA_LIST,L"Achievements / progress");item(achievementsMenu,EX_RA_HARDCORE,L"Hardcore restrictions",achievementClient.hardcore());sub(main,achievementsMenu,L"RetroAchievements");
    item(settings,EX_PAD_EDIT,L"Remap gamepad action...");item(settings,EX_PAD_SAVE,L"Export gamepad profiles...");item(settings,EX_PAD_LOAD,L"Import gamepad profiles...");
    HMENU hardware=CreatePopupMenu();const wchar_t* modelNames[]={L"Auto",L"DMG (CPU B)",L"CGB (CPU E)",L"Pocket",L"Light",L"CGB 0",L"CGB A",L"CGB B",L"CGB C",L"CGB D",L"CGB E",L"Game Boy Advance",L"Super Game Boy",L"Super Game Boy 2",L"Super Game Boy PAL"};
    for(unsigned i=0;i<15;++i)item(hardware,EX_HARDWARE+i,modelNames[i],(unsigned)selectedModel==i);sub(settings,hardware,tr(L"Hardware revision",L"Ревизия консоли"));
    item(file,EX_LIBRARY,tr(L"Game library / search",L"Библиотека игр / поиск"));item(file,EX_FAVORITE,tr(L"Toggle favorite for this game",L"Избранное для этой игры"));item(file,EX_NOTE,tr(L"Edit game note",L"Заметка об игре"));item(file,EX_COVER,tr(L"Select cover image",L"Выбрать обложку"));
    item(time,EX_BESS_SAVE,L"Export portable BESS...");item(time,EX_BESS_LOAD,L"Import BESS...");item(time,EX_CHECKPOINT,tr(L"Checkpoint with comment",L"Точка с комментарием"));item(time,EX_TIMELINE,tr(L"Timeline / branches / thumbnails",L"История / ветки / миниатюры"));item(time,EX_TAS,L"TAS: edit frame input range...");item(time,EX_TAS_PLAY,L"TAS: replay edited movie");item(time,EX_TAS_GRID,L"TAS: graphical input editor / preview");item(time,EX_TIMELINE_GRAPH,L"Graphical timeline / branches");
    item(videoMenu,EX_GPU,L"Direct3D 11 / GPU CRT",gpuEnabled);HMENU scales=CreatePopupMenu();item(scales,EX_SCALE,L"Nearest",scaleFilter==0);item(scales,EX_SCALE+1,L"Scale2x",scaleFilter==1);item(scales,EX_SCALE+2,L"Scale3x",scaleFilter==2);item(scales,EX_SCALE+3,L"HQ2x (GPU / CPU)",scaleFilter==3);item(scales,EX_XBRZ,L"xBRZ 3x (filter process)",scaleFilter==4);sub(videoMenu,scales,L"Pixel scaling");
    HMENU color=CreatePopupMenu();item(color,EX_COLOR,L"Raw RGB",colorCorrection==0);item(color,EX_COLOR+1,L"CGB LCD color matrix",colorCorrection==1);item(color,EX_COLOR+2,L"Gamma 1.15",colorCorrection==2);sub(videoMenu,color,L"Color correction");item(videoMenu,EX_PRESET_SAVE,L"Save this game's display preset");item(videoMenu,EX_PRESET_LOAD,L"Load this game's display preset");item(videoMenu,EX_VIDEO,L"AVI recording start / stop",video.recording());item(videoMenu,EX_VIRTUAL,L"Virtual console frame",virtualCase);
    HMENU rates=CreatePopupMenu();static const unsigned values[]={44100,48000,96000};for(int i=0;i<3;++i){const auto label=std::to_wstring(values[i])+L" Hz";item(rates,EX_RATE+i,label.c_str(),gb.apu.sampleRate==(int)values[i]);}sub(settings,rates,L"APU sample rate");
    HMENU eq=CreatePopupMenu();for(int i=0;i<4;++i){static const wchar_t* names[]={L"Flat",L"Bass",L"Treble",L"Voice"};item(eq,EX_EQ+i,names[i],eqPreset==i);}sub(settings,eq,L"3-band equalizer");
    HMENU channelVolumes=CreatePopupMenu();for(int channel=0;channel<4;++channel){HMENU levels=CreatePopupMenu();for(int i=0;i<5;++i){auto label=std::to_wstring(i*25)+L"%";item(levels,EX_CHVOL+channel*5+i,label.c_str(),gb.apu.channelVolume[channel]==i*25);}sub(channelVolumes,levels,(L"Channel "+std::to_wstring(channel+1)).c_str());}sub(settings,channelVolumes,L"Channel volumes");item(settings,EX_FLAC,L"Record FLAC...");
    HMENU hot=CreatePopupMenu();for(std::size_t i=0;i<hotkeys.size();++i){const int key=hotkeys[i];auto label=std::wstring(hotNames[i])+L" : "+(key&256?L"Ctrl+":L"")+(key&512?L"Shift+":L"")+(key&1024?L"Alt+":L"")+keyName(key&255);item(hot,EX_HOTKEY+(UINT)i,label.c_str());}sub(settings,hot,tr(L"Remap emulator hotkeys",L"Горячие клавиши эмулятора"));
    HMENU cheats=CreatePopupMenu();item(cheats,EX_CHEAT_ADD,L"Add Game Genie / GameShark / AR...");item(cheats,EX_CHEATS,L"Manager: enable / disable...");item(cheats,EX_CHEAT_CLEAR,L"Clear cheats");item(cheats,EX_SEARCH,L"RAM: start search");item(cheats,EX_SEARCH_CHANGED,L"RAM: changed since last scan");item(cheats,EX_SEARCH_EQUAL,L"RAM: equals value...");item(cheats,EX_FREEZE,L"RAM: results / freeze...");sub(main,cheats,tr(L"Cheats / RAM",L"Читы / RAM"));
    HMENU connection=CreatePopupMenu();item(connection,EX_LINK,tr(L"Connect a second local Game Boy",L"Подключить второй Game Boy"));item(connection,EX_DISCONNECT,L"Disconnect link");item(connection,EX_PAIR_SAVE,L"Save both linked consoles...");item(connection,EX_PAIR_LOAD,L"Restore both linked consoles...");item(connection,EX_PRINTER,L"Attach Game Boy Printer");item(connection,EX_PRINT_SAVE,L"Save last printer page...");item(connection,EX_CAMERA_IMAGE,L"Camera: use BMP image...");item(connection,EX_CAMERA_OPEN,L"Camera: enable webcam (VfW)...");item(connection,EX_CAMERA_OFF,L"Camera: disable webcam");item(connection,EX_NET_HOST,L"Network: host TCP session...");item(connection,EX_NET_JOIN,L"Network: join host IPv4...");item(connection,EX_NET_OFF,L"Network: disconnect / resynchronize");sub(main,connection,L"Link / Network / Printer / IR");
    item(help,EX_MEMORY,L"Edit memory byte...");item(help,EX_WATCH_READ,L"Watch CPU / DMA read...");item(help,EX_PPU_WATCH,L"Include PPU VRAM / OAM bus",debugger.ppuWatches());item(help,EX_WATCH_WRITE,L"Watch CPU write...");item(help,EX_INSPECT,L"Graphical PPU / APU / tile editor");item(help,EX_PROFILE,L"CPU profiler (Studio execution)",debugger.isProfiling());item(help,EX_LUA,L"Run Lua script...");item(help,EX_LUA_STOP,L"Stop Lua script");item(help,EX_PLUGIN,L"User Lua plugins...");item(help,EX_UPDATES,tr(L"Check for updates (on request)",L"Проверить обновления (по запросу)"));
}
