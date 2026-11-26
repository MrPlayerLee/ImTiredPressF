#include "../shared/Diagnostics.h"
#include "Client.h"
#include "PanelSkin.h"
#include <commctrl.h>
#include <algorithm>
#include <cmath>

namespace
{
using namespace PanelSkin;
constexpr wchar_t ClassName[] = L"ImTiredPressFWindow";
constexpr auto& Title = Labels::title;
struct ControlSpec { int id; const wchar_t* type; const wchar_t* text; DWORD style; };
const ControlSpec controls[] = {
    {Minimize,L"BUTTON",L"최소화",BS_PUSHBUTTON}, {Maximize,L"BUTTON",L"최대화 또는 복원",BS_PUSHBUTTON},
    {Close,L"BUTTON",L"닫기",BS_PUSHBUTTON}, {IDC_STATUS,L"STATIC",L"상태 : 중지",0},
    {IDC_OVERLAY_SCALE,TRACKBAR_CLASSW,Labels::scale,WS_TABSTOP|TBS_NOTICKS},
    {IDC_START,L"BUTTON",L"시작",BS_PUSHBUTTON|WS_TABSTOP},
    {IDC_STOP,L"BUTTON",L"중지",BS_PUSHBUTTON|WS_TABSTOP|WS_DISABLED},
    {IDC_ALTFSTOP,L"BUTTON",L"ALT+F 시작/중지 (ALT+G 정리/중지)",BS_AUTOCHECKBOX|WS_TABSTOP},
    {Load,L"BUTTON",L"로드",WS_TABSTOP}, {Unload,L"BUTTON",L"언로드",WS_TABSTOP},
    {CheckUpdate,L"BUTTON",L"업데이트 확인",WS_TABSTOP},
    {UpdateClient,L"BUTTON",L"클라이언트 업데이트",WS_TABSTOP|WS_DISABLED},
    {UpdateCore,L"BUTTON",L"코어 업데이트",WS_TABSTOP|WS_DISABLED}
};

void Layout(HWND hwnd)
{
    SetLayout(hwnd);
    for(const auto& c : controls) {
        const RECT r=Bounds(c.id);
        if(HWND child=GetDlgItem(hwnd,c.id))
            MoveWindow(child,r.left,r.top,r.right-r.left,r.bottom-r.top,TRUE);

    }
    RECT r{}; GetClientRect(hwnd,&r);
    if(IsZoomed(hwnd)) SetWindowRgn(hwnd,nullptr,TRUE);
    else {
        int arc=MulDiv(21,GetDpiForWindow(hwnd),96);
        HRGN region=CreateRoundRectRgn(0,0,r.right+1,r.bottom+1,arc,arc);
        if(region && !SetWindowRgn(hwnd,region,TRUE)) DeleteObject(region);
    }
    InvalidateRect(hwnd,nullptr,FALSE);
}

LRESULT Paint(HWND hwnd, bool mainWindow)
{
    PAINTSTRUCT ps{}; HDC dc=BeginPaint(hwnd,&ps);
    PanelSkin::Paint(hwnd,dc,mainWindow); EndPaint(hwnd,&ps);
    return 0;
}

LRESULT CALLBACK SkinControl(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR)
{
    switch(msg) {
    case WM_PAINT: return Paint(hwnd,false);
    case WM_PRINTCLIENT: PanelSkin::Paint(hwnd,reinterpret_cast<HDC>(wp),false); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEMOVE:
        if(!GetPropW(hwnd,L"PanelHover")) {
            SetPropW(hwnd,L"PanelHover",reinterpret_cast<HANDLE>(1));
            TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,hwnd,0}; TrackMouseEvent(&track);
            InvalidateRect(hwnd,nullptr,FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        RemovePropW(hwnd,L"PanelHover"); InvalidateRect(hwnd,nullptr,FALSE); break;
    case WM_ENABLE: case WM_SETFOCUS: case WM_KILLFOCUS: case WM_UPDATEUISTATE:
    case BM_SETCHECK: case BM_SETSTATE: case TBM_SETPOS: case WM_LBUTTONUP: case WM_LBUTTONDOWN:
    case WM_KEYDOWN: case WM_KEYUP: case WM_SETTEXT:
        {
            const auto result=DefSubclassProc(hwnd,msg,wp,lp);
            InvalidateRect(hwnd,nullptr,FALSE); return result;
        }
    case WM_NCDESTROY:
        RemovePropW(hwnd,L"PanelHover"); RemoveWindowSubclass(hwnd,SkinControl,1); break;
    }
    return DefSubclassProc(hwnd,msg,wp,lp);
}

}

LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    switch(msg) {
    case WM_PAINT: return Paint(hwnd,true);
    case WM_PRINTCLIENT: PanelSkin::Paint(hwnd,reinterpret_cast<HDC>(wp),true); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: if(wp!=SIZE_MINIMIZED) Layout(hwnd); return 0;
    case WM_DPICHANGED: {
        const RECT* r=reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
        return 0;}
    case WM_NCHITTEST: {
        POINT pt{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};
        ScreenToClient(hwnd,&pt);
        RECT title=PanelSkin::Bounds(PanelSkin::Minimize);
        if(pt.y>=title.top && pt.y<=title.bottom && pt.x<title.left) return HTCAPTION;
        break;}
    case WM_GETMINMAXINFO: {
        MONITORINFO m{sizeof(m)};
        if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&m)) {
            auto limits=reinterpret_cast<MINMAXINFO*>(lp);
            limits->ptMaxPosition={m.rcWork.left-m.rcMonitor.left,m.rcWork.top-m.rcMonitor.top};
            limits->ptMaxSize={m.rcWork.right-m.rcWork.left,m.rcWork.bottom-m.rcWork.top};
            return 0;
        } break;}
    case WM_COMMAND:
        switch(LOWORD(wp)) {
        case PanelSkin::Minimize: ShowWindow(hwnd,SW_MINIMIZE); break;
        case PanelSkin::Maximize: ShowWindow(hwnd,IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE); break;
        case PanelSkin::Close: PostMessageW(hwnd,WM_CLOSE,0,0); break;
        case PanelSkin::Load: Client::Load(); break;
        case PanelSkin::Unload: Client::Unload(); break;
        case PanelSkin::CheckUpdate: Client::CheckVersions(); break;
        case PanelSkin::UpdateClient: case PanelSkin::UpdateCore:
            if(Client::HasUpdate(LOWORD(wp)==PanelSkin::UpdateClient?0:1)) Client::ApplyUpdate(LOWORD(wp)==PanelSkin::UpdateClient?0:1);
            break;
        case IDC_START: case IDC_STOP:
            if(Client::core && !Client::unloading) {
                Client::notice.clear();
                if(LOWORD(wp)==IDC_START) Client::core->start(); else Client::core->stop();
            } break;
        case IDC_ALTFSTOP:
            if(Client::core && !Client::unloading && !Client::core->hotkeys(IsDlgButtonChecked(hwnd,IDC_ALTFSTOP)==BST_CHECKED)) {
                CheckDlgButton(hwnd,IDC_ALTFSTOP,BST_UNCHECKED); Client::notice=L"상태 : 단축키 등록 실패 · ImTiredPressF.log 확인";
            } break;
        }
        Client::Poll(); return 0;
    case WM_HOTKEY: case CoreSampleMessage:
        if(Client::core) Client::core->event(msg,wp,lp);
        return 0;
    case WM_TIMER:
        if(wp==UiTimer) {
#if ITPF_MCP
            Bridge::Tick(hwnd);
#endif
            if(IsWindow(hwnd)) Client::Poll();
        } else if(Client::core) Client::core->event(msg,wp,lp);
        return 0;
    case WM_HSCROLL:
        if(HWND slider=GetDlgItem(hwnd,IDC_OVERLAY_SCALE); slider==reinterpret_cast<HWND>(lp) && Client::core && !Client::unloading) {
            Client::core->scale(static_cast<float>(SendMessageW(slider,TBM_GETPOS,0,0))/100.f);
            InvalidateRect(slider,nullptr,FALSE);
        } return 0;
    case WM_CLOSE: Client::quitting=true; Client::Unload(); return 0;
    case WM_DESTROY:
        KillTimer(hwnd,UiTimer);
