#pragma once
#include "compat/array-size.hpp"
#include <type_traits>
#include <array>

namespace floormat::detail::map {

template<typename Out, typename F, typename A, size_t... Indexes>
CORRADE_ALWAYS_INLINE
constexpr auto map0(const F& fun, const A& array, std::index_sequence<Indexes...>)
{
    using out_element = std::remove_cvref_t<decltype( array_traits<Out>::get(std::declval<const Out&>(), 0) )>;
    return array_traits<Out>::make(static_cast<out_element>(fun(array_traits<A>::get(array, Indexes)))...);
}

template<typename Out, typename F, typename A>
[[deprecated("zero-length array!")]]
CORRADE_ALWAYS_INLINE
constexpr auto map0(const F&, const A&, std::index_sequence<>)
{
    return Out{};
}

} // namespace floormat::detail::map

namespace floormat {

template<typename Out = void, typename F, typename A>
requires requires (const F& fun, const A& array) { fun(array_traits<std::remove_cvref_t<A>>::get(array, 0)); }
constexpr auto map(const F& fun, const A& array)
{
    using traits = array_traits<std::remove_cvref_t<A>>;
    using result = std::decay_t<decltype( fun(traits::get(array, 0)) )>;
    using out = std::conditional_t<std::is_void_v<Out>, typename traits::template rebind<result>, Out>;
    static_assert(!std::is_same_v<result, void>);
    static_assert(sizeof(result));
    static_assert(static_array_size<out> == traits::size, "output container has the wrong size");
    using ::floormat::detail::map::map0;
    return map0<out>(fun, array, std::make_index_sequence<traits::size>{});
}

} // namespace floormat
