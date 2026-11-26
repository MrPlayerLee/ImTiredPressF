#include "WinRtCompatibility.h"
#include "Common_OpenCV.h"

//=========================================================
// Header
//=========================================================

#include <vector>
// Enable C++/WinRT implementation in this translation unit to provide
// necessary WINRT_IMPL_* symbols (e.g. RoGetActivationFactory wrappers).
#define WINRT_IMPL 1
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Ocr.h>


//=========================================================
// Shared State
//=========================================================

namespace Vision
{
    cv::Mat g_templateImage;

    double g_score = 0.0;

    bool g_detected = false;

    cv::Point g_matchPoint(-1, -1);

    // OCR engine and init flag
    winrt::Windows::Media::Ocr::OcrEngine g_ocrEngine{ nullptr };
    bool g_ocrInitialized = false;
    bool g_apartmentInitialized = false;
}

//=========================================================
// Vision
//=========================================================

namespace Vision
{
    //-----------------------------------------------------
    // Initialize
    //-----------------------------------------------------

    bool Initialize()
    {
        // Initialize WinRT apartment for OCR
        try
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            g_apartmentInitialized = true;
        }
        catch (...) { return false; }

        //-------------------------------------------------
        // 상태 초기화
        //-------------------------------------------------

        g_score = 0.0;

        g_detected = false;

        g_matchPoint = cv::Point(-1, -1);

        // Initialize OCR engine
        try
        {
            auto engine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromUserProfileLanguages();

            if (engine)
            {
                g_ocrEngine = engine;
                g_ocrInitialized = true;
            }
        }
        catch (...) { /* ignore OCR init failures */ }

        return g_ocrInitialized;
    }

    //-----------------------------------------------------
    // Shutdown
    //-----------------------------------------------------

    void Shutdown()
    {

        g_score = 0.0;

        g_detected = false;

        g_matchPoint = cv::Point(-1, -1);

        // Shutdown OCR
        try
        {
            g_ocrEngine = nullptr;
            g_ocrInitialized = false;
            if (g_apartmentInitialized)
            {
                winrt::uninit_apartment();
                g_apartmentInitialized = false;
            }
        }
        catch (...) { }
    }
}