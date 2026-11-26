#include "../shared/Diagnostics.h"
#include "Common.h"
#include "Overlay.h"
#include "GameWindow.h"
#include "OverlayLayout.h"
#include <gdiplus.h>
#include <windowsx.h>
#include <string>
#include <cwchar>
#include <cstring>
#include <memory>
#include <cmath>

extern "C" IMAGE_DOS_HEADER __ImageBase;
namespace
{
    HWND controller = nullptr, card = nullptr, pin = nullptr, target = nullptr;
    ULONG_PTR graphicsToken = 0;
    HWND previousTarget = nullptr;
    bool pinned = false, dragging = false, dirty = true;
    POINT dragOrigin{}, origin{};
    int offsetX = 16, offsetY = 16;
    float desiredScale = 1.0f, currentScale = 1.0f, drawnScale = 0.0f;
    ULONGLONG lastTick = 0, lastSearch = 0;
    MacroIndicator drawnIndicator = MacroIndicator::Stopped;
    bool drawnAuto = false;
    SIZE drawnSize{};
    DWORD targetPid = 0;

    struct Surface
    {
        HDC dc = nullptr;
        HBITMAP bitmap = nullptr;
        HGDIOBJ previous = nullptr;
        void* pixels = nullptr;
        int width = 0, height = 0;
        Surface(int w, int h) : width(w), height(h)
        {
            HDC screen = GetDC(nullptr);
            if (!screen) return;
            dc = CreateCompatibleDC(screen);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = w;
            info.bmiHeader.biHeight = -h;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            if (dc) bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            ReleaseDC(nullptr, screen);
            if (bitmap)
            {
                previous = SelectObject(dc, bitmap);
                if (!previous || previous == HGDI_ERROR) { pixels = nullptr; return; }
                std::memset(pixels, 0, static_cast<size_t>(w)*h*4);
            }
        }
        ~Surface()
        {
            if (previous && previous != HGDI_ERROR) SelectObject(dc, previous);
            if (dc) DeleteDC(dc);
            if (bitmap) DeleteObject(bitmap);
        }
        bool Publish(HWND window, POINT point)
        {
            if (!pixels) { ITPF_DIAG("overlay.surface allocation",0); return false; }
            SIZE size{width,height}; POINT source{};
            BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
            if(UpdateLayeredWindow(window,nullptr,&point,&size,dc,&source,0,&blend,ULW_ALPHA))return true;
            ITPF_DIAG("overlay.UpdateLayeredWindow",GetLastError()); return false;
        }
    };

    void RoundRect(Gdiplus::Graphics& g, Gdiplus::Brush& brush,
                   float x, float y, float w, float h, float radius)
    {
        Gdiplus::GraphicsPath path;
        const float d=radius*2;
        path.AddArc(x,y,d,d,180,90); path.AddArc(x+w-d,y,d,d,270,90);
        path.AddArc(x+w-d,y+h-d,d,d,0,90); path.AddArc(x,y+h-d,d,d,90,90);
        path.CloseFigure(); g.FillPath(&brush,&path);
    }

    using namespace Gdiplus;
    std::unique_ptr<PrivateFontCollection> fontCollection;
    std::unique_ptr<FontFamily> overlayFace;

    void LoadOverlayFont()
    {
        HMODULE exe=GetModuleHandleW(nullptr);
        HRSRC resource=FindResourceW(exe,MAKEINTRESOURCEW(210),RT_RCDATA);
        if (!resource) return;
        void* bytes=LockResource(LoadResource(exe,resource));
        fontCollection=std::make_unique<PrivateFontCollection>();
        if (!bytes || fontCollection->AddMemoryFont(bytes,SizeofResource(exe,resource))!=Ok) return;
        auto family=std::make_unique<FontFamily>();
        INT count=0;
        if (fontCollection->GetFamilies(1,family.get(),&count)==Ok && count==1)
            overlayFace=std::move(family);
    }

