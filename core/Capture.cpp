#include "../shared/Diagnostics.h"
#include "Common_OpenCV.h"

namespace
{
    // Destruction also runs when OpenCV allocation/conversion throws.
    struct CaptureResources
    {
        HWND window = nullptr;
        HDC windowDC = nullptr;
        HDC memoryDC = nullptr;
        HBITMAP bitmap = nullptr;
        HGDIOBJ previous = nullptr;
        bool selected = false;

        ~CaptureResources()
        {
            if (selected) SelectObject(memoryDC, previous);
            if (memoryDC) DeleteDC(memoryDC);
            if (bitmap) DeleteObject(bitmap);
            if (windowDC) ReleaseDC(window, windowDC);
        }

        bool Deselect()
        {
            HGDIOBJ result = SelectObject(memoryDC, previous);
            if (!result || result == HGDI_ERROR) return false;
            selected = false;
            return true;
        }
    };

    bool ConvertBitmapToMat(HDC dc, HBITMAP bitmap, int width, int height,
                            cv::Mat& outputImage)
    {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        cv::Mat image(height, width, CV_8UC4);
        // GetDIBits requires the bitmap to be deselected from all DCs.
        if (GetDIBits(dc, bitmap, 0, static_cast<UINT>(height), image.data,
                      &info, DIB_RGB_COLORS) != height) return false;
        cv::cvtColor(image, outputImage, cv::COLOR_BGRA2GRAY);
        return true;
    }
}

namespace Capture
{
    bool CaptureClient(HWND hwnd, cv::Mat& outputImage)
    {
        outputImage.release();
        if (!hwnd || !IsWindow(hwnd)) return false;
        RECT rect{};
        if (!GetClientRect(hwnd, &rect)) { ITPF_DIAG("capture.GetClientRect",GetLastError());return false; }
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0) return false;

        CaptureResources resources;
        resources.window = hwnd;
        resources.windowDC = GetDC(hwnd);
        if (!resources.windowDC) { ITPF_DIAG("capture.GetDC",GetLastError());return false; }
        resources.memoryDC = CreateCompatibleDC(resources.windowDC);
        if (!resources.memoryDC) { ITPF_DIAG("capture.CreateCompatibleDC",GetLastError());return false; }
        resources.bitmap = CreateCompatibleBitmap(resources.windowDC, width, height);
        if (!resources.bitmap) { ITPF_DIAG("capture.CreateCompatibleBitmap",GetLastError());return false; }
        resources.previous = SelectObject(resources.memoryDC, resources.bitmap);
        if (!resources.previous || resources.previous == HGDI_ERROR) { ITPF_DIAG("capture.SelectObject",GetLastError());return false; }
        resources.selected = true;

        // Preserve the existing backend order and client-area coordinates.
        BOOL success = PrintWindow(hwnd, resources.memoryDC, PW_CLIENTONLY);
        if (!success)
            success = BitBlt(resources.memoryDC, 0, 0, width, height,
                             resources.windowDC, 0, 0, SRCCOPY);

        if(!success){ ITPF_DIAG("capture.PrintWindow and BitBlt failed",GetLastError()); }
        if (!resources.Deselect() || !success) return false;
        return ConvertBitmapToMat(resources.windowDC, resources.bitmap, width, height,
                                  outputImage) && !outputImage.empty();
    }
}
