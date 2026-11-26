#pragma once
#include "../shared/Diagnostics.h"
#include <tlhelp32.h>
#include <cwchar>

namespace GameWindow {
inline bool SupportedImage(const wchar_t* path)
{
    const wchar_t* slash=std::wcsrchr(path,L'\\');
    const auto name=slash ? slash+1 : path;
    return !_wcsicmp(name,L"GenshinImpact.exe") || !_wcsicmp(name,L"YuanShen.exe");
}

enum class WindowKind { Hidden, Tool, Owned, NoClientRect, TooSmall, Eligible };
struct Candidate { LONGLONG area{}; int priority{}; };

inline WindowKind Classify(HWND window, Candidate& candidate)
{
    if (!IsWindowVisible(window)) return WindowKind::Hidden;
    if (GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOOLWINDOW) return WindowKind::Tool;
    wchar_t type[128]{};
    GetClassNameW(window,type,128);
    bool unity=!wcscmp(type,L"UnityWndClass");
    bool owned=GetWindow(window,GW_OWNER)!=nullptr;
    if (owned && !unity) return WindowKind::Owned;
    RECT rect{};
    if (!GetClientRect(window,&rect)) return WindowKind::NoClientRect;
    if (rect.right<64 || rect.bottom<32) return WindowKind::TooSmall;
    // Prefer the main Unity window, then an owned Unity render window, then other windows.
    candidate.priority=unity ? (owned ? 1 : 2) : 0;
    candidate.area=static_cast<LONGLONG>(rect.right)*rect.bottom;
    return WindowKind::Eligible;
}

inline DWORD QueryGameImage(DWORD pid, bool& supported)
{
    supported=false;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process) return GetLastError();
    wchar_t path[32768];DWORD length=32768;
    DWORD error=ERROR_SUCCESS;
    if(QueryFullProcessImageNameW(process,0,path,&length)) supported=SupportedImage(path);
    else error=GetLastError();
    CloseHandle(process);
    return error;
}

struct Match { HWND window{}; Candidate candidate{}; };

inline BOOL CALLBACK Consider(HWND window, LPARAM parameter)
{
    Candidate candidate;
    if(Classify(window,candidate)!=WindowKind::Eligible) return TRUE;
    DWORD pid{};
    GetWindowThreadProcessId(window,&pid);
    bool supported{};
    DWORD error=QueryGameImage(pid,supported);
    if(error && candidate.priority>0){ ITPF_DIAG("game.query Unity process image",error); }
    if(!supported) return TRUE;
    auto& best=*reinterpret_cast<Match*>(parameter);
    if(!best.window || candidate.priority>best.candidate.priority ||
       (candidate.priority==best.candidate.priority && candidate.area>best.candidate.area))
        best={window,candidate};
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

// Diagnose only supported game processes. No unrelated process names or window titles enter the log.
struct MissingReport {
    DWORD pid{}, processes{}, hidden{}, tool{}, owned{}, noRect{}, tooSmall{}, eligible{}, imageError{};
};
inline BOOL CALLBACK CountGameWindow(HWND window, LPARAM parameter)
{
    auto& report=*reinterpret_cast<MissingReport*>(parameter);
    DWORD pid{};
    GetWindowThreadProcessId(window,&pid);
    if(pid!=report.pid) return TRUE;
    Candidate candidate;
    switch(Classify(window,candidate)) {
    case WindowKind::Hidden: ++report.hidden; break;
    case WindowKind::Tool: ++report.tool; break;
    case WindowKind::Owned: ++report.owned; break;
    case WindowKind::NoClientRect: ++report.noRect; break;
    case WindowKind::TooSmall: ++report.tooSmall; break;
    case WindowKind::Eligible: {
        ++report.eligible;
        bool supported{};
        DWORD error=QueryGameImage(pid,supported);
        if(error) report.imageError=error;
        break;
    }
    }
    return TRUE;
}

inline DWORD InspectMissing(MissingReport& report)
{
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE) return GetLastError();
    PROCESSENTRY32W entry{};
    entry.dwSize=sizeof(entry);
    DWORD error=ERROR_SUCCESS;
    for(BOOL ok=Process32FirstW(snapshot,&entry); ; ok=Process32NextW(snapshot,&entry)) {
        if(!ok) {
            error=GetLastError();
            if(error==ERROR_NO_MORE_FILES) error=ERROR_SUCCESS;
            break;
        }
        if(!SupportedImage(entry.szExeFile)) continue;
        ++report.processes;
        report.pid=entry.th32ProcessID;
        if(!EnumWindows(CountGameWindow,reinterpret_cast<LPARAM>(&report))) { error=GetLastError(); break; }
    }
    CloseHandle(snapshot);
    return error;
}
}
