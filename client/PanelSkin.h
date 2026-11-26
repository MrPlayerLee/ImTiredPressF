#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <memory>

namespace PanelSkin
{
    enum Element { Minimize = 1100, Maximize, Close, Load, Unload, CheckUpdate, UpdateClient, UpdateCore };
    constexpr int Width = 386, Height = 460;
    bool Initialize(HINSTANCE module);
    void Shutdown();
    void SetLayout(HWND window);
    RECT Bounds(int id);
    void Paint(HWND window, HDC target, bool mainWindow);
}
