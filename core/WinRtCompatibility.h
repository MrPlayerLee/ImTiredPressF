#pragma once
// Windows SDK C++/WinRT includes experimental/coroutine under C++17, even
// when this program only uses the blocking .get() API. New MSVC versions
// require explicitly acknowledging that SDK compatibility path.
#ifndef _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
#endif
