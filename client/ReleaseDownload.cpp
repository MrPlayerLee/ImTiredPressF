#include "ReleaseDownload.h"
#include "TrustConfig.h"
#include <winhttp.h>
#include <chrono>
namespace Download {
namespace {
struct Http{HINTERNET h{};~Http(){if(h)WinHttpCloseHandle(h);}};
Trust::Bytes Get(const wchar_t* asset,size_t limit)
{
    Http session{WinHttpOpen(L"ImTiredPressF",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0)};
    if(!session.h)throw Trust::Failure("network");
    WinHttpSetTimeouts(session.h,4000,5000,5000,10000);
    Http host{WinHttpConnect(session.h,L"github.com",INTERNET_DEFAULT_HTTPS_PORT,0)};
    std::wstring path=L"/"+std::wstring(Repository)+L"/releases/latest/download/"+asset;
    Http req{WinHttpOpenRequest(host.h,L"GET",path.c_str(),nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE)};
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    if(!req.h || !WinHttpSetOption(req.h,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect)) ||
       !WinHttpSendRequest(req.h,nullptr,0,nullptr,0,0,0)||!WinHttpReceiveResponse(req.h,nullptr))throw Trust::Failure("network");
    DWORD status{},n=sizeof(status);
    if(!WinHttpQueryHeaders(req.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&n,nullptr))throw Trust::Failure("network");
    if(status!=200)throw Trust::Failure(status==404?"not_published":"http_error");
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(90);
    Trust::Bytes out;BYTE chunk[32768];
    for(;;){
        if(std::chrono::steady_clock::now()>deadline || !WinHttpReadData(req.h,chunk,sizeof(chunk),&n))throw Trust::Failure("network");
        if(!n)break;
        if(n>limit-out.size())throw Trust::Failure("asset_size");
        out.insert(out.end(),chunk,chunk+n);
    }
    return out;
}
}
Trust::Release Latest(bool mcp)
{
    auto doc=Get(mcp?L"ImTiredPressF-MCP.manifest":L"ImTiredPressF.manifest",2048);
    return Trust::VerifyManifest(doc,Get(mcp?L"ImTiredPressF-MCP.manifest.sig":L"ImTiredPressF.manifest.sig",104),mcp);
}
Package Component(int index, const Trust::Release& release)
{
    Package p;p.release=release;p.binary=Get(index?L"ImTiredPressF.dll":(release.mcp?L"ImTiredPressF-MCP.exe":L"ImTiredPressF.exe"),96*1024*1024);
    if(index)p.signature=Get(L"ImTiredPressF.dll.sig",104);
    Trust::ValidatePayload(p.release,index,p.binary,p.signature);return p;
}
}
