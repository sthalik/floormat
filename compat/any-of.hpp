#pragma once
#include "range-access.hpp"

namespace floormat {

template<typename R, typename Pred> requires range<R>
constexpr bool any_of(R&& r, Pred&& pred)
{
    for (auto&& x : r)
        if (pred(x))
            return true;
    return false;
}

} // namespace floormat
