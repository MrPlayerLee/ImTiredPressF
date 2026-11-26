#include "../core/Overlay.cpp"
#include <iostream>
#include <cstdlib>
#include <vector>

std::atomic<HWND> g_gameHwnd{nullptr};
std::atomic<bool> g_autoDetect{false};
MacroIndicator GetMacroIndicator() { return MacroIndicator::Stopped; }

namespace
{
    int overlayChecks=0;
    void Verify(bool condition,const char* message)
    {
        ++overlayChecks;
        if (!condition) { std::cerr<<message<<'\n'; std::exit(1); }
    }
    void SavePreview(const wchar_t* directory,const wchar_t* name,MacroIndicator indicator,bool automatic,float scale)
    {
        Surface surface(static_cast<int>(OverlayLayout::Width*scale),static_cast<int>(OverlayLayout::Height*scale));
        DrawCard(surface,scale,indicator,automatic);
        Gdiplus::Bitmap image(surface.width,surface.height,surface.width*4,
                             PixelFormat32bppPARGB,static_cast<BYTE*>(surface.pixels));
        const int sizePin=static_cast<int>(26*scale);
        Surface button(sizePin,sizePin); DrawPin(button);
        Gdiplus::Bitmap pinImage(button.width,button.height,button.width*4,
                                PixelFormat32bppPARGB,static_cast<BYTE*>(button.pixels));
        {
            Gdiplus::Graphics composition(&image);
            composition.DrawImage(&pinImage,static_cast<int>(218*scale),static_cast<int>(6*scale),sizePin,sizePin);
        }
        UINT count=0,size=0; Gdiplus::GetImageEncodersSize(&count,&size);
        std::vector<BYTE> storage(size);
        auto codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
        Gdiplus::GetImageEncoders(count,size,codecs);
        for (UINT i=0;i<count;++i)
            if (!std::wcscmp(codecs[i].MimeType,L"image/png"))
            {
                std::wstring path=std::wstring(directory)+L"\\"+name;
                Verify(image.Save(path.c_str(),&codecs[i].Clsid,nullptr)==Gdiplus::Ok,"save overlay preview");
                return;
            }
        Verify(false,"PNG encoder available");
    }
}