    void Star(Graphics& g, Brush& brush,float x,float y,float w,float h)
    {
        PointF pts[]={{x,y-h},{x+w*.25f,y-h*.24f},{x+w,y},{x+w*.25f,y+h*.24f},
                      {x,y+h},{x-w*.25f,y+h*.24f},{x-w,y},{x-w*.25f,y-h*.24f}};
        g.FillPolygon(&brush,pts,8);
    }

    void DrawFrame(Graphics& g)
    {
        LinearGradientBrush silver(PointF(0,0),PointF(34,104),Color(255,226,240,253),Color(255,124,164,222));
        LinearGradientBrush night(PointF(0,0),PointF(0,104),Color(247,13,19,31),Color(250,35,54,84));
        silver.SetWrapMode(WrapModeTileFlipXY);
        SolidBrush rim(Color(250,59,81,118)), ice(Color(247,219,237,255)), ghost(Color(34,111,146,202));
        RoundRect(g,rim,0,0,250,104,14);
        RoundRect(g,silver,1.8f,1.8f,246.4f,100.4f,12);
        RoundRect(g,night,3.1f,3.1f,243.8f,97.8f,11);
        Pen orbit(Color(34,133,161,204),.85f);
        g.DrawArc(&orbit,175,28,70,71,145,155);
        Star(g,ghost,211,66,25,30);
        Pen filigree(Color(245,229,225,214),1.15f);
        for(int corner=0;corner<2;++corner) {
            auto state=g.Save();
            if(corner){g.TranslateTransform(250,104);g.RotateTransform(180);}
            g.DrawBezier(&filigree,3,43,5,19,19,24,28,10);
            g.DrawBezier(&filigree,28,10,32,5,39,4,48,3);
            g.DrawBezier(&filigree,19,18,23,15,26,18,25,14);
            Star(g,ice,13,12,4,5);
            g.Restore(state);
        }
        LinearGradientBrush line(PointF(12,0),PointF(237,0),Color(20,170,196,230),Color(20,170,196,230));
        Color stops[]={Color(12,170,196,230),Color(255,204,230,255),Color(12,170,196,230)};
        REAL positions[]={0,.5f,1};line.SetInterpolationColors(stops,positions,3);
        g.FillRectangle(&line,10,54,230,1);
        Star(g,ice,125,54.5f,4.1f,3.1f);
        Star(g,ice,24,30,1.8f,7);
        Star(g,ice,44,77,1.4f,6);
        PointF teeth[32];
        for(int i=0;i<32;++i){
            const float a=i*0.19634954f-.09817477f;
            const float r=(i%4==1 || i%4==2)?6.3f:4.9f;
            teeth[i]={26+r*std::cos(a),77+r*std::sin(a)};
        }
        Pen gear(Color(255,183,218,252),1.5f);
        g.DrawPolygon(&gear,teeth,32);g.DrawEllipse(&gear,23.8f,74.8f,4.4f,4.4f);
    }

    void DrawCard(Surface& surface, float scale, MacroIndicator indicator, bool automatic)
    {
        if (!surface.pixels) return;
        Gdiplus::Bitmap bitmap(surface.width,surface.height,surface.width*4,
                              PixelFormat32bppPARGB,static_cast<BYTE*>(surface.pixels));
        Gdiplus::Graphics g(&bitmap);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        g.ScaleTransform(scale,scale);
        DrawFrame(g);
        Gdiplus::SolidBrush text(Gdiplus::Color(255,244,248,255));
        const FontFamily* face=overlayFace ? overlayFace.get() : FontFamily::GenericSansSerif();
        Font font(face,17,FontStyleBold,UnitPixel),secondary(face,15,FontStyleBold,UnitPixel);
        Gdiplus::StringFormat format;
        format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        g.DrawString(L"상태 :",-1,&font,Gdiplus::RectF(33,18,52,26),&format,&text);
        Gdiplus::Color color = indicator==MacroIndicator::Working ? Gdiplus::Color(255,64,156,255)
            : indicator==MacroIndicator::Ready ? Gdiplus::Color(255,59,214,132)
            : Gdiplus::Color(255,246,93,110);
        Gdiplus::SolidBrush light(color); g.FillEllipse(&light,84,24,12,12);
        const wchar_t* label=indicator==MacroIndicator::Working ? L"작동중"
            : indicator==MacroIndicator::Ready ? L"준비완료" : L"작동중지";
        g.DrawString(label,-1,&font,Gdiplus::RectF(103,18,105,26),&format,&text);
        g.DrawString(automatic ? L"자동감지 : 활성화" : L"자동감지 : 비활성화",
                     -1,&secondary,Gdiplus::RectF(50,66,190,27),&format,&text);
        g.Flush(Gdiplus::FlushIntentionSync);
    }

