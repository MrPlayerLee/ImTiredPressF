#pragma once

namespace ClientVariant {
inline constexpr const wchar_t* File(bool mcp) {
    return mcp ? L"ImTiredPressF(MCP).exe" : L"ImTiredPressF.exe";
}
}
