#include "Restart.h"
#include <filesystem>
#include <shellapi.h>

void Restart(const std::wstring& folder, const Trust::Release& release)
{
    // Keep the authenticated executable and the directory chain pinned through process creation.
    Trust::Directory dir(folder);
    auto file = Trust::Open(dir.path + L"ImTiredPressF.exe", "installed_missing");
    auto bytes = Trust::Read(file.h, 96 * 1024 * 1024);
    Trust::ValidatePayload(release, 0, bytes, {});
    std::wstring path = (std::filesystem::path(folder) / L"ImTiredPressF.exe").wstring();
    SHELLEXECUTEINFOW shell{sizeof(shell)};
    shell.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    shell.lpVerb = L"runas";
    shell.lpFile = path.c_str();
    shell.lpDirectory = folder.c_str();
    shell.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&shell)) throw Trust::Failure("restart_failed");
}
