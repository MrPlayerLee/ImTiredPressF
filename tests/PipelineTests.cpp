#include "../core/Runtime.cpp"
#include "../core/Capture.cpp"
#include <iostream>
#include <cstdlib>
#include <commctrl.h>
#include "../core/DialogueDetection.h"

namespace
{
    int checks = 0;
    void Check(bool condition, const char* label)
    {
        ++checks;
        if (!condition) { std::cerr << label << '\n'; std::exit(1); }
    }

    LRESULT CALLBACK PaintTest(HWND window, UINT message, WPARAM wp, LPARAM lp)
    {
        if (message == WM_PRINTCLIENT || message == WM_PRINT)
        {
            RECT client{};
            GetClientRect(window, &client);
            HDC dc = reinterpret_cast<HDC>(wp);
            HBRUSH red = CreateSolidBrush(RGB(255, 0, 0));
            FillRect(dc, &client, red);
            DeleteObject(red);
            client.top = (client.bottom - client.top) / 2;
            HBRUSH blue = CreateSolidBrush(RGB(0, 0, 255));
            FillRect(dc, &client, blue);
            DeleteObject(blue);
            return 0;
        }
        return DefWindowProcW(window, message, wp, lp);
    }
}

int main()
{
    {
        using namespace DialogueDetection;
        Check(IsEnterWord(L"Enter") && IsEnterWord(L"[ENTER]"), "case and prompt decoration normalization");
        Check(!IsEnterWord(L"Center") && !IsEnterWord(L"Entered"), "scene words do not match Enter");
        State d;
        Check(d.Observe(false, 0) == 0.5, "initial frame cannot start input");
        Check(d.Observe(false, 1499) == 0.5, "absence must persist before input");
        Check(d.Observe(false, 1500) == 1.0, "stable dialogue allows input");
        Check(d.Observe(true, 2000) == 0.5, "one Enter pauses input but does not end session");
        Check(d.Observe(false, 2500) == 0.5, "OCR flicker restarts confirmation");
        Check(d.Observe(false, 4000) == 1.0, "dialogue resumes after transient Enter");
        Check(d.Observe(true, 4500) == 0.5, "end candidate pauses input");
        Check(d.Observe(true, 4999) == 0.5, "end confirmation requires elapsed time");
        Check(d.Observe(true, 5000) == 0.0, "persistent Enter confirms end");
        d = {};
        Check(d.Observe(false, 6000) == 0.5, "new session discards old detection history");
        Check(d.Observe(false, 7500) == 1.0, "new session can run again");
    }
    const HWND foreground = GetForegroundWindow();
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW type{};
    type.hInstance = instance;
    type.lpfnWndProc = PaintTest;
    type.lpszClassName = L"ImTiredPressF_CaptureRegression";
    Check(RegisterClassW(&type) != 0, "register capture window");
    HWND window = CreateWindowExW(WS_EX_NOACTIVATE, type.lpszClassName, L"Capture test",
        WS_POPUP, 0, 0, 203, 117, nullptr, nullptr, instance, nullptr);
    Check(window != nullptr, "create private capture window");
    cv::Mat image;
    Check(Capture::CaptureClient(window, image), "capture drawn client");
    Check(image.cols == 203 && image.rows == 117 && image.type() == CV_8UC1,
          "capture dimensions and grayscale format");
    // Hidden-window PrintWindow can return success without drawing pixels.
    // Verify pixel layout with a known DDB, through the production converter.
    {
        CaptureResources fixture;
        fixture.windowDC=GetDC(nullptr);
        fixture.memoryDC=CreateCompatibleDC(fixture.windowDC);
        fixture.bitmap=CreateCompatibleBitmap(fixture.windowDC,203,117);
        Check(fixture.windowDC && fixture.memoryDC && fixture.bitmap,"create known bitmap fixture");
        fixture.previous=SelectObject(fixture.memoryDC,fixture.bitmap);
        Check(fixture.previous && fixture.previous!=HGDI_ERROR,"select known bitmap fixture");
        fixture.selected=true;
        RECT upper{0,0,203,58}, lower{0,58,203,117};
        HBRUSH red=CreateSolidBrush(RGB(255,0,0)), blue=CreateSolidBrush(RGB(0,0,255));
        Check(FillRect(fixture.memoryDC,&upper,red)!=0 && FillRect(fixture.memoryDC,&lower,blue)!=0,
              "paint known bitmap fixture");
        DeleteObject(red); DeleteObject(blue);
        Check(fixture.Deselect(),"deselect fixture before production conversion");
        cv::Mat converted;
        Check(ConvertBitmapToMat(fixture.windowDC,fixture.bitmap,203,117,converted),"convert known bitmap");
        Check(converted.at<unsigned char>(0, 0) == 76, "top-down red channel conversion");
        Check(converted.at<unsigned char>(116, 202) == 29, "last row and blue channel conversion");
    }
    DWORD resources = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 100; ++i)
        Check(Capture::CaptureClient(window, image), "repeated capture succeeds");
    Check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == resources,
          "repeated captures do not retain GDI objects");
    // Use a window owned by this thread so PrintWindow can complete synchronously.
    // A machine without an installed OCR recognizer can still test capture and retry.
    if (Vision::Initialize())
    {
        Vision::Update(window);
        Check(Vision::GetScore() >= 0, "capture, preprocess and recognize a private test frame");
        Vision::Update(nullptr);
        Check(Vision::GetScore() < 0, "invalid frame pauses detection");
        Vision::Update(window);
        Check(Vision::GetScore() == 0.5, "first valid frame after an error starts fresh");
        std::cout << "PASS Windows OCR on private frames before and after capture failure.\n";
    }
    else std::cout << "SKIP live OCR: Windows OCR initialization unavailable.\n";
    Vision::Shutdown();
    DestroyWindow(window);
    Check(!Capture::CaptureClient(window, image) && image.empty(),
          "destroyed capture target clears previous image");

    // Use the production UI, timers and OCR worker. Never start input or look
    // up the real game. All windows remain hidden throughout this test.
    WNDCLASSW owner{};
    owner.hInstance=instance; owner.lpszClassName=L"ImTiredPressF_CoreTestHost";
    owner.lpfnWndProc=[](HWND h,UINT m,WPARAM w,LPARAM l)->LRESULT {
        if(m==WM_TIMER || m==CoreSampleMessage || m==WM_HOTKEY) {Event(m,w,l);return 0;}
        if(m==WM_CLOSE) {CloseMacro();return 0;}
        return DefWindowProcW(h,m,w,l);
    };
    Check(RegisterClassW(&owner)!=0,"register core test host");
    HWND host=CreateWindowExW(0,owner.lpszClassName,L"",WS_POPUP,0,0,10,10,nullptr,nullptr,instance,nullptr);
    Check(host && Open(host),"initialize production core and OCR worker");
    Check(GetMacroIndicator()==MacroIndicator::Stopped,"overlay initially reports stopped");
    g_isRunning=true;
    Check(GetMacroIndicator()==MacroIndicator::Ready,"started without input reports ready");
    Event(WM_HOTKEY,IDHOT_ALT_F,0);
    Check(g_isRunning.load(),"disabled Alt+F does not stop input");
    g_enableAltF=true;
    Event(WM_HOTKEY,IDHOT_ALT_F,0);
    Check(!g_isRunning.load() && IsWindow(g_hWnd) && !g_exitThread.load(),
          "Alt+F stops input without closing the application");
    HandleMacroSample(1,2);
    Check(!g_isRunning.load(),"manual stop rejects queued input samples");
    g_isRunning=true; // Simulate a fresh start without targeting the real game.
    HandleMacroSample(1,0);
    Check(g_isRunning.load(),"manual stop invalidates old session results");
    HandleMacroSample(2,0);
    Check(!g_isRunning.load() && IsWindow(g_hWnd) && !g_exitThread.load(),
          "confirmed end stops input without closing software");
    HandleMacroSample(1,2);
    Check(!g_isRunning.load(),"queued old sample cannot automatically restart input");
    g_isRunning=true; // Simulate the next explicit start, without locating the real game.
    HandleMacroSample(1,0);
    Check(g_isRunning.load(),"previous-session end cannot stop a new session");
    HandleMacroSample(3,1);
    Check(g_isRunning.load(),"new-session detection waits without stopping");
    HandleMacroSample(3,0);
    Check(!g_isRunning.load() && IsWindow(g_hWnd),"second session can stop independently");
    g_isRunning=true;
    HandleMacroSample(4,-2);
    Check(g_isRunning.load() && GetMacroIndicator()==MacroIndicator::Ready,
          "one failed frame pauses input without ending the session");
    HandleMacroSample(4,1);
    HandleMacroSample(4,-2);
    Check(g_isRunning.load(), "valid frame clears consecutive error count");
    HandleMacroSample(4,-2);
    Check(!g_isRunning.load(), "consecutive failures stop the session");
    g_enableAltF=false;
    Check(g_hMacroThread != nullptr, "create production OCR worker");
    int macroTicks = 0;
    ULONGLONG deadline = GetTickCount64() + 300;
    MSG message{};
    while (GetTickCount64() < deadline)
    {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_TIMER && message.wParam == IDT_MACRO_UPDATE)
                ++macroTicks;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(1);
    }
    Check(macroTicks >= 2, "permanent macro timer advances while idle");
    HWND app = g_hWnd;
    Check(PostMessageW(app, WM_CLOSE, 0, 0) != FALSE, "request production cleanup");
    deadline = GetTickCount64() + 10000;
    while (!CleanupReady() && GetTickCount64() < deadline)
    {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(1);
    }
    Check(FinishClose() && IsWindow(app) && g_hMacroThread == nullptr, "unload releases core while preserving host");
    DestroyWindow(app);
    Check(GetForegroundWindow() == foreground, "private pipeline tests preserve foreground");
    std::cout << "PASS: " << checks << " capture and application-lifecycle checks.\n";
}
