#pragma once
#include <windows.h>
#include <string>
#include <stdexcept>

namespace InstallerProtocol {
inline std::wstring ReadyEvent(HANDLE process, DWORD pid)
{
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(process, &created, &exited, &kernel, &user))
        throw std::runtime_error("parent_process");
    ULARGE_INTEGER time{};
    time.LowPart = created.dwLowDateTime;
    time.HighPart = created.dwHighDateTime;
    return L"Local\\ImTiredPressF.Update." + std::to_wstring(pid) + L"." + std::to_wstring(time.QuadPart);
}
}
