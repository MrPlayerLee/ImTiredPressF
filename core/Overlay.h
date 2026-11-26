#pragma once
#include <windows.h>

namespace Overlay
{
    bool Initialize(HWND controller);
    void Shutdown();
    void Tick();
    void SetScale(float scale);
}