#if ITPF_MCP
        Bridge::Close();
#endif
        PanelSkin::Shutdown(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

bool CreateMainWindow(HINSTANCE module,int show)
{
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_BAR_CLASSES|ICC_WIN95_CLASSES};
    if(!InitCommonControlsEx(&cc) || !PanelSkin::Initialize(module)) { Diagnostics::Write("UI common controls or skin initialization",GetLastError());return false; }
    WNDCLASSW wc{};
    wc.lpfnWndProc=WndProc; wc.hInstance=module; wc.lpszClassName=ClassName;
    wc.hIcon=LoadIconW(module,MAKEINTRESOURCEW(1)); wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.style=CS_DBLCLKS;
    if(!RegisterClassW(&wc)) {Diagnostics::Write("UI.RegisterClass",GetLastError()); PanelSkin::Shutdown(); return false; }
    RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    const float factor=std::min({GetDpiForSystem()/96.f,(work.right-work.left)/float(PanelSkin::Width),
                                 (work.bottom-work.top)/float(PanelSkin::Height)});
    const int w=std::lround(PanelSkin::Width*factor),h=std::lround(PanelSkin::Height*factor);
    g_hWnd=CreateWindowExW(WS_EX_APPWINDOW,ClassName,Title,
        WS_POPUP|WS_CLIPCHILDREN|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
        work.left+(work.right-work.left-w)/2,work.top+(work.bottom-work.top-h)/2,w,h,nullptr,nullptr,module,nullptr);
    if(!g_hWnd) {Diagnostics::Write("UI.CreateWindow",GetLastError()); PanelSkin::Shutdown(); return false; }
    PanelSkin::SetLayout(g_hWnd);
    for(const auto& c : controls) {
        RECT r=PanelSkin::Bounds(c.id);
        HWND child=CreateWindowExW(0,c.type,c.text,WS_CHILD|WS_VISIBLE|c.style,
            r.left,r.top,r.right-r.left,r.bottom-r.top,g_hWnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(c.id)),module,nullptr);
        if(!child || !SetWindowSubclass(child,SkinControl,1,0)) {Diagnostics::Write("UI.control/subclass",GetLastError());DestroyWindow(g_hWnd); return false;}
    }
    g_hStatus=GetDlgItem(g_hWnd,IDC_STATUS); g_hStart=GetDlgItem(g_hWnd,IDC_START); g_hStop=GetDlgItem(g_hWnd,IDC_STOP);
    HWND slider=GetDlgItem(g_hWnd,IDC_OVERLAY_SCALE);
    SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(60,200)); SendMessageW(slider,TBM_SETPOS,TRUE,100);
    if(!SetTimer(g_hWnd,UiTimer,50,nullptr)) {Diagnostics::Write("UI.SetTimer",GetLastError()); DestroyWindow(g_hWnd); return false; }
    Client::Load();
#if ITPF_MCP
    if(!Bridge::Open(g_hWnd)) { Diagnostics::Write("MCP.initialize",GetLastError()); Client::notice=L"상태 : MCP 연결 초기화 실패"; }
#endif
    Client::Poll();
    Layout(g_hWnd); ShowWindow(g_hWnd,show); UpdateWindow(g_hWnd); return true;
}
