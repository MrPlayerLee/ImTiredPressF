#include "../client/VersionFeed.h"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv)
{
    if(argc!=2) return 1;
    using namespace VersionFeed;
    std::ifstream file(argv[1]); std::string manifest{std::istreambuf_iterator<char>(file),{}};
    if(!Parse(manifest).valid) return 2;
    auto ok=Parse("client = 1.20.30\r\ncore = 99.99.99\r\n");
    if(!ok.valid || ok.version[0]!=L"1.20.30" || ok.version[1]!=L"99.99.99") return 3;
    if(Number(L"1.20.30")<=Number(L"1.9.99") || Number(L"99.99.99")!=999999) return 4;
    for(const std::string bad:{"1.100.0","1.0.100","1.02.3","1.-2.3","1.2","1.2.3x"})
        if(Parse("client = "+bad+"\ncore = 1.2.3").valid) return 5;
    if(Parse("client = 1.2.3\nclient = 1.2.3\ncore = 1.0.0").valid || Parse("client = 1.0.0").valid) return 6;
    std::cout<<"PASS: GitHub version manifest parsing and numeric comparison.\n";
}
