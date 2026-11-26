#pragma once
#include <string>
#include <regex>
#include <sstream>
namespace VersionFeed {
struct Result { std::wstring version[2]; bool valid=false; };
inline Result Failed(const wchar_t* reason) {return {{reason,reason},false};}
inline Result Parse(const std::string& body) {
    const std::regex field(R"(^(client|core) = ((0|[1-9][0-9]?)\.(0|[1-9][0-9]?)\.(0|[1-9][0-9]?))$)");
    std::istringstream lines(body); std::string row; Result result;
    while(std::getline(lines,row)) {
        if(!row.empty() && row.back()=='\r') row.pop_back();
        if(row.rfind("client = ",0)!=0 && row.rfind("core = ",0)!=0) continue;
        std::smatch m; if(!std::regex_match(row,m,field)) return Failed(L"형식 오류");
        auto& value=result.version[m[1]=="client"?0:1];
        if(!value.empty()) return Failed(L"형식 오류");
        auto v=m[2].str(); value.assign(v.begin(),v.end());
    }
    if(result.version[0].empty() || result.version[1].empty()) return Failed(L"형식 오류");
    result.valid=true; return result;
}
inline int Number(const std::wstring& v)
{
    unsigned a,b,c;
    if(swscanf_s(v.c_str(),L"%u.%u.%u",&a,&b,&c)!=3) return -1;
    return int(a*10000+b*100+c);
}
}
