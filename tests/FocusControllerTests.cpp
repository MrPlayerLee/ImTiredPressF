#include "../core/FocusController.h"
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
HWND const game = reinterpret_cast<HWND>(1);
HWND const editor = reinterpret_cast<HWND>(2);
HWND const third = reinterpret_cast<HWND>(3);
int checks=0;
void Check(bool v,const char* label) { ++checks; if(!v){std::cerr<<label<<'\n';std::exit(1);} }
struct Fake : FocusPlatform
{
    HWND foreground=editor;
    bool valid=true, failUp=false, failNotify=false, failDown=false;
    bool switchDuringNotify=false;
    int releases=0, tracks=0, untracks=0;
    ULONGLONG now = 0;
    ULONGLONG Now() override { return now; }
    std::vector<std::string> events;
    HWND Foreground() override {return foreground;}
    bool Valid(HWND w) override {return w==game && valid;}
    bool Notify(HWND w,bool active) override {
        Check(w==game,"notification target"); events.push_back(active?"active":"inactive");
        if (switchDuringNotify) foreground=third;
        return !failNotify;
    }
    bool SuspendForReturn(HWND w) override {
        Check(w==game && foreground==game,"repair only targets foreground game");
        events.push_back("suspend"); return !failNotify;
    }
    bool Key(HWND w,bool down) override {
        Check(w==game,"key target"); events.push_back(down?"down":"up"); return !(down?failDown:failUp);
    }
    void Track(HWND) override {++tracks;}
    void Guard(HWND w) override {if(foreground!=w) ++releases;}
    void Untrack() override {++untracks;}
};
void Settle(FocusController& c, Fake& f) { f.now += 1000; for(int i=0;i<12;++i)c.Tick(false); }
#include "MessageTransportTests.h"
int main()
{
    {
        Fake f; FocusController c(f); Check(c.Begin(game),"start background");
        f.now = 30; c.Tick(true); Check(c.CanPulse() && c.Busy(),"active session between pulses");
        Check(f.events==std::vector<std::string>{"active","down","up"},"no deactivation per pulse");
        c.Begin(game); c.Stop(); Settle(c, f);
        Check(f.events==std::vector<std::string>{"active","down","up","active","down","up","inactive","inactive"},"ordered background stop");
        Check(f.foreground==editor && !c.Busy(),"editor remains foreground");
    }
    {
        Fake f; f.foreground=game; FocusController c(f); c.Begin(game);c.Stop();Settle(c, f);
        Check(f.events==std::vector<std::string>{"active","down","up","active","active"},"foreground stop retains active game");
        Check(f.releases==0,"foreground clip untouched");
    }
    {
        Fake f;FocusController c(f);c.Begin(game);f.foreground=game;c.Stop();Settle(c, f);
        Check(f.events.back()=="active","stop uses current not original foreground");
    }
    {
        Fake f;f.foreground=game;FocusController c(f);c.Begin(game);f.foreground=editor;c.Stop();Settle(c, f);
        Check(f.events.back()=="inactive" && f.foreground==editor,"foreground to background stop");
    }
    {
        Fake f;FocusController c(f);c.Begin(game);c.Tick(false);Settle(c, f);
        Check(!c.Busy() && f.events.back()=="inactive","automatic stop cleanup");
        auto size=f.events.size();c.Tick(true);c.Stop();Check(size==f.events.size(),"stale timer and repeated stop do nothing");
    }
    {
        Fake f;FocusController c(f);c.Begin(game);f.failUp=true;c.Stop();
        Check(!c.CanPulse() && f.events.back()=="up","failed release blocks new down/deactivation");
        f.failUp=false;c.Tick(false);Settle(c, f);Check(!c.Busy(),"release retry finishes");
    }
    {
        Fake f;f.failDown=true;FocusController c(f);Check(!c.Begin(game),"down failure");Settle(c, f);
        Check(f.events==std::vector<std::string>{"active","down","up","inactive","inactive"},"uncertain down gets release");
    }
    {
        Fake f;f.failNotify=true;FocusController c(f);Check(!c.Begin(game),"activation failure");
        Check(!c.CanPulse(),"partial activation needs cleanup");f.failNotify=false;c.Tick(false);Settle(c, f);
        Check(!c.Busy(),"partial activation cleanup");
    }
    {
        Fake f;FocusController c(f);c.Begin(game);f.valid=false;c.Tick(true);
        Check(!c.Busy() && f.untracks==1,"destroyed game releases ownership");
    }
    {
        Fake f;FocusController c(f);c.Begin(game);c.Stop();f.foreground=game;c.Tick(false);
        Check(f.events.back()=="active","user foreground change during settling corrected");
        f.foreground=third;c.Tick(false);Check(f.events.back()=="inactive" && f.foreground==third,"third app preserved");
        Settle(c, f);Check(!c.Busy(),"bounded settling");
    }
    {
        Fake f; FocusController c(f); c.Begin(game);
        f.now=1; c.Tick(true); Check(c.State()==FocusController::Phase::KeyDown,"early timer keeps F down");
        f.now=29; c.Tick(true); Check(c.State()==FocusController::Phase::KeyDown,"minimum hold duration");
        f.now=30; c.Tick(true); Check(c.State()==FocusController::Phase::Ready,"release at hold deadline");
        Check(f.events==std::vector<std::string>{"active","down","up"},"hold preserves existing notifications");
        c.Stop(); Settle(c, f);
    }
    {
        Fake f; FocusController c(f); c.Begin(game);
        f.now=1; c.Tick(false); Check(c.State()==FocusController::Phase::Settling,"stop releases without waiting for hold deadline");
        Settle(c, f);
    }
    {
        Fake f; FocusController c(f); c.Begin(game); c.Stop();
        f.foreground=game; f.failNotify=true; f.switchDuringNotify=true;
        const int guarded=f.releases;
        c.Tick(false);
        Check(f.releases==guarded+1,"failed correction still guards cursor after foreground changes");
        Check(c.Busy(),"failed correction retains cleanup session");
        f.failNotify=false; f.switchDuringNotify=false; Settle(c, f);
    }
    {
        Fake f; FocusController c(f); c.Begin(game); c.Stop();
        for (int i=0; i<100; ++i) c.Tick(false);
        Check(c.Busy(),"cleanup does not expire by timer count alone");
        f.now=999; c.Tick(false); Check(c.Busy(),"cleanup lasts one second");
        f.now=1000; f.failNotify=true; c.Tick(false);
        Check(c.Busy(),"final reconciliation failure retains cleanup ownership");
        f.failNotify=false; c.Tick(false);
        Check(!c.Busy(),"successful reconciliation completes cleanup");
        Check(c.Begin(game),"new session starts after automatic cleanup");
        c.Stop(); Settle(c, f);
    }
    {
        Fake f; FocusController c(f); c.Begin(game); c.Stop(); Settle(c,f);
        Check(c.State()==FocusController::Phase::AwaitingReturn && !c.Busy(),"stopped session watches later return without blocking start");
        const auto count=f.events.size(); const auto clips=f.releases;
        f.now+=60000; c.Tick(false);
        Check(f.events.size()==count && f.releases==clips,"late background wait neither notifies nor changes cursor clip");
        f.foreground=game; c.Tick(false); f.now+=99; c.Tick(false);
        Check(f.events.size()==count,"wait for stable real foreground");
        f.now+=1; c.Tick(false); Check(f.events.back()=="suspend" && c.Busy(),"first return releases stale internal input state");
        f.now+=99; c.Tick(false); Check(f.events.back()=="suspend","engine frame gap before reacquisition");
        f.now+=1; c.Tick(false);
        Check(f.events.back()=="active" && c.State()==FocusController::Phase::Idle,"reactivate after frame gap without another AltTab");
        const auto repaired=f.events.size(); f.now+=60000; c.Tick(false);
        Check(f.events.size()==repaired,"repair runs once and stops watching");
    }
    {
        Fake f; f.foreground=game; FocusController c(f); c.Begin(game);
        f.foreground=editor; f.now=30; c.Tick(true); c.Stop(); Settle(c,f);
        Check(c.State()==FocusController::Phase::AwaitingReturn,"foreground start and background stop also watches return");
        f.foreground=game; c.Tick(false); f.now+=100; c.Tick(false);
        f.foreground=third; f.now+=100; c.Tick(false);
        Check(f.events.back()=="inactive" && c.State()==FocusController::Phase::AwaitingReturn,"AltTab during repair respects new foreground and rearms");
        f.valid=false; c.Tick(false); Check(c.State()==FocusController::Phase::Idle,"game closure discards pending repair");
    }
    {
        Fake f; FocusController c(f); c.Begin(game); c.Stop(); Settle(c,f);
        Check(c.CanPulse() && c.Begin(game),"explicit background restart can replace pending return watch");
        c.Stop(); Settle(c,f); f.foreground=game;
        Check(!c.CanPulse(),"new input waits for outstanding foreground repair");
        c.Tick(false); f.now+=100; c.Tick(false); f.failNotify=true; f.now+=100; c.Tick(false);
        Check(c.Busy(),"failed reacquisition cannot silently discard pending recovery");
        f.failNotify=false; c.Tick(false); Check(!c.Busy(),"reacquisition retry completes");
    }
    TransportTest::Run();
    std::cout<<"PASS: "<<checks<<" targeted-message checks; no foreground or system-input API exists in controller.\n";
}
