#pragma once
#include "range-access.hpp"

namespace floormat {

template<typename R, typename T> requires range<R>
constexpr bool contains(R&& r, const T& value)
{
    for (const auto& x : r)
        if (x == value)
            return true;
    return false;
}

} // namespace floormat