int wmain(int argc,wchar_t** argv)
{
    for (int w : {64,125,250,800,1920})
        for (int h : {32,75,104,600,1080})
            for (float scale : {0.6f,1.0f,1.37f,2.0f})
                for (int position : {-500,0,16,2000})
                {
                    auto fit=OverlayLayout::Fit(w,h,position,position,scale);
                    Verify(fit.x>=0 && fit.y>=0 && fit.x+fit.width<=w && fit.y+fit.height<=h,"body stays inside game client");
                    Verify(fit.pinX>=fit.x && fit.pinY>=fit.y &&
                           fit.pinX+fit.pinSize<=fit.x+fit.width && fit.pinY+fit.pinSize<=fit.y+fit.height,"pin stays inside body");
                }
    float scale=1;
    for (int i=0;i<30;++i)
    {
        float next=OverlayLayout::Animate(scale,2,0.016f);
        Verify(next>=scale && next<=2 && next-scale<0.3f,"smooth monotone scaling");
        scale=next;
    }
    Verify(scale>1.99f,"scale animation converges");
    HWND before=GetForegroundWindow();
    HWND parent=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"Overlay test controller",WS_POPUP,
        0,0,100,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Verify(parent!=nullptr && Overlay::Initialize(parent),"initialize independent overlay UI");
    Verify((GetWindowLongPtrW(card,GWL_EXSTYLE)&WS_EX_NOACTIVATE)!=0,"body never requests activation");
    Verify(SendMessageW(card,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE,"drag click does not activate");
    SendMessageW(pin,WM_LBUTTONDOWN,MK_LBUTTON,0);
    Verify((GetWindowLongPtrW(card,GWL_EXSTYLE)&WS_EX_TRANSPARENT)!=0,"pinned body passes through clicks");
    Verify((GetWindowLongPtrW(pin,GWL_EXSTYLE)&WS_EX_TRANSPARENT)==0,"pin remains clickable");
    SendMessageW(pin,WM_LBUTTONDOWN,MK_LBUTTON,0);
    Verify((GetWindowLongPtrW(card,GWL_EXSTYLE)&WS_EX_TRANSPARENT)==0,"unpin enables dragging");
    // Offscreen fixtures test Z order and bounds without interacting with the
    // real game, moving the pointer or exposing a test window on the desktop.
    SetWindowPos(parent,HWND_TOP,30000,30000,800,600,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    HWND covering=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"Private cover",WS_POPUP,
        30000,30000,800,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    SetWindowPos(covering,HWND_TOP,30000,30000,800,600,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    target=parent; targetPid=GetCurrentProcessId();
    Refresh();
    Verify(IsWindowVisible(card) && IsWindowVisible(pin),"show overlay for visible private target");
    Verify(GetWindow(target,GW_HWNDPREV)==card && GetWindow(card,GW_HWNDPREV)==pin,
           "overlay pair lies directly above target");
    HWND next=covering;
    for (int i=0;i<100 && next && next!=pin;++i) next=GetWindow(next,GW_HWNDNEXT);
    Verify(next==pin,"higher unrelated window remains above overlay");
    offsetX=offsetY=10000; Refresh();
    RECT bounds{}; GetWindowRect(card,&bounds);
    Verify(bounds.left>=30000 && bounds.top>=30000 && bounds.right<=30800 && bounds.bottom<=30600,
           "drag beyond edge clamps real overlay window");
    SetWindowPos(parent,nullptr,30100,30100,800,600,SWP_NOZORDER|SWP_NOACTIVATE); Refresh();
    GetWindowRect(card,&bounds);
    Verify(bounds.left>=30100 && bounds.top>=30100 && bounds.right<=30900 && bounds.bottom<=30700,
           "real overlay follows target movement");
    ShowWindow(parent,SW_HIDE); Refresh();
    Verify(!IsWindowVisible(card) && !IsWindowVisible(pin),"hidden target hides body and pin");
    DestroyWindow(covering); target=nullptr;
    // Exercise actual no-activate layered rendering and shutdown without using
    // the real game or showing a window on the user's desktop.
    Surface surface(250,104);
    Verify(surface.pixels!=nullptr,"allocate premultiplied overlay bitmap");
    DrawCard(surface,1,MacroIndicator::Ready,true);
    const BYTE* pixels=static_cast<const BYTE*>(surface.pixels);
    const size_t dot=(30u*250u+90u)*4u;
    Verify(pixels[dot+1]>pixels[dot+2] && pixels[dot+1]>pixels[dot],"ready dot is green");
    DrawCard(surface,1,MacroIndicator::Working,true);
    Verify(pixels[dot]>pixels[dot+1] && pixels[dot]>pixels[dot+2],"working dot is blue");
    DrawCard(surface,1,MacroIndicator::Stopped,false);
    Verify(pixels[dot+2]>pixels[dot+1] && pixels[dot+2]>pixels[dot],"stopped dot is red");
    Verify(surface.Publish(card,{0,0}),"publish real layered window without activation");
    if (argc>1)
    {
        Verify(overlayFace!=nullptr,"load embedded Pretendard family");
        SavePreview(argv[1],L"overlay-ready.png",MacroIndicator::Ready,true,1.5f);
        SavePreview(argv[1],L"overlay-working.png",MacroIndicator::Working,true,1.5f);
        SavePreview(argv[1],L"overlay-stopped.png",MacroIndicator::Stopped,true,1.0f);
    }
    HWND oldCard=card,oldPin=pin;
    Overlay::Shutdown(); DestroyWindow(parent);
    Verify(!IsWindow(oldCard) && !IsWindow(oldPin),"overlay windows destroyed with controller");
    Verify(GetForegroundWindow()==before,"hidden overlay tests retain foreground");
    std::cout<<"PASS: "<<overlayChecks<<" overlay layout, pin, rendering and lifecycle checks.\n";
}
