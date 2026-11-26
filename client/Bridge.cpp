#include "Client.h"
#include "PanelSkin.h"
#include "TrustConfig.h"
#include <sddl.h>
#pragma comment(lib,"advapi32.lib")
#include <commctrl.h>
#include <vector>
#include <charconv>

namespace Bridge {
namespace {
HANDLE pipe=INVALID_HANDLE_VALUE;

bool elevated=false, connected=false, replied=false, servicing=false;
ULONGLONG since{};
struct Button {const char* key; int id;};
constexpr Button buttons[]={{"minimize",PanelSkin::Minimize},{"maximize",PanelSkin::Maximize},{"close",PanelSkin::Close},
    {"start",IDC_START},{"stop",IDC_STOP},{"hotkeys",IDC_ALTFSTOP},{"load",PanelSkin::Load},{"unload",PanelSkin::Unload},
    {"check_versions",PanelSkin::CheckUpdate},{"update_client",PanelSkin::UpdateClient},{"update_core",PanelSkin::UpdateCore}};
std::string Quote(const wchar_t* text)
{
    int n=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);
    std::string utf(n,'\0'); WideCharToMultiByte(CP_UTF8,0,text,-1,utf.data(),n,nullptr,nullptr);
    std::string out="\""; constexpr char hex[]="0123456789abcdef";
    for(int i=0;i<n-1;++i) {
        unsigned char ch=utf[i];
        if(ch=='\\'||ch=='"') {out+='\\';out+=char(ch);}
        else if(ch<32) {out+="\\u00";out+=hex[ch>>4];out+=hex[ch&15];}
        else out+=char(ch);
    }
    return out+"\"";
}
const char* Bool(bool b){return b?"true":"false";}
std::string State(HWND w)
{
    wchar_t buf[512]{}; GetWindowTextW(w,buf,512);
    std::string out="{\"ok\":true,\"pid\":"+std::to_string(GetCurrentProcessId())+",\"elevated\":"+Bool(elevated)+
        ",\"window_visible\":"+Bool(IsWindowVisible(w))+",\"core_loaded\":"+Bool(Client::core!=nullptr)+
        ",\"unloading\":"+Bool(Client::unloading)+",\"checking_versions\":"+Bool(Client::Checking())+",\"title\":"+Quote(buf)+",\"buttons\":{";
    out.insert(out.find("\"buttons\""),"\"running_client_version\":"+Quote(RunningClient)+","+"\"signature_status\":"+Quote(std::wstring(Client::trustState.begin(),Client::trustState.end()).c_str())+
        ",\"update_status\":"+Quote(std::wstring(Client::updateState.begin(),Client::updateState.end()).c_str())+",");
    bool first=true;
    for(const auto& b:buttons) {
        HWND c=GetDlgItem(w,b.id); GetWindowTextW(c,buf,512);
        if(!first) out+=','; first=false;
        out+="\""+std::string(b.key)+"\":{\"text\":"+Quote(buf)+",\"enabled\":"+Bool(IsWindowEnabled(c))+
             ",\"visible\":"+Bool(IsWindowVisible(c))+",\"checked\":"+Bool(b.id==IDC_ALTFSTOP && IsDlgButtonChecked(w,b.id)==BST_CHECKED)+"}";
    }
    GetWindowTextW(g_hStatus,buf,512);
    const std::pair<const char*,const wchar_t*> labels[]={{"status",buf},{"overlay_size",Labels::scale},{"auto_stop",Labels::automatic},
        {"core",Labels::core},{"update",Labels::update},{"component",Labels::component},{"current_version",Labels::current},
        {"latest_version",Labels::latest},{"client",Labels::exe},{"dll",Labels::dll},{"client_current",Client::Current(0)},
        {"client_latest",Client::Latest(0)},{"core_current",Client::Current(1)},{"core_latest",Client::Latest(1)}};
    out+="},\"labels\":{"; first=true;
    for(const auto& l:labels) {
        if(!first) out+=','; first=false;
        out+="\""+std::string(l.first)+"\":{\"text\":"+Quote(l.second)+",\"visible\":"+Bool(IsWindowVisible(w))+",\"enabled\":"+Bool(IsWindowEnabled(w))+"}";
    }
    HWND slider=GetDlgItem(w,IDC_OVERLAY_SCALE);
    return out+"},\"overlay_scale\":{\"value\":"+std::to_string(SendMessageW(slider,TBM_GETPOS,0,0))+
        ",\"min\":60,\"max\":200,\"enabled\":"+Bool(IsWindowEnabled(slider))+"}}";
}
std::string Execute(HWND w,const std::string& cmd)
{
    if(cmd=="state") return State(w);
    for(const auto& b:buttons) {
        if(cmd!="click "+std::string(b.key)) continue;
        HWND c=GetDlgItem(w,b.id);
        if(!IsWindowEnabled(c)) return R"({"ok":false,"error":"Control is disabled"})";
        SendMessageW(c,BM_CLICK,0,0);
        if(b.id==PanelSkin::Load && !Client::core) {
            return std::string(R"({"ok":false,"error":)")+Quote(std::wstring(Client::trustState.begin(),Client::trustState.end()).c_str())+"}";
        }
        return State(w);
    }
    if(cmd.rfind("scale ",0)==0) {
        int v=0; auto end=cmd.data()+cmd.size(); auto parsed=std::from_chars(cmd.data()+6,end,v);
        if(parsed.ec==std::errc{} && parsed.ptr==end && v>=60 && v<=200) {
            HWND c=GetDlgItem(w,IDC_OVERLAY_SCALE);
            if(!IsWindowEnabled(c)) return R"({"ok":false,"error":"Slider is disabled"})";
            SendMessageW(c,TBM_SETPOS,TRUE,v); SendMessageW(w,WM_HSCROLL,0,reinterpret_cast<LPARAM>(c));return State(w);
        }
    }
    return R"({"ok":false,"error":"Unknown control or invalid argument"})";
}
void Reset(){DisconnectNamedPipe(pipe);connected=false;replied=false;}
}
bool Open(HWND)
{
    HANDLE token{}; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return false;
    DWORD bytes{}; TOKEN_ELEVATION elevation{};
    GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&bytes); elevated=elevation.TokenIsElevated!=0;
    GetTokenInformation(token,TokenLogonSid,nullptr,0,&bytes); std::vector<BYTE> data(bytes);
    BOOL ok=GetTokenInformation(token,TokenLogonSid,data.data(),bytes,&bytes); CloseHandle(token);
    LPWSTR sid{};
    if(!ok || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_GROUPS*>(data.data())->Groups[0].Sid,&sid)) return false;
    // Grant the logon SID normal pipe I/O and attributes, excluding FILE_CREATE_PIPE_INSTANCE.
    std::wstring acl=L"D:P(A;;0x0012019b;;;"+std::wstring(sid)+L")S:(ML;;NW;;;ME)"; LocalFree(sid);
    PSECURITY_DESCRIPTOR sd{};
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&sd,nullptr)) return false;
    SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};
    auto name=L"\\\\.\\pipe\\ImTiredPressF.UI."+std::to_wstring(GetCurrentProcessId());
    pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_NOWAIT|PIPE_REJECT_REMOTE_CLIENTS,1,16384,256,0,&sa);
    LocalFree(sd); return pipe!=INVALID_HANDLE_VALUE;
}
void Tick(HWND w)
{
    if(pipe==INVALID_HANDLE_VALUE || servicing) return;
    if(!connected) {
        if(!ConnectNamedPipe(pipe,nullptr)) {
            const DWORD error=GetLastError();
            if(error==ERROR_NO_DATA){Reset();return;}
            if(error!=ERROR_PIPE_CONNECTED) return;
        }
        connected=true;since=GetTickCount64();
    }
    DWORD n=0;
    if(!PeekNamedPipe(pipe,nullptr,0,nullptr,&n,nullptr) || GetTickCount64()-since>5000){Reset();return;}
    if(!n || replied) return;
    char cmd[128]{};DWORD got=0;
    if(n>=sizeof(cmd) || !ReadFile(pipe,cmd,sizeof(cmd)-1,&got,nullptr)){Reset();return;}
    servicing=true;
    auto answer=Execute(w,std::string(cmd,got));
    DWORD sent=0; replied=WriteFile(pipe,answer.data(),static_cast<DWORD>(answer.size()),&sent,nullptr) && sent==answer.size();
    servicing=false;since=GetTickCount64();if(!replied) Reset();
}
void Close()
{
    if(pipe!=INVALID_HANDLE_VALUE){DisconnectNamedPipe(pipe);CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;}
}
}
