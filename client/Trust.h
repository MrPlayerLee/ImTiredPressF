#pragma once
#include <windows.h>
#include <array>
#include <functional>
#include <string>
#include <vector>
#include <stdexcept>
namespace Trust {
using Bytes=std::vector<unsigned char>;
using Digest=std::array<unsigned char,32>;
struct Failure : std::runtime_error { using std::runtime_error::runtime_error; };
struct Handle {
    HANDLE h=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE value=INVALID_HANDLE_VALUE):h(value){}
    ~Handle(){if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);}
    Handle(const Handle&)=delete;
    Handle(Handle&& other) noexcept:h(other.h){other.h=INVALID_HANDLE_VALUE;}
};
struct Directory {
    std::wstring path;
    std::vector<Handle> locks;
    explicit Directory(const std::wstring& absolute,bool updating=false);
};
Handle Open(const std::wstring& path,const char* missing);
Handle BeginInstaller(const Directory& directory);
Bytes Read(HANDLE file,size_t limit);
Digest Hash(const unsigned char* data,size_t size);
std::string Hex(const Digest&);
void Verify(const Bytes& bytes,const Bytes& signature,bool manifest=false);
HMODULE LoadCore(const std::wstring& folder);
void CacheInstaller(const std::wstring& folder,const Bytes& image);
const wchar_t* Message(const std::string& code);
struct Release {bool mcp=false; std::wstring version[2]; std::string hash[2];};
Release VerifyManifest(const Bytes& document,const Bytes& signature,bool mcp=false);
void ValidatePayload(const Release&,int component,const Bytes& binary,const Bytes& signature);
void Install(const std::wstring& folder,const Release&,int component,const Bytes& binary,const Bytes& signature,const std::wstring& runningVersion,const std::function<bool()>& activate={});
}
