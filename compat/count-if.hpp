#pragma once
#include "range-access.hpp"
#include "invoke.hpp"

namespace floormat {

template<typename R, typename Pred> requires range<R>
constexpr u32 count_if(R&& r, Pred&& pred)
{
    u32 n = 0;
    for (auto&& x : r)
        if (invoke(pred, x))
            n++;
    return n;
}

} // namespace floormat
