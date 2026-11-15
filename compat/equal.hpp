#pragma once
#include "range-access.hpp"

namespace floormat {

template<typename A, typename B> requires sized_range<A> && sized_range<B>
constexpr bool equal(A&& a, B&& b)
{
    if (floormat::size(a) != floormat::size(b))
        return false;
    auto it = floormat::begin(b);
    for (const auto& x : a)
    {
        if (!(x == *it))
            return false;
        ++it;
    }
    return true;
}

} // namespace floormat
