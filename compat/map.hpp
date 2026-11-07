#pragma once
#include "array-size.hpp"
#include <type_traits>
#include <utility>

namespace floormat::detail_map {

template<typename T> constexpr inline bool is_std_array = false;
template<typename T, usize N> constexpr inline bool is_std_array<std::array<T, N>> = true;

// MSVC keeps the stores that zero transform's default-constructed elements, though the loop overwrites each one.
#if defined _MSC_VER && !defined __clang__
constexpr inline bool transform_needs_trivial_ctor = true;
#else
constexpr inline bool transform_needs_trivial_ctor = false;
#endif

template<typename Out, typename F, typename A, usize... Indexes>
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

// A loop leaves unrolling to the compiler. map0's pack expansion calls fun once per element in straight-line
// code, so a fun that makes a call puts N call instructions in the caller.
template<typename Out, typename F, typename A>
CORRADE_ALWAYS_INLINE
constexpr Out transform(const F& fun, const A& array)
{
    Out ret;
    for (usize i = 0; i < array_traits<A>::size; i++)
        ret[i] = static_cast<typename Out::value_type>(fun(array_traits<A>::get(array, i)));
    return ret;
}

} // namespace floormat::detail_map

namespace floormat {

template<typename Out = void, typename F, typename A>
requires requires (const F& fun, const A& array) { fun(array_traits<std::remove_cvref_t<A>>::get(array, 0)); }
CORRADE_ALWAYS_INLINE
constexpr auto map(const F& fun, const A& array)
{
    using traits = array_traits<std::remove_cvref_t<A>>;
    using result = std::decay_t<decltype( fun(traits::get(array, 0)) )>;
    using out = std::conditional_t<std::is_void_v<Out>, typename traits::template rebind<result>, Out>;
    static_assert(!std::is_same_v<result, void>);
    static_assert(sizeof(result));
    static_assert(static_array_size<out> == traits::size, "output container has the wrong size");
    if constexpr (detail_map::is_std_array<out> && traits::size > 0 && std::is_default_constructible_v<out> &&
                  std::is_move_assignable_v<out> &&
                  (!detail_map::transform_needs_trivial_ctor || std::is_trivially_default_constructible_v<out>))
        return detail_map::transform<out>(fun, array);
    else
        return detail_map::map0<out>(fun, array, std::make_index_sequence<traits::size>{});
}

} // namespace floormat
