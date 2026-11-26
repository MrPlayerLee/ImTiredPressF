#include "../shared/Diagnostics.h"
#include "Common.h"
#include "Common_OpenCV.h"
#include "FocusController.h"
#include "TargetWindowInput.h"

namespace
{
    std::atomic<UINT_PTR> generation{ 1 };
    bool closing = false;
    bool startRequested = false;
    bool inputAllowed = false;
    double displayedScore = 0.0;
    const wchar_t* inputError = nullptr;

    TargetWindowInput platform(inputError);

    FocusController pulse(platform);

}

bool CanStart() { return !closing && !g_isRunning && !pulse.Busy(); }
bool CanStop() { return !closing && (g_isRunning || pulse.Busy()); }
bool CleanupReady() { return closing && !g_hMacroThread && !pulse.Busy(); }

void StartMacro()
{
    if (closing || g_isRunning.load()) return;
    if (pulse.Busy()) { startRequested = true; return; }
    startRequested = false;
    HWND game = FindWindowW(nullptr, L"원신");
    if (!game) game = FindWindowW(nullptr, L"Genshin Impact");
    if (!game)
    {
        Diagnostics::Write("macro.start game window missing");
        inputError = L"상태 : 원신 창을 찾을 수 없습니다.";
        return;
    }
    g_gameHwnd = game;
    ++generation;
    inputAllowed = false;
    displayedScore = 0.0;
    inputError = nullptr;
    g_isRunning = true;
}

void StopMacro()
{
    startRequested = false;
    g_isRunning = false;
    ++generation; // Invalidate results queued before stop, even after restart.
    inputAllowed = false;
    pulse.Stop();
}

void HandleMacroSample(WPARAM token, LPARAM sample)
{
    if (closing || !g_isRunning.load() || token != generation.load()) return;
    if (sample < 0)
    {
        StopMacro();
        Diagnostics::Write(sample==-1?"macro.game window lost":"macro.capture or OCR failed");
        inputError = sample == -1 ? L"상태 : 게임 창을 찾을 수 없습니다."
                                  : L"상태 : 화면 캡처/OCR 실패 - 입력 중지";
        return;
    }
    displayedScore = static_cast<double>(sample) / 2.0;
    if (sample == 0)
    {
        StopMacro();
        return;
    }
    inputAllowed = sample == 2;
    if (!inputAllowed)
        pulse.Stop();
    else if (pulse.CanPulse())
    {
        inputError = nullptr;
        pulse.Begin(g_gameHwnd.load());
    }
}

bool HandleMacroTimer(UINT_PTR timer)
{
    if (timer == IDT_MACRO_UPDATE)
    {
        pulse.Tick(!closing && g_isRunning.load() && inputAllowed);
        if (!closing && startRequested && !pulse.Busy()) StartMacro();
        // Keep processing UI messages while the OCR worker finishes.
        if (closing && !pulse.Busy() && (!g_hMacroThread ||
            WaitForSingleObject(g_hMacroThread, 0) == WAIT_OBJECT_0))
        {
            if (g_hMacroThread) CloseHandle(g_hMacroThread);
            g_hMacroThread = nullptr;
        }
        return true;
    }
    return false;
}

void CloseMacro()
{
    if (closing) return;
    closing = true;
    g_exitThread = true;
    StopMacro();
    UnregisterHotKey(g_hWnd, IDHOT_ALT_F);
    UnregisterHotKey(g_hWnd, IDHOT_ALT_G);
    // The permanent macro timer was checked at window creation and also
    // advances shutdown; closing never depends on allocating a new timer.
}

const wchar_t* MacroStatus()
{
    if (closing) return L"상태 : 종료 중";
    if (inputError) return inputError;
    if (!g_isRunning.load()) return pulse.Busy() ? L"상태 : 입력 정리 중" : L"상태 : 중지";
    if (displayedScore == 1.0) return L"상태 : 대화중";
    if (displayedScore == 0.5) return L"상태 : 대화 탐지중";
    return L"상태 : 대화 탐지중";
}

// Read-only UI projection. Input scheduling and OCR decisions are unchanged.
MacroIndicator GetMacroIndicator()
{
    if (closing || !g_isRunning.load() || inputError) return MacroIndicator::Stopped;
    const auto phase=pulse.State();
    if (inputAllowed && (phase==FocusController::Phase::KeyDown || phase==FocusController::Phase::Ready))
        return MacroIndicator::Working;
    return MacroIndicator::Ready;
}

DWORD WINAPI MacroThread(LPVOID)
{
    // Initialize, use and destroy the WinRT OCR apartment on this worker.
    const bool visionReady = Vision::Initialize();
    UINT_PTR lastGeneration = 0;
    while (!g_exitThread.load())
    {
        if (!g_isRunning.load()) { Sleep(10); continue; }
        const UINT_PTR token = generation.load();
        HWND game = g_gameHwnd.load();
        if (token != lastGeneration)
        {
            Vision::ResetDetection();
            lastGeneration = token;
        }
        LPARAM sample = 2;
        if (!IsWindow(game)) sample = -1;
        else
        {
            try
            {
                if (!visionReady) sample = -2;
                else
                {
                    Vision::Update(game);
                    sample = static_cast<LPARAM>(Vision::GetScore() * 2.0);
                }
            }
            catch (...) { ITPF_DIAG("macro.vision exception",0); sample = -2; }
        }
        if (!g_exitThread.load() && g_isRunning.load() && token == generation.load())
            PostMessageW(g_hWnd, WM_MACRO_SAMPLE, token, sample);
        for (int i = 0; i < 50 && !g_exitThread.load() && g_isRunning.load()
            && token == generation.load(); ++i) Sleep(10);
    }
    Vision::Shutdown();
    return 0;
}
