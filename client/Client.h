#pragma once
#include "../shared/CoreApi.h"
#include <string>
#include <array>
#include <utility>
#define IDC_STATUS 1001
#define IDC_START 1002
#define IDC_STOP 1003
#define IDC_ALTFSTOP 1005
#define IDC_OVERLAY_SCALE 1007
constexpr UINT UiTimer=2200;
extern HWND g_hWnd,g_hStatus,g_hStart,g_hStop;
bool CreateMainWindow(HINSTANCE,int);
LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
namespace Client {
extern const CoreApi* core;
extern bool unloading, quitting;
extern std::wstring notice;
void Load();
void Unload();
void Poll();
void CheckVersions();
bool Checking();
const wchar_t* Current(int index);
const wchar_t* Latest(int index);
bool HasUpdate(int index);
void ApplyUpdate(int component);
extern std::string trustState,updateState;
std::wstring Folder();
}
namespace Bridge {
bool Open(HWND);
void Tick(HWND);
void Close();
}
namespace Labels {
inline constexpr wchar_t title[]=L"F를 누르기 지친 당신을 위해", scale[]=L"오버레이 크기", automatic[]=L"대화 종료 시 자동 중지",
    core[]=L"코어 상태", update[]=L"업데이트", component[]=L"구성 요소", current[]=L"현재 버전", latest[]=L"최신 버전",
    exe[]=L"클라이언트 (EXE)", dll[]=L"코어 (DLL)";
}
