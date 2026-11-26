#pragma once
#include "../shared/Diagnostics.h"
#include <cwchar>

namespace GameWindow {
inline bool SupportedImage(const wchar_t* path)
{
    const wchar_t* slash=std::wcsrchr(path,L'\\');
    const auto name=slash ? slash+1 : path;
    return !_wcsicmp(name,L"GenshinImpact.exe") || !_wcsicmp(name,L"YuanShen.exe");
}

struct Match { HWND window{}; LONGLONG area{}; bool unity{}; };

inline BOOL CALLBACK Consider(HWND window, LPARAM parameter)
{
    if(!IsWindowVisible(window) || GetWindow(window,GW_OWNER) ||
       (GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOOLWINDOW))return TRUE;
    RECT rect{};
    if(!GetClientRect(window,&rect) || rect.right<64 || rect.bottom<32)return TRUE;
    wchar_t type[128]{};
    GetClassNameW(window,type,128);
    bool unity=!wcscmp(type,L"UnityWndClass");
    DWORD pid{};
    GetWindowThreadProcessId(window,&pid);
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process) {
        if(unity){ ITPF_DIAG("game.OpenProcess Unity window",GetLastError()); }
        return TRUE;
    }
    wchar_t path[32768];DWORD length=32768;
    bool queried=QueryFullProcessImageNameW(process,0,path,&length)!=FALSE;
    if(!queried && unity){ ITPF_DIAG("game.QueryFullProcessImageName Unity window",GetLastError()); }
    CloseHandle(process);
    if(!queried || !SupportedImage(path))return TRUE;
    auto& best=*reinterpret_cast<Match*>(parameter);
    LONGLONG area=static_cast<LONGLONG>(rect.right)*rect.bottom;
    if(!best.window || (unity && !best.unity) || (unity==best.unity && area>best.area))
        best={window,area,unity};
    return TRUE;
}

inline HWND Find()
{
    Match best;
    if(!EnumWindows(Consider,reinterpret_cast<LPARAM>(&best))){
        ITPF_DIAG("game.EnumWindows",GetLastError());
    }
    return best.window;
}
}
