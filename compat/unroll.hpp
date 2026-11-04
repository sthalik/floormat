#pragma once
#include <utility>

namespace floormat {

template<u32 Max, typename F>
constexpr CORRADE_ALWAYS_INLINE void unroll(F&& fn)
{
    [&]<usize... Is>(std::index_sequence<Is...>) {
        (..., fn(std::integral_constant<usize, Is>{}));
    }(std::make_index_sequence<Max>());
}

} // namespace floormat
