#pragma once
#include "range-access.hpp"

namespace floormat {

template<typename R, typename Pred> requires range<R>
constexpr bool all_of(R&& r, Pred&& pred)
{
    for (auto&& x : r)
        if (!pred(x))
            return false;
    return true;
}

} // namespace floormat
