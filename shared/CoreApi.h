#pragma once
#include <windows.h>
constexpr UINT CoreSampleMessage = WM_APP + 1;
constexpr UINT CoreMacroTimer = 2002, CoreOverlayTimer = 2004;
constexpr int StartId=1002, StopId=1003;
struct CoreApi {
    UINT size, abi;
    bool (*open)(HWND);
    void (*start)();
    void (*stop)();
    void (*event)(UINT,WPARAM,LPARAM);
    bool (*hotkeys)(bool);
    void (*scale)(float);
    const wchar_t* (*status)();
    bool (*canStart)();
    bool (*canStop)();
    void (*beginClose)();
    bool (*finishClose)();
};
using GetCoreApi = const CoreApi* (WINAPI*)(UINT);

