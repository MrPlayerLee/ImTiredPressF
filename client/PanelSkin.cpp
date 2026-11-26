#include "Client.h"
#include "PanelSkin.h"
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace PanelSkin
{
using namespace Gdiplus;
namespace
{
ULONG_PTR token{};
std::unique_ptr<PrivateFontCollection> fonts;
std::unique_ptr<FontFamily> face;
float scale = 1;
int originX = 0, originY = 0;
constexpr DWORD paper = 0xfffaf6ef, card = 0xfffffcf8, ink = 0xff253042;
constexpr DWORD gold = 0xffbd9458, edge = 0xffdeccb5, taupe = 0xff948678;
constexpr DWORD header = 0xfff2ece4, line = 0xffe5dbcf;

void Round(GraphicsPath& p, RectF r, float radius)
{
    const float d = 2 * radius;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.GetRight()-d, r.Y, d, d, 270, 90);
    p.AddArc(r.GetRight()-d, r.GetBottom()-d, d, d, 0, 90);
    p.AddArc(r.X, r.GetBottom()-d, d, d, 90, 90);
    p.CloseFigure();
}

void Box(Graphics& g, RectF r, float radius, DWORD fill, DWORD stroke = 0, float width = 1)
{
    GraphicsPath p;
    Round(p, r, radius);
    SolidBrush b(fill);
    g.FillPath(&b, &p);
    if (stroke) { Pen pen(stroke, width); g.DrawPath(&pen, &p); }
}

void Lines(Graphics& g, DWORD color, float width, std::initializer_list<PointF> pts)
{
    Pen pen(color, width);
    pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound);
    g.DrawLines(&pen, pts.begin(), static_cast<INT>(pts.size()));
}

void Shape(Graphics& g, DWORD color, std::initializer_list<PointF> pts)
{
    SolidBrush b(color);
    g.FillPolygon(&b, pts.begin(), static_cast<INT>(pts.size()));
}

void Star(Graphics& g, float x, float y, float r, DWORD color)
{
    float n = r * .32f;
    Shape(g,color,{{x,y-r},{x+n,y-n},{x+r,y},{x+n,y+n},{x,y+r},{x-n,y+n},{x-r,y},{x-n,y-n}});
}

void Label(Graphics& g, const wchar_t* text, RectF area, float size, bool centered = false, DWORD color = ink)
{
    Font font(face.get(),size,FontStyleBold,UnitPixel);
    SolidBrush brush(color);
    StringFormat format(StringFormatFlagsNoWrap);
    format.SetAlignment(centered ? StringAlignmentCenter : StringAlignmentNear);
    format.SetLineAlignment(StringAlignmentCenter);
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    g.DrawString(text,-1,&font,area,&format,&brush);
}