    void DrawPin(Surface& surface)
    {
        if (!surface.pixels) return;
        Gdiplus::Bitmap bitmap(surface.width,surface.height,surface.width*4,
                              PixelFormat32bppPARGB,static_cast<BYTE*>(surface.pixels));
        Gdiplus::Graphics g(&bitmap);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.ScaleTransform(surface.width/26.0f,surface.height/26.0f);
        Gdiplus::SolidBrush fill(pinned ? Gdiplus::Color(255,43,117,87) : Gdiplus::Color(255,57,67,84));
        SolidBrush edge(Color(255,173,211,254));
        RoundRect(g,edge,0,0,26,26,6);RoundRect(g,fill,1.1f,1.1f,23.8f,23.8f,5);
        if (!pinned) { g.TranslateTransform(13,13); g.RotateTransform(38); g.TranslateTransform(-13,-13); }
        Gdiplus::Pen stroke(Gdiplus::Color(255,244,248,255),1.6f);
        Gdiplus::PointF points[]{ {8,6},{18,6},{16,9},{16,14},{19,16},{7,16},{10,14},{10,9},{8,6} };
        g.DrawLines(&stroke,points,9); g.DrawLine(&stroke,13,16,13,22);
        g.Flush(Gdiplus::FlushIntentionSync);
    }

    void Hide()
    {
        if (dragging) { dragging=false; if (GetCapture()==card) ReleaseCapture(); }
        if (pin && IsWindowVisible(pin)) ShowWindow(pin,SW_HIDE);
        if (card && IsWindowVisible(card)) ShowWindow(card,SW_HIDE);
    }

