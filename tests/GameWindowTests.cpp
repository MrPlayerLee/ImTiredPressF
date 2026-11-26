#include "../core/GameWindow.h"
#include <iostream>
#include <string>

// Keep synthetic game windows on a private, non-interactive desktop.
int wmain(int argc,wchar_t** argv)
{
    auto name=L"ImTiredPressF.GameWindowTest."+std::to_wstring(GetCurrentProcessId());
    HDESK desktop=CreateDesktopW(name.c_str(),nullptr,nullptr,0,GENERIC_ALL,nullptr);
    if(!desktop || !SetThreadDesktop(desktop))return 1;
    auto create=[](const wchar_t* cls,const wchar_t* title,DWORD extra,int width) {
        return CreateWindowExW(extra,cls,title,WS_POPUP|WS_VISIBLE,0,0,width,200,
                               nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    };
    WNDCLASSW unity{};
    unity.lpfnWndProc=DefWindowProcW;unity.hInstance=GetModuleHandleW(nullptr);unity.lpszClassName=L"UnityWndClass";
    if(!RegisterClassW(&unity))return 1;
    auto fallback=create(L"STATIC",L"Genshin Impact",0,800);
    auto localized=create(unity.lpszClassName,L"Localized window title",0,400);
    auto tool=create(unity.lpszClassName,L"Tool",WS_EX_TOOLWINDOW,900);
    if(!fallback || !localized || !tool)return 1;
    bool supported=argc==2 && std::wstring(argv[1])==L"supported";
    if(GameWindow::Find()!=(supported?localized:nullptr))return 1;
    ShowWindow(localized,SW_HIDE);
    if(GameWindow::Find()!=(supported?fallback:nullptr))return 1;
    std::cout<<"PASS executable identity, title-independent selection and tool window exclusion.\n";
    return 0;
}
