#include "Client.h"
HWND g_hWnd{},g_hStatus{},g_hStart{},g_hStop{};
int WINAPI WinMain(HINSTANCE module,HINSTANCE,LPSTR,int show)
{
    if(!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32))return 1;
    if(!CreateMainWindow(module,show)) return 1;
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0) {
        if(!IsDialogMessageW(g_hWnd,&msg)) {TranslateMessage(&msg); DispatchMessageW(&msg);}
    }
    return static_cast<int>(msg.wParam);
}
