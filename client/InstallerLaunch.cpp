#include "InstallerLaunch.h"
#include "Client.h"
#include "Trust.h"
#include "../shared/InstallerProtocol.h"
#include <shellapi.h>
#include <shlobj.h>

namespace InstallerLaunch {
void Session::Close()
{
    if (process) CloseHandle(process);
    if (ready) CloseHandle(ready);
    process = ready = nullptr;
}

static Trust::Bytes EmbeddedInstaller()
{
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(310), RT_RCDATA);
    auto data = static_cast<const BYTE*>(LockResource(LoadResource(nullptr, resource)));
    DWORD size = SizeofResource(nullptr, resource);
    if (!data || !size) throw Trust::Failure("installer_missing");
    return {data, data + size};
}

void Session::Start(int component, HWND owner)
{
    const auto image = EmbeddedInstaller();
    const std::wstring leaf = L"Update_Installer.exe";
    std::wstring folder = Client::Folder();
    bool useInstalled = false;
    {
        Trust::Directory dir(folder);
        try {
            auto file = Trust::Open(dir.path + leaf, "installer_missing");
            useInstalled = Trust::Read(file.h, 16 * 1024 * 1024) == image;
        } catch (const Trust::Failure&) { /* Older installations can use the embedded copy. */ }
    }
    if (!useInstalled) {
        wchar_t local[MAX_PATH]{};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, local)))
            throw Trust::Failure("installer_cache");
        auto digest = Trust::Hex(Trust::Hash(image.data(), image.size()));
        std::wstring cache = L"ImTiredPressF-Installer-" + std::wstring(digest.begin(), digest.end());
        Trust::Directory parent(local, true);
        if (!CreateDirectoryW((parent.path + cache).c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
            throw Trust::Failure("installer_cache");
        folder = std::wstring(local) + L"\\" + cache + L"\\";
        Trust::CacheInstaller(folder, image);
    }
    Trust::Directory dir(folder);
    auto file = Trust::Open(dir.path + leaf, "installer_missing");
    if (Trust::Read(file.h, 16 * 1024 * 1024) != image) throw Trust::Failure("installer_integrity");

    DWORD pid = GetCurrentProcessId();
    auto eventName = InstallerProtocol::ReadyEvent(GetCurrentProcess(), pid);
    ready = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
    if (!ready || GetLastError() == ERROR_ALREADY_EXISTS) { Close(); throw Trust::Failure("installer_busy"); }
    std::wstring args = std::wstring(L"--type ") + (component ? L"dll" : L"client") + L" --pid " + std::to_wstring(pid);
    std::wstring path = folder + leaf;
    SHELLEXECUTEINFOW shell{sizeof(shell)};
    shell.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    shell.hwnd = owner;
    shell.lpVerb = L"runas";
    shell.lpFile = path.c_str();
    shell.lpParameters = args.c_str();
    shell.lpDirectory = folder.c_str();
    shell.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&shell) || !shell.hProcess) {
        DWORD error = GetLastError();
        Close();
        throw Trust::Failure(error == ERROR_CANCELLED ? "uac_cancelled" : "installer_launch");
    }
    process = shell.hProcess;
}
}
