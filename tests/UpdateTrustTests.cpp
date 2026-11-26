#include "../client/Trust.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <functional>
namespace fs=std::filesystem;
using Trust::Bytes;
Bytes ReadBytes(const fs::path& path){std::ifstream f(path,std::ios::binary);return Bytes(std::istreambuf_iterator<char>(f),{});}
void Expect(const char* code,const std::function<void()>& fn){
    try{fn();}catch(const Trust::Failure& e){if(std::string(e.what())==code)return;throw;}
    throw std::runtime_error(std::string("Expected rejection: ")+code);
}
struct RunningFixture {
    PROCESS_INFORMATION process{};
    explicit RunningFixture(const fs::path& image){
        STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
        if(!CreateProcessW(image.c_str(),nullptr,nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process))throw std::runtime_error("Cannot launch test client");
        WaitForInputIdle(process.hProcess,5000);
    }
    ~RunningFixture(){TerminateProcess(process.hProcess,0);WaitForSingleObject(process.hProcess,5000);CloseHandle(process.hThread);CloseHandle(process.hProcess);}
};
int wmain(int argc,wchar_t** argv){
    try{
        if(argc!=2)return 1;fs::path base=fs::absolute(argv[1]), dir=base/"install";
        fs::create_directories(dir);auto copy=[&](const char* from,const char* to){fs::copy_file(base/from,dir/to,fs::copy_options::overwrite_existing);};
        copy("1.exe","ImTiredPressF.exe");copy("1.dll","ImTiredPressF.dll");copy("1.dll.sig","ImTiredPressF.dll.sig");
        RunningFixture liveClient(dir/"ImTiredPressF.exe");
        auto doc=ReadBytes(base/"manifest"),sig=ReadBytes(base/"manifest.sig");
        Expect("manifest_repository",[&]{Trust::VerifyManifest(doc,sig,true);});
        Expect("manifest_repository",[&]{Trust::VerifyManifest(ReadBytes(base/"mcp.manifest"),ReadBytes(base/"mcp.manifest.sig"));});
        auto release=Trust::VerifyManifest(doc,sig);auto badDoc=doc;badDoc.back()^=3;
        Expect("integrity",[&]{Trust::VerifyManifest(badDoc,sig);});auto badSig=sig;badSig.back()^=1;
        Expect("untrusted_signature",[&]{Trust::VerifyManifest(doc,badSig);});
        for(int i:{0,1}){
            const char* name=i?"ImTiredPressF.dll":"ImTiredPressF.exe";
            Bytes before=ReadBytes(dir/name), binary=ReadBytes(base/(i?"2.dll":"2.exe")), side=i?ReadBytes(base/"2.dll.sig"):Bytes{};
            auto apply=[&](const Bytes& data,const Bytes& signature,const std::function<bool()>& activate={}){
                Trust::Install(dir.wstring(),release,i,data,signature,L"1.20.1",activate);
            };
            if(!i)Expect("downgrade",[&]{Trust::Install(dir.wstring(),release,i,binary,side,L"1.20.3");});
            auto corrupt=binary;corrupt.back()^=1;Expect("update_hash",[&]{apply(corrupt,side);});
            if(i){auto wrong=side;wrong.back()^=1;Expect("untrusted_signature",[&]{apply(binary,wrong);});
                Expect("core_activation_failed",[&]{apply(binary,side,[]{return false;});});
                Trust::Verify(ReadBytes(dir/name),ReadBytes(dir/"ImTiredPressF.dll.sig"));}
            if(ReadBytes(dir/name)!=before)throw std::runtime_error("Failed update changed installed file");
            apply(binary,side);if(ReadBytes(dir/name)!=binary)throw std::runtime_error("Verified update did not install");
            if(!i && ReadBytes(dir/"ImTiredPressF.previous.exe")!=before)throw std::runtime_error("Missing recovery image");
            Expect("downgrade",[&]{apply(binary,side);});
        }
        std::cout<<"PASS verified EXE/core install, manifest signature, tamper rejection, activation rollback, version floor.\n";
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<" (Win32 "<<GetLastError()<<")\n";return 1;}
}
