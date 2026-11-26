#pragma once
#include <windows.h>

// This interface intentionally cannot change foreground windows or inject
// system keyboard input. Every input notification names the game HWND.
struct FocusPlatform
{
    virtual ~FocusPlatform() = default;
    virtual HWND Foreground() = 0;
    virtual bool Valid(HWND) = 0;
    virtual bool Notify(HWND, bool active) = 0;
    virtual bool Restore(HWND w) { return Notify(w, Foreground() == w); }
    virtual bool SuspendForReturn(HWND) = 0;
    virtual bool Key(HWND, bool down) = 0;
    virtual void Track(HWND) = 0;
    virtual void Guard(HWND) = 0;
    virtual void Untrack() = 0;
    virtual ULONGLONG Now() { return GetTickCount64(); }
};

class FocusController
{
public:
    enum class Phase { Idle, Ready, KeyDown, Stopping, Settling, AwaitingReturn, Recovering };
    explicit FocusController(FocusPlatform& p) : os(p) {}
    bool Busy() const { return phase != Phase::Idle && phase != Phase::AwaitingReturn; }
    bool CanPulse() const { return phase == Phase::Idle || phase == Phase::Ready ||
        (phase == Phase::AwaitingReturn && os.Foreground() != game); }
    Phase State() const { return phase; }

    bool Begin(HWND target)
    {
        if (!CanPulse() || !os.Valid(target)) return false;
        if (phase == Phase::AwaitingReturn) Reset();
        if (phase == Phase::Idle)
        {
            game = target;
            os.Track(game);
            phase = Phase::Ready;
        }
        if (target != game) { Stop(); return false; }
        if (os.Foreground() != game) needsReturnRepair = true;
        // Reassert internal activation before each pulse, without foregrounding.
        if (!os.Notify(game, true)) { Stop(); return false; }
        held = true; // Timeout may mean key-down was processed: always pair it.
        if (!os.Key(game, true)) { Stop(); return false; }
        keyDownAt = os.Now();
        phase = Phase::KeyDown;
        os.Guard(game);
        return true;
    }

    void Tick(bool allowInput)
    {
        if (phase == Phase::Idle) return;
        if (!os.Valid(game)) { os.Guard(game); Reset(); return; }
        if (phase == Phase::AwaitingReturn)
        {
            // No timeout: the user may return minutes after input stopped.
            // Do not send messages or alter cursor clipping in another app.
            if (os.Foreground() != game) { returnObserved = false; return; }
            if (!returnObserved) { returnObserved = true; returnAt = os.Now(); return; }
            if (os.Now() - returnAt < 100) return;
            // Give the engine a real frame boundary between input-device release
            // and reacquisition, with the game already genuinely foreground.
            phase = Phase::Recovering;
            returnAt = os.Now();
            suspended = os.SuspendForReturn(game);
            return;
        }
        if (phase == Phase::Recovering)
        {
            if (os.Now() - returnAt < 100) return;
            if (!os.Restore(game)) return;
            if (suspended && os.Foreground() == game) Reset();
            else { phase = Phase::AwaitingReturn; returnObserved = false; }
            return;
        }
        if (os.Foreground() != game) needsReturnRepair = true;
        if (!allowInput && phase != Phase::Settling) Stop();
        else if (phase == Phase::KeyDown && os.Now() - keyDownAt >= 30)
        {
            if (os.Key(game, false)) { held = false; phase = Phase::Ready; }
            else phase = Phase::Stopping;
        }
        else if (phase == Phase::Stopping) Stop();
        else if (phase == Phase::Settling)
        {
            // Correct notifications if the user changed foreground during cleanup.
            bool active = os.Foreground() == game;
            if (active != finalActive)
            {
                if (!os.Restore(game)) { os.Guard(game); return; }
                finalActive = active;
            }
            os.Guard(game);
            if (os.Now() - settleStarted >= 1000)
            {
                // One final reconciliation also covers delayed engine activation.
                if (os.Restore(game))
                {
                    os.Guard(game);
                    if (needsReturnRepair) { phase = Phase::AwaitingReturn; returnObserved = false; }
                    else Reset();
                }
            }
            return;
        }
        if (Busy()) os.Guard(game);
    }

    void Stop()
    {
        if (!Busy() || phase == Phase::Settling || phase == Phase::Recovering) return;
        if (os.Foreground() != game) needsReturnRepair = true;
        phase = Phase::Stopping;
        if (!os.Valid(game)) { os.Guard(game); Reset(); return; }
        if (held)
        {
            if (!os.Key(game, false)) { os.Guard(game); return; }
            held = false;
        }
        finalActive = os.Foreground() == game;
        if (!os.Restore(game)) { os.Guard(game); return; }
        os.Guard(game);
        // Time-based cleanup is stable even when UI timers are delayed.
        settleStarted = os.Now();
        phase = Phase::Settling;
    }

private:
    void Reset()
    {
        os.Untrack(); game = nullptr; held = false; phase = Phase::Idle;
        needsReturnRepair = false; returnObserved = false; suspended = false;
    }
    FocusPlatform& os;
    HWND game = nullptr;
    Phase phase = Phase::Idle;
    bool held = false;
    bool finalActive = false;
    ULONGLONG keyDownAt = 0;
    ULONGLONG settleStarted = 0, returnAt = 0;
    bool needsReturnRepair = false, returnObserved = false, suspended = false;
};
