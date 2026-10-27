#pragma once
// Native editing and comparison panels, included after Studio dialog helpers.
static HWND comparisonWindow=nullptr,tasWindow=nullptr,timelineWindow=nullptr;
static std::array<std::unique_ptr<GameBoy>,4> comparisonMachines;
static std::array<std::wstring,4> comparisonNames;
static std::size_t tasFirst=0,tasCursor=0,tasTarget=0;static bool tasSeeking=false;
static std::size_t timelineFirst=0;
static bool tasCheatsWereEnabled=false;
static void closeEditingPanels(){for(HWND window:{studioWindow,inspectorWindow,comparisonWindow,tasWindow,timelineWindow})if(window)DestroyWindow(window);debuggerRunning=false;}
static void drawFrame(HDC dc,const RECT& rect,std::span<const u32> pixels){BITMAPINFO info{};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=160;info.bmiHeader.biHeight=-144;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    StretchDIBits(dc,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,0,0,160,144,pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);}
static LRESULT CALLBACK comparisonProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_TIMER){u8 keys=0;for(int i=0;i<8;++i)if(GetForegroundWindow()==w&&(GetAsyncKeyState(inputKeys[i])&0x8000))keys|=(u8)(1<<i);
        for(auto& g:comparisonMachines)if(g){InputMovie::setJoypad(g->mem,keys);g->runFrame();g->apu.clearQueue();}InvalidateRect(w,nullptr,FALSE);return 0;}
    if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT client{};GetClientRect(w,&client);FillRect(dc,&client,(HBRUSH)(COLOR_WINDOW+1));
        for(unsigned n=0;n<4;++n)if(comparisonMachines[n]){const int width=client.right/2,height=client.bottom/2,x=(int)(n%2)*width,y=(int)(n/2)*height;RECT image{x+8,y+36,x+width-8,y+height-38};drawFrame(dc,image,comparisonMachines[n]->ppu.pixels);
            const auto& g=*comparisonMachines[n];std::size_t differences=0;if(n)for(std::size_t i=0;i<g.ppu.pixels.size();++i)if(g.ppu.pixels[i]!=comparisonMachines[0]->ppu.pixels[i])++differences;
            std::wstring label=comparisonNames[n]+L" | PC="+wide(studio::hex(g.cpu.PC,4))+L" | clock="+std::to_wstring(g.clockRate);TextOutW(dc,x+8,y+8,label.c_str(),(int)label.size());
            label=L"Frame "+std::to_wstring(g.ppu.frameNumber)+L" | differing pixels vs panel 1: "+std::to_wstring(differences);TextOutW(dc,x+8,y+height-28,label.c_str(),(int)label.size());}
        EndPaint(w,&ps);return 0;}
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}if(msg==WM_DESTROY){KillTimer(w,1);comparisonWindow=nullptr;for(auto& g:comparisonMachines)g.reset();return 0;}return DefWindowProcW(w,msg,wp,lp);
}
static void showComparison(){if(gb.mem.rom.empty())return;setPaused(true);if(comparisonWindow){SetForegroundWindow(comparisonWindow);return;}
    const auto models=gb.mem.cgbROM?std::array{HardwareModel::CGB0,HardwareModel::CGBC,HardwareModel::CGBD,HardwareModel::CGBE}:std::array{HardwareModel::DMG,HardwareModel::Pocket,HardwareModel::Light,HardwareModel::CGBE};
    comparisonNames=gb.mem.cgbROM?std::array<std::wstring,4>{L"CGB 0",L"CGB C",L"CGB D",L"CGB E"}:std::array<std::wstring,4>{L"DMG B",L"Pocket",L"Light",L"CGB E compatibility"};
    for(unsigned i=0;i<4;++i){comparisonMachines[i]=std::make_unique<GameBoy>();if(!comparisonMachines[i]->load(gb.mem.romPath,models[i])){for(auto& g:comparisonMachines)g.reset();alert(L"Comparison ROM could not load");return;}comparisonMachines[i]->setPalette(1);}
    WNDCLASSW cls{};cls.lpfnWndProc=comparisonProc;cls.hInstance=appInstance;cls.lpszClassName=L"GBSHardwareComparison10";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
    comparisonWindow=CreateWindowExW(0,cls.lpszClassName,tr(L"Hardware comparison — keyboard input goes to all panels",L"Сравнение моделей — клавиши управляют всеми экранами"),WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,1000,880,hwnd,nullptr,appInstance,nullptr);SetTimer(comparisonWindow,1,16,nullptr);
}
static LRESULT CALLBACK tasProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_CREATE){CreateWindowW(L"BUTTON",L"Jump to frame...",WS_CHILD|WS_VISIBLE,8,8,120,28,w,(HMENU)1,appInstance,nullptr);CreateWindowW(L"BUTTON",L"Preview to selected",WS_CHILD|WS_VISIBLE,140,8,150,28,w,(HMENU)2,appInstance,nullptr);
        CreateWindowW(L"BUTTON",L"Trim after selected",WS_CHILD|WS_VISIBLE,304,8,150,28,w,(HMENU)3,appInstance,nullptr);return 0;}
    if(msg==WM_COMMAND){if(LOWORD(wp)==1){const auto value=prompt(L"TAS frame",L"Frame number (decimal)",std::to_wstring(tasCursor));if(value){u64 frame=0;if(studio::parseUnsigned(utf8(*value),frame,movie.frames()?movie.frames()-1:0)){tasCursor=(std::size_t)frame;tasFirst=(tasCursor/24)*24;}}}
        else if(LOWORD(wp)==2){lua.stop();tasCheatsWereEnabled=cheatEngine.enabled();cheatEngine.setEnabled(false);if(movie.playCurrent(gb)){tasTarget=tasCursor+1;tasSeeking=true;SetTimer(w,1,1,nullptr);}else cheatEngine.setEnabled(tasCheatsWereEnabled);}
        else if(LOWORD(wp)==3){movie.stop();movie.trim(tasCursor+1);}InvalidateRect(w,nullptr,FALSE);return 0;}
    if(msg==WM_TIMER&&tasSeeking){for(int n=0;n<12&&movie.position()<tasTarget;++n){if(!movie.beforeFrame(gb)){tasSeeking=false;break;}gb.runFrame();gb.apu.clearQueue();}
        if(movie.position()>=tasTarget||!tasSeeking){movie.stop();tasSeeking=false;KillTimer(w,1);cheatEngine.setEnabled(tasCheatsWereEnabled&&!achievementClient.hardcore()&&!network.active());prepareDisplay();InvalidateRect(hwnd,nullptr,FALSE);}InvalidateRect(w,nullptr,FALSE);return 0;}
    if(msg==WM_MOUSEWHEEL){const int direction=GET_WHEEL_DELTA_WPARAM(wp)>0?-1:1;if(direction<0)tasFirst=tasFirst>=24?tasFirst-24:0;else if(tasFirst+24<movie.frames())tasFirst+=24;InvalidateRect(w,nullptr,FALSE);return 0;}
    if(msg==WM_LBUTTONDOWN&&!tasSeeking){const int x=LOWORD(lp),y=HIWORD(lp);if(y>=76&&y<76+24*22){const auto frame=tasFirst+(std::size_t)(y-76)/22;if(frame<movie.frames()){tasCursor=frame;const int bit=(x-112)/46;if(x>=112&&bit>=0&&bit<8){movie.stop();movie.edit(frame,frame,(u8)(movie.inputs()[frame]^(1<<bit)));}}}InvalidateRect(w,nullptr,FALSE);return 0;}
    if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT client{};GetClientRect(w,&client);FillRect(dc,&client,(HBRUSH)(COLOR_WINDOW+1));SetBkMode(dc,TRANSPARENT);
        const wchar_t* names[]={L"R",L"L",L"U",L"D",L"A",L"B",L"Select",L"Start"};for(int bit=0;bit<8;++bit)TextOutW(dc,112+bit*46,52,names[bit],(int)wcslen(names[bit]));
        for(std::size_t row=0;row<24&&tasFirst+row<movie.frames();++row){const auto frame=tasFirst+row;const int y=76+(int)row*22;if(frame==tasCursor){RECT selected{4,y,490,y+22};HBRUSH brush=CreateSolidBrush(RGB(219,232,252));FillRect(dc,&selected,brush);DeleteObject(brush);}
            const auto label=std::to_wstring(frame);TextOutW(dc,8,y+2,label.c_str(),(int)label.size());for(int bit=0;bit<8;++bit){RECT cell{112+bit*46,y,154+bit*46,y+20};FrameRect(dc,&cell,(HBRUSH)GetStockObject(GRAY_BRUSH));if(movie.inputs()[frame]&(1<<bit)){InflateRect(&cell,-4,-4);FillRect(dc,&cell,(HBRUSH)GetStockObject(BLACK_BRUSH));}}}
        if(!gb.mem.rom.empty()){RECT image{510,52,830,340};drawFrame(dc,image,gb.ppu.pixels);}std::wstring status=tasSeeking?L"Seeking: "+std::to_wstring(movie.position())+L" / "+std::to_wstring(tasTarget):L"Click a cell to toggle. Wheel pages. Save using Time / Save input movie.";TextOutW(dc,510,366,status.c_str(),(int)status.size());EndPaint(w,&ps);return 0;}
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}if(msg==WM_DESTROY){KillTimer(w,1);if(tasSeeking){movie.stop();cheatEngine.setEnabled(tasCheatsWereEnabled&&!achievementClient.hardcore()&&!network.active());}tasSeeking=false;tasWindow=nullptr;return 0;}return DefWindowProcW(w,msg,wp,lp);
}
static void showTasEditor(){setPaused(true);movie.stop();if(tasWindow){SetForegroundWindow(tasWindow);return;}tasCursor=tasFirst=0;WNDCLASSW cls{};cls.lpfnWndProc=tasProc;cls.hInstance=appInstance;cls.lpszClassName=L"GBSTas10";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
    tasWindow=CreateWindowExW(0,cls.lpszClassName,tr(L"TAS input editor",L"Редактор TAS"),WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,880,690,hwnd,nullptr,appInstance,nullptr);}
