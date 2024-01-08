#pragma once
#include "compat/integer-types.hpp"
#include <type_traits>
#include <array>
#include <concepts>
#include <Corrade/Utility/Macros.h>

namespace floormat::detail::map {

template<typename T, typename F, size_t N, size_t... Indexes>
CORRADE_ALWAYS_INLINE
constexpr auto map0(const std::array<T, N>& array, const F& fun, std::index_sequence<Indexes...>)
{
    return std::array { fun(array[Indexes])... };
}

template<typename T, typename F>
[[deprecated("zero-length array!")]]
CORRADE_ALWAYS_INLINE
constexpr auto map0(const std::array<T, 0>&, const F&, std::index_sequence<>)
{
    return std::array<std::decay_t<std::invoke_result_t<std::decay_t<F>, const std::remove_cvref_t<T>&>>, 0>{};
}

} // namespace floormat::detail::map

namespace floormat {

template<typename T, std::invocable<const T&> F, size_t N>
constexpr auto map(const std::array<T, N>& array, const F& fun)
{
    using return_type = std::decay_t<decltype( fun(array[0]) )>;
    static_assert(!std::is_same_v<return_type, void>);
    static_assert(std::is_same_v<T, std::decay_t<T>>);
    static_assert(sizeof(return_type) > 0);
    using ::floormat::detail::map::map0;
    return map0(array, fun, std::make_index_sequence<N>{});
}

} // namespace floormat
