#include "Trust.h"
#include "TrustConfig.h"
#include "PeVersion.h"
#include "VersionFeed.h"
#include <bcrypt.h>
#include <winternl.h>
#pragma comment(lib,"ntdll.lib")
extern "C" NTSTATUS NTAPI NtSetInformationFile(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,FILE_INFORMATION_CLASS);
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <memory>
#include <sstream>
#include <regex>

namespace Trust {
namespace {
struct Alg {
    BCRYPT_ALG_HANDLE h{};
    explicit Alg(LPCWSTR algorithm){if(BCryptOpenAlgorithmProvider(&h,algorithm,MS_PRIMITIVE_PROVIDER,0)<0) throw Failure("crypto");}
    ~Alg(){BCryptCloseAlgorithmProvider(h,0);}
};
struct Key {BCRYPT_KEY_HANDLE h{};~Key(){if(h) BCryptDestroyKey(h);}};
constexpr size_t MaxBinary=96ull*1024*1024;
void CheckType(HANDLE h,bool directory)
{
    BY_HANDLE_FILE_INFORMATION f{};
    if(!GetFileInformationByHandle(h,&f) || (f.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) ||
        bool(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=directory || (!directory && (f.nNumberOfLinks!=1 || GetFileType(h)!=FILE_TYPE_DISK)))
        throw Failure("unsafe_path");
}

}
Directory::Directory(const std::wstring& absolute,bool updating)
{
    namespace fs=std::filesystem;
    fs::path full(absolute),current=full.root_path();
    if(!full.is_absolute() || current.native().size()!=3 || GetDriveTypeW(current.c_str())!=DRIVE_FIXED) throw Failure("unsafe_path");
    wchar_t volume[MAX_PATH]{};
    if(!GetVolumeNameForVolumeMountPointW(current.c_str(),volume,MAX_PATH))throw Failure("unsafe_path");
    current=volume;
    std::vector<fs::path> pieces;
    for(const auto& item:full.relative_path())if(!item.empty())pieces.push_back(item);
    if(pieces.empty())throw Failure("unsafe_path");
    for(size_t i=0;i<pieces.size();++i) {
        const auto& piece=pieces[i];
        if(piece.empty()) continue;
        if(piece==L".." || piece==L"." || piece.native().find(L':')!=std::wstring::npos) throw Failure("unsafe_path");
        current/=piece;
        Handle lock(CreateFileW(current.c_str(),FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|((updating && i+1==pieces.size()) ? FILE_SHARE_WRITE:0),nullptr,OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        if(lock.h==INVALID_HANDLE_VALUE) throw Failure("directory_lock");
        CheckType(lock.h,true); locks.emplace_back(std::move(lock));
    }
    path=current.wstring()+L"\\";
}
Handle Open(const std::wstring& path,const char* missing)
{
    Handle h(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(h.h==INVALID_HANDLE_VALUE) {DWORD e=GetLastError();throw Failure(e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND?missing:"file_lock");}
    CheckType(h.h,false);return h;
}
Bytes Read(HANDLE file,size_t limit)
{
    LARGE_INTEGER size{},zero{};
    if(!GetFileSizeEx(file,&size) || size.QuadPart<=0 || static_cast<ULONGLONG>(size.QuadPart)>limit ||
       !SetFilePointerEx(file,zero,nullptr,FILE_BEGIN)) throw Failure("invalid_file");
    Bytes b(static_cast<size_t>(size.QuadPart));DWORD n{};
    if(!ReadFile(file,b.data(),static_cast<DWORD>(b.size()),&n,nullptr) || n!=b.size()) throw Failure("file_read");
    return b;
}
Digest Hash(const unsigned char* data,size_t size)
{
    Alg alg(BCRYPT_SHA256_ALGORITHM);Digest digest{};
    if(size>MAXDWORD || BCryptHash(alg.h,nullptr,0,const_cast<PUCHAR>(data),static_cast<ULONG>(size),digest.data(),32)<0) throw Failure("crypto");
    return digest;
}
std::string Hex(const Digest& digest){
    const char digits[]="0123456789abcdef";std::string out;
    for(auto b:digest){out+=digits[b>>4];out+=digits[b&15];}return out;
}
void Verify(const Bytes& bytes,const Bytes& signature,bool manifest)
{
    constexpr size_t record=104;
    if(signature.size()!=record && (manifest || signature.size()!=record*2)) throw Failure("untrusted_signature");
    Alg alg(BCRYPT_ECDSA_P256_ALGORITHM);Key key;
    if(BCryptImportKeyPair(alg.h,nullptr,BCRYPT_ECCPUBLIC_BLOB,&key.h,const_cast<PUCHAR>(TrustedKey),sizeof(TrustedKey),0)<0) throw Failure("crypto");
    auto digest=Hash(bytes.data(),bytes.size());bool matches=false;
    for(size_t at=0;at<signature.size();at+=record) {
        auto p=signature.data()+at;auto signedHash=Hash(p,40);
        if(memcmp(p,manifest?"ITPF-M01":"ITPF-C01",8) || BCryptVerifySignature(key.h,nullptr,signedHash.data(),32,const_cast<PUCHAR>(p+40),64,0)<0)
            throw Failure("untrusted_signature");
        if(!memcmp(p+8,digest.data(),32)) matches=true;
    }
    if(!matches) throw Failure("integrity");
}
HMODULE LoadCore(const std::wstring& folder)
{
    Directory dir(folder);auto path=dir.path+L"ImTiredPressF.dll";
    auto file=Open(path,"dll_missing");auto signature=Open(path+L".sig","signature_missing");
    auto data=Read(file.h,MaxBinary);
    Verify(data,Read(signature.h,208));
    if(VersionFeed::Number(PeVersion(data))<VersionFeed::Number(MinimumCore))throw Failure("downgrade");
    if(GetModuleHandleW(L"ImTiredPressF.dll")) throw Failure("already_mapped");
    // Resolve the verified handle to the kernel volume name so a drive-letter remap
    // cannot redirect the loader. The file and its directory chain stay open here.
    std::vector<wchar_t> nt(32768);
    DWORD n=GetFinalPathNameByHandleW(file.h,nt.data(),static_cast<DWORD>(nt.size()),VOLUME_NAME_NT);
    if(!n || n>=nt.size())throw Failure("unsafe_path");
    std::wstring image=L"\\\\.\\GLOBALROOT"+std::wstring(nt.data(),n);
    auto module=LoadLibraryExW(image.c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module) throw Failure("load_failed_"+std::to_string(GetLastError()));return module;
}
Release VerifyManifest(const Bytes& doc,const Bytes& sig)
{
    Verify(doc,sig,true);
    std::wstring repo(Repository);std::string expected;for(auto ch:repo)expected+=static_cast<char>(ch);
    std::istringstream stream(std::string(doc.begin(),doc.end()));std::string row;
    auto field=[&](const std::string& prefix){
        if(!std::getline(stream,row)||row.rfind(prefix,0)!=0) throw Failure("manifest_format");
        return row.substr(prefix.size());
    };
    if(field("")!="ImTiredPressF-release-v1" || field("repository=")!="https://github.com/"+expected) throw Failure("manifest_repository");
    Release r;
    for(int i=0;i<2;++i) {
        auto v=field(i?"core=":"client=");
        if(!std::regex_match(v,std::regex(R"((0|[1-9][0-9]?)\.(0|[1-9][0-9]?)\.(0|[1-9][0-9]?))"))) throw Failure("manifest_version");
        r.version[i].assign(v.begin(),v.end());
    }
    for(int i=0;i<2;++i){r.hash[i]=field(i?"dll_sha256=":"exe_sha256=");if(!std::regex_match(r.hash[i],std::regex("[a-f0-9]{64}"))) throw Failure("manifest_hash");}
    if(std::getline(stream,row)) throw Failure("manifest_format");
    return r;
}
void ValidatePayload(const Release& r,int i,const Bytes& binary,const Bytes& sig)
{
    if(binary.empty() || binary.size()>MaxBinary || Hex(Hash(binary.data(),binary.size()))!=r.hash[i]) throw Failure("update_hash");
    if(i) Verify(binary,sig);
}
namespace {
// Use the opened directory object for every installer leaf. Its name cannot redirect a write.
Handle Leaf(const Directory& dir,const std::wstring& name,ACCESS_MASK access,ULONG share,ULONG disposition,ULONG options,const char* error)
{
    UNICODE_STRING unicode{};unicode.Buffer=const_cast<PWSTR>(name.c_str());
    unicode.Length=static_cast<USHORT>(name.size()*sizeof(wchar_t));unicode.MaximumLength=unicode.Length;
    OBJECT_ATTRIBUTES attr{};InitializeObjectAttributes(&attr,&unicode,OBJ_CASE_INSENSITIVE|0x1000,dir.locks.back().h,nullptr);
    IO_STATUS_BLOCK io{};HANDLE h{};
    auto status=NtCreateFile(&h,access|SYNCHRONIZE,&attr,&io,nullptr,FILE_ATTRIBUTE_NORMAL,share,disposition,
        FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|FILE_OPEN_REPARSE_POINT|options,nullptr,0);
    if(status<0){SetLastError(RtlNtStatusToDosError(status));throw Failure(error);}
    Handle result(h);CheckType(h,false);return result;
}
void Rename(HANDLE file,HANDLE parent,const std::wstring& name)
{
    DWORD len=DWORD(name.size()*sizeof(wchar_t));
    std::vector<BYTE> buf(sizeof(FILE_RENAME_INFO)+len);
    auto info=reinterpret_cast<FILE_RENAME_INFO*>(buf.data());
    info->Flags=FILE_RENAME_FLAG_REPLACE_IF_EXISTS|FILE_RENAME_FLAG_POSIX_SEMANTICS;
    info->RootDirectory=parent;info->FileNameLength=len;memcpy(info->FileName,name.data(),len);
    IO_STATUS_BLOCK ios{};
    NTSTATUS status=NtSetInformationFile(file,&ios,info,static_cast<ULONG>(buf.size()),static_cast<FILE_INFORMATION_CLASS>(65));
    if(status<0){SetLastError(RtlNtStatusToDosError(status));throw Failure("update_replace");}
}
// Keep the staged file handle through the rename. Never open an existing path for writing.
struct Stage {
    std::wstring path;
    Handle file;
    HANDLE parent;
    static std::wstring Name(const Directory& dir){
        std::array<BYTE,32> random{};
        if(BCryptGenRandom(nullptr,random.data(),32,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) throw Failure("crypto");
        auto id=Hex(random);return L".itpf-"+std::wstring(id.begin(),id.end())+L".tmp";
    }
    Stage(const Directory& dir,const Bytes& data):path(Name(dir)),file(Leaf(dir,path,GENERIC_READ|GENERIC_WRITE|DELETE,FILE_SHARE_READ,FILE_CREATE,0,"update_stage")),parent(dir.locks.back().h){
        if(file.h==INVALID_HANDLE_VALUE) throw Failure("update_stage");
        DWORD wrote{};
        if(!WriteFile(file.h,data.data(),static_cast<DWORD>(data.size()),&wrote,nullptr)||wrote!=data.size()||!FlushFileBuffers(file.h)) {
            Discard();throw Failure("update_stage");
        }
    }
    void Discard(){FILE_DISPOSITION_INFO d{TRUE};SetFileInformationByHandle(file.h,FileDispositionInfo,&d,sizeof(d));}
    ~Stage(){if(!path.empty()) Discard();}
    void Commit(const std::wstring& destination) {
        auto name=std::filesystem::path(destination).filename().wstring();
        Rename(file.h,parent,name);
        CloseHandle(file.h);file.h=INVALID_HANDLE_VALUE;
        path.clear();
    }
};
}
void CacheInstaller(const std::wstring& folder,const Bytes& image)
{
    Directory dir(folder,true);
    auto path=dir.path+L"Update_Installer.exe";
    try {
        auto file=Open(path,"installer_missing");
        if(Read(file.h,16*1024*1024)==image)return;
    } catch(const Failure& e) {
        if(std::string(e.what())!="installer_missing")throw;
    }
    Stage copy(dir,image);
    copy.Commit(path);
}
Handle BeginInstaller(const Directory& dir)
{
    return Leaf(dir,L".itpf-installer.lock",GENERIC_READ|DELETE,0,FILE_CREATE,FILE_DELETE_ON_CLOSE,"update_busy");
}
void Install(const std::wstring& folder,const Release& release,int i,const Bytes& bytes,const Bytes& sig,const std::wstring& running,const std::function<bool()>& activate)
{
    Directory dir(folder,true);ValidatePayload(release,i,bytes,sig);
    const auto dest=dir.path+(i?L"ImTiredPressF.dll":L"ImTiredPressF.exe");
    // Serialize installers across processes. The lock is scoped to this exact installation.
    Handle gate=Leaf(dir,L".itpf-update.lock",GENERIC_READ|DELETE,0,FILE_CREATE,FILE_DELETE_ON_CLOSE,"update_busy");
    Stage binary(dir,bytes);
    // Version resources are read from the staged PE as data. The signed hash binds those bytes.
    if(PeVersion(bytes)!=release.version[i]) throw Failure("pe_version");
    Bytes oldRecord,oldBinary;
    {
        auto old=Leaf(dir,i?L"ImTiredPressF.dll":L"ImTiredPressF.exe",i?GENERIC_READ:GENERIC_READ|DELETE,FILE_SHARE_READ,FILE_OPEN,0,"installed_missing");auto oldData=Read(old.h,MaxBinary);auto oldVersion=PeVersion(oldData);
        if(VersionFeed::Number(release.version[i])<=VersionFeed::Number(oldVersion) ||
           (!i && VersionFeed::Number(release.version[i])<=VersionFeed::Number(running))) throw Failure("downgrade");
        if(!i) {
            // A running process retains its image section. Keep a recovery copy
            // while publishing the staged and verified executable under its normal name.
            const auto parent=dir.locks.back().h;
            Rename(old.h,parent,L"ImTiredPressF.previous.exe");
            try {binary.Commit(dest);}
            catch(...) {Rename(old.h,parent,L"ImTiredPressF.exe");throw;}
            return;
        }
        {
            auto side=Leaf(dir,L"ImTiredPressF.dll.sig",GENERIC_READ,FILE_SHARE_READ,FILE_OPEN,0,"signature_missing");auto records=Read(side.h,208);oldBinary=std::move(oldData);
            Verify(oldBinary,records);
            auto digest=Hash(oldBinary.data(),oldBinary.size());
            for(size_t at=0;at<records.size();at+=104) if(!memcmp(records.data()+at+8,digest.data(),32)) {
                oldRecord.assign(records.begin()+at,records.begin()+at+104);break;
            }
        }
    }
    if(sig.size()!=104) throw Failure("untrusted_signature");
    if (!activate) {
        Stage previousSignature(dir,oldRecord);previousSignature.Commit(L"ImTiredPressF.previous.dll.sig");
        Stage previousCore(dir,oldBinary);previousCore.Commit(L"ImTiredPressF.previous.dll");
    }
    // Old and new both verify across either side of the atomic binary rename, including a crash.
    Bytes transition=oldRecord;transition.insert(transition.end(),sig.begin(),sig.end());
    {Stage side(dir,transition);side.Commit(dest+L".sig");}
    try {binary.Commit(dest);}
    catch(...) {try{Stage rollback(dir,oldRecord);rollback.Commit(dest+L".sig");}catch(...){} throw;}
    if(activate && !activate()){
        Stage rollback(dir,oldBinary);rollback.Commit(dest);
        Stage oldSide(dir,oldRecord);oldSide.Commit(dest+L".sig");throw Failure("core_activation_failed");
    }
    try{Stage finalSide(dir,sig);finalSide.Commit(dest+L".sig");}catch(...){/* Transition is valid for the committed binary. */}
}
const wchar_t* Message(const std::string& c){
    if(c=="dll_missing")return L"DLL 파일 없음";
    if(c=="signature_missing")return L"서명 파일 없음";
    if(c=="integrity")return L"DLL 무결성 검증 실패";
    if(c=="untrusted_signature")return L"신뢰할 수 없는 서명";
    if(c=="directory_lock" || c=="unsafe_path" || c=="file_lock")return L"경로 또는 파일 잠금 실패";
    if(c=="downgrade")return L"동일·구버전 업데이트 거부";
    if(c.rfind("load_failed",0)==0 || c=="already_mapped")return L"DLL 로드 실패";
    if(c=="update_replace")return L"파일 교체 실패";
    return L"검증 실패";
}
}