static LRESULT CALLBACK timelineProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_MOUSEWHEEL){const int direction=GET_WHEEL_DELTA_WPARAM(wp)>0?-1:1;if(direction<0)timelineFirst=timelineFirst>=8?timelineFirst-8:0;else if(timelineFirst+8<timeline.points.size())timelineFirst+=8;InvalidateRect(w,nullptr,FALSE);return 0;}
    if(msg==WM_LBUTTONDBLCLK){const auto row=timelineFirst+(std::size_t)std::max(0,(int)HIWORD(lp)-32)/40;if(row<timeline.points.size()&&timeline.restore(gb,row)){audio.open(audioLatency,(unsigned)gb.apu.sampleRate);configureEQ();audio.flush(gb.apu);rewindBuffer.clear();prepareDisplay();InvalidateRect(hwnd,nullptr,FALSE);InvalidateRect(w,nullptr,FALSE);}return 0;}
    if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT client{};GetClientRect(w,&client);FillRect(dc,&client,(HBRUSH)(COLOR_WINDOW+1));SetBkMode(dc,TRANSPARENT);const wchar_t* hint=L"Double-click a checkpoint to restore it. New checkpoints branch from the selected node. Wheel scrolls.";TextOutW(dc,8,8,hint,(int)wcslen(hint));
        std::map<std::string,std::size_t> rows;std::map<std::string,unsigned> lanes;unsigned nextLane=0;for(std::size_t i=0;i<timeline.points.size();++i){const auto& p=timeline.points[i];rows[p.id]=i;auto parent=lanes.find(p.parent);unsigned lane=parent==lanes.end()?nextLane++:parent->second;if(i&&timeline.points[i-1].id!=p.parent)lane=nextLane++;lanes[p.id]=lane;}
        HPEN pen=CreatePen(PS_SOLID,2,RGB(64,96,172));auto old=SelectObject(dc,pen);
        for(std::size_t i=timelineFirst;i<timeline.points.size()&&i<timelineFirst+16;++i){const auto& point=timeline.points[i];const int x=24+(int)(lanes[point.id]%5)*44,y=48+(int)(i-timelineFirst)*40;const auto parent=rows.find(point.parent);
            if(parent!=rows.end()&&parent->second>=timelineFirst&&parent->second<timelineFirst+16){MoveToEx(dc,24+(int)(lanes[point.parent]%5)*44,48+(int)(parent->second-timelineFirst)*40,nullptr);LineTo(dc,x,y);}Ellipse(dc,x-5,y-5,x+5,y+5);
            std::wstring label=(point.id==timeline.branchHead()?L"● ":L"")+wide(point.comment)+L" | "+std::to_wstring(point.cycles/GB_CLOCK)+L" s | "+wide(point.id);RECT text{260,y-10,client.right-8,y+20};DrawTextW(dc,label.c_str(),-1,&text,DT_SINGLELINE|DT_END_ELLIPSIS);}
        SelectObject(dc,old);DeleteObject(pen);EndPaint(w,&ps);return 0;}
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}if(msg==WM_DESTROY){timelineWindow=nullptr;return 0;}return DefWindowProcW(w,msg,wp,lp);
}
static void showTimelineGraph(){setPaused(true);if(timelineWindow){SetForegroundWindow(timelineWindow);return;}timelineFirst=timeline.points.size()>16?timeline.points.size()-16:0;WNDCLASSW cls{};cls.style=CS_DBLCLKS;cls.lpfnWndProc=timelineProc;cls.hInstance=appInstance;cls.lpszClassName=L"GBSTimeline10";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
    timelineWindow=CreateWindowExW(0,cls.lpszClassName,tr(L"Checkpoint timeline / branches",L"Временная шкала / ветки"),WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,980,760,hwnd,nullptr,appInstance,nullptr);}
