#pragma once
#include "../core/TargetWindowInput.h"
#include <thread>
#include <atomic>
#include <mutex>
#pragma comment(lib, "user32.lib")

// A private hidden window, not the real game or editor. Run the production
// transport across threads and inspect the messages actually received.
namespace TransportTest
{
    std::mutex gate;
    std::vector<UINT> received;
    std::atomic<HWND> window{nullptr};
    std::atomic<HWND> otherWindow{nullptr};
    LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
    {
        if (message == WM_CANCELMODE || message == WM_ACTIVATEAPP || message == WM_NCACTIVATE || message == WM_ACTIVATE || message == WM_SETFOCUS || message == WM_KILLFOCUS ||
            message == WM_KEYDOWN || message == WM_KEYUP)
        {
            std::lock_guard<std::mutex> lock(gate);
            received.push_back(message);
            // Emulate the game's internal handling without DefWindowProc changing focus.
            return 0;
        }
        if (message == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
        if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(hwnd,message,wp,lp);
    }
    void Run()
    {
        std::thread receiver([] {
            WNDCLASSW wc{}; wc.lpfnWndProc=Proc; wc.hInstance=GetModuleHandleW(nullptr);
            wc.lpszClassName=L"ImTiredPressF_PrivateTransportTest";
            RegisterClassW(&wc);
            otherWindow=CreateWindowExW(WS_EX_NOACTIVATE,wc.lpszClassName,L"Other private test",
                                  WS_OVERLAPPED,0,0,200,200,nullptr,nullptr,wc.hInstance,nullptr);
            window=CreateWindowExW(WS_EX_NOACTIVATE,wc.lpszClassName,L"Private test",
                                  WS_OVERLAPPED,0,0,200,200,nullptr,nullptr,wc.hInstance,nullptr);
            MSG msg{};
            while(GetMessageW(&msg,nullptr,0,0)>0) DispatchMessageW(&msg);
        });
        for(int i=0;i<200 && !window.load();++i) Sleep(10);
        HWND target=window.load(); Check(target!=nullptr,"private test window created");
        HWND before=GetForegroundWindow();
        const wchar_t* error=nullptr;
        TargetWindowInput transport(error);
        {
            std::lock_guard<std::mutex> lock(gate); received.clear();
        }
        Check(transport.Notify(target,true),"real activation message delivery");
        Check(transport.Key(target,true),"real key-down delivery");
        Check(transport.Key(target,false),"real key-up delivery");
        Check(transport.Notify(target,false),"real deactivation message delivery");
        {
            std::lock_guard<std::mutex> lock(gate);
            Check(received==std::vector<UINT>{WM_ACTIVATE,WM_SETFOCUS,WM_KEYDOWN,WM_KEYUP,WM_ACTIVATE,WM_KILLFOCUS},
                  "real cross-thread message ordering");
        }
        Check(GetForegroundWindow()==before,"production transport does not switch foreground");
        {
            std::lock_guard<std::mutex> lock(gate); received.clear();
        }
        Check(transport.Restore(target), "restore real background activation state");
        {
            std::lock_guard<std::mutex> lock(gate);
            Check(received == std::vector<UINT>{WM_CANCELMODE, WM_ACTIVATEAPP, WM_NCACTIVATE, WM_ACTIVATE, WM_KILLFOCUS},
                  "complete background cleanup notification order");
        }
        Check(GetForegroundWindow()==before,"cleanup preserves foreground");
        Check(!transport.SuspendForReturn(target),"never suspend a nonforeground window for return repair");
        transport.Track(target);
        Check(transport.Valid(target),"tracked target retains identity");
        Check(!transport.Valid(otherWindow.load()),"another live HWND is not the tracked target");
        Check(!transport.Key(otherWindow.load(),true),"reject input to another HWND");
        Check(transport.LastSendError()==ERROR_INVALID_WINDOW_HANDLE,"record invalid-target diagnostic");
        transport.Untrack();
        PostMessageW(target,WM_CLOSE,0,0);
        receiver.join();
        std::cout<<"PASS: production Win32 transport, private HWND targeting and message order.\n";
    }
}
