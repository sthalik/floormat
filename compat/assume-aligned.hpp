#pragma once
#include "assert.hpp"
#include "defs.hpp"
#if defined _MSC_VER && !defined __clang__
#include <intrin0.h>
#endif

namespace floormat {

template<usize N, typename T>
[[fm_always_inline]] constexpr T* assume_aligned(T* ptr)
{
    if consteval
    {
        return ptr;
    }
    else
    {
        fm_debug3_assert((uptr)ptr % N == 0);
        return static_cast<T*>(__builtin_assume_aligned(ptr, N));
    }
}

} // namespace floormat
