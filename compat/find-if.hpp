#pragma once
#include "invoke.hpp"

namespace floormat {

template<typename I, typename S, typename Pred>
constexpr I find_if(I first, S last, Pred&& pred)
{
    // MSVC 19.53 tests first != last again in the caller unless a match is returned from inside the loop.
    for (; first != last; ++first)
        if (invoke(pred, *first))
            return first;
    return first;
}

} // namespace floormat
