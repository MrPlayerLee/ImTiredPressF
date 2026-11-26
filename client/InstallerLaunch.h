#pragma once
#include <windows.h>

namespace InstallerLaunch {
struct Session {
    HANDLE process{};
    HANDLE ready{};
    ~Session() { Close(); }
    void Close();
    void Start(int component, HWND owner);
};
}
