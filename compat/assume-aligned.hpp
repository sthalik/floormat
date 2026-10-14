#pragma once
#include "assert.hpp"
#include "defs.hpp"

namespace floormat {

template<size_t N, typename T>
[[fm_always_inline]] constexpr T* assume_aligned(T* ptr)
{
    if consteval
    {
        return ptr;
    }
    else
    {
        fm_debug3_assert((uintptr_t)ptr % N == 0);
        return static_cast<T*>(__builtin_assume_aligned(ptr, N));
    }
}

} // namespace floormat
