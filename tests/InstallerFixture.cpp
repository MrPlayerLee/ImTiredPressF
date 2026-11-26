// Transport and restart adapters for the offline installer process test.
#include "../client/ReleaseDownload.h"
#include "../installer/Restart.h"
#include <filesystem>
#include <fstream>
#include <iterator>

static Trust::Bytes ReadFixture(const std::filesystem::path& name)
{
    std::ifstream stream(name, std::ios::binary);
    if (!stream) throw Trust::Failure("fixture_missing");
    return {std::istreambuf_iterator<char>(stream), {}};
}
namespace Download {
Trust::Release Latest()
{
    return Trust::VerifyManifest(ReadFixture("manifest"), ReadFixture("manifest.sig"));
}
Package Component(int index, const Trust::Release& release)
{
    std::ofstream("fetched") << index;
    Package p{release, ReadFixture(index ? "2.dll" : "2.exe"), {}};
    if (index) p.signature = ReadFixture("2.dll.sig");
    Trust::ValidatePayload(release, index, p.binary, p.signature);
    return p;
}
}
void Restart(const std::wstring& folder, const Trust::Release& release)
{
    auto path = std::filesystem::path(folder) / L"ImTiredPressF.exe";
    Trust::ValidatePayload(release, 0, ReadFixture(path), {});
    STARTUPINFOW startup{sizeof(startup)};
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(path.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, folder.c_str(), &startup, &child))
        throw Trust::Failure("fixture_restart");
    std::ofstream("restarted.pid") << child.dwProcessId;
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
}
