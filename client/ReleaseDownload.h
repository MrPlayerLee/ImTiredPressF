#pragma once
#include "Trust.h"
namespace Download {
struct Package {Trust::Release release;Trust::Bytes binary,signature;};
Trust::Release Latest(bool mcp);
Package Component(int index, const Trust::Release& release);
}
