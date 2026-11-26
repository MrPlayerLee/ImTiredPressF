#pragma once
#include <cstdint>
#include <string>

namespace DialogueDetection
{
    inline bool IsEnterWord(const std::wstring& word)
    {
        std::wstring normalized;
        for (wchar_t c : word)
        {
            if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
            if (c == L'[' || c == L']' || c == L'(' || c == L')' || c == L' ')
                continue;
            normalized += c;
        }
        return normalized == L"enter";
    }

    // Scores: 0 = confirmed end, 0.5 = uncertain (no input), 1 = ongoing.
    // Each observation is one captured frame, regardless of OCR variants.
    class State
    {
    public:
        double Observe(bool enter, uint64_t now)
        {
            if (enter)
            {
                absent = false;
                if (!present) { present = true; presentSince = now; hits = 1; }
                else if (hits < 2) ++hits;
                return hits >= 2 && now - presentSince >= 500 ? 0.0 : 0.5;
            }
            present = false;
            hits = 0;
            if (!absent) { absent = true; absentSince = now; }
            return now - absentSince >= 1500 ? 1.0 : 0.5;
        }
    private:
        bool present = false, absent = false;
        unsigned hits = 0;
        uint64_t presentSince = 0, absentSince = 0;
    };
}
