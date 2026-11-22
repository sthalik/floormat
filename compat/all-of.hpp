#pragma once
#include "range-access.hpp"
#include "invoke.hpp"

namespace floormat {

template<typename R, typename Pred> requires range<R>
constexpr bool all_of(R&& r, Pred&& pred)
{
    for (auto&& x : r)
        if (!invoke(pred, x))
            return false;
    return true;
}

} // namespace floormat