    // Only move our windows in Z order, directly above the game and below
    // every pre-existing higher window. Never make a desktop-topmost overlay.
    void Position(HWND window, HWND after, int x,int y,int width,int height,UINT flags)
    {
        if(!SetWindowPos(window,after,x,y,width,height,flags)) { ITPF_DIAG("overlay.SetWindowPos",GetLastError()); }
    }
    void StackAboveTarget()
    {
        if (GetWindowLongPtrW(target,GWL_EXSTYLE)&WS_EX_TOPMOST)
        {
            Position(card,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
            Position(pin,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
            return;
        }
        if (GetWindowLongPtrW(card,GWL_EXSTYLE)&WS_EX_TOPMOST)
            Position(card,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        if (GetWindowLongPtrW(pin,GWL_EXSTYLE)&WS_EX_TOPMOST)
            Position(pin,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        HWND above=GetWindow(target,GW_HWNDPREV);
        while (above==card || above==pin) above=GetWindow(above,GW_HWNDPREV);
        if (above && (GetWindowLongPtrW(above,GWL_EXSTYLE)&WS_EX_TOPMOST))
        {
            // Inserting after a topmost window can promote this window. For
            // non-topmost games the top of the ordinary band is sufficient.
            above=HWND_TOP;
        }
        if (!above) above=HWND_TOP;
        HWND current=GetWindow(target,GW_HWNDPREV);
        if (current!=card || !IsWindowVisible(card))
            Position(card,above,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        if (GetWindow(card,GW_HWNDPREV)!=pin || !IsWindowVisible(pin))
            Position(pin,above,
                         0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    }

    void Refresh()
    {
        if (!card || !pin) return;
        if (!IsWindowVisible(controller)) { ITPF_DIAG("overlay.hidden controller not visible",0); Hide(); return; }
        if (!target || !IsWindow(target)) { ITPF_DIAG("overlay.no matching game window",0); Hide(); return; }
        if (!IsWindowVisible(target) || IsIconic(target)) { ITPF_DIAG("overlay.hidden game invisible/minimized",0); Hide(); return; }
        // For topmost/fullscreen windows, show only while the game really has
        // foreground. Event observation promptly hides the pair on app switch.
        if ((GetWindowLongPtrW(target,GWL_EXSTYLE)&WS_EX_TOPMOST) &&
            GetAncestor(GetForegroundWindow(),GA_ROOT)!=target) { ITPF_DIAG("overlay.hidden topmost game not foreground",0); Hide(); return; }
        DWORD pid=0; GetWindowThreadProcessId(target,&pid);
        if (pid!=targetPid) { ITPF_DIAG("overlay.game process changed",0); target=nullptr; Hide(); return; }
        RECT client{};
        if (!GetClientRect(target,&client)) { ITPF_DIAG("overlay.GetClientRect",GetLastError()); Hide(); return; }
        const int width=client.right, height=client.bottom;
        if (width<64 || height<32) { ITPF_DIAG("overlay.game area too small",0); Hide(); return; }
        POINT screen{};
        if (!ClientToScreen(target,&screen)) { ITPF_DIAG("overlay.ClientToScreen",GetLastError()); Hide(); return; }
        auto layout=OverlayLayout::Fit(width,height,offsetX,offsetY,currentScale);
        offsetX=layout.x; offsetY=layout.y;
        MacroIndicator indicator=GetMacroIndicator();
        bool automatic=g_autoDetect.load();
        bool redraw=dirty || drawnScale!=layout.scale || drawnSize.cx!=layout.width ||
            drawnSize.cy!=layout.height || drawnIndicator!=indicator || drawnAuto!=automatic;
        POINT position{screen.x+layout.x,screen.y+layout.y};
        POINT pinPosition{screen.x+layout.pinX,screen.y+layout.pinY};
        if (redraw)
        {
            Surface body(layout.width,layout.height), button(layout.pinSize,layout.pinSize);
            DrawCard(body,layout.scale,indicator,automatic); DrawPin(button);
            if (!body.Publish(card,position) || !button.Publish(pin,pinPosition)) { Hide(); return; }
            drawnScale=layout.scale; drawnSize={layout.width,layout.height};
            drawnIndicator=indicator; drawnAuto=automatic; dirty=false;
        }
        else
        {
            Position(card,nullptr,position.x,position.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
            Position(pin,nullptr,pinPosition.x,pinPosition.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        }
        // Show in the correct order atomically, without ShowWindow raising an
        // initially hidden popup above unrelated applications.
        const bool restoring = !IsWindowVisible(card) || !IsWindowVisible(pin);
        StackAboveTarget();
        if (redraw || restoring) {
            char summary[256]{};
            sprintf_s(summary,"overlay.render game=%dx%d card=%dx%d visible=%d pinVisible=%d "
                      "onMonitor=%d cardDirectlyAboveGame=%d gameForeground=%d gameTopmost=%d",
                      width,height,layout.width,layout.height,!!IsWindowVisible(card),!!IsWindowVisible(pin),
                      MonitorFromWindow(card,MONITOR_DEFAULTTONULL)!=nullptr,
                      GetWindow(target,GW_HWNDPREV)==card, GetAncestor(GetForegroundWindow(),GA_ROOT)==target,
                      !!(GetWindowLongPtrW(target,GWL_EXSTYLE)&WS_EX_TOPMOST));
            ITPF_DIAG(summary,0);
        }
    }

    void TogglePin()
    {
        pinned=!pinned;
        LONG_PTR style=GetWindowLongPtrW(card,GWL_EXSTYLE);
        if (pinned) style|=WS_EX_TRANSPARENT; else style&=~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
        SetWindowLongPtrW(card,GWL_EXSTYLE,style);
        Position(card,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        dirty=true; Refresh();
    }

    LRESULT CALLBACK OverlayProc(HWND window,UINT message,WPARAM wp,LPARAM lp)
    {
        switch(message)
        {
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: { PAINTSTRUCT paint{}; BeginPaint(window,&paint); EndPaint(window,&paint); return 0; }
        case WM_LBUTTONDOWN:
            if (window==pin) { TogglePin(); return 0; }
            if (!pinned)
            {
                GetCursorPos(&dragOrigin); origin={offsetX,offsetY};
                dragging=true; SetCapture(window);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (dragging && window==card)
            {
                POINT point{}; GetCursorPos(&point);
                offsetX=origin.x+point.x-dragOrigin.x; offsetY=origin.y+point.y-dragOrigin.y;
                Refresh();
            }
            return 0;
        case WM_LBUTTONUP:
            if (dragging && window==card) { dragging=false; ReleaseCapture(); }
            return 0;
        case WM_CAPTURECHANGED: dragging=false; return 0;
        case WM_CLOSE: return 0;
        }
        return DefWindowProcW(window,message,wp,lp);
    }


}

namespace Overlay
{
    bool Initialize(HWND parent)
    {
        controller=parent;
        Gdiplus::GdiplusStartupInput startup;
        auto status=Gdiplus::GdiplusStartup(&graphicsToken,&startup,nullptr);
        if(status!=Gdiplus::Ok){Diagnostics::Write("overlay.GdiplusStartup",status);return false;}
        LoadOverlayFont();
        WNDCLASSW type{};
        type.hInstance=reinterpret_cast<HINSTANCE>(&__ImageBase); type.lpfnWndProc=OverlayProc;
        type.lpszClassName=L"ImTiredPressF_Overlay"; type.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        if (!RegisterClassW(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) { Diagnostics::Write("overlay.RegisterClass",GetLastError());Shutdown(); return false; }
        constexpr DWORD style=WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE;
        card=CreateWindowExW(style,type.lpszClassName,L"원신 입력 상태",WS_POPUP,0,0,250,104,
                             nullptr,nullptr,type.hInstance,nullptr);
        if(!card)Diagnostics::Write("overlay.CreateWindow card",GetLastError());
        pin=CreateWindowExW(style,type.lpszClassName,L"오버레이 고정",WS_POPUP,0,0,26,26,
                            nullptr,nullptr,type.hInstance,nullptr);
        if(!pin)Diagnostics::Write("overlay.CreateWindow pin",GetLastError());
        if (!card || !pin) { Shutdown(); return false; }
        // Keep this status UI out of supported desktop capture paths so it
        // does not become part of a captured OCR region after dragging.
        if(!SetWindowDisplayAffinity(card,WDA_EXCLUDEFROMCAPTURE)) Diagnostics::Write("overlay.SetWindowDisplayAffinity card",GetLastError());
        if(!SetWindowDisplayAffinity(pin,WDA_EXCLUDEFROMCAPTURE)) Diagnostics::Write("overlay.SetWindowDisplayAffinity pin",GetLastError());
        // Tick refreshes window position and foreground state every frame.
        lastTick=GetTickCount64(); dirty=true;
        return true;
    }

    void Shutdown()
    {
        Hide();
        if (pin) DestroyWindow(pin);
        if (card) DestroyWindow(card);
        pin=card=controller=target=previousTarget=nullptr;
        UnregisterClassW(L"ImTiredPressF_Overlay",reinterpret_cast<HINSTANCE>(&__ImageBase));
        overlayFace.reset();fontCollection.reset();
        if (graphicsToken) Gdiplus::GdiplusShutdown(graphicsToken);
        graphicsToken=0;
    }

    void SetScale(float scale) { desiredScale=(std::clamp)(scale,0.6f,2.0f); }

    void Tick()
    {
        const ULONGLONG now=GetTickCount64();
        const float seconds=(std::min)(0.05f,static_cast<float>(now-lastTick)/1000.0f);
        lastTick=now;
        currentScale=OverlayLayout::Animate(currentScale,desiredScale,seconds);
        if (!lastSearch || now-lastSearch>=1000 || (target && !IsWindow(target)))
        {
            target=GameWindow::Find(); targetPid=0;
            if (target) GetWindowThreadProcessId(target,&targetPid);
            lastSearch=now;
            if (target!=previousTarget) { Diagnostics::Write(target?"overlay.game detected":"overlay.game lost");offsetX=offsetY=16; dirty=true; previousTarget=target; }
        }
        Refresh();
    }
}