enum class Icon { Power, Layers, Chat, Puzzle, Up, Down, Refresh, Screen, Chip };
void Symbol(Graphics& g, Icon id, float x, float y, float size, DWORD color)
{
    const auto save = g.Save();
    g.TranslateTransform(x,y); g.ScaleTransform(size/24,size/24);
    Pen pen(color,2.15f);
    pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound);
    SolidBrush b(color);
    GraphicsPath p;
    switch(id)
    {
    case Icon::Power:
        g.DrawArc(&pen,3.f,3.f,18.f,18.f,310.f,280.f);
        g.DrawLine(&pen,12.f,1.f,12.f,11.f); break;
    case Icon::Layers:
        Lines(g,color,2.1f,{{1,8},{12,1},{23,8},{12,15},{1,8}});
        Lines(g,color,2.1f,{{3,13},{1,15},{12,22},{23,15},{21,13}}); break;
    case Icon::Chat:
        p.AddLine(4,3,20,3); p.AddArc(18,3,4,4,270,90);
        p.AddLine(22,5,22,17); p.AddArc(18,15,4,4,0,90);
        p.AddLine(20,19,9,19); p.AddLine(9,19,5,23); p.AddLine(5,23,5,19);
        p.AddArc(2,15,4,4,90,90); p.AddArc(2,3,4,4,180,90);
        p.CloseFigure(); g.DrawPath(&pen,&p);
        for(float cx : {6.f,11.f,16.f}) g.FillEllipse(&b,cx,9.4f,2.5f,2.5f);
        break;
    case Icon::Puzzle:
        p.AddLine(2,6,8,6); p.AddBezier(8,6,5,-1,15,-1,13,6);
        p.AddLine(13,6,19,6); p.AddLine(19,6,19,11); p.AddBezier(19,11,25,8,25,18,19,16);
        p.AddLine(19,16,19,22); p.AddLine(19,22,13,22); p.AddBezier(13,22,15,15,6,15,8,22);
        p.AddLine(8,22,2,22); p.AddLine(2,22,2,16); p.AddBezier(2,16,8,19,8,9,2,12);
        p.CloseFigure(); g.DrawPath(&pen,&p); break;
    case Icon::Up: case Icon::Down:
        Lines(g,color,2.5f,{{2,16},{2,21},{22,21},{22,16}});
        Lines(g,color,2.5f,{{12,3},{12,15}});
        if(id==Icon::Up) Lines(g,color,2.5f,{{7,8},{12,3},{17,8}});
        else Lines(g,color,2.5f,{{7,10},{12,15},{17,10}});
        break;
    case Icon::Refresh:
        g.DrawArc(&pen,2.f,2.f,20.f,20.f,198.f,133.f);
        g.DrawArc(&pen,2.f,2.f,20.f,20.f,18.f,133.f);
        Lines(g,color,2.2f,{{22,2},{22,8},{16,8}});
        Lines(g,color,2.2f,{{2,22},{2,16},{8,16}}); break;
    case Icon::Screen:
        Round(p,{1.5f,3,21,14},1.4f); g.DrawPath(&pen,&p);
        Lines(g,color,2,{{12,17},{12,21}}); Lines(g,color,2,{{7,22},{17,22}}); break;
    case Icon::Chip:
        Round(p,{4,4,16,16},2); g.DrawPath(&pen,&p);
        for(float v : {8.f,12.f,16.f}) {
            g.DrawLine(&pen,v,1.f,v,4.f); g.DrawLine(&pen,v,20.f,v,23.f);
            g.DrawLine(&pen,1.f,v,4.f,v); g.DrawLine(&pen,20.f,v,23.f,v);
        } break;
    }
    g.Restore(save);
}

void Frame(Graphics& g, RectF r)
{
    Box(g,r,10,card,edge,1.35f);
    GraphicsPath inner;
    Round(inner,r,17);
    Pen outline(edge,1.0f); g.DrawPath(&outline,&inner);
    for(float x : {r.X+6,r.GetRight()-6})
        for(float y : {r.Y+6,r.GetBottom()-6}) Star(g,x,y,2.7f,edge);
}

void Dashboard(Graphics& g)
{
    Box(g,{.8f,.8f,Width-1.6f,Height-1.6f},9,paper,0xffc9a779,1.4f);
    Lines(g,line,1,{{2,35},{Width-2,35}});
    Star(g,17,17,8,gold); Star(g,17,17,3,0xffd7bd8c);
    Lines(g,edge,1,{{31,9},{31,26}});
    Label(g,Labels::title,{36,0,230,35},16);
    Frame(g,{8,42,370,199}); Frame(g,{8,248,370,49}); Frame(g,{8,304,370,147});
    Symbol(g,Icon::Layers,19,91,20,taupe);
    Label(g,Labels::scale,{45,85,99,32},15);
    Symbol(g,Icon::Chat,19,172,20,0xff927c60);
    Label(g,Labels::automatic,{44,165,310,32},15);
    Lines(g,line,1,{{18,202},{368,202}});
    Symbol(g,Icon::Puzzle,18,262,20,0xff9e8055);
    Label(g,Labels::core,{44,252,87,40},15);
    Symbol(g,Icon::Refresh,19,315,19,gold);
    Label(g,Labels::update,{43,310,125,30},15);
    Box(g,{15,346,356,24},5,header);
    SolidBrush band(header); g.FillRectangle(&band,15.f,359.f,356.f,11.f);
    Label(g,Labels::component,{22,346,104,24},12,true);
    Label(g,Labels::current,{135,346,60,24},12,true);
    Label(g,Labels::latest,{210,346,60,24},12,true);
    for(int i=0;i<2;++i) {
        const float y=static_cast<float>(370+i*38);
        Symbol(g,i ? Icon::Chip : Icon::Screen,19,y+10,17,taupe);
        Label(g,i ? Labels::dll : Labels::exe,{39,y,116,37},13);
        Label(g,Client::Current(i),{139,y,52,37},12,true);
        Label(g,L"/",{193,y,18,37},13,true);
        Label(g,Client::Latest(i),{204,y,72,37},12,true);
        Lines(g,line,1,{{17,y+38},{369,y+38}});
    }
}

