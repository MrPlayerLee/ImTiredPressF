#pragma once
#include <algorithm>
#include <cmath>
#include <windows.h>

namespace OverlayLayout
{
    constexpr float Width = 250.0f, Height = 104.0f;
    struct Placement { int x, y, width, height, pinX, pinY, pinSize; float scale; };
    inline Placement Fit(int clientWidth, int clientHeight, int x, int y, float scale)
    {
        scale = (std::max)(0.0f, (std::min)(scale,
            (std::min)(clientWidth / Width, clientHeight / Height)));
        int width = (std::min)(clientWidth, static_cast<int>(std::ceil(Width * scale)));
        int height = (std::min)(clientHeight, static_cast<int>(std::ceil(Height * scale)));
        x = (std::clamp)(x, 0, (std::max)(0, clientWidth - width));
        y = (std::clamp)(y, 0, (std::max)(0, clientHeight - height));
        int pinSize = (std::max)(1, static_cast<int>(std::ceil(26.0f * scale)));
        return {x, y, width, height,
                x + width - pinSize - static_cast<int>(6.0f * scale),
                y + static_cast<int>(6.0f * scale), pinSize, scale};
    }
    inline float Animate(float current, float target, float seconds)
    {
        if (std::abs(target-current) < 0.001f) return target;
        return current + (target-current) * (1.0f-std::exp(-seconds*18.0f));
    }
}
