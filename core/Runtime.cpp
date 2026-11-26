#include "../shared/Diagnostics.h"
#include "Common.h"
#include "Overlay.h"
#include "../shared/CoreApi.h"

HWND g_hWnd{};
std::atomic<HWND> g_gameHwnd{};
HANDLE g_hMacroThread{};
std::atomic<bool> g_isRunning{false}, g_exitThread{false}, g_autoDetect{true}, g_enableAltF{false};

namespace {
void ReleaseResources()
{
    KillTimer(g_hWnd,CoreMacroTimer); KillTimer(g_hWnd,CoreOverlayTimer);
    Overlay::Shutdown();
    MSG msg{};
    while(PeekMessageW(&msg,g_hWnd,CoreSampleMessage,CoreSampleMessage,PM_REMOVE)) {}
    while(PeekMessageW(&msg,g_hWnd,WM_HOTKEY,WM_HOTKEY,PM_REMOVE)) {}
    g_hWnd=nullptr;
}
bool Open(HWND owner)
{
    g_hWnd=owner;
    if(!Overlay::Initialize(owner)) {
        Diagnostics::Write("core.overlay initialization failed");
    } else if(!SetTimer(owner,CoreMacroTimer,30,nullptr)) {
        Diagnostics::Write("core.SetTimer macro",GetLastError());
    } else if(!SetTimer(owner,CoreOverlayTimer,16,nullptr)) {
        Diagnostics::Write("core.SetTimer overlay",GetLastError());
    } else {
        g_hMacroThread=CreateThread(nullptr,0,MacroThread,nullptr,0,nullptr);
        if(g_hMacroThread)return true;
        Diagnostics::Write("core.CreateThread macro",GetLastError());
    }
    ReleaseResources(); return false;
}
bool Hotkeys(bool enable)
{
    if(enable==g_enableAltF.load()) return true;
    if(enable) {
        if(!RegisterHotKey(g_hWnd,IDHOT_ALT_F,MOD_ALT|MOD_NOREPEAT,'F')) { Diagnostics::Write("hotkey.RegisterHotKey Alt+F",GetLastError()); return false; }
        if(!RegisterHotKey(g_hWnd,IDHOT_ALT_G,MOD_ALT|MOD_NOREPEAT,'G')) {
            Diagnostics::Write("hotkey.RegisterHotKey Alt+G",GetLastError());
            UnregisterHotKey(g_hWnd,IDHOT_ALT_F); return false;
        }
        Diagnostics::Write("hotkey.registered Alt+F and Alt+G");
    } else { UnregisterHotKey(g_hWnd,IDHOT_ALT_F); UnregisterHotKey(g_hWnd,IDHOT_ALT_G); }
    g_enableAltF=enable; return true;
}
void Event(UINT msg,WPARAM wp,LPARAM lp)
{
    if(msg==WM_TIMER && wp==CoreOverlayTimer) Overlay::Tick();
    if(msg==WM_TIMER && wp==CoreMacroTimer) HandleMacroTimer(wp);
    if(msg==CoreSampleMessage) HandleMacroSample(wp,lp);
    if(msg==WM_HOTKEY && g_enableAltF && !g_exitThread) {
        if(wp==IDHOT_ALT_F && !g_isRunning) StartMacro();
        else if(wp==IDHOT_ALT_F || wp==IDHOT_ALT_G) StopMacro();
    }
}
bool FinishClose()
{
    if(!g_exitThread || !CleanupReady()) return false;
    ReleaseResources(); return true;
}
}
extern "C" __declspec(dllexport) const CoreApi* WINAPI ImTiredPressF_GetApi(UINT abi)
{
    static const CoreApi api{sizeof(CoreApi),1,Open,StartMacro,StopMacro,Event,Hotkeys,Overlay::SetScale,
                             MacroStatus,CanStart,CanStop,CloseMacro,FinishClose};
    return abi==api.abi ? &api : nullptr;
}
