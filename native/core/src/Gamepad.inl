std::uint32_t PollPad() {
    using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
    static auto getState=[]()->GetState {
        for(auto name:{L"xinput1_4.dll",L"xinput9_1_0.dll"})if(auto module=LoadLibraryExW(name,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32))return reinterpret_cast<GetState>(GetProcAddress(module,"XInputGetState"));
        return nullptr;
    }();
    DWORD process{};GetWindowThreadProcessId(GetForegroundWindow(),&process);
    if(process!=GetCurrentProcessId()||!getState){padRepeat={};return 0;}
    if(padToken!=session.token){padToken=session.token;padRepeat={};padSeen=false;}
    XINPUT_STATE state{};bool connected=false;
    for(DWORD i=0;i<4;++i)if(getState(i,&state)==ERROR_SUCCESS){connected=true;break;}
    if(!connected){padRepeat={};return 0;}
    std::uint32_t down=state.Gamepad.wButtons;
    auto x=state.Gamepad.sThumbLX,y=state.Gamepad.sThumbLY;
    if(std::max(std::abs(int(x)),std::abs(int(y)))>14000) {
        if(std::abs(int(x))>std::abs(int(y)))down|=x<0?4:8;else down|=y<0?2:1;
    }
    auto now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto pressed=padRepeat.Update(down,now);
    if(pressed&&!padSeen){padSeen=true;logger::info("Controller navigation active ticket={}",session.token);}
    return pressed;
}
