#include "../shared/Diagnostics.h"
#include "../shared/ClientVariant.h"
#include "Client.h"
#include "PanelSkin.h"
#include <commctrl.h>
#include "ReleaseDownload.h"
#include "TrustConfig.h"
#include "InstallerLaunch.h"
#include <future>
#include <chrono>
#include <vector>
#include <sstream>
#include "VersionFeed.h"

namespace Client {
const CoreApi* core{};
bool unloading=false, quitting=false;
std::wstring notice;
std::string trustState="idle", updateState="idle";
namespace {
HMODULE library{};
std::wstring installed[2], latest[2]={L"—",L"—"};
std::array<bool,2> updated{};
using namespace VersionFeed;
std::future<Trust::Release> transfer;
InstallerLaunch::Session installer;
int operation = -1;
bool wasLoaded = false;
bool launching = false;
std::wstring DiskVersion(const std::wstring& path)
{
    DWORD bytes=GetFileVersionInfoSizeW(path.c_str(),nullptr);
    std::vector<BYTE> info(bytes);
    VS_FIXEDFILEINFO* v=nullptr; UINT count=0;
    if(!bytes || !GetFileVersionInfoW(path.c_str(),0,bytes,info.data()) ||
       !VerQueryValueW(info.data(),L"\\",reinterpret_cast<void**>(&v),&count) || count<sizeof(*v)) return L"—";
    return std::to_wstring(HIWORD(v->dwFileVersionMS))+L"."+std::to_wstring(LOWORD(v->dwFileVersionMS))+L"."+std::to_wstring(HIWORD(v->dwFileVersionLS));
}
void SetEnabled(int id,bool enabled) {
    HWND control=GetDlgItem(g_hWnd,id);
    if(control && !!IsWindowEnabled(control)!=enabled) EnableWindow(control,enabled);
}
void RefreshInstalled()
{
    auto dir=Folder(); installed[0]=DiskVersion(dir+ClientVariant::File(ITPF_MCP!=0)); installed[1]=DiskVersion(dir+L"ImTiredPressF.dll");
    for(int i=0;i<2;++i) updated[i]=Number(latest[i])>Number(installed[i]) && Number(latest[i])>=0;
}
}
std::wstring Folder() {
    wchar_t path[32768]{}; GetModuleFileNameW(nullptr,path,32768);
    std::wstring folder(path); return folder.substr(0,folder.find_last_of(L'\\')+1);
}
void Load()
{
    if(library || quitting || operation >= 0) return;
    RefreshInstalled();
    try {library=Trust::LoadCore(Folder());trustState="verified";}
    catch(const std::exception& e){trustState=e.what();if(trustState!="dll_missing")Diagnostics::Write(e.what());notice=std::wstring(L"상태 : ")+Trust::Message(trustState);return;}
    auto get=reinterpret_cast<GetCoreApi>(GetProcAddress(library,"ImTiredPressF_GetApi"));
    auto candidate=get?get(1):nullptr;
    if(!candidate || candidate->size!=sizeof(CoreApi) || candidate->abi!=1 || !candidate->open(g_hWnd)) {
        Diagnostics::Write("core.ABI or initialization failed");
        FreeLibrary(library); library=nullptr; trustState="load_failed"; notice=L"상태 : 코어 호환 또는 초기화 오류"; return;
    }
    core=candidate; notice.clear();
    char versions[120]{};
    sprintf_s(versions,"verified client=%ls core=%ls",RunningClient,installed[1].c_str());
    Diagnostics::Write(versions);
    auto pos=SendDlgItemMessageW(g_hWnd,IDC_OVERLAY_SCALE,TBM_GETPOS,0,0);
    core->scale(static_cast<float>(pos)/100.f);
    if(IsDlgButtonChecked(g_hWnd,IDC_ALTFSTOP)==BST_CHECKED && !core->hotkeys(true)) {
        CheckDlgButton(g_hWnd,IDC_ALTFSTOP,BST_UNCHECKED); notice=L"상태 : 단축키 등록 실패";
    }
}
void Unload()
{
    if(!core || unloading) return;
    unloading=true; notice.clear(); core->beginClose();
}
bool Checking(){return transfer.valid() || operation >= 0;}
void CheckVersions()
{
    if(Checking() || quitting)return;
    RefreshInstalled();
    latest[0]=latest[1]=L"확인 중";updated.fill(false);updateState="checking";
    try { transfer=std::async(std::launch::async,Download::Latest,ITPF_MCP!=0); }
    catch(...) {Diagnostics::Write("update.start transfer");updateState="transfer_failed";notice=L"상태 : 업데이트 확인 실패";}
    InvalidateRect(g_hWnd,nullptr,FALSE);
}
const wchar_t* Current(int i){return installed[i].c_str();}
const wchar_t* Latest(int i){return latest[i].c_str();}
bool HasUpdate(int i){return updated[i] && !Checking() && !unloading && !quitting;}
void ApplyUpdate(int i)
{
    if((i!=0 && i!=1) || !HasUpdate(i))return;
    operation=i;
    wasLoaded=core!=nullptr;
    updateState="starting_installer";
    if(i==1 && core)Unload();
}
void TransferError(const std::string& code)
{
    Diagnostics::Write(code.c_str());
    updateState=code;
    if(code=="not_published" || code=="network" || code=="http_error"){
        latest[0]=latest[1]=code=="not_published"?L"미게시":L"연결 오류";
        notice=code=="not_published"?L"상태 : 서명된 릴리스 미게시":L"상태 : 업데이트 연결 실패";
    }else{
        latest[0]=latest[1]=L"검증 실패";
        notice=code=="downgrade"?L"상태 : 구버전 업데이트 차단":L"상태 : 업데이트 파일 검증·적용 실패";
    }
    updated.fill(false);
}
void Poll()
{
    if(unloading && core->finishClose()) {
        core=nullptr; FreeLibrary(library); library=nullptr; unloading=false;
        trustState="unloaded";notice=L"상태 : 코어 언로드됨"; RefreshInstalled();
    }
    if(transfer.valid() && transfer.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto release=transfer.get();
            for(int i=0;i<2;++i)latest[i]=release.version[i];
            RefreshInstalled();
            updateState="manifest_verified";
        } catch(const std::exception& e){TransferError(e.what());}
        InvalidateRect(g_hWnd,nullptr,FALSE);
    }
    if(operation>=0 && !unloading && !launching) {
        auto finish=[](bool success,const std::string& status) {
            if(!success && status!="cancelled")Diagnostics::Write(status.c_str());
            bool load=operation==1 && (success || wasLoaded) && !quitting;
            installer.Close();operation=-1;
            if(load)Load();
            RefreshInstalled();updateState=status;
            if(success && load && !core)updateState="core_activation_failed";
            else if(success)notice=L"상태 : 코어 업데이트 완료";
            else if(status=="uac_cancelled")notice=L"상태 : 업데이트 취소됨";
            else notice=L"상태 : 업데이트 도우미 실행 또는 설치 실패";
            InvalidateRect(g_hWnd,nullptr,FALSE);
        };
        if(!installer.process) {
            if(quitting)finish(false,"cancelled");
            else {
                launching=true;
                try {installer.Start(operation,g_hWnd);updateState="installer_running";}
                catch(const std::exception& e){finish(false,e.what());}
                launching=false;
            }
        } else if(WaitForSingleObject(installer.process,0)==WAIT_OBJECT_0) {
            DWORD code=1;
            GetExitCodeProcess(installer.process,&code);
            finish(code==0,code==0?"installed":"installer_failed");
        } else if(operation==0 && WaitForSingleObject(installer.ready,0)==WAIT_OBJECT_0) {
            installer.Close();operation=-1;quitting=true;Unload();
        }
    }
    if(quitting && !core && !Checking()) {DestroyWindow(g_hWnd);return;}
    bool ready=core && !unloading && !quitting;
    SetEnabled(PanelSkin::Load,!core && !quitting && operation<0); SetEnabled(PanelSkin::Unload,ready);
    SetEnabled(IDC_START,ready && core->canStart()); SetEnabled(IDC_STOP,ready && core->canStop());
    SetEnabled(IDC_ALTFSTOP,ready); SetEnabled(IDC_OVERLAY_SCALE,ready);
    SetEnabled(PanelSkin::CheckUpdate,!Checking() && !quitting);
    SetEnabled(PanelSkin::UpdateClient,HasUpdate(0) && !quitting); SetEnabled(PanelSkin::UpdateCore,HasUpdate(1) && !quitting);
    const wchar_t* status=unloading?L"상태 : 코어 정리 중":!notice.empty()?notice.c_str():core?core->status():L"상태 : 코어 언로드됨";
    wchar_t old[256]{}; GetWindowTextW(g_hStatus,old,256);
    if(core && trustState=="verified" && wcscmp(status,L"상태 : 중지")==0)status=L"상태 : 중지 · 서명 확인됨";
    if(wcscmp(status,old)!=0) SetWindowTextW(g_hStatus,status);
}
}

