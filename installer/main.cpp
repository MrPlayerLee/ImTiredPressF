#include "../shared/ClientVariant.h"
#include "../client/ReleaseDownload.h"
#include "Restart.h"
#include "../shared/InstallerProtocol.h"
#include <shellapi.h>
#include <filesystem>
#include "../client/PeVersion.h"
#include "../client/VersionFeed.h"
#include <memory>

namespace {
struct Arguments {
    bool dll;
    DWORD pid;
};
Arguments Parse()
{
    int count{};
    auto argv = CommandLineToArgvW(GetCommandLineW(), &count);
    std::unique_ptr<void, decltype(&LocalFree)> buffer(argv, LocalFree);
    if (!argv || count != 5 || wcscmp(argv[1], L"--type") || wcscmp(argv[3], L"--pid"))
        throw Trust::Failure("client_required");
    std::wstring type(argv[2]), pid(argv[4]);
    if ((type != L"dll" && type != L"client") || pid.empty() || pid.size() > 10 ||
        pid.find_first_not_of(L"0123456789") != std::wstring::npos) throw Trust::Failure("client_required");
    auto value = std::stoull(pid);
    if (!value || value > MAXDWORD) throw Trust::Failure("client_required");
    return {type == L"dll", static_cast<DWORD>(value)};
}


}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int)
{
    bool accepted = false;
    try {
        if (!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)) return 1;
        auto args = Parse();
        Trust::Handle parent(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, args.pid));
        if (!parent.h) throw Trust::Failure("client_required");
        wchar_t image[32768]{};
        DWORD length = ARRAYSIZE(image);
        if (!QueryFullProcessImageNameW(parent.h, 0, image, &length)) throw Trust::Failure("client_required");
        std::filesystem::path client(image);
        const bool mcp = _wcsicmp(client.filename().c_str(), ClientVariant::File(true)) == 0;
        if (!mcp && _wcsicmp(client.filename().c_str(), ClientVariant::File(false))) throw Trust::Failure("client_required");
        const auto folder = client.parent_path().wstring();
        Trust::Directory directory(folder, true);
        auto gate = Trust::BeginInstaller(directory);
        auto name = InstallerProtocol::ReadyEvent(parent.h, args.pid);
        Trust::Handle ready(OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str()));
        if (!ready.h) throw Trust::Failure("client_required");
        int component = args.dll ? 1 : 0;
        auto offer = Download::Latest(mcp);
        {
            auto installed = Trust::Open(directory.path + (args.dll ? L"ImTiredPressF.dll" : ClientVariant::File(mcp)), "installed_missing");
            auto version = Trust::PeVersion(Trust::Read(installed.h, 96 * 1024 * 1024));
            if (VersionFeed::Number(offer.version[component]) <= VersionFeed::Number(version))
                throw Trust::Failure("downgrade");
        }
        if (!SetEvent(ready.h)) throw Trust::Failure("client_required");
        accepted = true;
        if (!args.dll && WaitForSingleObject(parent.h, 60000) != WAIT_OBJECT_0)
            throw Trust::Failure("client_exit_timeout");
        auto package = Download::Component(component,offer);
        Trust::Install(folder, package.release, component, package.binary, package.signature, L"");
        if (!args.dll) Restart(folder, package.release);
        return 0;
    } catch (const std::exception& e) {
        if (std::string(e.what()) == "client_required") return 2;
        auto text = std::wstring(L"업데이트에 실패했습니다.\n") + Trust::Message(e.what()) +
            L" (" + std::wstring(e.what(), e.what()+strlen(e.what())) + L")";
        if (accepted) text += L"\n클라이언트가 종료되었다면 설치 폴더에서 다시 실행하세요.";
        MessageBoxW(nullptr, text.c_str(), L"ImTiredPressF", MB_OK | MB_ICONERROR);
        return 1;
    }
}