static std::filesystem::path executableFolder(){wchar_t path[32768]{};const auto n=GetModuleFileNameW(nullptr,path,32768);return n&&n<32768?std::filesystem::path(path).parent_path():std::filesystem::path();}
static void showCompatibility(){const auto path=executableFolder()/L"docs"/L"compatibility.tsv";const auto bytes=Memory::loadBytes(path,4*1024*1024);std::istringstream in(std::string(bytes.begin(),bytes.end()));std::vector<std::wstring> rows;std::string line;
    while(std::getline(in,line))if(!line.empty()&&line[0]!='#')rows.push_back(wide(line));if(rows.empty())rows.push_back(L"No compatibility data is bundled. Run the hardware test scripts to generate docs/compatibility.tsv.");chooseRow(tr(L"Compatibility database — recorded test results",L"База совместимости — результаты тестов"),rows);}
static void showHomebrew(){std::vector<std::wstring> rows={L"GameBoy Studio DMG demo | original source included",L"GameBoy Studio CGB demo | original source included",L"Browse the gbdev homebrew showcase (authors and downloads)",L"Import a local homebrew ROM into your library..."};
    const auto item=chooseRow(tr(L"Homebrew catalog",L"Каталог homebrew"),rows);if(!item)return;if(*item<2)openROM(executableFolder()/(*item?L"demo.gbc":L"demo.gb"));else if(*item==2)ShellExecuteW(hwnd,L"open",L"https://gbdev.io/showcase/",nullptr,nullptr,SW_SHOWNORMAL);else {std::filesystem::path path;if(chooseFile(path,L"Game Boy ROM (*.gb;*.gbc;*.zip;*.gz)\0*.gb;*.gbc;*.zip;*.gz\0",L"gb",false,L"Import homebrew"))openROM(path);}}
