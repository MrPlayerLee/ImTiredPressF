#include "WinRtCompatibility.h"
#include "Common_OpenCV.h"
#include "Common.h"
#include "DialogueDetection.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>
#include <limits>
#include <algorithm>

namespace Vision { extern winrt::Windows::Media::Ocr::OcrEngine g_ocrEngine; extern bool g_ocrInitialized; }
namespace
{
    DialogueDetection::State detection;

    bool ReadEnter(const cv::Mat& gray)
    {
        cv::Mat bgra;
        cv::cvtColor(gray, bgra, cv::COLOR_GRAY2BGRA);
        const size_t bytes = bgra.total() * bgra.elemSize();
        if (bytes > (std::numeric_limits<uint32_t>::max)()) throw std::runtime_error("OCR image too large");
        winrt::Windows::Storage::Streams::DataWriter writer;
        writer.WriteBytes(winrt::array_view<uint8_t>(bgra.data, bgra.data + bytes));
        auto bitmap = winrt::Windows::Graphics::Imaging::SoftwareBitmap::CreateCopyFromBuffer(
            writer.DetachBuffer(), winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
            bgra.cols, bgra.rows);
        auto result = Vision::g_ocrEngine.RecognizeAsync(bitmap).get();
        // Match a whole word: scene text such as "Center" must not end a session.
        for (const auto& line : result.Lines())
            for (const auto& word : line.Words())
                if (DialogueDetection::IsEnterWord(word.Text().c_str())) return true;
        return false;
    }
}

namespace Vision
{
    void ResetDetection()
    {
        detection = {};
        g_detected = false;
        g_score = 0.5;
    }

    void Update(HWND hwnd)
    {
        try
        {
            if (!g_ocrInitialized) throw std::runtime_error("OCR unavailable");
            cv::Mat frame;
            if (!Capture::CaptureClient(hwnd, frame) || frame.empty())
                throw std::runtime_error("Capture failed");
            // Preserve the existing bottom-left client-area search region.
            const int top = frame.rows * 3 / 4;
            if (frame.cols < 4 || frame.rows < 4) throw std::runtime_error("Invalid capture size");
            cv::Mat roi = frame(cv::Rect(0, top, frame.cols / 4, frame.rows - top));
            cv::Mat gray;
            if (roi.type() == CV_8UC1) gray = roi;
            else if (roi.type() == CV_8UC3) cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
            else if (roi.type() == CV_8UC4) cv::cvtColor(roi, gray, cv::COLOR_BGRA2GRAY);
            else throw std::runtime_error("Invalid capture format");

            // Normalize glyph size, respecting the installed OCR engine's limit.
            const double limit = winrt::Windows::Media::Ocr::OcrEngine::MaxImageDimension();
            const double scale = (std::min)(2160.0 / frame.rows,
                limit / static_cast<double>((std::max)(gray.cols, gray.rows)));
            cv::Mat enlarged;
            cv::resize(gray, enlarged, cv::Size(), scale, scale, cv::INTER_CUBIC);
            bool enter = ReadEnter(enlarged);
            if (!enter)
            {
                // Remove slowly varying scene brightness, retaining bright UI strokes.
                // The raw pass remains available for dark/outlined prompt variants.
                cv::Mat strokes, mask;
                cv::morphologyEx(enlarged, strokes, cv::MORPH_TOPHAT,
                    cv::getStructuringElement(cv::MORPH_RECT, cv::Size(15, 15)));
                cv::threshold(strokes, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
                cv::bitwise_not(mask, mask);
                enter = ReadEnter(mask);
            }
            g_score = detection.Observe(enter, GetTickCount64());
            g_detected = g_score == 1.0;
        }
        catch (...)
        {
            ResetDetection();
            g_score = -1.0;
        }
    }

    bool IsDetected() { return g_detected; }
    double GetScore() { return g_score; }
    cv::Point GetMatchPoint() { return g_matchPoint; }
}
