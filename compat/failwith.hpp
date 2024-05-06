#pragma once
#include "compat/assert.hpp"
#if !defined __GNUG__ && !defined _MSC_VER
#include <utility>
#endif

namespace floormat {

template<typename T> T failwith(const char* str)
{
    fm_abort("%s", str);
#ifdef __GNUG__
    __builtin_unreachable();
#elif defined _MSC_VER
    __assume(false);
#else
    std::unreachable();
#endif
}

} // namespace floormat
