#pragma once
#include <bit>
#include <cr/Pair.h>
#include <cr/StructuredBindings.h>

namespace floormat {

// signed >> rounds toward -inf since C++20
template<int D>
constexpr Pair<int, int> floor_divmod(int x)
{
    static_assert(D > 0);
    if constexpr(std::has_single_bit((unsigned)D))
        return { x >> std::countr_zero((unsigned)D), x & (D-1) };
    else
    {
        int q = x / D, r = x % D;
        if (r < 0)
        {
            q--;
            r += D;
        }
        return { q, r };
    }
}

template<int D>
constexpr int floor_div(int x)
{
    return floor_divmod<D>(x).first();
}

} // namespace floormat
