#pragma once
#include "../shared/Diagnostics.h"
#include "FocusController.h"
class TargetWindowInput final : public FocusPlatform
    {
    public:
        explicit TargetWindowInput(const wchar_t*& error) : inputError(error) {}
        ~TargetWindowInput() override { Untrack(); }
        HWND Foreground() override { return GetForegroundWindow(); }
        bool Valid(HWND w) override
        {
            if (!w || !IsWindow(w)) return false;
            if (!tracked) return true;
            DWORD pid = 0;
            DWORD tid = GetWindowThreadProcessId(w, &pid);
            return w == target && targetPid && targetTid &&
                   pid == targetPid && tid == targetTid &&
                   (!process || WaitForSingleObject(process, 0) == WAIT_TIMEOUT);
        }
        bool Send(HWND w, UINT message, WPARAM wp, LPARAM lp)
        {
            if (!Valid(w))
            {
                lastSendError = ERROR_INVALID_WINDOW_HANDLE;
                inputError = L"상태 : 대상 창 변경 또는 종료 - 입력 중지";
                return false;
            }
            DWORD_PTR result = 0;
            SetLastError(ERROR_SUCCESS);
            bool ok = SendMessageTimeoutW(w, message, wp, lp,
                SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_ERRORONEXIT, 100, &result) != 0;
            lastSendError = ok ? ERROR_SUCCESS : GetLastError();
            if (!ok) { ITPF_DIAG("input.SendMessageTimeout",lastSendError); }
            if (!ok) inputError = L"상태 : 게임 메시지 응답 실패 / 정리 재시도";
            return ok;
        }
        DWORD LastSendError() const { return lastSendError; }
        bool Notify(HWND w, bool active) override
        {
            // Recheck before deactivation: never deliberately deactivate a
            // game that the user has brought to the foreground.
            if (!active && Foreground() == w) active = true;
            HWND other = Foreground();
            bool ok = Send(w, WM_ACTIVATE, active ? WA_ACTIVE : WA_INACTIVE,
                           reinterpret_cast<LPARAM>(other == w ? nullptr : other));
            if (!ok) return false;
            if (!active && Foreground() == w)
                return Notify(w, true);
            return Send(w, active ? WM_SETFOCUS : WM_KILLFOCUS,
                        reinterpret_cast<WPARAM>(other == w ? nullptr : other), 0);
        }
        bool Restore(HWND w) override
        {
            // Reconcile app activation as well as keyboard/window focus. Merely
            // sending WM_KILLFOCUS leaves some engines internally activated.
            // Never attach input queues, switch foreground or synthesize Alt/Tab.
            const HWND foreground = Foreground();
            const bool active = foreground == w;
            if (!active) lastOther = foreground;
            const HWND other = active ? lastOther : foreground;
            const DWORD otherThread = other && other != w && IsWindow(other) ?
                GetWindowThreadProcessId(other, nullptr) : 0;
            if (!active && !Send(w, WM_CANCELMODE, 0, 0)) return false;
            if (Foreground() != foreground) return false;
            if (!Send(w, WM_ACTIVATEAPP, active ? TRUE : FALSE, otherThread)) return false;
            if (Foreground() != foreground) return false;
            if (!Send(w, WM_NCACTIVATE, active ? TRUE : FALSE, 0)) return false;
            if (Foreground() != foreground) return false;
            if (!Notify(w, active)) return false;
            // A concurrent Alt+Tab makes this attempt incomplete; the timer
            // retries against the new foreground instead of accepting stale state.
            return Foreground() == foreground;
        }
        bool SuspendForReturn(HWND w) override
        {
            // Only the genuinely foreground game is eligible. This intentionally
            // resets its internal input state; it does not move OS foreground,
            // attach threads, inject code, or synthesize keyboard/mouse input.
            const auto sendIfForeground = [&](UINT message, WPARAM wp) {
                return Foreground() == w && Send(w, message, wp, 0) && Foreground() == w;
            };
            return sendIfForeground(WM_KILLFOCUS, 0) &&
                   sendIfForeground(WM_ACTIVATE, WA_INACTIVE) &&
                   sendIfForeground(WM_ACTIVATEAPP, FALSE);
        }
        bool Key(HWND w, bool down) override
        {
            const ULONG scan = MapVirtualKeyW('F', MAPVK_VK_TO_VSC);
            const ULONG bits = 1u | (scan << 16) | (down ? 0u : 0xC0000000u);
            return Send(w, down ? WM_KEYDOWN : WM_KEYUP, 'F', static_cast<LPARAM>(bits));
        }
        void Track(HWND w) override
        {
            Untrack();
            tracked = true;
            target = w;
            targetTid = GetWindowThreadProcessId(w, &targetPid);
            if (targetTid && targetPid)
                // Some targets deny process access. PID/TID checks remain
                // active even when an optional lifetime handle is unavailable.
                process = OpenProcess(SYNCHRONIZE, FALSE, targetPid);
            owner = Foreground();
            lastOther = owner != w ? owner : nullptr;
            haveBaseline = GetClipCursor(&baseline) != FALSE;
            haveGameRect = GetWindowRect(w, &gameRect) != FALSE;
            // A foreground game's existing clip must not become another app's baseline.
            if (owner == w) haveBaseline = false;
        }
        void Guard(HWND w) override
        {
            HWND foreground = Foreground();
            if (!tracked || !foreground || foreground == w) return;
            RECT current{};
            if (!GetClipCursor(&current)) return;
            if (haveBaseline && EqualRect(&current, &baseline)) return;
            if (Valid(w)) haveGameRect = GetWindowRect(w, &gameRect) != FALSE;
            // Windows exposes no clip owner. Only touch a changed restrictive
            // rectangle contained in this game's bounds during our activation.
            if (!haveGameRect || current.left < gameRect.left || current.top < gameRect.top ||
                current.right > gameRect.right || current.bottom > gameRect.bottom) return;
            RECT desktop{GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), 0, 0};
            desktop.right = desktop.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
            desktop.bottom = desktop.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
            if (EqualRect(&current, &desktop)) return;
            if (Foreground() != foreground) return;
            ClipCursor(haveBaseline && foreground == owner ? &baseline : nullptr);
        }
        void Untrack() override
        {
            if (process) CloseHandle(process);
            process = nullptr;
            target = nullptr;
            targetPid = targetTid = 0;
            tracked = false;
        }
    private:
        const wchar_t*& inputError;
        HWND target = nullptr;
        DWORD targetPid = 0, targetTid = 0;
        HANDLE process = nullptr;
        DWORD lastSendError = ERROR_SUCCESS;
        HWND owner = nullptr;
        HWND lastOther = nullptr;
        RECT baseline{}, gameRect{};
        bool tracked = false, haveBaseline = false, haveGameRect = false;
};

