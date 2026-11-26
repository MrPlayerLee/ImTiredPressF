#pragma once
#include <windows.h>
#include <cstdio>
#include <cwchar>

namespace Diagnostics {
// Keep diagnostics independent of the optional DLL and preserve callers' Win32 errors.
inline void Write(const char* operation, DWORD code=0) noexcept {
    DWORD saved=GetLastError();
    wchar_t path[32768]{};
    DWORD count=GetEnvironmentVariableW(L"LOCALAPPDATA",path,_countof(path));
    if(count && count<_countof(path)-40) {
        wcscat_s(path,L"\\ImTiredPressF");
        CreateDirectoryW(path,nullptr);
        wcscat_s(path,L"\\ImTiredPressF.log");
        HANDLE file=CreateFileW(path,FILE_APPEND_DATA|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,
                                nullptr,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(file!=INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION info{};
            if(GetFileInformationByHandle(file,&info) && info.nNumberOfLinks==1 &&
               !(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) {
                SYSTEMTIME now{};GetSystemTime(&now);
                SYSTEMTIME local{};
                char localTime[48]{};
                if (SystemTimeToTzSpecificLocalTime(nullptr, &now, &local))
                    sprintf_s(localTime, " local=%04u-%02u-%02uT%02u:%02u:%02u",
                              local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
                char line[512]{};
                int n=_snprintf_s(line,sizeof(line),_TRUNCATE,
                    "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ%s pid=%lu tid=%lu %s code=%lu (0x%08lX)%s\r\n",
                    now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds,
                    localTime,GetCurrentProcessId(),GetCurrentThreadId(),operation,code,code,
                    code==ERROR_HOTKEY_ALREADY_REGISTERED?" ERROR_HOTKEY_ALREADY_REGISTERED":"");
                DWORD written{};
                if(n>0)WriteFile(file,line,n,&written,nullptr);
            }
            CloseHandle(file);
        }
    }
    SetLastError(saved);
}
}

// Polling and rendering sites can be reached every 16 ms. Report changed errors
// immediately and repeat persistent identical ones at most once per five minutes.
#define ITPF_DIAG(event, value) do { \
    const DWORD diagCode=(value); static thread_local DWORD priorCode; \
    static thread_local ULONGLONG nextReport; const auto nowTick=GetTickCount64(); \
    if(nowTick>=nextReport || priorCode!=diagCode) { \
        priorCode=diagCode; nextReport=nowTick+300000; Diagnostics::Write(event,diagCode); \
    } \
} while(false)
