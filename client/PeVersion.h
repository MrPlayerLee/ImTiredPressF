#pragma once
#include "Trust.h"
#include <cstring>
namespace Trust {
// Read RT_VERSION from file bytes without mapping the image or running loader callbacks.
inline std::wstring PeVersion(const Bytes& bytes)
{
    auto fail=[](){throw Failure("pe_version");};
    auto copy=[&](size_t at,void* dst,size_t n){if(at>bytes.size() || n>bytes.size()-at)fail();memcpy(dst,bytes.data()+at,n);};
    IMAGE_DOS_HEADER dos{};copy(0,&dos,sizeof(dos));
    if(dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0)fail();
    size_t pe=static_cast<size_t>(dos.e_lfanew);IMAGE_NT_HEADERS64 nt{};copy(pe,&nt,sizeof(nt));
    if(nt.Signature!=IMAGE_NT_SIGNATURE || nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
       nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_RESOURCE)fail();
    auto raw=[&](DWORD rva,size_t size)->size_t {
        size_t section=pe+offsetof(IMAGE_NT_HEADERS64,OptionalHeader)+nt.FileHeader.SizeOfOptionalHeader;
        for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i){
            IMAGE_SECTION_HEADER s{};copy(section+i*sizeof(s),&s,sizeof(s));
            if(rva<s.VirtualAddress)continue;
            size_t offset=rva-s.VirtualAddress;if(offset>s.SizeOfRawData || size>s.SizeOfRawData-offset)continue;
            size_t result=s.PointerToRawData+offset;if(result>bytes.size() || size>bytes.size()-result)fail();return result;
        }
        fail();return 0;
    };
    auto resource=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];size_t root=raw(resource.VirtualAddress,resource.Size);
    auto readEntry=[&](size_t offset,void* out,size_t n){if(offset>resource.Size || n>resource.Size-offset)fail();copy(root+offset,out,n);};
    size_t next=0;
    for(unsigned level=0;level<3;++level){
        IMAGE_RESOURCE_DIRECTORY dir{};readEntry(next,&dir,sizeof(dir));
        size_t start=next+sizeof(dir);bool found=false;
        for(unsigned i=0;i<unsigned(dir.NumberOfNamedEntries)+dir.NumberOfIdEntries;++i){
            IMAGE_RESOURCE_DIRECTORY_ENTRY e{};readEntry(start+i*sizeof(e),&e,sizeof(e));
            if(level==0 && (e.NameIsString || e.Id!=16))continue;
            if(bool(e.DataIsDirectory)!=(level<2))fail();
            next=e.OffsetToData&0x7fffffffU;found=true;break;
        }
        if(!found)fail();
    }
    IMAGE_RESOURCE_DATA_ENTRY entry{};readEntry(next,&entry,sizeof(entry));
    constexpr wchar_t key[]=L"VS_VERSION_INFO";
    constexpr size_t fixed=(6+sizeof(key)+3)&~size_t(3);
    if(entry.Size<fixed+sizeof(VS_FIXEDFILEINFO))fail();
    size_t info=raw(entry.OffsetToData,entry.Size);WORD header[3]{};copy(info,header,sizeof(header));
    if(header[0]>entry.Size || header[0]<fixed+sizeof(VS_FIXEDFILEINFO) || header[1]!=sizeof(VS_FIXEDFILEINFO) ||
       memcmp(bytes.data()+info+6,key,sizeof(key)))fail();
    VS_FIXEDFILEINFO v{};copy(info+fixed,&v,sizeof(v));
    unsigned a=HIWORD(v.dwFileVersionMS),b=LOWORD(v.dwFileVersionMS),c=HIWORD(v.dwFileVersionLS);
    if(v.dwSignature!=VS_FFI_SIGNATURE || LOWORD(v.dwFileVersionLS) || a>99 || b>99 || c>99)fail();
    return std::to_wstring(a)+L"."+std::to_wstring(b)+L"."+std::to_wstring(c);
}
}
