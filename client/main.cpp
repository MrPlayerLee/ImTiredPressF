#include "../shared/Diagnostics.h"
#include "../shared/ClientVariant.h"
#include "Trust.h"
#include "Client.h"
HWND g_hWnd{},g_hStatus{},g_hStart{},g_hStop{};
int WINAPI WinMain(HINSTANCE module,HINSTANCE,LPSTR,int show)
{
    Diagnostics::Write(ITPF_MCP?"client.session MCP":"client.session standard");
    HANDLE token{};
    if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)){
        TOKEN_ELEVATION elevation{};DWORD returned{};
        if(GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&returned))
            Diagnostics::Write(elevation.TokenIsElevated?"client.elevated":"client.non-elevated");
        CloseHandle(token);
    }
    using VersionFn=LONG(WINAPI*)(OSVERSIONINFOW*);
    auto osVersion=reinterpret_cast<VersionFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion"));
    OSVERSIONINFOW os{sizeof(os)};
    if(osVersion && osVersion(&os)==0)Diagnostics::Write("Windows build",os.dwBuildNumber);
    if(!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)){Diagnostics::Write("client.SetDefaultDllDirectories",GetLastError());return 1;}
    // Events and mutexes share one kernel namespace. Atomic creation both reserves
    // the single slot and identifies its owner, even before a window exists.
    constexpr auto slot=L"Local\\ImTiredPressF.Client.Instance";
    constexpr bool mcp=ITPF_MCP!=0;
    SetLastError(ERROR_SUCCESS);
    HANDLE marker=mcp ? CreateEventExW(nullptr,slot,0,SYNCHRONIZE)
                      : CreateMutexExW(nullptr,slot,0,SYNCHRONIZE);
    DWORD error=GetLastError();
    if (!marker || error==ERROR_ALREADY_EXISTS) {
        Diagnostics::Write("client.instance",error);
        if(marker)CloseHandle(marker);
        std::wstring message=L"클라이언트 실행 상태를 확인할 수 없습니다.";
        if(error==ERROR_ALREADY_EXISTS || error==ERROR_INVALID_HANDLE)
            message=std::wstring(ClientVariant::File(error==ERROR_ALREADY_EXISTS ? mcp : !mcp))+L" 가 실행중입니다.";
        MessageBoxW(nullptr,message.c_str(),L"ImTiredPressF",MB_OK|MB_ICONWARNING);
        return 1;
    }
    Trust::Handle instance(marker);
    if(!CreateMainWindow(module,show)) return 1;
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0) {
        if(!IsDialogMessageW(g_hWnd,&msg)) {TranslateMessage(&msg); DispatchMessageW(&msg);}
    }
    return static_cast<int>(msg.wParam);
}