// A beveled plaque with shallow shoulders, mirrored about its center.
void Plaque(GraphicsPath& path, float w, float h, float inset)
{
    const float a=inset, b=w-inset, t=inset, d=h-inset;
    path.StartFigure();
    path.AddLine(a+16,t,b-16,t);
    path.AddBezier(b-16,t,b-9,t,b-11,t+7,b-4,t+9);
    path.AddBezier(b-4,t+9,b,t+10,b,t+12,b,t+15);
    path.AddLine(b,t+15,b,d-15);
    path.AddBezier(b,d-15,b,d-10,b-2,d-10,b-5,d-9);
    path.AddBezier(b-5,d-9,b-12,d-7,b-9,d,b-17,d);
    path.AddLine(b-17,d,a+17,d);
    path.AddBezier(a+17,d,a+10,d,a+12,d-6,a+5,d-9);
    path.AddBezier(a+5,d-9,a,d-10,a,d-11,a,d-15);
    path.AddLine(a,d-15,a,t+15);
    path.AddBezier(a,t+15,a,t+10,a+2,t+10,a+5,t+9);
    path.AddBezier(a+5,t+9,a+12,t+6,a+10,t,a+16,t);
    path.CloseFigure();
}

void Action(Graphics& g, int id, float w, float h, bool enabled, bool hover, bool down)
{
    DWORD top=enabled ? (down ? 0xffae8a57 : hover ? 0xffdfc38d : 0xffd3b47f) : 0xffeeeae5;
    DWORD bottom=enabled ? 0xffb08e5e : 0xffe4dfda;
    DWORD border=enabled ? 0xffcba76c : 0xffbdb8b3, fg=enabled ? 0xfffffbef : 0xffa29c98;
    GraphicsPath outer,inner;
    Plaque(outer,w,h,1.4f); Plaque(inner,w,h,3.5f);
    LinearGradientBrush gradient(RectF(0,0,w,h),Color(top),Color(bottom),82.f);
    Pen rim(border,2.f), hairline(enabled ? 0xffffe6b3 : 0xfff4efea,1.f);
    g.FillPath(&gradient,&outer); g.DrawPath(&rim,&outer); g.DrawPath(&hairline,&inner);
    Star(g,12,h/2,4.5f,enabled ? 0xfff9e8c0 : 0xffbfbab5);
    Star(g,w-12,h/2,4.5f,enabled ? 0xfff9e8c0 : 0xffbfbab5);
    const float cx=w/2-22, cy=h/2;
    if(id==IDC_START) Shape(g,fg,{{cx-7,cy-8},{cx+8,cy},{cx-7,cy+8}});
    else Box(g,{cx-7,cy-7,14,14},2,fg);
    Label(g,id==IDC_START ? L"시작" : L"중지",{w/2-6,0,50,h},18,false,fg);
}

