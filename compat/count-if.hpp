#pragma once
#include "range-access.hpp"

namespace floormat {

template<typename R, typename Pred> requires range<R>
constexpr u32 count_if(R&& r, Pred&& pred)
{
    u32 n = 0;
    for (auto&& x : r)
        if (pred(x))
            n++;
    return n;
}

} // namespace floormat
