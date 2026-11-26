#pragma once

//=========================================================
// Header
//=========================================================

#include <windows.h>
#include <atomic>

//=========================================================
// Vision
//=========================================================

#include "Common_OpenCV.h"

// Timer
#define IDT_MACRO_UPDATE 2002
#define IDT_OVERLAY_UPDATE 2004

// Hotkey
#define IDHOT_ALT_F 3001
#define IDHOT_ALT_G 3002

//=========================================================
// User Message
//=========================================================

#define WM_MACRO_SAMPLE (WM_APP + 1)

//=========================================================
// Global Variables
//=========================================================

extern HWND g_hWnd;



extern std::atomic<HWND> g_gameHwnd;

extern HANDLE g_hMacroThread;

//=========================================================
// Global State
//=========================================================

extern std::atomic<bool> g_isRunning;
extern std::atomic<bool> g_exitThread;

// Compatibility projection for the unchanged overlay: detection is always enabled.
extern std::atomic<bool> g_autoDetect;
extern std::atomic<bool> g_enableAltF;

//=========================================================
// Macro
//=========================================================

DWORD WINAPI MacroThread(LPVOID lpParam);

// UI-thread input/focus controller. The worker only posts OCR samples.
void StartMacro();
void StopMacro();
void HandleMacroSample(WPARAM token, LPARAM sample);
bool HandleMacroTimer(UINT_PTR timer);
void CloseMacro();
const wchar_t* MacroStatus();
enum class MacroIndicator { Stopped, Ready, Working };
MacroIndicator GetMacroIndicator();
namespace Vision { void ResetDetection(); }

bool CanStart();
bool CanStop();
bool CleanupReady();
