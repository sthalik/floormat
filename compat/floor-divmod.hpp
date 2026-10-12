#pragma once
#include <bit>
#include <cr/Pair.h>
#include <cr/StructuredBindings.h>
#include <mg/Vector2.h>

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
constexpr Pair<Vector2i, Vector2i> floor_divmod(Vector2i v)
{
    const auto [qx, rx] = floor_divmod<D>(v.x());
    const auto [qy, ry] = floor_divmod<D>(v.y());
    return { {qx, qy}, {rx, ry} };
}

template<int D>
constexpr int floor_div(int x)
{
    return floor_divmod<D>(x).first();
}

constexpr Pair<int, int> floor_divmod(int x, int d)
{
    if (std::has_single_bit((unsigned)d))
        return { x >> std::countr_zero((unsigned)d), x & (d-1) };
    int q = x / d, r = x % d;
    if (r < 0)
    {
        q--;
        r += d;
    }
    return { q, r };
}

constexpr int floor_div(int x, int d)
{
    return floor_divmod(x, d).first();
}

} // namespace floormat