RECT DesignBounds(int id)
{
    switch(id) {
    case Minimize: return {268,2,305,33}; case Maximize: return {306,2,343,33}; case Close: return {344,2,382,33};
    case IDC_STATUS: return {16,49,370,81};
    case IDC_OVERLAY_SCALE: return {146,84,369,118};
    case IDC_START: return {22,122,187,162}; case IDC_STOP: return {199,122,364,162};
    case IDC_ALTFSTOP: return {18,205,367,236};
    case Load: return {144,255,250,290}; case Unload: return {259,255,369,290};
    case CheckUpdate: return {251,310,369,340};
    case UpdateClient: return {285,375,369,403}; case UpdateCore: return {285,413,369,441};
    default: return {};
    }
}

void Control(Graphics& g, HWND hwnd)
{
    const int id=GetDlgCtrlID(hwnd);
    const RECT bounds=DesignBounds(id);
    const float w=static_cast<float>(bounds.right-bounds.left), h=static_cast<float>(bounds.bottom-bounds.top);
    const bool enabled=IsWindowEnabled(hwnd)!=FALSE, hover=GetPropW(hwnd,L"PanelHover")!=nullptr;
    const bool down=enabled && (id==IDC_START || id==IDC_STOP) && (SendMessageW(hwnd,BM_GETSTATE,0,0)&BST_PUSHED);
    g.Clear(Color(id>=Minimize && id<=Close ? paper : card));
    switch(id) {
    case Minimize: case Maximize: case Close: {
        if(hover) Box(g,{3,2,w-6,h-4},5,id==Close ? 0xfff1dfd8 : header);
        const float cx=w/2,cy=h/2;
        if(id==Minimize) Lines(g,taupe,2,{{cx-6,cy},{cx+6,cy}});
        else if(id==Maximize) {
            Pen pen(0xff60594f,1.8f);
            if(IsZoomed(GetParent(hwnd))) {g.DrawRectangle(&pen,cx-4,cy-6,10.f,10.f); Box(g,{cx-6,cy-4,10,10},1,paper,0xff60594f,1.6f);}
            else g.DrawRectangle(&pen,cx-6,cy-6,12.f,12.f);
        } else {Lines(g,0xff60594f,2,{{cx-6,cy-6},{cx+6,cy+6}}); Lines(g,0xff60594f,2,{{cx-6,cy+6},{cx+6,cy-6}});}
        break; }
    case IDC_STATUS: {
        Box(g,{0,0,w,h},12,0xfff5f0e9);
        Symbol(g,Icon::Power,10,6,20,0xff918576);
        wchar_t text[256]{}; GetWindowTextW(hwnd,text,256);
        Label(g,text,{39,0,w-43,h},wcslen(text)>19 ? 13.f : 17.f); break;}
    case IDC_OVERLAY_SCALE: {
        RECT thumb{}; SendMessageW(hwnd,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
        const float x=std::clamp((thumb.left+thumb.right)*.5f/scale,8.f,w-8);
        Lines(g,0xffc4c2c1,4,{{6,h/2},{w-7,h/2}});
        Lines(g,0xffc9a262,4,{{6,h/2},{x,h/2}});
        GraphicsPath pin;
        pin.AddArc(x-5,5.f,5.f,5.f,180,90); pin.AddArc(x,5.f,5.f,5.f,270,90);
        pin.AddLine(x+5,7.5f,x+5,22.f); pin.AddLine(x+5,22.f,x,28.f); pin.AddLine(x,28.f,x-5,22.f);
        pin.CloseFigure(); SolidBrush fill(0xfffffbf1); Pen border(gold,2); border.SetLineJoin(LineJoinRound);
        g.FillPath(&fill,&pin); g.DrawPath(&border,&pin); break;}
    case IDC_START: case IDC_STOP: Action(g,id,w,h,enabled,hover,down); break;
    case IDC_ALTFSTOP: {
        const bool on=SendMessageW(hwnd,BM_GETCHECK,0,0)==BST_CHECKED;
        Box(g,{1.5f,7.5f,16,16},4,on ? gold : card,on ? gold : 0xffbeb0a0,1.8f);
        if(on) Lines(g,0xffffffff,2.4f,{{5,15},{8,19},{14,12}});
        Label(g,L"ALT+F 시작/중지 (ALT+G 정리/중지)",{25,0,w-26,h},14); break;}
    default: {
        const bool secondary=(id==Unload);
        const DWORD rim=!enabled || secondary ? 0xffb6b0aa : 0xffc9a66b;
        GraphicsPath p; Round(p,{1.4f,1.4f,w-2.8f,h-2.8f},id==Load || secondary ? 12.f : 10.f);
        LinearGradientBrush bg(RectF(0,0,w,h),Color(card),Color(secondary ? 0xffede8e3 : 0xfff5ecdc),90.f);
        Pen pen(rim,1.65f); g.FillPath(&bg,&p); g.DrawPath(&pen,&p);
        if(id==Load || secondary) {
            Symbol(g,secondary ? Icon::Down : Icon::Up,secondary ? 17.f : 23.f,9,16,secondary ? taupe : 0xff927b57);
            Label(g,secondary ? L"언로드" : L"로드",{secondary ? 40.f : 46.f,0,w-42,h},15);
        } else if(id==CheckUpdate) {
            Symbol(g,Icon::Refresh,12,8,15,0xff8c795f); Label(g,L"업데이트 확인",{32,0,w-34,h},13);
        } else Label(g,Labels::update,{0,0,w,h},13,true);
        break;}
    }
    if(GetFocus()==hwnd && !(SendMessageW(hwnd,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)) {
        Pen focus(0xff80674a,1); focus.SetDashStyle(DashStyleDot); g.DrawRectangle(&focus,4.f,3.f,w-8,h-6);
    }
}
} // namespace

bool Initialize(HINSTANCE module)
{
    GdiplusStartupInput input;
    if(GdiplusStartup(&token,&input,nullptr)!=Ok) return false;
    fonts=std::make_unique<PrivateFontCollection>();
    HRSRC resource=FindResourceW(module,MAKEINTRESOURCEW(210),RT_RCDATA);
    if(resource) {
        void* bytes=LockResource(LoadResource(module,resource));
        if(bytes && fonts->AddMemoryFont(bytes,SizeofResource(module,resource))==Ok) {
            face=std::make_unique<FontFamily>();
            INT count=0; fonts->GetFamilies(1,face.get(),&count);
            if(count==1 && face->IsStyleAvailable(FontStyleBold)) return true;
        }
    }
    Shutdown();
    return false;
}

void Shutdown()
{
    face.reset(); fonts.reset();
    if(token) { GdiplusShutdown(token); token=0; }
}

void SetLayout(HWND hwnd)
{
    RECT r{}; GetClientRect(hwnd,&r);
    scale=std::max(.1f,std::min(r.right/static_cast<float>(Width),r.bottom/static_cast<float>(Height)));
    originX=std::lround((r.right-Width*scale)/2); originY=std::lround((r.bottom-Height*scale)/2);
}
RECT Bounds(int id)
{
    const RECT r=DesignBounds(id);
    return {originX+std::lround(r.left*scale),originY+std::lround(r.top*scale),
            originX+std::lround(r.right*scale),originY+std::lround(r.bottom*scale)};
}

void Paint(HWND hwnd, HDC target, bool mainWindow)
{
    RECT r{}; GetClientRect(hwnd,&r);
    if(!face || r.right<=0 || r.bottom<=0) return;
    HDC memory=CreateCompatibleDC(target);
    HBITMAP buffer=CreateCompatibleBitmap(target,r.right,r.bottom);
    HGDIOBJ prev=SelectObject(memory,buffer);
    {
        Graphics g(memory); g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit); g.SetPageUnit(UnitPixel);
        if(mainWindow) { g.Clear(Color(paper)); g.TranslateTransform(static_cast<float>(originX),static_cast<float>(originY)); }
        g.ScaleTransform(scale,scale);
        if(mainWindow) Dashboard(g); else Control(g,hwnd);
    }
    BitBlt(target,0,0,r.right,r.bottom,memory,0,0,SRCCOPY);
    SelectObject(memory,prev); DeleteObject(buffer); DeleteDC(memory);
}
}
