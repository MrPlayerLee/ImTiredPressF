#include "../shared/Diagnostics.h"
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

    struct SampleContext
    {
        const char* stage = "engine";
        const char* pass = "none";
        cv::Size frame{}, ocr{};

        void Report(const char* kind, DWORD code) const
        {
            char event[256]{};
            sprintf_s(event, "vision.failure stage=%s pass=%s frame=%dx%d ocr=%dx%d kind=%s",
                      stage, pass, frame.width, frame.height, ocr.width, ocr.height, kind);
            Diagnostics::Write(event, code);
        }
    };

    bool ReadEnter(const cv::Mat& gray, SampleContext& context)
    {
        context.stage = "ocr.convert";
        context.ocr = gray.size();
        cv::Mat bgra;
        cv::cvtColor(gray, bgra, cv::COLOR_GRAY2BGRA);
        const size_t bytes = bgra.total() * bgra.elemSize();
        if (bytes > (std::numeric_limits<uint32_t>::max)()) throw std::runtime_error("OCR image too large");
        context.stage = "ocr.buffer";
        winrt::Windows::Storage::Streams::DataWriter writer;
        writer.WriteBytes(winrt::array_view<uint8_t>(bgra.data, bgra.data + bytes));
        auto buffer = writer.DetachBuffer();
        context.stage = "ocr.bitmap";
        auto bitmap = winrt::Windows::Graphics::Imaging::SoftwareBitmap::CreateCopyFromBuffer(
            buffer, winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
            bgra.cols, bgra.rows);
        context.stage = "ocr.recognize";
        auto result = Vision::g_ocrEngine.RecognizeAsync(bitmap).get();
        context.stage = "ocr.words";
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
        SampleContext context;
        try
        {
            if (!g_ocrInitialized) throw std::runtime_error("OCR unavailable");
            cv::Mat frame;
            context.stage = "capture";
            if (!Capture::CaptureClient(hwnd, frame) || frame.empty())
                throw std::runtime_error("Capture failed");
            context.frame = frame.size();
            context.stage = "crop";
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
            context.stage = "ocr.dimensionLimit";
            const double limit = winrt::Windows::Media::Ocr::OcrEngine::MaxImageDimension();
            const double scale = (std::min)(2160.0 / frame.rows,
                limit / static_cast<double>((std::max)(gray.cols, gray.rows)));
            cv::Mat enlarged;
            context.stage = "resize";
            cv::resize(gray, enlarged, cv::Size(), scale, scale, cv::INTER_CUBIC);
            context.pass = "raw";
            bool enter = ReadEnter(enlarged, context);
            if (!enter)
            {
                // Remove slowly varying scene brightness, retaining bright UI strokes.
                // The raw pass remains available for dark/outlined prompt variants.
                cv::Mat strokes, mask;
                context.pass = "contrast";
                context.stage = "filter";
                cv::morphologyEx(enlarged, strokes, cv::MORPH_TOPHAT,
                    cv::getStructuringElement(cv::MORPH_RECT, cv::Size(15, 15)));
                cv::threshold(strokes, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
                cv::bitwise_not(mask, mask);
                enter = ReadEnter(mask, context);
            }
            g_score = detection.Observe(enter, GetTickCount64());
            g_detected = g_score == 1.0;
            return;
        }
        catch (const winrt::hresult_error& error) { context.Report("WinRT", error.code()); }
        catch (const cv::Exception& error) { context.Report("OpenCV", error.code); }
        catch (const std::exception& error) { context.Report(error.what(), E_FAIL); }
        catch (...) { context.Report("unknown", E_UNEXPECTED); }
        ResetDetection();
        g_score = -1.0;
    }

    bool IsDetected() { return g_detected; }
    double GetScore() { return g_score; }
    cv::Point GetMatchPoint() { return g_matchPoint; }
}
