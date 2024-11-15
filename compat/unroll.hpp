#pragma once

namespace floormat {

template<uint32_t Max, typename F>
constexpr CORRADE_ALWAYS_INLINE void unroll(F&& fn)
{
    [&]<size_t... Is>(std::index_sequence<Is...>) {
        (..., fn(std::integral_constant<size_t, Is>{}));
    }(std::make_index_sequence<Max>());
}

} // namespace floormat
