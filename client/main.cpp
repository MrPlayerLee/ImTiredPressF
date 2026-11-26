#include "../shared/ClientVariant.h"
#include "Trust.h"
#include "Client.h"
HWND g_hWnd{},g_hStatus{},g_hStart{},g_hStop{};
int WINAPI WinMain(HINSTANCE module,HINSTANCE,LPSTR,int show)
{
    if(!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32))return 1;
    // Events and mutexes share one kernel namespace. Atomic creation both reserves
    // the single slot and identifies its owner, even before a window exists.
    constexpr auto slot=L"Local\\ImTiredPressF.Client.Instance";
    constexpr bool mcp=ITPF_MCP!=0;
    SetLastError(ERROR_SUCCESS);
    HANDLE marker=mcp ? CreateEventExW(nullptr,slot,0,SYNCHRONIZE)
                      : CreateMutexExW(nullptr,slot,0,SYNCHRONIZE);
    DWORD error=GetLastError();
    if (!marker || error==ERROR_ALREADY_EXISTS) {
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
